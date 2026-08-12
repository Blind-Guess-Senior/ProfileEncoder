export module logger;

import std;
import statistics_result;

export class Logger
{
public:
    explicit Logger(const std::filesystem::path& log_folder, const bool console_output_disabled,
                    const bool ffmpeg_output_disabled, const bool stat_disabled = false,
                    const bool matrix_enabled = false, const std::filesystem::path& matrix_folder = {}) :
        m_consoleOutputDisabled(console_output_disabled), m_ffmpegOutputDisabled(ffmpeg_output_disabled),
        m_statisticsDisabled(stat_disabled), m_matrixEnabled(matrix_enabled)
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

        if (!m_matrixEnabled) {
            return;
        }
        m_matrixFilePath = matrix_folder / (timeString + ".statistics.md");

        if (const std::filesystem::path parentDir = m_matrixFilePath.parent_path();
            !std::filesystem::exists(parentDir)) {
            if (std::error_code ec; !std::filesystem::create_directories(parentDir, ec)) {
                if (ec) {
                    std::cerr << "Fatal error: cannot create statistics dir." << ec.message() << "\n";
                    throw std::runtime_error("Failed to create statistics folder.");
                }
            }
        }
        m_matrixFile.open(m_matrixFilePath);

        if (!m_matrixFile.is_open()) {
            std::cerr << "Fatal error: cannot open statistics file.\n";
            throw std::runtime_error("Failed to open statistics file.");
        }
        if (m_statisticsDisabled) {
            m_matrixFile << "|     | speed | compress rate |\n"
                         << "| --- | ----- | ------------- |\n";
        }
        else {
            m_matrixFile << "|     | speed | compress rate | PSNR | SSIM | VMAF | XPSNR |\n"
                         << "| --- | ----- | ------------- | ---- | ---- | ---- | ----- |\n";
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

    void Matrix(const std::filesystem::path& target_file, stat_result::statistics_values values)
    {
        if (!m_matrixEnabled) {
            return;
        }

        constexpr std::array infoKeys{"speed", "compression_ratio"};
        constexpr std::array statKeys{"PSNR", "SSIM", "VMAF", "XPSNR"};

        m_matrixFile << "| " << target_file.generic_string();

        for (const auto& k : infoKeys) {
            m_matrixFile << " | ";
            const auto& iterator = values.find(k);
            if (iterator != values.end() && iterator->second)
                m_matrixFile << *iterator->second;
            else {
                m_matrixFile << "N/A";
            }
        }
        if (m_statisticsDisabled) {
            goto end;
        }
        for (const auto& k : statKeys) {
            m_matrixFile << " | ";
            const auto& iterator = values.find(k);
            if (iterator != values.end() && iterator->second)
                m_matrixFile << *iterator->second;
            else {
                m_matrixFile << "N/A";
            }
        }
    end:
        m_matrixFile << " |\n";
    }

private:
    std::filesystem::path m_logFilePath;
    std::ofstream m_logFile;
    const bool m_consoleOutputDisabled;
    const bool m_ffmpegOutputDisabled;

    const bool m_statisticsDisabled;

    std::filesystem::path m_matrixFilePath;
    std::ofstream m_matrixFile;
    const bool m_matrixEnabled;
};
