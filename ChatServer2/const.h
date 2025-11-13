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
	constexpr auto USER_SESSION_PREFIX = "user_session_";
	constexpr auto USER_INFO_PREFIX = "user_info_";
	constexpr auto USER_FRIEND_STATUS = "user_friend_status_";
	constexpr auto FRIEND_REQUEST_PREFIX = "friend_request_";
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

	MESSAGE_GET_SEARCH_USER = 1007,
	MESSAGE_GET_SEARCH_USER_RESPONSE = 1008,

	MESSAGE_APPLY_FRIEND = 1009,
	MESSAGE_APPLY_FRIEND_RESPONSE = 1010,

	MESSAGE_NOTIFY_ADD_FRIEND = 1011,

	MESSAGE_APPROVAL_FRIEND = 1013,
	MESSAGE_APPROVAL_FRIEND_RESPONSE = 1014,

	MESSAGE_NOTIFY_APPROVAL_FRIEND = 1015,
};

enum class AddStatusCodes {
	NotFriend,
	NotConsent,
	MutualFriend,
};


enum class ApplyStatusCodes {
	Pending,
	Accepted,
	Rejected,
};