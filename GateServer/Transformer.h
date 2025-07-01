#pragma once
#include <string>
#include <cassert>
extern unsigned char toHex(unsigned char x) {
	return (x > 9 ? (x + 55) : (x + 48));
}
extern unsigned char fromHex(unsigned char x) {
	unsigned char ch;
	if (x >= 'A' && x <= 'Z') {
		ch = x - 'A' + 10;
	}
	else if (x >= 'a' && x <= 'z') {
		ch = x - 'a' + 10;
	}
	else if (x >= '0' && x <= '9') {
		ch = x - '0';
	}
	else {
		assert(0);
	}
	return ch;
}
extern std::string urlEncoder(const std::string& str) {
	std::string strTemp;
	size_t length = str.length();

	for (size_t i = 0; i < length; ++i) {
		if (isalnum((unsigned char)str[i]) || (str[i] == '-') || (str[i] == '_') || (str[i] == '.') || (str[i] == '~')) {
			strTemp += str[i];
		}
		else if (str[i] == ' ') {
			strTemp += '+';
		}
		else {
			strTemp += '%';
			strTemp += toHex((unsigned char)(str[i] >> 4));
			strTemp += toHex((unsigned char)(str[i] & 0x0F));
		}
	}
	return strTemp;
}
extern std::string urlDecoder(const std::string& str) {
	std::string strTemp;
	size_t length = str.length();

	for (size_t i = 0; i < length; ++i) {
		if (str[i] == '+') {
			strTemp += ' ';
		}
		else if (str[i] == '%') {
			assert(i + 2 < length);
			auto high = fromHex((unsigned char)str[++i]);
			auto low = fromHex((unsigned char)str[++i]);
			strTemp += std::to_string((high << 4) + low);
		}
		else {
			strTemp += str[i];
		}
	}
	return strTemp;
}