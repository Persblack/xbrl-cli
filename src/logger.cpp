#include "logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <vector>

void setupLogger(const std::string& logFile, bool verbose, bool quiet) {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, 5 * 1024 * 1024, 3); // 5 MB, 3 files

    // Console: level based on flags
    if (quiet) {
        consoleSink->set_level(spdlog::level::err);
    } else if (verbose) {
        consoleSink->set_level(spdlog::level::debug);
    } else {
        consoleSink->set_level(spdlog::level::info);
    }

    // File: debug only on --verbose; amount-bearing lines live at debug level,
    // so a default run leaves no balance figures in the log file.
    fileSink->set_level(verbose ? spdlog::level::debug : spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
    auto logger = std::make_shared<spdlog::logger>("ebilanz", sinks.begin(), sinks.end());
    logger->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_default_logger(logger);
}
