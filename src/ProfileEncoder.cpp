
import std;
import argparse;
import logger;
import helpers;
import process_runner;

using std::string;
using std::string_view;
using std::vector;

namespace stdfs = std::filesystem;

constexpr string_view LOG_FOLDER = "log";
constexpr string_view PROFILE_FOLDER = "profiles";

static void configure_argument_parser(argparse::ArgumentParser& parser);
[[nodiscard]] static vector<string> collect_profile_info(Logger& logger, const string& profile_raw);
static void collect_input_file(Logger& logger, vector<stdfs::path>& target_files, std::unordered_set<stdfs::path>& seen,
                               const string& input);
static void process_input_file(Logger& logger, const process_runner::FFmpegRunner& ffmpeg_runner,
                               const stdfs::path& input, const string& profile_raw);

static bool is_stat_disabled;

static std::filesystem::path cwd;
static std::filesystem::path exe_path;
static std::filesystem::path profile_path;
static std::filesystem::path log_path;

int main(const int argc, const char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    argparse::ArgumentParser argumentParser{"ProfileEncoder", "0.1.0"};
    configure_argument_parser(argumentParser);

    const vector<string> arguments(argv, argv + argc);
    argumentParser.parse_args(arguments);

    const auto profileRaw = argumentParser.get<string>("--profile");
    auto inputs = argumentParser.get<vector<string>>("--input");
    // auto y = argumentParser.get<bool>("-y");
    is_stat_disabled = argumentParser.get<bool>("--no-stat");
    const bool isLogDisabled = argumentParser.get<bool>("--no-log");
    const bool isFFmpegLogDisabled = argumentParser.get<bool>("--no-ffmpeg-log");

    cwd = stdfs::current_path() / "";
    exe_path = normalized_path(argv[0]).parent_path(); // https://stackoverflow.com/a/55579815
    profile_path = exe_path / PROFILE_FOLDER / "";
    log_path = exe_path / LOG_FOLDER / "";

    Logger logger{log_path, isLogDisabled, isFFmpegLogDisabled};

    // Log toggle info
    if (is_stat_disabled) {
        logger.Log("Statistics disabled. No statistics info will be output.");
    }
    if (isLogDisabled) {
        logger.Log("Console log disabled.");
    }

    process_runner::FFmpegRunner ffmpegRunner{logger, collect_profile_info(logger, profileRaw)};

    // Collect input files
    vector<stdfs::path> targetFiles{};
    std::unordered_set<stdfs::path> seen{};

    for (const auto& input : inputs) {
        collect_input_file(logger, targetFiles, seen, input);
    }

    // Process input files
    for (const auto& target : targetFiles) {
        process_input_file(logger, ffmpegRunner, target, profileRaw);
    }

    return 0;
}

void configure_argument_parser(argparse::ArgumentParser& parser)
{
    parser.add_description("Profile Encoder, a batch encoder for re-enc files in pre-defined profiles.");
    parser.add_argument("-p", "--profile")
        .default_value<string>("default")
        .help("Choose one profile of encode. Profiles are under ./profiles folder.");
    parser.add_argument("-i", "--input")
        .append()
        .default_value<vector<string>>(vector<string>{"./input.txt"})
        .nargs(argparse::nargs_pattern::at_least_one)
        .help("Input files. Can be a single file or a txt that every single line refers to a file. Multiple input arg "
              "will be composed.");
    // parser.add_argument("-y").flag().help("Automatically replace output file.");
    parser.add_argument("--no-stat").flag().help("Turn off statistics.");
    parser.add_argument("--no-log").flag().help("Turn off command line log output. File log will still preserved.");
    parser.add_argument("--no-ffmpeg-log").flag().help("Turn off ffmpeg stderr output.");
}

vector<string> collect_profile_info(Logger& logger, const string& profile_raw)
{
    // Process profile
    bool isLocalProfile = false;
    if (profile_raw[0] == '.') {
        isLocalProfile = true;
    }
    stdfs::path targetProfileFile;
    if (isLocalProfile) {
        targetProfileFile = cwd / stdfs::path{profile_raw.substr(1) + ".txt"};
    }
    else {
        targetProfileFile = profile_path / stdfs::path{profile_raw + ".txt"};
    }

    if (!stdfs::exists(targetProfileFile)) {
        logger.Log("Error: Position {} does not contain given profile '{}'.",
                   isLocalProfile ? cwd.generic_string() : profile_path.generic_string(),
                   isLocalProfile ? profile_raw.substr(1) : profile_raw);
        throw std::runtime_error("Error: profile not found.");
    }
    if (!stdfs::is_regular_file(targetProfileFile)) {
        logger.Log("Profile '{}' in position {} is not a valid file.", profile_raw,
                   isLocalProfile ? cwd.generic_string() : profile_path.generic_string());
        throw std::runtime_error("Error: profile not valid.");
    }

    // Read profile
    std::ifstream profileFile{targetProfileFile};
    if (!profileFile.is_open()) {
        std::cerr << "Fatal error: cannot open profile file '" << targetProfileFile.generic_string() << "'.\n";
        logger.Log("Fatal error: cannot open profile file '{}'", targetProfileFile.generic_string());
        throw std::runtime_error("Fatal error: cannot open profile file.");
    }
    vector<string> profileParams{};
    string param;
    while (profileFile >> param) {
        profileParams.push_back(param);
    }

    logger.Log("Encoding with profile params {}", profileParams);

    profileFile.close();

    return profileParams;
}

void collect_input_file(Logger& logger, vector<stdfs::path>& target_files, std::unordered_set<stdfs::path>& seen,
                        const string& input)
{
    std::filesystem::path inputPath = normalized_path(input);
    if (!stdfs::exists(inputPath)) {
        logger.Log("Error: input file {} does not exist. Skipped.", inputPath.generic_string());
        return;
    }
    if (!stdfs::is_regular_file(inputPath)) {
        logger.Log("Error: input file {} is not valid. Skipped.", inputPath.generic_string());
        return;
    }
    if (is_txt(inputPath)) {
        std::ifstream inputList{inputPath};

        if (!inputList.is_open()) {
            logger.Log("Error: cannot open input list file '{}'. Skipped.", inputPath.generic_string());
            return;
        }

        string line;
        while (std::getline(inputList, line)) {
            // TODO: list file position based path?
            if (line.empty()) {
                continue;
            }
            collect_input_file(logger, target_files, seen, line);
        }

        return;
    }

    if (seen.insert(inputPath).second) {
        target_files.emplace_back(inputPath);
    }
}

void process_input_file(Logger& logger, const process_runner::FFmpegRunner& ffmpeg_runner, const stdfs::path& input,
                        const string& profile_raw)
{
    const auto outputPath =
        input.parent_path() / (input.stem().generic_string() + "_" + profile_raw + input.extension().generic_string());

    logger.Log("Started to encode {} in profile {}.", input.generic_string(), profile_raw);
    try {
        const auto [exitCode, elapsedRaw, speed] = ffmpeg_runner.RunFFmpeg(input, outputPath);
        const auto elapsed = std::chrono::duration<double>(elapsedRaw).count();
        if (exitCode == 0) {
            logger.Log("Encoding {} successfully.", input.generic_string());
            logger.Log("Encoding '{}' to '{}' completed in {} seconds. Speed: {}", input.generic_string(),
                       outputPath.generic_string(), elapsed, speed ? std::to_string(*speed) : "N/A");
        }
        else {
            logger.Log("Failed to encode {}.", input.generic_string());
        }
    }
    catch (const std::exception ex) {
        logger.Log("Fatal error: FFmpeg process error when encoding file '{}': {}", input.generic_string(), ex.what());
        return;
    }
}
