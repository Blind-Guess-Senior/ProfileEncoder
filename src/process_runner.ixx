module;

#include <chrono>
#include <expected>
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

export namespace process_runner
{
    struct ProcessResult
    {
        int exitCode;
        std::chrono::steady_clock::duration elapsed;
        std::optional<double> speed;
    };

    struct AnalyzeResult
    {
        int exitCode;
        std::chrono::steady_clock::duration elapsed;
    };

    struct AnalysisSpec
    {
        std::string_view name;
        std::string_view filter;
        std::string_view summaryMarker;
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

        [[nodiscard]] std::expected<ProcessResult, std::string> RunEncode(const std::filesystem::path& input,
                                                                          const std::filesystem::path& output) const
        {
            std::vector<std::string> arguments{
                "-hide_banner", "-nostdin", "-progress", "pipe:1", "-nostats", "-i", input.generic_string(),
            };

            arguments.insert_range(arguments.end(), m_params);
            arguments.emplace_back(output.generic_string());
            arguments.emplace_back("-y");

            try {
                asio::io_context ctx{};

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

                return ProcessResult{.exitCode = exitCode, .elapsed = elapsed, .speed = finalSpeed};
            }
            catch (const std::exception& ex) {
                return std::unexpected(std::format("Fatal error: FFmpeg process error when encoding file '{}': {}",
                                                   input.generic_string(), ex.what()));
            }
        }

        void RunAnalyzes(const std::filesystem::path& encoded, const std::filesystem::path& origin) const
        {
            constexpr std::array analyses{
                AnalysisSpec{"PSNR", "psnr", "PSNR y:"},
                AnalysisSpec{"VMAF", "libvmaf", "VMAF score:"},
                AnalysisSpec{"SSIM", "ssim", "SSIM Y:"},
                AnalysisSpec{"XPSNR", "xpsnr", "XPSNR"},
            };

            for (const auto& analysis : analyses) {
                m_logger.Log("Start {} analysis for '{}'.", analysis.name, encoded.generic_string());
                const auto result = RunAnalyze(encoded, origin, analysis);

                if (!result) {
                    m_logger.Log("{} analysis failed for '{}': {}", analysis.name, encoded.generic_string(),
                                 result.error());
                    continue;
                }

                m_logger.Log("{} Analysis for '{}' succeed: {}", analysis.name, encoded.generic_string(),
                             result.value());
            }
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

                                       if (error == asio::error::eof || error == asio::error::broken_pipe) {
                                           if (!buffer.empty()) {
                                               handler(buffer);
                                               buffer.clear();
                                           }
                                           return;
                                       }

                                       read_error = error;
                                   });
        }

        [[nodiscard]] std::expected<std::string, std::string> RunAnalyze(const std::filesystem::path& encoded,
                                                                         const std::filesystem::path& origin,
                                                                         const AnalysisSpec& analysis) const
        {
            try {
                asio::io_context ctx{};
                std::vector<std::string> arguments{"-hide_banner",
                                                   "-nostdin",
                                                   "-nostats",

                                                   "-i",
                                                   encoded.generic_string(),
                                                   "-i",
                                                   origin.generic_string(),

                                                   "-filter_complex",
                                                   std::format("[0:v:0][1:v:0]{}[metric]", analysis.filter),
                                                   "-map",
                                                   "[metric]",
                                                   "-an",
                                                   "-f",
                                                   "null",
                                                   "-"};

                asio::readable_pipe stderrPipe{ctx};

                std::string stderrBuffer;
                std::optional<boost::system::error_code> stderrError;
                std::optional<std::string> summary;

                bp::process proc{ctx, m_ffmpeg, arguments, bp::process_stdio{.err = stderrPipe}};

                ReadLines(
                    stderrPipe, stderrBuffer,
                    [this, &analysis, &summary](std::string_view line) -> void
                    {
                        m_logger.Log("analyze: {}", line);

                        if (const auto markerPosition = line.find(analysis.summaryMarker);
                            markerPosition != std::string_view::npos) {
                            summary = std::string{line.substr(markerPosition)};
                        }
                    },
                    stderrError);

                ctx.run();

                if (stderrError) {
                    return std::unexpected(std::format("FFmpeg stderr read error: {}. {} analyzing failed.",
                                                       stderrError->message(), analysis.name));
                }

                const int exitCode = proc.wait();

                if (exitCode != 0) {
                    return std::unexpected(std::format("FFmpeg exited with code {}", exitCode));
                }

                if (!summary) {
                    return std::unexpected("FFmpeg produced no analysis summary.");
                }

                return *summary;
            }
            catch (const std::exception& ex) {
                return std::unexpected(ex.what());
            }
        }
    };
} // namespace process_runner
