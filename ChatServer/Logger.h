#pragma once
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>


class Logger
{
public:
	static void init(const std::string log_file, size_t max_file_size, size_t max_file_number) {
		std::vector<spdlog::sink_ptr> sinks;

		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::debug);
		sinks.push_back(console_sink);


		auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
			log_file,
			max_file_size,
			max_file_number
		);
		rotating_sink->set_level(spdlog::level::trace);
		sinks.push_back(rotating_sink);

		instance() = std::make_shared<spdlog::logger>("server_logger", sinks.begin(), sinks.end());
		spdlog::set_default_logger(instance());

		instance()->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%^%l%$] [%s(line:%#) %!] %v");


#ifdef _DEBUG
		instance()->set_level(spdlog::level::debug);
#else
		instance()->set_level(spdlog::level::info);
#endif

		instance()->flush_on(spdlog::level::err);
	}

	static std::shared_ptr<spdlog::logger>& instance() {
		static std::shared_ptr<spdlog::logger> logger;
		return logger;
	}
};

#define LOG_DEBUG(...) \
Logger::instance()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...) \
Logger::instance()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...) \
Logger::instance()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) \
Logger::instance()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)
#define LOG_CRITICAL(...) \
Logger::instance()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)
