#pragma once

namespace StatusServerCode {
	constexpr auto UserRouteKey = "status_user:";
	constexpr auto NodeLoadKey = "status_node_load:";
	constexpr auto NodeKey = "status_node:";
};


enum class ErrorCodes
{
	SUCCESS = 0,
	ERROR_JSON = 1001,
	RPC_FAILED = 1002,
	UID_INVALID = 1003,
	TOKEN_INVALID = 1004,
	REPEATED_LOGIN = 1005,
};


enum class RegisterResponseCodes {
	REGISTER_SUCCESS = 0,
	VERIFY_CODE_TIMEOUT = 1001,
	VERIFY_CODE_ERROR = 1002,
	USER_EXISTS = 1003,
};

enum class ResetResponseCodes {
	RESET_SUCCESS = 0,
	VERIFY_CODE_TIMEOUT = 1001,
	VERIFY_CODE_ERROR = 1002,
	USER_NOT_EXISTS = 1003,
};

enum class LoginResponseCodes {
	LOGIN_SUCCESS = 0,
	LOGIN_ERROR = 1001,
};