module;

#include <boost/asio.hpp>
#include <boost/process/v2/process.hpp>
#include "boost/process/v2/environment.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

export module process_runner;

namespace asio = boost::asio;
namespace bp = boost::process::v2;

export namespace process_runner
{
    int run_ffmpeg(const std::filesystem::path& input, const std::vector<std::string>& params,
                   const std::filesystem::path& output)
    {
        const auto ffmpeg = bp::environment::find_executable("ffmpeg");

        if (ffmpeg.empty()) {
            throw std::runtime_error("ffmpeg was not found in PATH.");
        }

        asio::io_context ctx{};

        std::vector<std::string> arguments{
            "-hide_banner",
            "-nostdin",
            "-i",
            input.generic_string(),
        };
        arguments.insert_range(arguments.end(), params);
        arguments.emplace_back(output.generic_string());
        // if (y) {
        arguments.emplace_back("-y");
        //}

        bp::process proc{ctx, ffmpeg, arguments};

        return proc.wait();
    }
} // namespace process_runner
