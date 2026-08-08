
import std;
import std.compat;
import argparse;
import fs_utils;

using std::print;
using std::println;
using std::string;
using std::vector;

namespace stdfs = std::filesystem;

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
}


int main(const int argc, const char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    argparse::ArgumentParser argumentParser{"ProfileEncoder", "0.1.0"};
    configure_argument_parser(argumentParser);

    const vector<string> arguments(argv, argv + argc);
    argumentParser.parse_args(arguments);

    const auto profile = argumentParser.get<string>("--profile");
    auto inputs = argumentParser.get<vector<string>>("--input");
    const auto isStatDisabled = argumentParser.get<bool>("--no-stat");

    if (isStatDisabled) {
        println("Statistics disabled. No statistics info will be output.");
    }

    auto testPath1 = normalize_path(inputs[0]);

    println("string: {}", testPath1.string());
    println("generic string: {}", testPath1.generic_string());

    return 0;
}
