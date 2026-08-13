module;

// If your build got error, try comment import std; and uncomment those traditional include.
// #include <charconv>
// #include <chrono>
// #include <expected>
// #include <filesystem>
// #include <stdexcept>
// #include <string>
// #include <vector>

#include "boost/asio.hpp"
#include "boost/process/v2/environment.hpp"
#include "boost/process/v2/process.hpp"
#include "boost/process/v2/stdio.hpp"

export module process_runner;

import std;
import statistics_result;
import logger;

namespace asio = boost::asio;
namespace bp = boost::process::v2;

namespace
{
    std::optional<double> parse_speed(std::string_view speed)
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

    std::optional<double> parse_analysis_value(std::string_view summary, std::string_view marker)
    {
        const auto markerPosition = summary.find(marker);
        if (markerPosition == std::string_view::npos) {
            return std::nullopt;
        }

        auto valueRaw = summary.substr(markerPosition + marker.size());

        const auto firstNonSpace = valueRaw.find_first_not_of(" \t");
        if (firstNonSpace == std::string_view::npos) {
            return std::nullopt;
        }
        valueRaw.remove_prefix(firstNonSpace);

        double value;
        const auto [ptr, ec] = std::from_chars(valueRaw.data(), valueRaw.data() + valueRaw.size(), value);

        if (ec != std::errc{} || ptr == valueRaw.data()) {
            return std::nullopt;
        }

        return value;
    }
} // namespace

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

    enum class ANALYSIS_METRIC
    {
        PSNR,
        SSIM,
        VMAF,
        XPSNR
    };
    struct AnalysisSpec
    {
        ANALYSIS_METRIC metric;
        std::string_view name;
        std::string_view filter;
        std::string_view summaryMarker;
        std::string_view valueMarker;
    };

    class FFmpegRunner
    {
    public:
        explicit FFmpegRunner(Logger& logger, std::array<std::vector<std::string>, 3>&& params) :
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
                "-hide_banner", "-nostdin", "-progress", "pipe:1", "-nostats",
            };

            arguments.append_range(m_params[0]);
            arguments.emplace_back("-i");
            arguments.push_back(input.generic_string());

            arguments.append_range(m_params[1]);
            arguments.push_back(output.generic_string());

            arguments.append_range(m_params[2]);
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

        void RunAnalyzes(const std::filesystem::path& encoded, const std::filesystem::path& origin,
                         stat_result::statistics_values& results) const
        {
            constexpr std::array analyses{
                AnalysisSpec{
                    .metric = ANALYSIS_METRIC::PSNR,
                    .name = "PSNR",
                    .filter = "psnr",
                    .summaryMarker = "PSNR y:",
                    .valueMarker = "average:",
                },
                AnalysisSpec{
                    .metric = ANALYSIS_METRIC::SSIM,
                    .name = "SSIM",
                    .filter = "ssim",
                    .summaryMarker = "SSIM Y:",
                    .valueMarker = "All:",
                },
                AnalysisSpec{
                    .metric = ANALYSIS_METRIC::VMAF,
                    .name = "VMAF",
                    .filter = "libvmaf",
                    .summaryMarker = "VMAF score:",
                    .valueMarker = "VMAF score:",
                },
                AnalysisSpec{
                    .metric = ANALYSIS_METRIC::XPSNR,
                    .name = "XPSNR",
                    .filter = "xpsnr",
                    .summaryMarker = "XPSNR",
                    .valueMarker = "minimum:",
                },
            };

            for (const auto& analysis : analyses) {
                m_logger.Log("Start {} analysis for '{}'.", analysis.name, encoded.generic_string());
                const auto result = RunAnalyze(encoded, origin, analysis);

                if (!result) {
                    m_logger.Log("{} analysis failed for '{}': {}", analysis.name, encoded.generic_string(),
                                 result.error());
                    continue;
                }

                const auto [summary, value] = result.value();

                m_logger.Log("{} Analysis for '{}' succeed: {}", analysis.name, encoded.generic_string(), summary);

                results.emplace(analysis.name, value);
            }
        }

    private:
        const boost::filesystem::path m_ffmpeg;
        Logger& m_logger;
        const std::array<std::vector<std::string>, 3> m_params;

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

        [[nodiscard]] std::expected<std::pair<std::string, double>, std::string>
        RunAnalyze(const std::filesystem::path& encoded, const std::filesystem::path& origin,
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
                            summary = line.substr(markerPosition);
                            m_logger.Log("{} summary: {}", analysis.name, *summary);
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

                const auto value = parse_analysis_value(*summary, analysis.valueMarker);
                if (value) {
                    std::pair p{*summary, *value};
                    return p;
                }
                else {
                    return std::unexpected(std::format("Failed to parse {} value from '{}'.", analysis.name, *summary));
                }
            }
            catch (const std::exception& ex) {
                return std::unexpected(ex.what());
            }
        }
    };
} // namespace process_runner
