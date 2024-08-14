#pragma once

#include <iostream>
#include <string_view>

#ifdef DISABLE_SPDLOG_FMT_CONSTEVAL
#    define FMT_CONSTEVAL
#endif

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace berry {

class Log {
    inline static std::shared_ptr<spdlog::logger> consoleSink = nullptr;
    inline static std::shared_ptr<spdlog::logger> fileSink = nullptr;
    inline static std::shared_ptr<spdlog::logger> logger = nullptr;

public:
    static void Init() noexcept
    {
        try {
            consoleSink = spdlog::stdout_color_mt<spdlog::async_factory>("console_logger");
#ifdef NDEBUG
            consoleSink->sinks()[0]->set_level(spdlog::level::info);
#else
            consoleSink->sinks()[0]->set_level(spdlog::level::debug);
#endif
            fileSink = spdlog::basic_logger_mt<spdlog::async_factory>("file_logger", "logs/log.txt", true);
            fileSink->sinks()[0]->set_level(spdlog::level::trace);

            std::vector<spdlog::sink_ptr> sinks { consoleSink->sinks()[0], fileSink->sinks()[0] };
            logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
            logger->set_pattern("[%T][%^%=7t%$][%^%=7l%$] %v");
            logger->set_level(spdlog::level::trace);

            spdlog::flush_every(std::chrono::seconds(3));
        } catch (spdlog::spdlog_ex const& ex) {
            std::cout << "File log init failed: " << ex.what() << std::endl;
        }
    }

    static void Deinit() noexcept
    {
        spdlog::shutdown();
    }

    // FORMATTING EXAMPLES
    // spdlog::info("Welcome to spdlog!");
    // spdlog::error("Some error message with arg: {}", 1);
    // spdlog::warn("Easy padding in numbers like {:08d}", 12);
    // spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    // spdlog::info("Support for floats {:03.2f}", 1.23456);
    // spdlog::info("Positional args are {1} {0}..", "too", "supported");
    // spdlog::info("{:<30}", "left aligned");

    template<typename... Args>
    constexpr static void trace(Args&&... args) noexcept
    {
        logger->trace(std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr static void debug(Args&&... args) noexcept
    {
        logger->debug(std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr static void info(Args&&... args) noexcept
    {
        logger->info(std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr static void warn(Args&&... args) noexcept
    {
        logger->warn(std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr static void error(Args&&... args) noexcept
    {
        logger->error(std::forward<Args>(args)...);
        fileSink->flush();
    }


    static void info(std::string_view sv) noexcept
    {
        logger->info(sv);
    }
    static void debug(std::string_view sv) noexcept
    {
        logger->debug(sv);
    }
    static void error(std::string_view sv) noexcept
    {
        logger->error(sv);
    }
};

namespace log {
inline static void timer(std::string_view msg, double time)
{
    berry::Log::trace("{:>6.2f}s  {}", time, msg);
}
}

}
