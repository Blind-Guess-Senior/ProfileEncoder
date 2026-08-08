export module fs_utils;

import std;
namespace stdfs = std::filesystem;

export [[nodiscard]] stdfs::path normalize_path(const std::string_view path_raw)
{
    stdfs::path path{path_raw};

    return path.lexically_normal();
}
