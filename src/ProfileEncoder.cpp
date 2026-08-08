
import std;
import std.compat;
import argparse;
import logger;

// using std::println;
using std::string;
using std::string_view;
using std::vector;

namespace stdfs = std::filesystem;

constexpr string_view LOG_FOLDER = "log";
constexpr string_view PROFILE_FOLDER = "profiles";

bool is_stat_disabled;
// Logger* logger;

static void configure_argument_parser(argparse::ArgumentParser& parser);
[[nodiscard]] stdfs::path normalized_path(const std::string_view path_raw);

int main(const int argc, const char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    argparse::ArgumentParser argumentParser{"ProfileEncoder", "0.1.0"};
    configure_argument_parser(argumentParser);

    const vector<string> arguments(argv, argv + argc);
    argumentParser.parse_args(arguments);

    const auto profileRaw = argumentParser.get<string>("--profile");
    auto inputs = argumentParser.get<vector<string>>("--input");
    is_stat_disabled = argumentParser.get<bool>("--no-stat");
    const bool isLogDisabled = argumentParser.get<bool>("--no-log");

    auto cwd = stdfs::current_path() / "";
    auto exePath = stdfs::weakly_canonical(stdfs::path{argv[0]}).parent_path(); // https://stackoverflow.com/a/55579815
    auto profilePath = exePath / PROFILE_FOLDER / "";
    auto logPath = exePath / LOG_FOLDER / "";

    Logger logger{logPath, isLogDisabled};

    // Log toggle info
    if (is_stat_disabled) {
        logger.Log("Statistics disabled. No statistics info will be output.");
    }
    if (isLogDisabled) {
        logger.Log("Console log disabled.");
    }

    // Process profile
    bool isLocalProfile = false;
    if (profileRaw[0] == '.') {
        isLocalProfile = true;
    }
    stdfs::path targetProfileFile;
    if (isLocalProfile) {
        targetProfileFile = cwd / stdfs::path{profileRaw.substr(1) + ".txt"};
    }
    else {
        targetProfileFile = profilePath / stdfs::path{profileRaw + ".txt"};
    }

    if (!stdfs::exists(targetProfileFile)) {
        logger.Log("Position {} does not contain given profile '{}'.",
                   isLocalProfile ? cwd.generic_string() : profilePath.generic_string(),
                   isLocalProfile ? profileRaw.substr(1) : profileRaw);
        return 0;
    }
    if (!stdfs::is_regular_file(targetProfileFile)) {
        logger.Log("Profile '{}' in position {} is not a valid file.", profileRaw,
                   isLocalProfile ? cwd.generic_string() : profilePath.generic_string());
        return 0;
    }

    // Read profile
    std::ifstream profileFile{targetProfileFile};
    if (!profileFile.is_open()) {
        std::cerr << "Fatal error: cannot open profile file '" << targetProfileFile.generic_string() << "'.\n";
        logger.Log("Fatal error: cannot open profile file '{}'", targetProfileFile.generic_string());
        return 0;
    }
    vector<string> profileParams{};
    string param;
    while (profileFile >> param) {
        profileParams.push_back(param);
    }

    logger.Log("Encoding with profile params {}", profileParams);

    auto testPath1 = normalized_path(inputs[0]);

    return 0;
}

static void configure_argument_parser(argparse::ArgumentParser& parser)
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
    parser.add_argument("--no-stat").flag().help("Turn off statistics info.");
    parser.add_argument("--no-log").flag().help("Turn off command line log output. File log will still preserved.");
}

[[nodiscard]] stdfs::path normalized_path(const std::string_view path_raw)
{
    stdfs::path path{path_raw};
    return stdfs::weakly_canonical(path);
}
