/*
 * MIT License
 *
 * Copyright (c) 2018-2021 Wudi <wudi@wudilabs.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <tchar.h>
#include <locale.h>
#include <io.h>
#include <Shlwapi.h>
#include "../version.h"
#include "getopt.h"
#include "Common.h"
#include "AudioFileReader.h"
#include "SpleeterProcessor.h"

static void _displayHelp(int argc, TCHAR *argv[]) {
    _tprintf(_T("%s %s\n"), _T(PROGRAM_NAME), _T(PROGRAM_VERSION));
    _tprintf(_T("\n"));

    _tprintf(_T("Usage: %s [options] <input_file_path>\n"), argv[0]);
    _tprintf(_T("\n"));

    _tprintf(_T("Options:\n"));
    _tprintf(_T("    -m, --model         Spleeter model name\n"));
    _tprintf(_T("                        (2stems, 4stems, 5stems-16kHz, ..., default: 2stems)\n"));
    _tprintf(_T("    -o, --output        Output base file name\n"));
    _tprintf(_T("                        (in the format of filename.ext, default: <input_file_path>)\n"));
    _tprintf(_T("    -b, --bitrate       Output file bit rate\n"));
    _tprintf(_T("                        (128k, 192000, 256k, ..., default: 256k)\n"));
    _tprintf(_T("    --overwrite         Overwrite when the target output file exists\n"));
    _tprintf(_T("    -h, --help          Display this help and exit\n"));
    _tprintf(_T("    -v, --version       Display program version and exit\n"));
    _tprintf(_T("\n"));

    _tprintf(_T("Examples:\n"));
    _tprintf(_T("    %s -m 2stems song.mp3\n"), argv[0]);
    _tprintf(_T("    - Splits song.mp3 into 2 tracks: vocals, accompaniment\n"));
    _tprintf(_T("    - Outputs 2 files: song.vocals.mp3, song.accompaniment.mp3\n"));
    _tprintf(_T("    - Output file format is same as input, using default bitrate 256kbps\n"));
    _tprintf(_T("\n"));
    _tprintf(_T("    %s -m 4stems -o result.m4a -b 192k song.mp3\n"), argv[0]);
    _tprintf(_T("    - Splits song.mp3 into 4 tracks: vocals, drums, bass, other\n"));
    _tprintf(_T("    - Outputs 4 files: result.vocals.m4a, result.drums.m4a, ...\n"));
    _tprintf(_T("    - Output file format is M4A, using bitrate 192kbps\n"));
    _tprintf(_T("\n"));
    _tprintf(_T("    %s --model 5stems-16kHz --bitrate 320000 song.mp3\n"), argv[0]);
    _tprintf(_T("    - Long option example\n"));
    _tprintf(_T("    - Use fine-tuned model, spliting song.mp3 into 5 tracks\n"));
}

static void _displayVersion(void) {
    _tprintf(_T("%s\n"), _T(PROGRAM_VERSION));
}

/**
 * 在文件路径原有的扩展名前添加额外的扩展名
 *
 * @param   destFilePathBuffer              目标文件路径缓冲区
 * @param   destFilePathBufferCharCount     destFilePathBuffer 所能容纳的字符数
 * @param   srcFilePath                     原始文件路径
 * @param   extraExtension                  要添加的额外扩展名 (不包含起始位置的 ".")
 */
static bool _addExtraExtensionBeforeOriginalExtension(TCHAR *destFilePathBuffer, size_t destFilePathBufferCharCount,
        const TCHAR *srcFilePath, const TCHAR *extraExtension) {
    TCHAR *originalExtension = PathFindExtension(srcFilePath);

    int srcFilePathWithoutExtensionLength = (int)(originalExtension - srcFilePath);
    if ((srcFilePathWithoutExtensionLength < 0)
            || (srcFilePathWithoutExtensionLength > (FILE_PATH_MAX_SIZE - 1))) {
        return false;
    }

    TCHAR srcFilePathWithoutExtension[FILE_PATH_MAX_SIZE] = { _T('\0') };
    _tcsncpy(srcFilePathWithoutExtension, srcFilePath, srcFilePathWithoutExtensionLength);
    srcFilePathWithoutExtension[FILE_PATH_MAX_SIZE - 1] = _T('\0');

    int writtenLength = _sntprintf(destFilePathBuffer, destFilePathBufferCharCount, _T("%s.%s%s"),
            srcFilePathWithoutExtension, extraExtension, originalExtension);
    if ((writtenLength >= 0) && (writtenLength < destFilePathBufferCharCount)) {
        return true;
    } else {
        return false;
    }
}

static bool _checkInputFilePath(const TCHAR *inputFilePath) {
    // 检查指定的输入文件是否存在
    if (_taccess(inputFilePath, 0) == -1) {
        _ftprintf(stderr, _T("Error: The specified input file \"%s\" does not exist.\n"), inputFilePath);
        return false;
    }

    // 检查指定的输入文件是否可读
    if (_taccess(inputFilePath, 4) == -1) {
        _ftprintf(stderr, _T("Error: The specified input file \"%s\" cannot be read.\n"), inputFilePath);
        return false;
    }

    return true;
}

static bool _getOutputFilePath(TCHAR *outputFilePathBuffer, size_t outputFilePathBufferCharCount,
        const TCHAR *outputBaseFilePath, const TCHAR *trackName) {
    // 检查 outputBaseFilePath 加上 trackName 后是否超长
    if (!_addExtraExtensionBeforeOriginalExtension(outputFilePathBuffer, outputFilePathBufferCharCount,
            outputBaseFilePath, trackName)) {
        _ftprintf(stderr, _T("Error: The output file path for track \"%s\" is too long.\n"), trackName);
        return false;
    }
    outputFilePathBuffer[outputFilePathBufferCharCount - 1] = _T('\0');

    return true;
}

static bool _checkOutputFilePath(const TCHAR *outputFilePath, int overwriteFlag) {
    // 如果 outputFilePath 存在
    if (_taccess(outputFilePath, 0) != -1) {
        // 如果未指定 --overwrite 选项
        if (!overwriteFlag) {
            _ftprintf(stderr, _T("Error: The output file \"%s\" has already existed. Add --overwrite option to ignore.\n"), outputFilePath);
            return false;
        }

        // 如果 outputFilePath 不可写入
        if (_taccess(outputFilePath, 2) == -1) {
            _ftprintf(stderr, _T("Error: The output file \"%s\" cannot be written.\n"), outputFilePath);
            return false;
        }
    }

    return true;
}

static bool _tryParseBitrate(int *parsedResultBitrate, const TCHAR *optionValue) {
    if (parsedResultBitrate == NULL) {
        return false;
    }

    TCHAR optionValueBuffer[10] = { _T('\0') };
    _tcsncpy(optionValueBuffer, optionValue, (10 - 1));
    optionValueBuffer[10 - 1] = _T('\0');

    size_t optionValueLength = _tcsclen(optionValueBuffer);

    int multiplier;
    if ((optionValueBuffer[optionValueLength - 1] == 'k')
            || (optionValueBuffer[optionValueLength - 1] == 'K')) {
        multiplier = 1000;
    } else {
        multiplier = 1;
    }

    unsigned long parsedValue = _tcstoul(optionValueBuffer, NULL, 10);
    if (parsedValue > 0) {
        *parsedResultBitrate = parsedValue * multiplier;
        return true;
    } else {
        return false;
    }
}

int _tmain(int argc, TCHAR *argv[]) {
    setlocale(LC_ALL, "");

    ////////////////////////////////////////////////// 获取命令行参数 //////////////////////////////////////////////////

#if defined(_DEBUG) && 0
    if (argc <= 1) {
        argc = 1;

        // argv[argc++] = _T("--output");
        // argv[argc++] = _T("D:\\Projects\\SpleeterMsvcExe\\bin\\x64\\Debug\\result.m4a");
        argv[argc++] = _T("--overwrite");
        argv[argc++] = _T("D:\\Projects\\SpleeterMsvcExe\\bin\\x64\\Debug\\demo.mp3");

        argv[argc++] = _T("-b");
        argv[argc++] = _T("128k");
    }
#endif

#if defined(_DEBUG) && 0
    for (int i = 0; i < argc; i++) {
        _tprintf(_T("argv[%d] = %s\n"), i, argv[i]);
    }
    _tprintf(_T("\n"));
#endif

    TCHAR inputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
    TCHAR outputBaseFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
    TCHAR modelName[FILE_PATH_MAX_SIZE] = { _T('\0') };

    int outputFileBitrate = 0;

    static int overwriteFlag = 0;
    static int verboseFlag = 0;
    static int debugFlag = 0;
    static int helpFlag = 0;
    static int versionFlag = 0;

    while (true) {
        static struct option longOptions[] = {
            // name,            has_arg,    flag,           val
            {_T("model"),       ARG_REQ,    0,              _T('m')},
            {_T("output"),      ARG_REQ,    0,              _T('o')},
            {_T("bitrate"),     ARG_REQ,    0,              _T('b')},
            {_T("overwrite"),   ARG_NONE,   &overwriteFlag, 1},
            {_T("verbose"),     ARG_NONE,   &verboseFlag,   1},
            {_T("debug"),       ARG_NONE,   &debugFlag,     1},
            {_T("help"),        ARG_NONE,   0,              _T('h')},
            {_T("version"),     ARG_NONE,   0,              _T('v')},
            {ARG_NULL,          ARG_NULL,   ARG_NULL,       ARG_NULL}
        };

        int longOptionIndex = 0;
        int optionChar = getopt_long(argc, argv, _T("m:o:b:hv"), longOptions, &longOptionIndex);
        if (optionChar == -1) {
            // 所有选项都已被解析
            break;
        }

        // Handle options
        switch (optionChar) {
            case 0:
                // flag 不为 NULL, 或 val 为 0 的长选项 (flag 不为 NULL 时 val 不应设置为 0)

                // 如果选项设置了 flag, 则此处什么也不做
                if (longOptions[longOptionIndex].flag != 0) {
                    // --overwrite
                    // --verbose
                    // --debug
                    break;
                }

                // 处理未设置 flag 的，且 val 为 0 的选项
                // 当前没有
                break;

            case _T('m'):
                // -m, --model
                if (optarg != NULL) {
                    _tcsncpy(modelName, optarg, (FILE_PATH_MAX_SIZE - 1));
                    modelName[FILE_PATH_MAX_SIZE - 1] = _T('\0');
                }
                break;

            case _T('o'):
                // -o, --output
                if (optarg != NULL) {
                    // 检查输出文件基础路径的长度
                    if (_tcsclen(optarg) > (FILE_PATH_MAX_SIZE - 1)) {
                        _ftprintf(stderr, _T("Error: The specified output base file name \"%s\" is too long (%d > %d characters).\n"),
                                optarg, (int)_tcsclen(optarg), (int)(FILE_PATH_MAX_SIZE - 1));
                        return EXIT_FAILURE;
                    }

                    _tcsncpy(outputBaseFilePath, optarg, (FILE_PATH_MAX_SIZE - 1));
                    outputBaseFilePath[FILE_PATH_MAX_SIZE - 1] = _T('\0');
                }
                break;

            case _T('b'):
                // -b, --bitrate
                if (optarg != NULL) {
                    if (!_tryParseBitrate(&outputFileBitrate, optarg)) {
                        _ftprintf(stderr, _T("Error: Failed to parse the specified bitrate \"%s\".\n"),
                                optarg);
                        return EXIT_FAILURE;
                    }
                }
                break;

            case _T('h'):
                // -h, --help
                helpFlag = 1;
                break;

            case _T('v'):
                // -v, --version
                versionFlag = 1;
                break;

            case '?':
                // 解析命令行参数时有错误产生
                // 此时 getopt_long() 已经输出了错误提示信息，直接退出
                return EXIT_FAILURE;
                break;

            default:
                _ftprintf(stderr, _T("Error: Unknown error"));
                return EXIT_FAILURE;
                break;
        }
    }

    if (verboseFlag) {
        g_verboseMode = true;
    }

    if (debugFlag) {
        g_debugMode = true;
    }

#if defined(_DEBUG) && 0
    g_debugMode = true;
#endif

    if (helpFlag || (argc <= 1)) {
        // 显示帮助信息并退出
        _displayHelp(argc, argv);
        return EXIT_SUCCESS;
    }

    if (versionFlag) {
        // 显示版本信息并退出
        _displayVersion();
        return EXIT_SUCCESS;
    }

    if (optind < argc) {
        // 除了 longOptions 中配置的选项外，还有其他参数未处理

        // 期望只剩余一个输入文件路径的参数未处理
        if ((optind + 1) != argc) {
            _ftprintf(stderr, _T("Error: Specified more than one input file path.\n"));
            return EXIT_FAILURE;
        }

        TCHAR *nonOptionArgumentValue = argv[optind];

        // 检查输入文件路径的长度
        if (_tcsclen(nonOptionArgumentValue) > (FILE_PATH_MAX_SIZE - 1)) {
            _ftprintf(stderr, _T("Error: The specified input file path \"%s\" is too long (%d > %d characters).\n"),
                    nonOptionArgumentValue, (int)_tcsclen(nonOptionArgumentValue), (int)(FILE_PATH_MAX_SIZE - 1));
            return EXIT_FAILURE;
        }

        _tcsncpy(inputFilePath, nonOptionArgumentValue, (FILE_PATH_MAX_SIZE - 1));
        inputFilePath[FILE_PATH_MAX_SIZE - 1] = _T('\0');
    } else {
        // 未指定输入文件路径

        _ftprintf(stderr, _T("Error: Not specified the input file path.\n"));
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////// 检查命令行参数 //////////////////////////////////////////////////

    // 检查是否已指定输入文件路径
    if (_tcsclen(inputFilePath) == 0) {
        _ftprintf(stderr, _T("Error: Not specified the input file path.\n"));
        return EXIT_FAILURE;
    }

    // 如果未指定 modelName, 则使用默认值
    if (_tcsclen(modelName) == 0) {
        _tcsncpy(modelName, _T("2stems"), (FILE_PATH_MAX_SIZE - 1));
        modelName[FILE_PATH_MAX_SIZE - 1] = _T('\0');
    }

    // 检查所指定的 modelName 是否存在
    const SpleeterModelInfo *modelInfo = SpleeterProcessor_getModelInfo(modelName);
    if (modelInfo == NULL) {
        _ftprintf(stderr, _T("Error: Unrecognized model name \"%s\".\n"), modelName);
        return EXIT_FAILURE;
    }

    // 如果未指定输出文件基础路径，则使用默认值
    if (_tcsclen(outputBaseFilePath) == 0) {
        // 默认使用 <input_file_path>
        _tcsncpy(outputBaseFilePath, inputFilePath, (FILE_PATH_MAX_SIZE - 1));
        outputBaseFilePath[FILE_PATH_MAX_SIZE - 1] = _T('\0');
    }

    // 检查是否已指定输出文件基础路径
    if (_tcsclen(outputBaseFilePath) == 0) {
        _ftprintf(stderr, _T("Error: Not specified the output base file path.\n"));
        return EXIT_FAILURE;
    }

    // 如果未指定输出文件的 bitrate, 则使用默认值
    if (outputFileBitrate <= 0) {
        outputFileBitrate = 256000;
    }

    ////////////////////////////////////////////////// 检查输入文件 //////////////////////////////////////////////////

    _tprintf(_T("Input file:\n"));
    _tprintf(_T("%s\n"), inputFilePath);
    _tprintf(_T("\n"));

    if (!_checkInputFilePath(inputFilePath)) {
        // 返回错误时, _checkInputFilePath() 已输出错误信息
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////// 检查输出文件 //////////////////////////////////////////////////

    _tprintf(_T("Output files:\n"));
    for (int i = 0; i < modelInfo->outputCount; i++) {
        TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
        if (!_getOutputFilePath(outputFilePath, FILE_PATH_MAX_SIZE, outputBaseFilePath, modelInfo->trackNames[i])) {
            // 返回错误时, _getOutputFilePath() 已输出错误信息
            return EXIT_FAILURE;
        }

        _tprintf(_T("%s\n"), outputFilePath);

        if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
            // 返回错误时, _checkOutputFilePath() 已输出错误信息
            return EXIT_FAILURE;
        }
    }
    _tprintf(_T("\n"));

    ////////////////////////////////////////////////// 开始处理 //////////////////////////////////////////////////

    const AudioSampleType spleeterSampleType = {
        .sampleRate = SPLEETER_MODEL_AUDIO_SAMPLE_RATE,
        .channelCount = SPLEETER_MODEL_AUDIO_CHANNEL_COUNT,
        .sampleValueFormat = AUDIO_SAMPLE_VALUE_FORMAT_FLOAT_INTERLACED
    };

    const AudioFileFormat outputAudioFileFormat = {
        .formatName = NULL,
        .bitRate = outputFileBitrate
    };

    // 读取音频文件

    AudioDataSource *audioDataSourceStereo = AudioFile_readAll(inputFilePath, &spleeterSampleType);

    // 使用 Spleeter 进行处理

    SpleeterProcessorResult *result = NULL;
    if (SpleeterProcessor_split(modelName, audioDataSourceStereo, &result) != 0) {
        _ftprintf(stderr, _T("Error: Spleeter processor failed. Use --debug to get more information.\n"));
        return EXIT_FAILURE;
    }
    if (result == NULL) {
        _ftprintf(stderr, _T("Error: Spleeter processor returns no result. Use --debug to get more information.\n"));
        return EXIT_FAILURE;
    }

    for (int i = 0; i < result->trackCount; i++) {
        SpleeterProcessorResultTrack *track = &result->trackList[i];

        TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
        if (!_getOutputFilePath(outputFilePath, FILE_PATH_MAX_SIZE, outputBaseFilePath, track->trackName)) {
            // 返回错误时, _getOutputFilePath() 已输出错误信息
            return EXIT_FAILURE;
        }

        if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
            // 返回错误时, _checkOutputFilePath() 已输出错误信息
            return EXIT_FAILURE;
        }

        if (!AudioFile_writeAll(outputFilePath, &outputAudioFileFormat, &spleeterSampleType,
                (void *)track->audioDataSource->sampleValues, track->audioDataSource->sampleCountPerChannel)) {
            _ftprintf(stderr, _T("Error: Failed to write output file \"%s\".\n"), outputFilePath);
            return EXIT_FAILURE;
        }

        Common_updateProgress(STAGE_AUDIO_FILE_WRITER, (i + 1), result->trackCount);
    }

    SpleeterProcessorResult_free(&result);

    _tprintf(_T("\n"));
    _tprintf(_T("Completed.\n"));

    return EXIT_SUCCESS;
}
