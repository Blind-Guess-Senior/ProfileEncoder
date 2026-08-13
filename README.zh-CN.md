# ProfileEncoder

[English](README.md) | 简体中文

ProfileEncoder 是一个基于预设配置的 FFmpeg 批量编码前端。它可以读取文本配置中的编码参数，对一个或多个输入文件进行编码，并可选计算 PSNR、SSIM、VMAF 和 XPSNR 质量指标。

仅支持 YUV 视频的质量分析表格输出。

## 环境要求

- 支持 `C++23`、`C++` Modules 和 `import std` 的编译器
- CMake 3.30.0 - 4.4.2
- Ninja
- vcpkg，并已配置 `VCPKG_ROOT`
- 运行时可从 `PATH` 找到 FFmpeg

Windows preset 基于 MSVC。请在 Visual Studio 开发者终端中运行下列命令以使用 `cl.exe`。

## 编译

### Windows

构建：

```powershell
cmake --preset x64-release
cmake --build out/build/x64-release
```

安装：

```powershell
cmake --install out/build/x64-release
```

将 `x64-release` 替换为 `x64-debug` 以构建 debug 版本。

### Linux

暂未配置。

## 使用方式

```text
ProfileEncoder [参数]
```

示例：

```powershell
ProfileEncoder --profile default --input video.mkv --matrix
```

### 参数

| 参数 | 说明 |
| --- | --- |
| `-p`, `--profile <名称>` | 选择编码配置。未传入此参数时，将启动 TUI 以选择配置。普通配置从程序所在目录下的 `profiles` 文件夹读取；以 `.` 开头的名称表示从当前工作目录读取配置。 |
| `-i`, `--input <文件...>` | 添加输入文件。`txt` 格式的文件将被视为每行为一个输入的列表。重复的 `--input` 参数将被合并。默认值为 `./input.txt` （基于当前工作目录）。 |
| `-o`, `--output <路径>` | 将每个输出文件放在相对于其输入文件所在目录的指定路径下。 |
| `--output-dir <路径>` | 将所有输出文件放在指定目录下。相对路径基于当前工作目录解析。 |
| `--output-profile` | 将每个输出文件放在其输入文件所在目录下以 profile 命名的文件夹内。 |
| `--matrix` | 将编码速度、压缩比和质量分析结果写为 Markdown 表格。与 `--no-stat` 同时使用时，仅写入编码速度和压缩比。 |
| `--no-stat` | 跳过 PSNR、SSIM、VMAF 和 XPSNR 质量分析。 |
| `--no-log` | 禁用控制台日志输出，文件日志仍会保留。 |
| `--no-ffmpeg-log` | 不在日志中记录编码阶段的 FFmpeg stderr 内容。 |

### 输入

列表文件中的相对路径将相对于该文件解析。命令行输入和列表内容支持通配语法，实现基于 [p-ranav/glob: Glob for C\+\+17](https://github.com/p-ranav/glob)。

列表文件会被递归解析。

写入 `#noglob` 以禁用语法展开，仅限直属于当前列表的条目。

### Profile

Profile 是一个包含 FFmpeg 参数的文本文件。默认情况下，参数按空白字符分隔，因此相关的选项可以写在同一行。标记 `#1`、`#2` 和 `#3` 用于指定后续参数的位置：

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

例如，上面的 profile 将会被传递给 ffmpeg 这样的参数：
`-hwaccel cuda -i 这里是输入文件 -c:v hevc_nvenc -preset p7 -profile:v main10 -tune hq -rc constqp -cq 19 -multipass fullres -spatial_aq 1 -c:a copy 这里是输出文件 -y`

无标记情况会被认为 `#2` 位置。

当某个参数包含空格、需要将整行作为一个参数读取时，可以使用 `#line`；使用 `#word` 可切换回按空白字符分隔参数。

```text
#2
-metadata
#line
title=My encoded video
#word
-c:a copy
```

### 输出

编码结果命名为 `<源文件名>_<配置名>.<源扩展名>`，默认保存在源文件旁。通过 `--output` 指定输入文件所在目录下的路径；通过 `--output-dir` 将所有结果放到同一个目录；通过 `--output-profile` 可将结果放入输入文件所在目录下以 profile 命名的文件夹。

普通日志保存于程序旁的 `logs` 文件夹，统计结果保存于程序旁的 `statistics` 文件夹。

## References

[p-ranav/argparse: Argument Parser for Modern C++](https://github.com/p-ranav/argparse)

[boostorg/process: Boost Process](https://github.com/boostorg/process)

[ArthurSonzogni/FTXUI: :computer: C++ Functional Terminal User Interface. :heart:](https://github.com/ArthurSonzogni/FTXUI)

[p-ranav/glob: Glob for C++17](https://github.com/p-ranav/glob)
