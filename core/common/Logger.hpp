#pragma once

#include <filesystem>
#include <memory>

#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace sqlgui::core {

class AppLogger {
public:
    explicit AppLogger(const std::filesystem::path& log_path = "sqlgui.log") {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
        logger_ = std::make_shared<spdlog::logger>(
            "sqlgui",
            spdlog::sinks_init_list {console_sink, file_sink});
        logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
        logger_->set_level(spdlog::level::info);
        logger_->flush_on(spdlog::level::warn);
    }

    [[nodiscard]] std::shared_ptr<spdlog::logger> shared() const noexcept {
        return logger_;
    }

    [[nodiscard]] spdlog::logger& get() const noexcept {
        return *logger_;
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace sqlgui::core
