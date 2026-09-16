module;
#include <memory>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>

export module logging;
import std;

namespace
{
    thread_local std::shared_ptr<spdlog::logger> this_thread_logger;
}

export void set_thread_logger(const std::string& name)
{
    this_thread_logger = spdlog::get(name);

    if (!this_thread_logger) {
        this_thread_logger = spdlog::stdout_color_st(name);
    }
}

export spdlog::logger& thread_logger()
{
    if (!this_thread_logger) {
        return *spdlog::default_logger();
    }

    return *this_thread_logger;
}
