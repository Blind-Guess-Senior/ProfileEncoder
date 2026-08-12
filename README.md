# ProfileEncoder

English | [简体中文](README.zh-CN.md)

ProfileEncoder is a profile-based batch frontend for FFmpeg. It encodes one or more input files with parameters loaded from a text profile and can optionally calculate PSNR, SSIM, VMAF, and XPSNR statistics.

Quality analysis matrix output supports YUV video inputs only.

## Requirements

- A `C++23` compiler with `C++` module and `import std` support
- CMake 3.30.0 - 4.4.2
- Ninja
- vcpkg, with `VCPKG_ROOT` configured
- FFmpeg available in `PATH` at runtime

Windows presets bases on MSVC toolchain. Run the commands below from a Visual Studio developer shell to use `cl.exe`.

## Build

### Windows

Build:

```powershell
cmake --preset x64-release
cmake --build out/build/x64-release
```

Install:

```powershell
cmake --install out/build/x64-release
```

Use `x64-debug` instead of `x64-release` for a debug build.

### Linux

Haven't configured yet.

## Usage

```text
ProfileEncoder [options]
```

Example:

```powershell
ProfileEncoder --profile default --input video.mkv --matrix
```

### Options

| Option | Description |
| --- | --- |
| `-p`, `--profile <name>` | Select an encoding profile. When omitted, a TUI is opened for profile selection. Normal profiles are read from the `profiles` directory next to the executable. A name beginning with `.` selects a profile from the current working directory. |
| `-i`, `--input <files...>` | Add input files. A `txt` file is treated as a list with one input per line. Repeated `--input` options are combined. The default is `./input.txt` relative to the current working directory. |
| `--matrix` | Write encoding speed, compression ratio, and quality analysis results as a Markdown table. When used with `--no-stat`, only encoding speed and compression ratio are written. |
| `--no-stat` | Skip PSNR, SSIM, VMAF, and XPSNR quality analyses. |
| `--no-log` | Disable console log output. File logging remains enabled. |
| `--no-ffmpeg-log` | Suppress encoding-stage FFmpeg stderr lines from the log. |

### Input

Relative paths in a list file are resolved relative to that file. Command-line inputs and list entries support wildcard syntax through [p-ranav/glob: Glob for C++17](https://github.com/p-ranav/glob).

List files are parsed recursively.

Write `#noglob` to disable wildcard expansion for entries directly belonging to the current list.

### Profile

A profile is a text file containing FFmpeg arguments. By default, arguments are separated by whitespace, so related options may be written on the same line. The markers `#1`, `#2`, and `#3` select where the following arguments are placed:

```text
#1
-hwaccel cuda
#2
-c:v hevc_nvenc -preset p7
-profile:v main10
-tune hq
-rc constqp -cq
19 -multipass fullres
-spatial_aq 1
-c:a copy
#3
-y
```

For example, the profile above is passed to FFmpeg as arguments like this:
`-hwaccel cuda -i <input file> -c:v hevc_nvenc -preset p7 -profile:v main10 -tune hq -rc constqp -cq 19 -multipass fullres -spatial_aq 1 -c:a copy <output file> -y`

If no markers are present, all arguments use position `#2`.

Use `#line` when an argument contains spaces and should be read as a complete line. Use `#word` to switch back to whitespace-separated arguments.

```text
#2
-metadata
#line
title=My encoded video
#word
-c:a copy
```

### Output

Encoded files are written next to their source files using the name `<source>_<profile>.<source_extension>`. Regular logs are stored in the `logs` folder, and statistics are stored in the `statistics` folder, both next to the executable.

## References

[p-ranav/argparse: Argument Parser for Modern C++](https://github.com/p-ranav/argparse)

[boostorg/process: Boost Process](https://github.com/boostorg/process)

[ArthurSonzogni/FTXUI: C++ Functional Terminal User Interface](https://github.com/ArthurSonzogni/FTXUI)

[p-ranav/glob: Glob for C++17](https://github.com/p-ranav/glob)
