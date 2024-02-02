# SpleeterMsvcExe

## Changelog ([中文](#更新日志))

### v2.0 (2024-02-02)

- Added support to 22kHz models, and also updated the model files (the new program cannot use the old model files)
- Added environment checks, which will display an error message when the CPU lacks required features or when DLL loading fails
- Added --tracks parameter to allow user to select output tracks or do simple mixing
- Added --debug switch to output debug information
- Modified --output parameter to be a file path format string that supports using variables
- Replaced tensorflow library with a customized version 1.15.5-mod.1, which allows set the filename of saved_model.pb
- Upgraded ffmpeg-win64 library to v6.1.1
- Fixed the bug that sometimes the encoder will generate warnings "N frames left in the queue on closing"

### v1.0 (2021-05-12)

- Initial version

## 更新日志

### v2.0 (2024-02-02)

- 添加了对 22kHz 模型的支持，同时更新了模型文件 (新版本程序不可以使用旧版本的模型文件)
- 添加了对运行环境的检查，当 CPU 缺少必要特性或 DLL 加载失败时，将给出错误提示
- 添加了 --tracks 参数，允许用户选择要输出的音轨，或进行简单的混音
- 添加了 --debug 开关，用于输出调试信息
- 将 --output 参数修改为支持文件路径格式字符串，并支持使用变量
- 将 tensorflow 库替换为了定制的 1.15.5-mod.1 版本，该版本允许指定不同的 saved_model.pb 文件名
- 将 ffmpeg-win64 库升级到了 v6.1.1 版本
- 解决了有时会导致编码器输出警告信息 "N frames left in the queue on closing" 的 bug

### v1.0 (2021-05-12)

- 初始版本
