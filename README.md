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
| `-i`, `--input <files...>` | Add input files. An input may be a media file or a text file containing one path per line. Multiple values and repeated `--input` options are combined. The default is `./input.txt`. |
| `--matrix` | Write encoding speed, compression ratio, and quality analysis results as a Markdown table. When used with `--no-stat`, only encoding speed and compression ratio are written. |
| `--no-stat` | Skip PSNR, SSIM, VMAF, and XPSNR quality analyses. |
| `--no-log` | Disable console log output. File logging remains enabled. |
| `--no-ffmpeg-log` | Suppress encoding-stage FFmpeg stderr lines from the log. |

### Profile syntax

A profile is a text file containing one complete FFmpeg argument per line. Empty and whitespace-only lines are ignored. The optional marker lines `#1`, `#2`, and `#3` select where the following arguments are placed:

```text
#1
-hwaccel
cuda
#2
-c:v
hevc_nvenc
-preset
p7
-profile:v
main10
-tune
hq
-rc
constqp
-cq
19
-multipass
fullres
-spatial_aq
1
-c:a
copy
#3
-y
```

For example, the profile above is passed to FFmpeg as arguments like this:
`-hwaccel cuda -i <input file> -c:v hevc_nvenc -preset p7 -profile:v main10 -tune hq -rc constqp -cq 19 -multipass fullres -spatial_aq 1 -c:a copy <output file> -y`

If no markers are present, all arguments use position `#2`.

### Output

Encoded files are written next to their source files using the name `<source>_<profile>.<source_extension>`. Regular logs are stored in the `logs` folder, and statistics are stored in the `statistics` folder, both next to the executable.
