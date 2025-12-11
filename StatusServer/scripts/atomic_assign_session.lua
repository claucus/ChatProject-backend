-- Atomic assignment lua script for StatusServer
-- ARGV:
-- 1 uid
-- 2 new_name
-- 3 new_host
-- 4 new_port
-- 5 new_token
-- 6 expires_at
-- 7 now
local uid = ARGV[1]
local new_name = ARGV[2]
local new_host = ARGV[3]
local new_port = tonumber(ARGV[4])
local new_token = ARGV[5]
local expires_at = tonumber(ARGV[6])
local now = tonumber(ARGV[7])

local key_user = "status_user:" .. uid
local key_new_load = "status_node_load:" .. new_name

local prev_raw = redis.call("GET", key_user)
local prev_tbl = nil
if prev_raw and prev_raw ~= false and prev_raw ~= "" then
    prev_tbl = cjson.decode(prev_raw)
    local prev_name = prev_tbl["server_name"]
    if prev_name and prev_name ~= "" and prev_name ~= new_name then
        local key_prev_load = "status_node_load:" .. prev_name
        redis.call("DECR", key_prev_load)
    end
end

redis.call("INCR", key_new_load)

local new_route = {
    assigned = true,
    server_name = new_name,
    server_host = new_host,
    server_port = new_port,
    token = new_token,
    expires_at = expires_at,
    online = false,
    last_update = now
}

redis.call("SET", key_user, cjson.encode(new_route))

if prev_raw and prev_raw ~= false and prev_raw ~= "" then
    return prev_raw
else
    return nil
end
