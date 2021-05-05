# SpleeterMsvcExe

[![GitHub release](https://img.shields.io/github/release/wudicgi/SpleeterMsvcExe.svg)](https://github.com/wudicgi/SpleeterMsvcExe/releases/latest) [![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/wudicgi/SpleeterMsvcExe/master/LICENSE)

## 1. Introduction ([中文](#1-简介))

SpleeterMsvcExe is a Windows command line program for [Spleeter](https://github.com/deezer/spleeter), which can be used directly.

It is written in pure C language, using ffmpeg to read and write audio files, and using Tensorflow C API to make use of Spleeter models. No need to install Python environment, and it does not contain anything related to Python.

Furthermore, SpleeterMsvcExe reduced the memory usage through segmented processing, hence it can handle single audio file over 30 minutes. Due to the length-extending process, all segments can be concatenated seamlessly.

## 2. Usage

Download the latest release version program, and extract.

Run the `download_models.bat` batch script in models folder, to automatically download and extract Spleeter model files. Or manually download the `models-all.zip` file in releases, and extract it to models folder (if the 16kHz models are needed, you need to run `generate_16kHz.bat` too).

Drag-and-drop the song.mp3 file to `spleeter.exe`, or execute the following command:

```
spleeter.exe song.mp3
```

It will split song.mp3 into two tracks: song.vocals.mp3 and song.accompaniment.mp3

If it reports missing DLL files, please install [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019 (x64)](https://aka.ms/vs/16/release/vc_redist.x64.exe) ([Source](https://support.microsoft.com/en-us/topic/the-latest-supported-visual-c-downloads-2647da03-1eea-4433-9aff-95f26a218cc0)).

## 3. Help and more examples

```
Usage: spleeter [options] <input_file_path>

Options:
    -m, --model         Spleeter model name
                        (2stems, 4stems, 5stems-16kHz, ..., default: 2stems)
    -o, --output        Output base file name
                        (in the format of filename.ext, default: <input_file_path>)
    -b, --bitrate       Output file bit rate
                        (128k, 192000, 256k, ..., default: 256k)
    --overwrite         Overwrite when the target output file exists
    -h, --help          Display this help and exit
    -v, --version       Display program version and exit

Examples:
    spleeter -m 2stems song.mp3
    - Splits song.mp3 into 2 tracks: vocals, accompaniment
    - Outputs 2 files: song.vocals.mp3, song.accompaniment.mp3
    - Output file format is same as input, using default bitrate 256kbps

    spleeter -m 4stems -o result.m4a -b 192k song.mp3
    - Splits song.mp3 into 4 tracks: vocals, drums, bass, other
    - Outputs 4 files: result.vocals.m4a, result.drums.m4a, ...
    - Output file format is M4A, using bitrate 192kbps

    spleeter --model 5stems-16kHz --bitrate 320000 song.mp3
    - Long option example
    - Use fine-tuned model, spliting song.mp3 into 5 tracks
```

## 4. Thanks

- Thanks to [Deezer](https://www.deezer.com/) for open-sourced the [Spleeter](https://github.com/deezer/spleeter) project.

- Thanks to [Guillaume Vincke](https://github.com/gvne) for bringing out the [spleeterpp](https://github.com/gvne/spleeterpp) project. The Spleeter processing part of this project made reference to its implementation, and used the converted Spleeter model files provided by spleeterpp project.

---

## 1. 简介

SpleeterMsvcExe 是 [Spleeter](https://github.com/deezer/spleeter) 的 Windows 命令行程序，可直接运行使用。

纯 C 语言编写，使用 ffmpeg 读取和写入音频文件，使用 Tensorflow C API 调用 Spleeter 模型。无需安装 Python 环境，内部也不包含任何 Python 相关内容。

此外，SpleeterMsvcExe 还通过分段处理减少了内存占用，它可以处理长度超过 30 分钟的单个音频文件。分段时有长度扩展处理，使各分段可无缝连接，合并结果无可感知的差异。

## 2. 使用方法

下载最新的 release 版本程序，解压到任意位置。

执行 models 目录中的 `download_models.bat` 脚本自动下载并解压 Spleeter 模型文件。或手动下载 release 中的 `models-all.zip` 文件并解压到 models 目录中 (如果需要 16kHz 模型还需手动执行 `generate_16kHz.bat`)。

将 song.mp3 文件拖拽到 `spleeter.exe` 上，或在命令行执行

```
spleeter.exe song.mp3
```

即可将 song.mp3 分离为人声 (song.vocals.mp3) 和伴奏 (song.accompaniment.mp3) 两个音轨。

如果运行时报告缺少 DLL 文件，请安装 [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019 (x64)](https://aka.ms/vs/16/release/vc_redist.x64.exe) ([来源页面](https://support.microsoft.com/en-us/topic/the-latest-supported-visual-c-downloads-2647da03-1eea-4433-9aff-95f26a218cc0))。

## 3. 帮助和更多示例

```
使用: spleeter [选项] <输入文件路径>

选项:
    -m, --model         Spleeter 模型名称
                        (2stems, 4stems, 5stems-16kHz, ..., 默认值: 2stems)
    -o, --output        输出基础文件名
                        (格式为 文件名.扩展名，默认值: <输入文件路径>)
    -b, --bitrate       输出文件的比特率
                        (128k, 192000, 256k, ..., 默认值: 256k)
    --overwrite         当目标输出文件已存在时直接覆盖
    -h, --help          显示帮助文本并退出
    -v, --version       显示程序版本号并退出

示例:
    spleeter -m 2stems song.mp3
    - 将 song.mp3 分离为 2 个音轨: vocals, accompaniment (人声，伴奏)
    - 输出 2 个文件: song.vocals.mp3, song.accompaniment.mp3
    - 输出文件使用和输入相同的 MP3 格式，比特率为默认的 256kbps

    spleeter -m 4stems -o result.m4a -b 192k song.mp3
    - 将 song.mp3 分离为 4 个音轨: vocals, drums, bass, other (人声，鼓，贝斯，其它)
    - 输出 4 个文件: result.vocals.m4a, result.drums.m4a, ...
    - 输出文件使用 M4A 格式，比特率为 192kbps

    spleeter --model 5stems-16kHz --bitrate 320000 song.mp3
    - 使用长选项 (long option) 参数的示例
    - 使用最高到 16kHz 的精细模型，将 song.mp3 分离为 5 个音轨
```

## 4. 感谢

- 感谢 [Deezer](https://www.deezer.com/) 开源了 [Spleeter](https://github.com/deezer/spleeter) 项目。

- 感谢 [Guillaume Vincke](https://github.com/gvne) 带来了 [spleeterpp](https://github.com/gvne/spleeterpp) 项目。本项目的 Spleeter 处理部分代码参考其实现，并使用了其提供的转换后的 Spleeter 模型文件。
