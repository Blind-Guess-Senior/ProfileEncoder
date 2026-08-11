# ProfileEncoder

[English](README.md) | 简体中文

ProfileEncoder 是一个基于预设配置的 FFmpeg 批量编码前端。它可以读取文本配置中的编码参数，对一个或多个输入文件进行编码，并可选计算 PSNR、SSIM、VMAF 和 XPSNR 质量指标。

仅支持 YUV 视频的质量分析表格输出。

## 环境要求

- 支持 `C++23`、`C++` Modules 和 `import std` 的编译器
- CMake 3.30 或更高版本
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
| `-p`, `--profile <名称>` | 选择编码配置，默认为 `default`。普通配置从程序所在目录下的 `profiles` 文件夹读取；以 `.` 开头的名称表示从当前工作目录读取配置。 |
| `-i`, `--input <文件...>` | 添加输入文件。输入可以是媒体文件，也可以是每行包含一个路径的文本文件。多个值及重复出现的 `--input` 参数会被合并。默认值为 `./input.txt`。 |
| `--matrix` | 将 PSNR、SSIM、VMAF 和 XPSNR 结果写为 Markdown 表格。不能与 `--no-stat` 同时使用。 |
| `--no-stat` | 跳过所有质量分析。 |
| `--no-log` | 禁用控制台日志输出，文件日志仍会保留。 |
| `--no-ffmpeg-log` | 不在日志中记录编码阶段的 FFmpeg stderr 内容。 |

编码结果保存在源文件旁，文件名格式为 `<源文件名>_<配置名>.<扩展名>`。普通日志保存在程序旁的 `log` 文件夹，Markdown 统计结果保存在 `statistics` 文件夹。
