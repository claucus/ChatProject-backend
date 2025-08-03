#pragma once

constexpr auto MAX_LENGTH = 1024 * 2;
constexpr auto BUFFER_SIZE = 1024 * 2;

constexpr auto MAX_RECEIVE_QUEUE = 1000;
constexpr auto MAX_SEND_QUEUE = 1000;


constexpr auto HEADER_ID_LENGTH = 2;
constexpr auto HEADER_DATA_LENGTH = 2;

constexpr auto HEADER_TOTAL_LENGTH = HEADER_DATA_LENGTH + HEADER_ID_LENGTH;


namespace ChatServiceConstant {
	constexpr auto LOGIN_COUNT = "login_count";
	constexpr auto USER_TOKEN_PREFIX = "user_token_";
	constexpr auto USER_INFO_PREFIX = "user_info_";
	constexpr auto USER_IP_PREFIX = "user_ip_";
}

enum class ErrorCodes
{
	SUCCESS = 0,
	ERROR_JSON = 1001,
	RPC_FAILED = 1002,
	UID_INVALID = 1003,
	TOKEN_INVALID = 1004,
};


enum class MessageID {
	MESSAGE_CHAT_LOGIN = 1005,
	MESSAGE_CHAT_LOGIN_RESPONSE = 1006,
};
