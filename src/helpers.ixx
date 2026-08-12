module;

#include "p-ranav/glob.hpp"

export module helpers;

import std;

export [[nodiscard]] bool is_txt(const std::filesystem::path& path)
{
    const auto ext = path.extension().generic_string();
    constexpr std::string_view txt = ".txt";

    return std::ranges::equal(ext, txt, [](const unsigned char a, const unsigned char b)
                              { return std::tolower(a) == std::tolower(b); });
}

export [[nodiscard]] std::filesystem::path normalized_path(std::string_view path_raw)
{
    const std::filesystem::path path{path_raw};
    return std::filesystem::weakly_canonical(path);
}

export [[nodiscard]]
bool contains_glob_pattern(const std::filesystem::path& path)
{
    const auto text = path.generic_string();
    return text.find_first_of("*?[") != std::string::npos;
}

export [[nodiscard]]
bool contains_recursive_glob(const std::filesystem::path& path)
{
    for (const auto& component : path) {
        if (component.generic_string() == "**") {
            return true;
        }
    }
    return false;
}

export [[nodiscard]]
std::vector<std::filesystem::path> expand_glob(const std::filesystem::path& path)
{
    const auto pattern = path.generic_string();

    if (contains_recursive_glob(path)) {
        return glob::rglob(pattern);
    }

    return glob::glob(pattern);
}
