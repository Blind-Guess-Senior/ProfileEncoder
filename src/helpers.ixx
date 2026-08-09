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
