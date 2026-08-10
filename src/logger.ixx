export module logger;

import std;

export class Logger
{
public:
    explicit Logger(const std::filesystem::path& log_folder, const bool console_output_disabled,
                    const bool ffmpeg_output_disabled) :
        m_consoleOutputDisabled(console_output_disabled), m_ffmpegOutputDisabled(ffmpeg_output_disabled)
    {
        const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        const auto timePoint = std::chrono::current_zone()->to_local(now);
        const auto timeString = std::format("{:%Y%m%d%H%M%S}", timePoint);

        m_logFilePath = log_folder / (timeString + ".log");

        if (const std::filesystem::path parentDir = m_logFilePath.parent_path(); !std::filesystem::exists(parentDir)) {
            if (std::error_code ec; !std::filesystem::create_directories(parentDir, ec)) {
                if (ec) {
                    std::cerr << "Fatal error: cannot create log dir." << ec.message() << "\n";
                    throw std::runtime_error("Failed to create log folder.");
                }
            }
        }

        m_logFile.open(m_logFilePath);
        if (!m_logFile.is_open()) {
            std::cerr << "Fatal error: cannot open log file.\n";
            throw std::runtime_error("Failed to open log file.");
        }
    }

    template <typename... Args>
    void Log(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(std::format(fmt, std::forward<Args>(args)...));
    }

    void Log(std::string_view log)
    {
        constexpr std::string_view ffmpegPrefix = "ffmpeg:";
        if (m_ffmpegOutputDisabled && log.starts_with(ffmpegPrefix)) {
            return;
        }
        const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
        const auto timePoint = std::chrono::current_zone()->to_local(now);
        const auto timeString = "[" + std::format("{:%F %T}", timePoint) + "] ";

        if (!m_consoleOutputDisabled) {
            std::println("{}", log);
        }
        m_logFile << timeString << log << "\n";
    }

private:
    std::filesystem::path m_logFilePath;
    std::ofstream m_logFile;
    bool m_consoleOutputDisabled;
    bool m_ffmpegOutputDisabled;
};
