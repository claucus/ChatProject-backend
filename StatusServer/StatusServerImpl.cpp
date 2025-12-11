#include "StatusServerImpl.h"
#include "RedisConPool.h"
#include "ConfigManager.h"
#include "ChatGrpcClient.h"
#include "Logger.h"
#include "const.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h> // optional: if available for HMAC; otherwise replace with your own
#include <algorithm>
#include <fstream>

static long long now_seconds() 
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string load_file(const std::string& path) {
    std::ifstream ifs(path);

    if (!ifs) {
        return std::string();
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

StatusServerImpl::StatusServerImpl() 
{
    LOG_INFO("StatusServerImpl initializing");
    // nothing else for now
    _atomicAssignScript = load_file("scripts/atomic_assign_session.lua");
    if (_atomicAssignScript.empty()) {
        LOG_WARN("Could not load lua script");
    }
    else {
        LOG_INFO("loaded lua script ,{} bytes", _atomicAssignScript.size());
    }
}

StatusServerImpl::~StatusServerImpl() 
{
    LOG_INFO("StatusServerImpl shutting down");
}

// ------------ Node management helpers ------------
void StatusServerImpl::addOrUpdateNode(const status::RegisterNodeReq& req) 
{
    std::lock_guard<std::mutex> g(_nodesMutex);
    auto it = _nodes.find(req.name());
    long long ts = now_seconds();
    if (it == _nodes.end()) {
        auto n = std::make_shared<NodeInfo>();
        n->name = req.name();
        n->host = req.server_host();
        n->port = req.server_port();
        n->capacity = req.capacity() > 0 ? req.capacity() : 100;
        n->current_load = 0;
        n->last_heartbeat = ts;
        _nodes[n->name] = n;
    } else {
        auto n = it->second;
        n->host = req.server_host();
        n->port = req.server_port();
        n->capacity = req.capacity() > 0 ? req.capacity() : n->capacity;
        n->last_heartbeat = ts;
    }
    rebuildNodeListLocked();
}

void StatusServerImpl::removeNode(const std::string& name) 
{
    std::lock_guard<std::mutex> g(_nodesMutex);
    auto it = _nodes.find(name);
    if (it != _nodes.end()) _nodes.erase(it);
    rebuildNodeListLocked();
}

void StatusServerImpl::rebuildNodeListLocked() 
{
    _nodeList.clear();
    _nodeList.reserve(_nodes.size());
    for (auto &p : _nodes) _nodeList.push_back(p.second);
    // keep deterministic ordering
    std::sort(_nodeList.begin(), _nodeList.end(),
        [](const std::shared_ptr<NodeInfo>& a, const std::shared_ptr<NodeInfo>& b){
            return a->name < b->name;
        }
    );
}

// Evict nodes not updated recently
void StatusServerImpl::evictStaleNodes(long long nowSec) 
{
    std::vector<std::string> toRemove;
    {
        std::lock_guard<std::mutex> g(_nodesMutex);
        for (auto &kv : _nodes) {
            auto n = kv.second;
            long long last = n->last_heartbeat.load();
            if (nowSec - last > NODE_STALE_THRESHOLD_SEC) {
                toRemove.push_back(kv.first);
            }
        }
        for (auto &name : toRemove) _nodes.erase(name);
        if (!toRemove.empty()) rebuildNodeListLocked();
    }
    if (!toRemove.empty()) {
        LOG_WARN("Evicted stale nodes: {}", toRemove.size());
    }
}

// ------------ Token generation (HMAC using secret from Config) ------------
std::string StatusServerImpl::generateToken(const std::string& uid, const std::string& serverName, int ttlSeconds)
{
    // Very simple token format: uid|server|expires|hmac
    // Replace with JWT or robust HMAC in production.
    auto& cfg = ConfigManager::GetInstance();
    std::string secret = cfg["StatusServer"]["token_secret"];
    long long expires = now_seconds() + ttlSeconds;

    std::ostringstream oss;
    oss << uid << "|" << serverName << "|" << expires;
    std::string payload = oss.str();

    // compute HMAC-SHA256 if OpenSSL available else use simple salted hash
    unsigned char* digest = nullptr;
    unsigned int len = 0;
#ifdef OPENSSL_VERSION_TEXT
    digest = HMAC(EVP_sha256(),
                  secret.data(), (int)secret.size(),
                  reinterpret_cast<const unsigned char*>(payload.data()), payload.size(),
                  nullptr, &len);
    std::ostringstream hex;
    for (unsigned int i = 0; i < len; ++i) {
        hex << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return payload + "|" + hex.str();
#else
    // fallback simple: append secret reversed (NOT SECURE — replace in prod)
    std::string fake = payload + "|" + std::string(secret.rbegin(), secret.rend());
    return fake;
#endif
}

bool StatusServerImpl::validateToken(const std::string& token, const std::string& uid, const std::string& serverName) 
{
    // Minimal validation: check payload prefix uid|serverName
    // In production parse HMAC/JWT and verify signature + expiry
    if (token.empty()) return false;
    if (token.find(uid + "|" + serverName + "|") != 0) return false;
    return true;
}

// ------------ Redis helpers for user route ------------
bool StatusServerImpl::persistUserRouteToRedis(const std::string& uid, const json& j) 
{
    try {
        auto& redis = RedisConPool::GetInstance();
        std::string key = StatusServerCode::UserRouteKey + uid;
        redis.set(key, j.dump());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("persistUserRouteToRedis exception: {}", e.what());
        return false;
    }
}

bool StatusServerImpl::readUserRouteFromRedis(const std::string& uid, json& out) 
{
    try {
        auto& redis = RedisConPool::GetInstance();
        auto val = redis.get(StatusServerCode::UserRouteKey + uid);
        if (val->empty()) return false;
        out = json::parse(*val);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("readUserRouteFromRedis exception: {}", e.what());
        return false;
    }
}

bool StatusServerImpl::removeUserRouteFromRedis(const std::string& uid) 
{
    try {
        auto& redis = RedisConPool::GetInstance();
        redis.del(StatusServerCode::UserRouteKey + uid);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("removeUserRouteFromRedis exception: {}", e.what());
        return false;
    }
}

// -------------------- RPC Implementations --------------------

grpc::Status StatusServerImpl::RegisterNode(grpc::ServerContext*, const status::RegisterNodeReq* req, status::RegisterNodeResp* resp)
{
    if (!req) {
        resp->set_error(1);
        return grpc::Status::OK;
    }
    LOG_INFO("RegisterNode: {} {}:{}", req->name(), req->server_host(), req->server_port());
    addOrUpdateNode(*req);

    // optionally persist node info to Redis for cross-process visibility
    try {
        json j;
        j["name"] = req->name();
        j["host"] = req->server_host();
        j["port"] = req->server_port();
        j["capacity"] = req->capacity();
        j["last_heartbeat"] = now_seconds();
        RedisConPool::GetInstance().set(StatusServerCode::NodeKey + req->name(), j.dump());
    } catch (...) {
        LOG_WARN("Could not persist node info to Redis");
    }

    resp->set_error(0);
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::DeregisterNode(grpc::ServerContext*, const status::DeregisterNodeReq* req, status::DeregisterNodeResp* resp)
{
    if (!req) {
        resp->set_error(1);
        return grpc::Status::OK;
    }

    LOG_INFO("DeregisterNode: {}", req->name());
    removeNode(req->name());
    try { 
        RedisConPool::GetInstance().del(StatusServerCode::NodeKey + req->name()); 
    } catch(...) {
        
    }
    resp->set_error(0);
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::Heartbeat(grpc::ServerContext*, const status::HeartbeatReq* req, status::HeartbeatResp* resp)
{
    if (!req) {
        resp->set_error(1);
        return grpc::Status::OK;
    }
    LOG_DEBUG("Heartbeat from {}:{}", req->server_host(), req->server_port());
    // Update node by matching host+port or name (if provided)
    std::lock_guard<std::mutex> g(_nodesMutex);
    bool found = false;
    for (auto &p : _nodes) {
        auto n = p.second;
        if (n->host == req->server_host() && n->port == req->server_port()) {
            n->last_heartbeat = now_seconds();
            found = true;
            break;
        }
    }
    if (!found) {
        // optional: add a fallback node entry
        LOG_WARN("Heartbeat from unknown node {}:{}", req->server_host(), req->server_port());
    }
    resp->set_error(0);
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::AllocateServer(grpc::ServerContext*, const status::AllocateServerReq* req, status::AllocateServerResp* resp) 
{
    if (!req) {
        resp->set_error(1);
        return grpc::Status::OK;
    }

    // Periodically evict stale nodes
    evictStaleNodes(now_seconds());

    std::string uid = req->uid();
    // 1) check sticky assignment
    json current;
    if (readUserRouteFromRedis(uid, current)) {
        bool assigned = current.value("assigned", false);
        if (assigned) {
            std::string assignedName = current.value("server_name", std::string());
            {
                std::lock_guard<std::mutex> g(_nodesMutex);
                auto it = _nodes.find(assignedName);
                if (it != _nodes.end()) {
                    auto n = it->second;
                    resp->set_error(0);
                    resp->set_assigned(true);
                    resp->set_server_name(n->name);
                    resp->set_server_host(n->host);
                    resp->set_server_port(n->port);
                    resp->set_token(current.value("token", std::string()));
                    resp->set_expires_at(current.value("expires_at", 0));
                    return grpc::Status::OK;
                }
            }
            // fallthrough to allocate new if node not found
        }
    }

    // 2) pick least-loaded node (simple algorithm)
    std::shared_ptr<NodeInfo> chosen = nullptr;
    {
        std::lock_guard<std::mutex> g(_nodesMutex);
        double best = 1e9;
        for (auto &kv : _nodes) {
            auto n = kv.second;
            long long last = n->last_heartbeat.load();
            if (now_seconds() - last > NODE_STALE_THRESHOLD_SEC) continue;
            double ratio = (double)n->current_load.load() / std::max(1, n->capacity);
            if (ratio < best) {
                best = ratio;
                chosen = n;
            }
        }
        // fallback: round-robin
        if (!chosen && !_nodeList.empty()) {
            chosen = _nodeList[_rrIndex % _nodeList.size()];
            _rrIndex++;
        }
        if (!chosen) {
            resp->set_error(2); // no nodes available
            return grpc::Status::OK;
        }
        // reserve slot
        chosen->current_load.fetch_add(1);
    }

    // 3) generate token & persist
    int ttl = DEFAULT_TOKEN_TTL;
    std::string token = generateToken(uid, chosen->name, ttl);
    long long expiresAt = now_seconds() + ttl;
    long long now = now_seconds();

    try {
        auto& redis = RedisConPool::GetInstance();
        std::vector<std::string> keys;
        std::vector<std::string> argv = {
            uid,
            chosen->name,
            chosen->host,
            std::to_string(chosen->port),
            token,
            std::to_string(expiresAt),
            std::to_string(now)
        };

        auto prevOpt = redis.eval<sw::redis::Optional<std::string>>(_atomicAssignScript,keys.begin(),keys.end(),argv.begin(),argv.end());

		resp->set_error(0);
		resp->set_assigned(true);
		resp->set_server_name(chosen->name);
		resp->set_server_host(chosen->host);
		resp->set_server_port(chosen->port);
		resp->set_token(token);
		resp->set_expires_at(expiresAt);

		// If previous session existed and was online, async kick
		if (!prevOpt.value().empty()) {
			try {
				json prev = json::parse(prevOpt.value());
				bool prev_online = prev.value("online", false);
				std::string prev_name = prev.value("server_name", std::string());
				std::string prev_host = prev.value("server_host", std::string());
				int prev_port = prev.value("server_port", 0);

				if (prev_online && !prev_name.empty() && prev_name != chosen->name) {
					std::thread(
                        [uid, prev_host, prev_port]() {
							try {
								ChatGrpcClient::GetInstance()->KickUser(prev_host, prev_port, uid, static_cast<int>(ErrorCodes::REPEATED_LOGIN));
								LOG_INFO("Kick request sent to {}:{} for uid={}", prev_host, prev_port, uid);
							}
							catch (const std::exception& ex) {
								LOG_WARN("Kick request failed for uid={} host={}:{} : {}", uid, prev_host, prev_port, ex.what());
							}
						}
                    ).detach();
				}
			}
			catch (...) {
				LOG_WARN("Failed to parse prev session for uid={}", uid);
			}
		}
    }
    catch (const std::exception& e) {
        LOG_WARN("Exception caught: {}", e.what());
    }

    json route;
    route["assigned"] = true;
    route["server_name"] = chosen->name;
    route["server_host"] = chosen->host;
    route["server_port"] = chosen->port;
    route["token"] = token;
    route["expires_at"] = expiresAt;
    route["online"] = false;
    route["last_update"] = now_seconds();

    persistUserRouteToRedis(uid, route);

    resp->set_error(0);
    resp->set_server_host(chosen->host);
    resp->set_server_port(chosen->port);
    resp->set_token(token);
    resp->set_expires_at(expiresAt);
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::ReportOnline(grpc::ServerContext*, const status::OnlineReportReq* req, status::OnlineReportResp* resp)
{
    if (!req) {
        resp->set_error(1);
        return grpc::Status::OK;
    }

    // Validate token optionally
    if (!validateToken(req->token(), req->uid(), req->server_name())) {
        LOG_WARN("ReportOnline token validation failed uid={} server={}", req->uid(), req->server_name());
        // proceed but mark error if you want. Here we accept but log.
    }

    json route;
    if (!readUserRouteFromRedis(req->uid(), route)) {
        // create a route if absence (rare)
        route["assigned"] = true;
        route["server_name"] = req->server_name();
    }
    route["online"] = true;
    route["server_host"] = req->server_host();
    route["server_port"] = req->server_port();
    route["token"] = req->token();
    route["last_update"] = now_seconds();

    persistUserRouteToRedis(req->uid(), route);
    resp->set_error(0);
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::ReportOffline(grpc::ServerContext*, const status::OfflineReportReq* req, status::OfflineReportResp* resp)
{
    if (!req) {
        resp->set_error(1);
        return grpc::Status::OK;
    }

    json route;
    if (!readUserRouteFromRedis(req->uid(), route)) {
        resp->set_error(1);
        return grpc::Status::OK;
    }

    route["online"] = false;
    route["last_logout"] = now_seconds();

    // decrement assigned node load if we can
    std::string serverName = route.value("server_name", std::string());
    if (!serverName.empty()) {
        std::lock_guard<std::mutex> g(_nodesMutex);
		try {
			auto& redis = RedisConPool::GetInstance();
			redis.decr(StatusServerCode::NodeLoadKey + serverName);
		}
		catch (...) {
			LOG_WARN("ReportOffline: redis decrement failed for uid={}", req->uid());
		}
    }

    persistUserRouteToRedis(req->uid(), route);
    resp->set_error(0);
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::QueryUserRoute(grpc::ServerContext*, const status::RouteReq* req, status::RouteResp* resp)
{
    if (!req) {
        resp->set_error(1);
        resp->set_online(false);
        return grpc::Status::OK;
    }

    json route;
    if (!readUserRouteFromRedis(req->uid(), route)) {
        resp->set_error(2); // not found
        resp->set_online(false);
        return grpc::Status::OK;
    }

    resp->set_error(0);
    resp->set_online(route.value("online", false));
    resp->set_server_name(route.value("server_name", ""));
    resp->set_server_host(route.value("server_host", ""));
    resp->set_server_port(route.value("server_port", 0));
    resp->set_token(route.value("token", ""));
    resp->set_last_logout(route.value("last_logout", 0));
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::KickUser(grpc::ServerContext* context, const status::KickUserReq* req, status::KickUserResp* resp)
{
    if (!req) { resp->set_error(1); return grpc::Status::OK; }
    std::string uid = req->uid();
    int reason = req->reason();
    // Lookup route to find server host/port (we could accept host/port in the KickUserRequest too)
    json route;
    if (!readUserRouteFromRedis(uid, route)) {
        resp->set_error(2);
        return grpc::Status::OK;
    }
    std::string host = route.value("server_host", "");
    int port = route.value("server_port", 0);
    if (host.empty() || port == 0) {
        resp->set_error(3);
        return grpc::Status::OK;
    }

    try {
        auto r = ChatGrpcClient::GetInstance()->KickUser(host, port, uid, reason);
        resp->set_error(r.error());
    } catch (const std::exception& e) {
        LOG_WARN("KickUser forward failed: {}", e.what());
        resp->set_error(4);
    }
    return grpc::Status::OK;
}

grpc::Status StatusServerImpl::GetNodes(grpc::ServerContext*, const status::GetNodesReq*, status::GetNodesResp* resp)
{
    std::lock_guard<std::mutex> g(_nodesMutex);
    for (auto &kv : _nodes) {
        auto n = kv.second;
        auto* item = resp->add_nodes();
        item->set_name(n->name);
        item->set_server_host(n->host);
        item->set_server_port(n->port);
        item->set_current_load(n->current_load.load());
        item->set_capacity(n->capacity);
        item->set_last_heartbeat(n->last_heartbeat.load());
    }
    resp->set_error(0);
    return grpc::Status::OK;
}