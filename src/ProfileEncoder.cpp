
import std;

import argparse;
import ftxui;

import statistics_result;
import logger;
import helpers;
import process_runner;

using std::string;
using std::string_view;
using std::vector;

namespace stdfs = std::filesystem;

constexpr string_view LOG_FOLDER = "logs";
constexpr string_view PROFILE_FOLDER = "profiles";
constexpr string_view MATRIX_FOLDER = "statistics";

static void configure_argument_parser(argparse::ArgumentParser& parser);

[[nodiscard]] static vector<string> collect_profile_names(const stdfs::path& folder);
[[nodiscard]] static std::array<vector<string>, 3> collect_profile_info(Logger& logger, const string& profile_raw);

static void collect_input_file(Logger& logger, vector<stdfs::path>& target_files, std::unordered_set<stdfs::path>& seen,
                               const string& input, const stdfs::path& base_directory, bool enable_glob = true);
static void collect_concrete_input_file(Logger& logger, vector<stdfs::path>& target_files,
                                        std::unordered_set<stdfs::path>& seen, const stdfs::path& input_path);

static void process_input_file(Logger& logger, const process_runner::FFmpegRunner& ffmpeg_runner,
                               const stdfs::path& input_path, const string& profile_raw);

[[nodiscard]] static std::optional<string> profile_select_menu(Logger& logger, const stdfs::path& profiles_folder);
[[nodiscard]] static std::optional<string> input_profile_name();

static bool is_stat_disabled;

static stdfs::path cwd;
static stdfs::path exe_path;
static stdfs::path profiles_path;
static stdfs::path log_path;
static stdfs::path matrix_path;

int main(const int argc, const char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    cwd = stdfs::current_path() / "";
    exe_path = normalized_path(argv[0]).parent_path(); // https://stackoverflow.com/a/55579815
    profiles_path = exe_path / PROFILE_FOLDER / "";
    log_path = exe_path / LOG_FOLDER / "";
    matrix_path = exe_path / MATRIX_FOLDER / "";

    argparse::ArgumentParser argumentParser{"ProfileEncoder", "0.1.0"};
    configure_argument_parser(argumentParser);

    // MSVC bug: if template unfold happens in argparse's "std", it would error.
    const vector<string> arguments(argv, argv + argc);
    argumentParser.parse_args(arguments);

    const auto isProfileInputted = argumentParser.is_used("--profile");
    string profileRaw;
    auto inputs = argumentParser.get<vector<string>>("--input");
    const bool isMatrixEnabled = argumentParser.get<bool>("--matrix");
    is_stat_disabled = argumentParser.get<bool>("--no-stat");
    const bool isLogDisabled = argumentParser.get<bool>("--no-log");
    const bool isFFmpegLogDisabled = argumentParser.get<bool>("--no-ffmpeg-log");

    Logger logger{log_path, isLogDisabled, isFFmpegLogDisabled, is_stat_disabled, isMatrixEnabled, matrix_path};

    // Log toggle info
    if (is_stat_disabled) {
        logger.Log("Statistics disabled.");
    }
    if (isLogDisabled) {
        logger.Log("Console log disabled.");
    }

    if (isProfileInputted) {
        profileRaw = argumentParser.get<string>("--profile");
    }
    else {
        const auto selectedProfile = profile_select_menu(logger, profiles_path);
        if (!selectedProfile) {
            return 0;
        }
        profileRaw = *selectedProfile;
    }

    const process_runner::FFmpegRunner ffmpegRunner{logger, collect_profile_info(logger, profileRaw)};

    // Collect input files
    vector<stdfs::path> targetFiles{};
    std::unordered_set<stdfs::path> seen{};

    for (const auto& input : inputs) {
        collect_input_file(logger, targetFiles, seen, input, cwd);
    }

    logger.Log("{} files found. Start encoding {}.", targetFiles.size(),
               is_stat_disabled ? "without statistics" : "with statistics");

    // Process input files
    for (const auto& target : targetFiles) {
        process_input_file(logger, ffmpegRunner, target, profileRaw);
    }

    return 0;
}

void configure_argument_parser(argparse::ArgumentParser& parser)
{
    parser.add_description("Profile Encoder, a batch encoder for re-enc files in pre-defined profiles.");
    parser.add_argument("-p", "--profile").help("Choose one profile of encode. Profiles are under ./profiles folder.");
    parser.add_argument("-i", "--input")
        .append()
        .default_value<vector<string>>(vector<string>{"./input.txt"})
        .nargs(argparse::nargs_pattern::at_least_one)
        .help("Input files. Can be a single file or a txt that every single line refers to a file. Multiple input arg "
              "will be composed.");
    parser.add_argument("--matrix").flag().help("Output markdown table style statistics data.");
    parser.add_argument("--no-stat").flag().help("Turn off video quality statistics.");
    parser.add_argument("--no-log").flag().help("Turn off command line log output. File log will still preserved.");
    parser.add_argument("--no-ffmpeg-log").flag().help("Turn off ffmpeg stderr output.");
}

vector<string> collect_profile_names(const stdfs::path& folder)
{
    vector<string> profiles;

    if (!stdfs::is_directory(folder)) {
        return profiles;
    }

    for (const auto& entry : stdfs::directory_iterator(folder)) {
        if (entry.is_regular_file() && is_txt(entry.path())) {
            profiles.emplace_back(entry.path().stem().generic_string());
        }
    }

    std::ranges::sort(profiles);
    return profiles;
}

std::array<vector<string>, 3> collect_profile_info(Logger& logger, const string& profile_raw)
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
        targetProfileFile = profiles_path / stdfs::path{profile_raw + ".txt"};
    }

    if (!stdfs::exists(targetProfileFile)) {
        logger.Log("Error: Position {} does not contain given profile '{}'.",
                   isLocalProfile ? cwd.generic_string() : profiles_path.generic_string(),
                   isLocalProfile ? profile_raw.substr(1) : profile_raw);
        throw std::runtime_error("Error: profile not found.");
    }
    if (!stdfs::is_regular_file(targetProfileFile)) {
        logger.Log("Profile '{}' in position {} is not a valid file.", profile_raw,
                   isLocalProfile ? cwd.generic_string() : profiles_path.generic_string());
        throw std::runtime_error("Error: profile not valid.");
    }

    // Read profile
    std::ifstream profileFile{targetProfileFile};
    if (!profileFile.is_open()) {
        std::cerr << "Fatal error: cannot open profile file '" << targetProfileFile.generic_string() << "'.\n";
        logger.Log("Fatal error: cannot open profile file '{}'", targetProfileFile.generic_string());
        throw std::runtime_error("Fatal error: cannot open profile file.");
    }
    std::array<vector<string>, 3> profileParams;
    string param;
    std::size_t currentPosition = 1;
    enum class READ_MODE
    {
        Line,
        Word,
    };
    auto mode = READ_MODE::Word;

    while (true) {
        switch (mode) {
        case READ_MODE::Line:
            {
                if (!std::getline(profileFile, param)) {
                    goto ReadParamEnd;
                }

                if (param.ends_with('\r')) {
                    param.pop_back();
                } // If a file in Linux use crlf, it may cause problem.

                if (param.empty() || param.find_first_not_of(" \t\r") == string::npos) {
                    continue;
                }
                break;
            }
        case READ_MODE::Word:
            {
                if (!(profileFile >> param)) {
                    goto ReadParamEnd;
                }
                break;
            }
        }

        if (param == "#1") {
            currentPosition = 0;
        }
        else if (param == "#2") {
            currentPosition = 1;
        }
        else if (param == "#3") {
            currentPosition = 2;
        }
        else if (param == "#line") {
            mode = READ_MODE::Line;
        }
        else if (param == "#word") {
            mode = READ_MODE::Word;
        }
        else {
            profileParams[currentPosition].push_back(std::move(param));
        }
    }

ReadParamEnd:

    logger.Log("Encoding with profile params {}", profileParams);

    //profileFile.close(); // Dtor will automatically close.

    return profileParams;
}

void collect_input_file(Logger& logger, vector<stdfs::path>& target_files, std::unordered_set<stdfs::path>& seen,
                        const string& input, const stdfs::path& base_directory, const bool enable_glob)
{
    std::filesystem::path inputPath{input};

    if (inputPath.is_relative()) {
        inputPath = base_directory / inputPath;
    }

    if (enable_glob && contains_glob_pattern(inputPath)) {
        const auto matches = expand_glob(inputPath);

        if (matches.empty()) {
            logger.Log("Input pattern {} matched no files. Skipped.", inputPath.generic_string());
            return;
        }

        for (const auto& match : matches) {
            collect_concrete_input_file(logger, target_files, seen, match);
        }
        return;
    }

    collect_concrete_input_file(logger, target_files, seen, normalized_path(inputPath.generic_string()));
}

void collect_concrete_input_file(Logger& logger, vector<stdfs::path>& target_files,
                                 std::unordered_set<stdfs::path>& seen, const stdfs::path& input_path)
{
    const auto inputPath = normalized_path(input_path.generic_string());

    if (!stdfs::exists(inputPath)) {
        logger.Log("Error: input file {} does not exist. Skipped.", inputPath.generic_string());
        return;
    }

    if (!stdfs::is_regular_file(inputPath)) {
        logger.Log("Error: input file {} is not valid. Skipped.", inputPath.generic_string());
        return;
    }

    // Deduplicate before txt check
    if (!seen.insert(inputPath).second) {
        return;
    }

    if (!is_txt(inputPath)) {
        target_files.push_back(inputPath);
        return;
    }

    std::ifstream inputList{inputPath};

    if (!inputList.is_open()) {
        logger.Log("Error: cannot open input list file '{}'. Skipped.", inputPath.generic_string());
        return;
    }

    bool enableGlob = true;
    string line;
    while (std::getline(inputList, line)) {
        if (line.ends_with('\r')) {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }
        if (line == "#noglob") {
            enableGlob = false;
            continue;
        }

        collect_input_file(logger, target_files, seen, line, inputPath.parent_path(), enableGlob);
    }
}

void process_input_file(Logger& logger, const process_runner::FFmpegRunner& ffmpeg_runner,
                        const stdfs::path& input_path, const string& profile_raw)
{
    stat_result::statistics_values results;

    const auto outputPath = input_path.parent_path() /
        (input_path.stem().generic_string() + "_" + profile_raw + input_path.extension().generic_string());

    // Encode
    logger.Log("Started to encode {} in profile {}.", input_path.generic_string(), profile_raw);

    const auto runResult = ffmpeg_runner.RunEncode(input_path, outputPath);
    if (!runResult) {
        logger.Log("{}", runResult.error());
        if (!is_stat_disabled) {
            logger.Log("Statistics skipped.");
        }
        return;
    }
    const auto [exitCode, elapsedRaw, speed] = runResult.value();

    const auto elapsed = std::chrono::duration<double>(elapsedRaw).count();
    if (exitCode == 0) {
        logger.Log("Encoding {} successfully.", input_path.generic_string());
        logger.Log("Encoding '{}' to '{}' completed in {} seconds. Speed: {}", input_path.generic_string(),
                   outputPath.generic_string(), elapsed, speed ? std::to_string(*speed) : "N/A");
    }
    else {
        logger.Log("Failed to encode '{}'.", input_path.generic_string());
        if (!is_stat_disabled) {
            logger.Log("Statistics skipped.");
        }
        return;
    }
    // Store result
    results["speed"] = speed;
    // Calculate compress rate
    std::error_code inputError;
    std::error_code outputError;

    const auto inputSize = std::filesystem::file_size(input_path, inputError);
    const auto outputSize = std::filesystem::file_size(outputPath, outputError);

    if (!inputError && !outputError && inputSize != 0) {
        results["compression_ratio"] = static_cast<double>(outputSize) / static_cast<double>(inputSize);
    }
    else {
        results["compression_ratio"] = std::nullopt;
    }

    // Analyze
    if (!is_stat_disabled) {
        ffmpeg_runner.RunAnalyzes(outputPath, input_path, results);
    }

    logger.Matrix(outputPath, results);
}

std::optional<string> profile_select_menu(Logger& logger, const stdfs::path& profiles_folder)
{
    std::vector<std::string> entries = collect_profile_names(profiles_folder);
    const auto manualInputIndex = entries.size();
    entries.emplace_back("Enter profile name...");

    int selected = 0;
    bool cancelled = false;

    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    ftxui::MenuOption options;
    options.on_enter = screen.ExitLoopClosure();

    const auto menu = ftxui::Menu(&entries, &selected, options);

    const auto component = ftxui::CatchEvent(menu,
                                             [&](const ftxui::Event event)
                                             {
                                                 if (event != ftxui::Event::Escape) {
                                                     return false;
                                                 }

                                                 cancelled = true;
                                                 screen.ExitLoopClosure()();
                                                 return true;
                                             });

    screen.Loop(component);

    if (cancelled) {
        logger.Log("Cancelled. Program exited.");
        return std::nullopt;
    }

    if (static_cast<std::size_t>(selected) == manualInputIndex) {
        return input_profile_name();
    }

    logger.Log("Select profile {}", entries[selected]);

    return entries[selected];
}

std::optional<string> input_profile_name()
{
    string profile;
    bool cancelled = false;

    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    auto input = ftxui::Input(&profile, "Profile name: ");

    const auto component = ftxui::CatchEvent(input,
                                             [&](const ftxui::Event event)
                                             {
                                                 if (event == ftxui::Event::Escape) {
                                                     cancelled = true;
                                                     screen.ExitLoopClosure()();
                                                     return true;
                                                 }

                                                 if (event == ftxui::Event::Return && !profile.empty()) {
                                                     screen.ExitLoopClosure()();
                                                     return true;
                                                 }

                                                 return false;
                                             });

    screen.Loop(component);

    if (cancelled) {
        return std::nullopt;
    }

    return profile;
}
