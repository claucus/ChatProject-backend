#pragma once

constexpr auto CODE_PREFIX = "code_";


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