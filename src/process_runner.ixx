module;

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "boost/asio.hpp"
#include "boost/process/v2/environment.hpp"
#include "boost/process/v2/process.hpp"
#include "boost/process/v2/stdio.hpp"

export module process_runner;

import logger;

namespace asio = boost::asio;
namespace bp = boost::process::v2;

static std::optional<double> parse_speed(std::string_view speed);

export namespace process_runner
{
    struct ProcessResult
    {
        int exitCode;
        std::chrono::steady_clock::duration elapsed;
        std::optional<double> speed;
    };

    class FFmpegRunner
    {
    public:
        explicit FFmpegRunner(Logger& logger, std::vector<std::string>&& params) :
            m_ffmpeg(bp::environment::find_executable("ffmpeg")), m_logger(logger), m_params(std::move(params))
        {
            if (m_ffmpeg.empty()) {
                throw std::runtime_error("ffmpeg was not found in PATH.");
            }
        }

        [[nodiscard]] ProcessResult RunFFmpeg(const std::filesystem::path& input,
                                              const std::filesystem::path& output) const
        {
            asio::io_context ctx{};

            std::vector<std::string> arguments{
                "-hide_banner", "-nostdin", "-progress", "pipe:1", "-nostats", "-i", input.generic_string(),
            };

            arguments.insert_range(arguments.end(), m_params);
            arguments.emplace_back(output.generic_string());
            arguments.emplace_back("-y");

            asio::readable_pipe stdoutPipe{ctx};
            asio::readable_pipe stderrPipe{ctx};

            std::optional<double> currentSpeed;
            std::optional<double> finalSpeed;

            std::string stdoutBuffer;
            std::string stderrBuffer;

            std::optional<boost::system::error_code> stdoutError;
            std::optional<boost::system::error_code> stderrError;

            const auto start = std::chrono::steady_clock::now();

            bp::process proc{ctx, m_ffmpeg, arguments, bp::process_stdio{.out = stdoutPipe, .err = stderrPipe}};

            ReadLines(
                stdoutPipe, stdoutBuffer,
                [this, &currentSpeed, &finalSpeed](std::string_view line)
                {
                    if (line.starts_with("speed=")) {
                        currentSpeed = parse_speed(line);
                    }
                    else if (line == "progress=continue") {
                        currentSpeed.reset();
                    }
                    else if (line == "progress=end") {
                        finalSpeed = currentSpeed;
                    }
                },
                stdoutError);
            ReadLines(
                stderrPipe, stderrBuffer, [this](std::string_view line) { m_logger.Log("ffmpeg: {}", line); },
                stderrError);

            ctx.run();
            if (stdoutError) {
                m_logger.Log("FFmpeg stdout read error: {}. Encoding probably failed.", stdoutError->message());
            }

            if (stderrError) {
                m_logger.Log("FFmpeg stderr read error: {}. Encoding probably failed.", stderrError->message());
            }

            const int exitCode = proc.wait();
            const auto elapsed = std::chrono::steady_clock::now() - start;

            return {.exitCode = exitCode, .elapsed = elapsed, .speed = finalSpeed};
        }

    private:
        const boost::filesystem::path m_ffmpeg;
        Logger& m_logger;
        const std::vector<std::string> m_params;

        using read_line_handler = std::function<void(std::string_view)>;
        static void ReadLines(asio::readable_pipe& pipe, std::string& buffer, read_line_handler handler,
                              std::optional<boost::system::error_code>& read_error)
        {
            asio::async_read_until(pipe, asio::dynamic_buffer(buffer), '\n',
                                   [&pipe, &buffer, &read_error, handler = std::move(handler)](
                                       const boost::system::error_code& error, const std::size_t size) mutable -> void
                                   {
                                       if (!error) {
                                           std::string line = buffer.substr(0, size);
                                           buffer.erase(0, size);

                                           while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                                               line.pop_back();
                                           }

                                           handler(line);
                                           ReadLines(pipe, buffer, std::move(handler), read_error);
                                           return;
                                       }

                                       if (error == asio::error::eof) {
                                           if (!buffer.empty()) {
                                               handler(buffer);
                                               buffer.clear();
                                           }
                                           return;
                                       }

                                       read_error = error;
                                   });
        }
    };
} // namespace process_runner

static std::optional<double> parse_speed(std::string_view speed)
{
    constexpr std::string_view prefix = "speed=";
    if (!speed.starts_with(prefix)) {
        return std::nullopt;
    }

    auto rest = speed.substr(prefix.size());

    const auto firstNonSpace = rest.find_first_not_of(' ');
    if (firstNonSpace == std::string_view::npos) {
        return std::nullopt;
    }
    rest.remove_prefix(firstNonSpace);

    double value;
    const auto [ptr, ec] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
    if (ec != std::errc{}) {
        return std::nullopt;
    }

    return value;
}
