#pragma once

#include <grpcpp/grpcpp.h>
#include "status.grpc.pb.h"   // generated from status.proto (package message)
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>
using json = nlohmann::json;

/**
 * @brief Node information structure
 * 
 */
struct NodeInfo {
    std::string name;
    std::string host;
    int port = 0;
    int capacity = 100; // default weight/capacity
    std::atomic<int> current_load{0};
    std::atomic<long long> last_heartbeat{0}; // epoch seconds
};

/**
 * @brief Implementation of the Status Service
 * 
 */
class StatusServerImpl final : public status::StatusService::Service {
public:
    /**
     * @brief Construct a new Status Server Impl object
     * 
     */
    StatusServerImpl();

    /**
     * @brief Destroy the Status Server Impl object
     * 
     */
    ~StatusServerImpl();

    /**
     * @brief Register a new node with the status server
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status RegisterNode(grpc::ServerContext* context, const status::RegisterNodeReq* req, status::RegisterNodeResp* resp) override;

    /**
	* @brief Deregister a node from the status server
	*
	* @param context
	* @param req
	* @param resp
	* @return grpc::Status
	*/
    grpc::Status DeregisterNode(grpc::ServerContext * context, const status::DeregisterNodeReq * req, status::DeregisterNodeResp * resp) override;

    /**
     * @brief heartbeat from a node to indicate it's alive
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status Heartbeat(grpc::ServerContext* context, const status::HeartbeatReq* req, status::HeartbeatResp* resp) override;

    /**
     * @brief allocate a server for a user
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status AllocateServer(grpc::ServerContext* context, const status::AllocateServerReq* req, status::AllocateServerResp* resp) override;

    /**
     * @brief report online status for a user
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status ReportOnline(grpc::ServerContext* context, const status::OnlineReportReq* req, status::OnlineReportResp* resp) override;

    /**
     * @brief report offline status for a user
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status ReportOffline(grpc::ServerContext* context, const status::OfflineReportReq* req, status::OfflineReportResp* resp) override;

    /**
     * @brief query user route information
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status QueryUserRoute(grpc::ServerContext* context, const status::RouteReq* req, status::RouteResp* resp) override;

    /**
     * @brief Kick a user if already online
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status KickUser(grpc::ServerContext* context, const status::KickUserReq* req, status::KickUserResp* resp) override;

    /**
     * @brief Get the Nodes object
     * 
     * @param context 
     * @param req 
     * @param resp 
     * @return grpc::Status 
     */
    grpc::Status GetNodes(grpc::ServerContext* context, const status::GetNodesReq* req, status::GetNodesResp* resp) override;

private:
    // Node management
    std::mutex _nodesMutex;
    std::unordered_map<std::string, std::shared_ptr<NodeInfo>> _nodes; // name -> NodeInfo
    std::vector<std::shared_ptr<NodeInfo>> _nodeList; // snapshot for iteration
    size_t _rrIndex = 0;

    // Redis read / write helpers for user route
    bool persistUserRouteToRedis(const std::string& uid, const json& j);
    bool readUserRouteFromRedis(const std::string& uid, json& out);
    bool removeUserRouteFromRedis(const std::string& uid);

    // utilities
    std::string generateToken(const std::string& uid, const std::string& serverName, int ttlSeconds = 3600);
    bool validateToken(const std::string& token, const std::string& uid, const std::string& serverName);

    // node helpers
    void addOrUpdateNode(const status::RegisterNodeReq& req);
    void removeNode(const std::string& name);
    void rebuildNodeListLocked();

    // eviction helper
    void evictStaleNodes(long long nowSec);

    // constants
    const int NODE_STALE_THRESHOLD_SEC = 60; // heartbeat expiry
    const int DEFAULT_TOKEN_TTL = 3600;

    std::string _atomicAssignScript;
};