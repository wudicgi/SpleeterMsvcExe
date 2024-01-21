/*
 * MIT License
 *
 * Copyright (c) 2018 Wudi <wudi@wudilabs.org>
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
#include <intrin.h>
#include "../version.h"
#include "getopt.h"
#include "Common.h"
#include "AudioFileReader.h"
#include "SpleeterProcessor.h"

#define MSG_INFO(fmt, ...)     do {     \
    _tprintf(fmt, __VA_ARGS__);         \
} while (0)

#define MSG_WARNING(fmt, ...)     do {                      \
    _ftprintf(stderr, _T("\nWarning: ") fmt, __VA_ARGS__);  \
} while (0)

#define MSG_ERROR(fmt, ...)     do {                        \
    _ftprintf(stderr, _T("\nError: ") fmt, __VA_ARGS__);    \
} while (0)

/**
 * 显示帮助文本
 */
static void _displayHelp(int argc, TCHAR *argv[]) {
    MSG_INFO(_T("%s %s\n"), _T(PROGRAM_NAME), _T(PROGRAM_VERSION));
    MSG_INFO(_T("\n"));

    MSG_INFO(_T("Usage: %s [options] <input_file_path>\n"), argv[0]);
    MSG_INFO(_T("\n"));

    MSG_INFO(_T("Options:\n"));
    MSG_INFO(_T("    -m, --model         Spleeter model name, i.e. the folder name in models folder\n"));
    MSG_INFO(_T("                        (2stems, 4stems, 5stems-22khz, ..., default: 2stems)\n"));
    MSG_INFO(_T("    -o, --output        Output file path format\n"));
    MSG_INFO(_T("                        Default is empty, which is equivalent to $(DirPath)\\$(BaseName).$(TrackName).$(Ext)\n"));
    MSG_INFO(_T("                        Supported variable names and example values:\n"));
    MSG_INFO(_T("                            $(FullPath)                 D:\\Music\\test.mp3\n"));
    MSG_INFO(_T("                            $(DirPath)                  D:\\Music\n"));
    MSG_INFO(_T("                            $(FileName)                 test.mp3\n"));
    MSG_INFO(_T("                            $(BaseName)                 test\n"));
    MSG_INFO(_T("                            $(Ext)                      mp3\n"));
    MSG_INFO(_T("                            $(TrackName)                vocals,drums,bass,...\n"));
    MSG_INFO(_T("    -b, --bitrate       Output file bitrate\n"));
    MSG_INFO(_T("                        (128k, 192000, 256k, ..., default: 256k)\n"));
    MSG_INFO(_T("    -t, --tracks        Output track list (comma separated track names)\n"));
    MSG_INFO(_T("                        Default value is empty to output all tracks\n"));
    MSG_INFO(_T("                        Available track names:\n"));
    MSG_INFO(_T("                            input, vocals, accompaniment, drums, bass, piano, other\n"));
    MSG_INFO(_T("                        Examples:\n"));
    MSG_INFO(_T("                            accompaniment               Output accompaniment track only\n"));
    MSG_INFO(_T("                            vocals,drums                Output vocals and drums tracks\n"));
    MSG_INFO(_T("                            mixed=vocals+drums          Mix vocals and drums as \"mixed\" track\n"));
    MSG_INFO(_T("                            vocals,acc=input-vocals     Output vocals and accompaniment for 4stems model\n"));
    MSG_INFO(_T("    --overwrite         Overwrite when the target output file exists\n"));
    MSG_INFO(_T("    --verbose           Display detailed processing information\n"));
    MSG_INFO(_T("    --debug             Display debug information\n"));
    MSG_INFO(_T("    -h, --help          Display this help and exit\n"));
    MSG_INFO(_T("    -v, --version       Display program version and exit\n"));
    MSG_INFO(_T("\n"));

    MSG_INFO(_T("Examples:\n"));
    MSG_INFO(_T("    %s -m 2stems song.mp3\n"), argv[0]);
    MSG_INFO(_T("    - Splits song.mp3 into 2 tracks: vocals, accompaniment\n"));
    MSG_INFO(_T("    - Outputs 2 files: song.vocals.mp3, song.accompaniment.mp3\n"));
    MSG_INFO(_T("    - Output file format is same as input, using default bitrate 256kbps\n"));
    MSG_INFO(_T("\n"));
    MSG_INFO(_T("    %s -m 4stems -o result.m4a -b 192k song.mp3\n"), argv[0]);
    MSG_INFO(_T("    - Splits song.mp3 into 4 tracks: vocals, drums, bass, other\n"));
    MSG_INFO(_T("    - Outputs 4 files: result.vocals.m4a, result.drums.m4a, ...\n"));
    MSG_INFO(_T("    - Output file format is M4A, using bitrate 192kbps\n"));
    MSG_INFO(_T("\n"));
    MSG_INFO(_T("    %s --model 5stems-22khz --bitrate 320000 song.mp3\n"), argv[0]);
    MSG_INFO(_T("    - Long option example\n"));
    MSG_INFO(_T("    - Using the model of which upper frequency limit is 22kHz\n"));
    MSG_INFO(_T("    - Splits song.mp3 into 5 tracks\n"));
}

/**
 * 显示程序版本号
 */
static void _displayVersion(void) {
    MSG_INFO(_T("%s\n"), _T(PROGRAM_VERSION));
}

/**
 * 检查 CPU 是否支持 AVX 和 AVX2
 *
 * 代码来自 tensorflow 项目中的 tensorflow/core/platform/cpu_info.cc
 */
static bool _checkCpuFeatures(void) {
    // AVX
    {
        // To get general information and extended features we send eax = 1 and
        // ecx = 0 to cpuid.  The response is returned in eax, ebx, ecx and edx.
        // (See Intel 64 and IA-32 Architectures Software Developer's Manual
        // Volume 2A: Instruction Set Reference, A-M CPUID).
        int cpu_info[4] = { 0 };
        __cpuidex(cpu_info, 1, 0);
        uint32_t ecx = cpu_info[2];

        const uint64_t xcr0_xmm_mask = 0x2;
        const uint64_t xcr0_ymm_mask = 0x4;

        const uint64_t xcr0_avx_mask = xcr0_xmm_mask | xcr0_ymm_mask;

        const bool have_avx =
            // Does the OS support XGETBV instruction use by applications?
            ((ecx >> 27) & 0x1) &&
            // Does the OS save/restore XMM and YMM state?
            ((_xgetbv(0) & xcr0_avx_mask) == xcr0_avx_mask) &&
            // Is AVX supported in hardware?
            ((ecx >> 28) & 0x1);

        if (!have_avx) {
            MSG_ERROR(_T("The current processor lacks AVX support, the program will exit.\n"));

            return false;
        }
    }

    // AVX2
    {
        // Get standard level 7 structured extension features (issue CPUID with
        // eax = 7 and ecx= 0), which is required to check for AVX2 support as
        // well as other Haswell (and beyond) features.  (See Intel 64 and IA-32
        // Architectures Software Developer's Manual Volume 2A: Instruction Set
        // Reference, A-M CPUID).
        int cpu_info[4] = { 0 };
        __cpuidex(cpu_info, 7, 0);
        uint32_t ebx = cpu_info[1];

        bool have_avx2 = ((ebx >> 5) & 0x1);

        if (!have_avx2) {
            MSG_ERROR(_T("The current processor lacks AVX2 support, the program will exit.\n"));

            return false;
        }
    }

    return true;
}

/**
 * 检查 tensorflow 的 DLL 是否可被成功加载
 */
static bool _checkDllLoading(void) {
    HMODULE dllHandle = LoadLibrary(_T("tensorflow.dll"));
    if (dllHandle == NULL) {
        DWORD error = GetLastError();
        PTCHAR errorMessageBuffer = NULL;
        size_t errorMessageLength = FormatMessage((FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS),
                NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errorMessageBuffer, 0, NULL);
        if (errorMessageLength != 0) {
            MSG_ERROR(_T("Failed to load tensorflow.dll.\n")
                    _T("Error code:    0x%08x\n")
                    _T("Error message: %s\n"),
                    error, errorMessageBuffer);
        } else {
            MSG_ERROR(_T("Failed to load tensorflow.dll. Error code: 0x%08x\n"),
                    error);
        }
        if (errorMessageBuffer != NULL) {
            LocalFree(errorMessageBuffer);
        }

        return false;
    }

    // DLL 稍后会被使用到，因此这里不释放
    // FreeLibrary(dllHandle);

    return true;
}

/**
 * 在文件路径原有的扩展名前添加额外的扩展名
 *
 * 例如对于 srcFilePath = "dir\file.mp3", extraExtension = "vocals", 返回结果为
 * "dir\file.vocals.mp3"
 *
 * @param   destFilePathBuffer              目标文件路径缓冲区
 * @param   destFilePathBufferCharCount     destFilePathBuffer 所能容纳的字符数
 * @param   srcFilePath                     原始文件路径
 * @param   extraExtension                  要添加的额外扩展名 (不包含起始位置的 ".")
 *
 * @return  成功时返回 true, 失败时返回 false
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

/**
 * 根据输出文件路径格式字符串生成指定轨道对应的输出文件路径
 *
 * @param   dest                    用于存储所生成指定轨道对应的输出文件路径的缓冲区 (大小为 FILE_PATH_MAX_SIZE)
 * @param   src                     输出文件路径格式字符串
 * @param   inputFileFullPath       输入文件完整路径
 * @param   trackName               当前轨道名称
 * @param   outputTrackCount        要输出的所有轨道的数量
 *
 * @return  成功时返回 true, 失败时返回 false
 */
static bool _convertOutputFilePathFormatString(TCHAR *dest, const TCHAR *src,
        const TCHAR *inputFileFullPath, const TCHAR *trackName, int outputTrackCount) {
    TCHAR *destPtr = dest;
    TCHAR *destEnd = dest + FILE_PATH_MAX_SIZE;

    size_t srcLength = _tcsnlen(src, FILE_PATH_MAX_SIZE);
    if (srcLength >= FILE_PATH_MAX_SIZE) {
        MSG_ERROR(_T("The specified output file path format \"%s\" is too long.\n"), src);
        return false;
    }
    const TCHAR *srcEnd = src + srcLength;

    bool containsTrackName = false;

    const TCHAR *srcPtr = src;
    while (srcPtr < srcEnd) {
        if ((*srcPtr == _T('$')) && ((srcPtr + 1) < srcEnd) && (*(srcPtr + 1) == _T('('))) {
            // 遇到了 $(VariableName) 形式变量名的起始部分 "$("

            const TCHAR *variableNameBegin = srcPtr + 2;
            const TCHAR *variableNameEnd = _tcschr(variableNameBegin, _T(')'));
            if (variableNameEnd != NULL) {
                // 查找到了变量名的结束部分 ")"

                // 计算变量名的长度
                size_t variableNameLength = variableNameEnd - variableNameBegin;

                // 复制变量名到 variableNameBuffer 中，用于后续字符串比较
                TCHAR variableNameBuffer[FILE_PATH_MAX_SIZE] = { _T('\0') };
                memcpy(variableNameBuffer, variableNameBegin, variableNameLength * sizeof(TCHAR));
                variableNameBuffer[FILE_PATH_MAX_SIZE - 1] = _T('\0');

                // 将对应的变量值复制到 variableValueBuffer 中
                TCHAR variableValueBuffer[FILE_PATH_MAX_SIZE] = { _T('\0') };
                if (_tcscmp(_T("FullPath"), variableNameBuffer) == 0) {
                    // 输入文件的完整路径
                    _tcsncpy(variableValueBuffer, inputFileFullPath, (FILE_PATH_MAX_SIZE - 1));
                } else if (_tcscmp(_T("DirPath"), variableNameBuffer) == 0) {
                    // 输入文件所在目录的路径
                    _tcsncpy(variableValueBuffer, inputFileFullPath, (FILE_PATH_MAX_SIZE - 1));
                    if (!PathRemoveFileSpec(variableValueBuffer)) {
                        MSG_ERROR(_T("Failed to call PathRemoveFileSpec()\n"));
                        return false;
                    }
                } else if (_tcscmp(_T("FileName"), variableNameBuffer) == 0) {
                    TCHAR *inputFileName = PathFindFileName(inputFileFullPath);
                    if (inputFileName != inputFileFullPath) {
                        // 输入文件的文件名
                        _tcsncpy(variableValueBuffer, inputFileName, (FILE_PATH_MAX_SIZE - 1));
                    } else {
                        // 查找输入文件路径中的文件名失败
                        variableValueBuffer[0] = _T('\0');
                    }
                } else if (_tcscmp(_T("BaseName"), variableNameBuffer) == 0) {
                    TCHAR *inputFileName = PathFindFileName(inputFileFullPath);
                    if (inputFileName != inputFileFullPath) {
                        // 输入文件的文件名去除扩展名后的结果
                        _tcsncpy(variableValueBuffer, inputFileName, (FILE_PATH_MAX_SIZE - 1));
                        TCHAR *fileExtension = PathFindExtension(variableValueBuffer);
                        if (*fileExtension == '.') {
                            *fileExtension = _T('\0');
                        }
                    } else {
                        // 查找输入文件路径中的文件名失败
                        variableValueBuffer[0] = _T('\0');
                    }
                } else if (_tcscmp(_T("Ext"), variableNameBuffer) == 0) {
                    TCHAR *inputFileExtension = PathFindExtension(inputFileFullPath);
                    if (*inputFileExtension == '.') {
                        // 输入文件的扩展名
                        _tcsncpy(variableValueBuffer, inputFileExtension + 1, (FILE_PATH_MAX_SIZE - 1));
                    } else {
                        // 查找输入文件路径中的扩展名失败
                        variableValueBuffer[0] = _T('\0');
                    }
                } else if (_tcscmp(_T("TrackName"), variableNameBuffer) == 0) {
                    // 轨道名称
                    _tcsncpy(variableValueBuffer, trackName, (FILE_PATH_MAX_SIZE - 1));

                    containsTrackName = true;
                } else {
                    // 不合法的变量名
                    MSG_ERROR(_T("Unrecognized variable name \"%s\"\n"),
                            variableNameBuffer);
                    return false;
                }

                variableValueBuffer[FILE_PATH_MAX_SIZE - 1] = _T('\0');
                size_t variableValueLength = _tcsclen(variableValueBuffer);
                if ((destPtr + variableValueLength) >= destEnd) {
                    // 变量值的长度超过 FILE_PATH_MAX_SIZE 的限制
                    MSG_ERROR(_T("The variable value \"%s\" is too long\n"), variableValueBuffer);
                    return false;
                }
                memcpy(destPtr, variableValueBuffer, variableValueLength * sizeof(TCHAR));
                destPtr += variableValueLength;
                srcPtr += 2 + variableNameLength + 1;   // 跳过已处理完的 $(VariableName) 形式的变量名
                continue;
            }
        }

        if ((destPtr + 1) >= destEnd) {
            // dest 缓冲区已满
            MSG_ERROR(_T("The concatenating output file path is already too long.\n"));
            return false;
        }
        *(destPtr++) = *(srcPtr++);
    }

    if ((outputTrackCount > 1) && !containsTrackName) {
        // 输出文件名格式字符串中不包含轨道名称
        MSG_ERROR(_T("The output file path format must contain a \"$(TrackName)\" when output multiple tracks.\n"));
        return false;
    }

    if ((destPtr + 1) >= destEnd) {
        // dest 缓冲区已满
        MSG_ERROR(_T("The concatenating output file path is already too long.\n"));
        return false;
    }
    *(destPtr++) = _T('\0');

    return true;
}

/**
 * 检查指定的输入文件是否存在和可读
 *
 * @param   inputFilePath       要检查输入文件的路径
 *
 * @return  如果文件存在且可读，返回 true, 否则返回 false
 */
static bool _checkInputFilePath(const TCHAR *inputFilePath) {
    // 检查指定的输入文件是否存在
    if (_taccess(inputFilePath, 0) == -1) {
        MSG_ERROR(_T("The specified input file \"%s\" does not exist.\n"), inputFilePath);
        return false;
    }

    // 检查指定的输入文件是否可读
    if (_taccess(inputFilePath, 4) == -1) {
        MSG_ERROR(_T("The specified input file \"%s\" cannot be read.\n"), inputFilePath);
        return false;
    }

    return true;
}

/**
 * 获取输出文件路径
 *
 * @param   outputFilePathBuffer    用于存储输出文件路径的缓冲区
 * @param   outputFilePathFormat    输出文件路径格式字符串 (为空时将直接在 inputFileFullPath 的原有扩展名前添加 outputTrackName)
 * @param   outputTrackCount        输出轨道的总数量
 * @param   outputTrackName         当前输出轨道名称
 * @param   inputFileFullPath       输入音频文件的完整路径
 *
 * @return  成功时返回 true, 失败时返回 false
 */
static bool _getOutputFilePath(TCHAR *outputFilePathBuffer, const TCHAR *outputFilePathFormat,
        int outputTrackCount, const TCHAR *outputTrackName, const TCHAR *inputFileFullPath) {
    if (_tcsclen(outputFilePathFormat) == 0) {
        // 检查 inputFileFullPath 加上 trackName 后是否超长
        if (!_addExtraExtensionBeforeOriginalExtension(outputFilePathBuffer, FILE_PATH_MAX_SIZE,
                inputFileFullPath, outputTrackName)) {
            MSG_ERROR(_T("The output file path for track \"%s\" is too long.\n"), outputTrackName);
            return false;
        }
        outputFilePathBuffer[FILE_PATH_MAX_SIZE - 1] = _T('\0');
    } else {
        if (!_convertOutputFilePathFormatString(outputFilePathBuffer, outputFilePathFormat,
                inputFileFullPath, outputTrackName, outputTrackCount)) {
            return false;
        }
    }

    return true;
}

/**
 * 检查指定的输出文件路径是否可用
 *
 * @param   outputFilePath      要检查的输出文件路径
 * @param   overwriteFlag       当指定路径的输出文件已存在时，是否允许覆盖
 *
 * @return  如果指定路径的文件已存在，或不可覆盖，或不可写入，则返回 false, 否则返回 true
 */
static bool _checkOutputFilePath(const TCHAR *outputFilePath, int overwriteFlag) {
    // 如果 outputFilePath 存在
    if (_taccess(outputFilePath, 0) != -1) {
        // 如果未指定 --overwrite 选项
        if (!overwriteFlag) {
            MSG_ERROR(_T("The output file \"%s\" has already existed. Add --overwrite option to ignore.\n"), outputFilePath);
            return false;
        }

        // 如果 outputFilePath 不可写入
        if (_taccess(outputFilePath, 2) == -1) {
            MSG_ERROR(_T("The output file \"%s\" cannot be written.\n"), outputFilePath);
            return false;
        }
    }

    return true;
}

/**
 * 尝试解析比特率数值
 *
 * @param   parsedResultBitrate     指向用于存储解析结果的变量的指针
 * @param   optionValue             要解析的文本 (可为纯数字，也可包含 'k' 或 'K' 单位后缀)
 *
 * @return  解析成功时返回 true, 失败时返回 false
 */
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

/** TrackList 中 TrackItem 的最大数量 */
#define TRACK_ITEM_MAX_COUNT        10

/** TrackItem 中 SourceTrackItem 的最大数量 */
#define SOURCE_TRACK_MAX_COUNT      10

/** trackName 字符数组的大小 */
#define TRACK_NAME_MAX_SIZE         100

/**
 * 源轨道条目
 */
typedef struct {
    /** 是否是减去该轨道 (即在混音时该轨道是否需要反相) */
    bool    toSubtract;

    /** 轨道名称 (vocals, drums, ...) */
    TCHAR   trackName[TRACK_NAME_MAX_SIZE];
} SourceTrackItem;

/**
 * 轨道条目
 */
typedef struct {
    /** 轨道名称 (当源轨道条目列表不为空时，为最终输出的轨道名称；否则为 Spleeter 模型中的轨道名称) */
    TCHAR               trackName[TRACK_NAME_MAX_SIZE];

    /** 源轨道条目列表 */
    SourceTrackItem     sourceTrackItems[SOURCE_TRACK_MAX_COUNT];

    /** 源轨道条目数量 */
    int                 sourceTrackItemCount;
} TrackItem;

/**
 * 轨道列表
 */
typedef struct {
    /** 轨道条目列表 */
    TrackItem   trackItems[TRACK_ITEM_MAX_COUNT];

    /** 轨道条目数量 */
    int         trackItemCount;
} TrackList;

/**
 * 尝试解析轨道名称
 *
 * 合法的轨道名称长度为 1 至 (TRACK_NAME_MAX_SIZE - 1) 个字符，仅可包含字母、数字和下划线
 *
 * @param   parsedTrackName     用于存放解析结果的字符数组
 * @param   str                 要解析的文本
 */
static size_t _tryParseTrackName(TCHAR parsedTrackName[TRACK_NAME_MAX_SIZE], const TCHAR *str) {
    size_t trackNameLength = _tcsspn(str, _T("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"));
    if ((trackNameLength > 0) && (trackNameLength < TRACK_NAME_MAX_SIZE)) {
        _tcsncpy(parsedTrackName, str, trackNameLength);
        parsedTrackName[trackNameLength] = _T('\0');

        return trackNameLength;
    } else {
        return 0;
    }
}

/**
 * 尝试解析轨道列表表达式
 *
 * @param   parsedTrackList     用于存放解析结果的结构体
 * @param   optionValue         要解析的文本
 *
 * @return  解析成功时返回 true, 失败时返回 false
 */
static bool _tryParseTrackList(TrackList *parsedTrackList, const TCHAR *optionValue) {
    memset(parsedTrackList, 0, sizeof(TrackList));

    const TCHAR *p = optionValue;
    const TCHAR *pEnd = p + _tcsclen(p);
    while (p < pEnd) {
        if (parsedTrackList->trackItemCount >= TRACK_ITEM_MAX_COUNT) {
            return false;
        }
        TrackItem *trackItem = &parsedTrackList->trackItems[parsedTrackList->trackItemCount];

        size_t trackNameLength = _tryParseTrackName(trackItem->trackName, p);
        if (trackNameLength == 0) {
            return false;
        }
        parsedTrackList->trackItemCount++;
        p += trackNameLength;
        if (p >= pEnd) {
            break;
        }

        if (*p == _T(',')) {
            p++;    // skip ','
            continue;
        }

        if (*p == _T('=')) {
            p++;    // skip '='
            if (p >= pEnd) {
                return false;
            }

            while (p < pEnd) {
                if (trackItem->sourceTrackItemCount >= SOURCE_TRACK_MAX_COUNT) {
                    return false;
                }
                SourceTrackItem *sourceTrackItem = &trackItem->sourceTrackItems[trackItem->sourceTrackItemCount];

                if (*p == _T('-')) {
                    sourceTrackItem->toSubtract = true;
                    p++;
                } else {
                    if (*p == _T('+')) {
                        p++;
                    }
                    sourceTrackItem->toSubtract = false;
                }
                if (p >= pEnd) {
                    return false;
                }

                size_t sourceTrackNameLength = _tryParseTrackName(sourceTrackItem->trackName, p);
                if (sourceTrackNameLength == 0) {
                    return false;
                }
                trackItem->sourceTrackItemCount++;
                p += sourceTrackNameLength;
                if (p >= pEnd) {
                    break;
                }

                if (*p == _T(',')) {
                    p++;
                    break;
                }
            }
        }
    }

    return true;
}

/**
 * 检查已选择的 Spleeter 模型中是否包含指定名称的输出轨道
 *
 * @param   modelInfo       指向 SpleeterModelInfo 结构体的指针
 * @param   trackName       要检查的轨道名称
 *
 * @return  如果包含则返回 true, 否则返回 false
 */
static bool _checkSpleeterModelTrackName(const SpleeterModelInfo *modelInfo, const TCHAR *trackName) {
    bool foundTrackName = false;
    for (int i = 0; i < modelInfo->outputCount; i++) {
        if (_tcscmp(trackName, modelInfo->trackNames[i]) == 0) {
            foundTrackName = true;
            break;
        }
    }

    if (!foundTrackName) {
        MSG_ERROR(_T("Specified track name \"%s\" does not exist in %s model.\n"), trackName, modelInfo->basicName);
        return false;
    }

    return true;
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

        argv[argc++] = _T("-m");
        argv[argc++] = _T("4stems");

        argv[argc++] = _T("--tracks");
        argv[argc++] = _T("vocals,vocals_and_drums=vocals+drums,vocals_removed_1=input-vocals,")
                _T("vocals_removed_2=drums+bass+other,invert_test=-input+bass-other");

        argv[argc++] = _T("--output");
        argv[argc++] = _T("$(DirPath)\\$(BaseName)-separated-$(TrackName).$(Ext)");
    }
#endif

#if defined(_DEBUG) && 0
    for (int i = 0; i < argc; i++) {
        MSG_INFO(_T("argv[%d] = %s\n"), i, argv[i]);
    }
    MSG_INFO(_T("\n"));
#endif

    TCHAR inputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
    TCHAR outputFilePathFormat[FILE_PATH_MAX_SIZE] = { _T('\0') };
    TCHAR modelName[FILE_PATH_MAX_SIZE] = { _T('\0') };

    int outputFileBitrate = 0;

    TrackList trackList = { 0 };

    static int overwriteFlag = 0;
    static int verboseFlag = 0;
    static int debugFlag = 0;
    static int helpFlag = 0;
    static int versionFlag = 0;

    static int disableCpuCheckFlag = 0;
    static int disableDllCheckFlag = 0;

    while (true) {
        static struct option longOptions[] = {
            // name,            has_arg,    flag,           val
            {_T("model"),       ARG_REQ,    0,              _T('m')},
            {_T("output"),      ARG_REQ,    0,              _T('o')},
            {_T("bitrate"),     ARG_REQ,    0,              _T('b')},
            {_T("tracks"),      ARG_REQ,    0,              _T('t')},
            {_T("overwrite"),   ARG_NONE,   &overwriteFlag, 1},
            {_T("verbose"),     ARG_NONE,   &verboseFlag,   1},
            {_T("debug"),       ARG_NONE,   &debugFlag,     1},
            {_T("help"),        ARG_NONE,   0,              _T('h')},
            {_T("version"),     ARG_NONE,   0,              _T('v')},
            {_T("disable-cpu-check"),   ARG_NONE,   &disableCpuCheckFlag, 1},
            {_T("disable-dll-check"),   ARG_NONE,   &disableDllCheckFlag, 1},
            {ARG_NULL,          ARG_NULL,   ARG_NULL,       ARG_NULL}
        };

        int longOptionIndex = 0;
        int optionChar = getopt_long(argc, argv, _T("m:o:b:t:hv"), longOptions, &longOptionIndex);
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
                    // 检查输出文件路径格式字符串的长度
                    if (_tcsclen(optarg) > (FILE_PATH_MAX_SIZE - 1)) {
                        MSG_ERROR(_T("The specified output file path format \"%s\" is too long (%d > %d characters).\n"),
                                optarg, (int)_tcsclen(optarg), (int)(FILE_PATH_MAX_SIZE - 1));
                        return EXIT_FAILURE;
                    }

                    _tcsncpy(outputFilePathFormat, optarg, (FILE_PATH_MAX_SIZE - 1));
                    outputFilePathFormat[FILE_PATH_MAX_SIZE - 1] = _T('\0');
                }
                break;

            case _T('b'):
                // -b, --bitrate
                if (optarg != NULL) {
                    if (!_tryParseBitrate(&outputFileBitrate, optarg)) {
                        MSG_ERROR(_T("Failed to parse the specified bitrate \"%s\".\n"), optarg);
                        return EXIT_FAILURE;
                    }
                }
                break;

            case _T('t'):
                // -t, --tracks
                if (optarg != NULL) {
                    if (!_tryParseTrackList(&trackList, optarg)) {
                        MSG_ERROR(_T("Failed to parse the specified track list \"%s\".\n"), optarg);
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
                MSG_ERROR(_T("Unknown error"));
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
            MSG_ERROR(_T("Specified more than one input file path.\n"));
            return EXIT_FAILURE;
        }

        TCHAR *nonOptionArgumentValue = argv[optind];

        // 检查输入文件路径的长度
        if (_tcsclen(nonOptionArgumentValue) > (FILE_PATH_MAX_SIZE - 1)) {
            MSG_ERROR(_T("The specified input file path \"%s\" is too long (%d > %d characters).\n"),
                    nonOptionArgumentValue, (int)_tcsclen(nonOptionArgumentValue), (int)(FILE_PATH_MAX_SIZE - 1));
            return EXIT_FAILURE;
        }

        _tcsncpy(inputFilePath, nonOptionArgumentValue, (FILE_PATH_MAX_SIZE - 1));
        inputFilePath[FILE_PATH_MAX_SIZE - 1] = _T('\0');
    } else {
        // 未指定输入文件路径

        MSG_ERROR(_T("Not specified the input file path.\n"));
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////// 检查 CPU 特性和 DLL 加载 //////////////////////////////////////////////////

    if (disableCpuCheckFlag) {
        MSG_WARNING(_T("The CPU feature check has been disabled. If there is no AVX/AVX2 support,\n")
                _T("the initialization loading of tensorflow.dll will fail and return error code 0xc0000142.\n"));
    } else {
        if (!_checkCpuFeatures()) {
            return EXIT_FAILURE;
        }
    }

    if (disableDllCheckFlag) {
        MSG_WARNING(_T("The DLL loading check has been disabled. If the loading fails,\n")
                _T("the program may quietly exit without displaying any error messages.\n"));
    } else {
        if (!_checkDllLoading()) {
            return EXIT_FAILURE;
        }
    }

    ////////////////////////////////////////////////// 检查命令行参数 //////////////////////////////////////////////////

    // 检查是否已指定输入文件路径
    if (_tcsclen(inputFilePath) == 0) {
        MSG_ERROR(_T("Not specified the input file path.\n"));
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
        MSG_ERROR(_T("Unrecognized model name \"%s\".\n"), modelName);
        return EXIT_FAILURE;
    }

    // 如果未指定输出文件路径格式字符串，则使用默认值
    if (_tcsclen(outputFilePathFormat) == 0) {
        _tcsncpy(outputFilePathFormat, _T(""), (FILE_PATH_MAX_SIZE - 1));
        outputFilePathFormat[FILE_PATH_MAX_SIZE - 1] = _T('\0');
    }

    // 如果未指定输出文件的 bitrate, 则使用默认值
    if (outputFileBitrate <= 0) {
        outputFileBitrate = 256000;
    }

    ////////////////////////////////////////////////// 检查输入文件 //////////////////////////////////////////////////

    if (!_checkInputFilePath(inputFilePath)) {
        return EXIT_FAILURE;
    }

    TCHAR inputFileFullPath[FILE_PATH_MAX_SIZE] = { _T('\0') };

    if (GetFullPathName(inputFilePath, FILE_PATH_MAX_SIZE, inputFileFullPath, NULL) >= FILE_PATH_MAX_SIZE) {
        MSG_ERROR(_T("Failed to get the full path of input file \"%s\".\n"), inputFilePath);
        return EXIT_FAILURE;
    }

    MSG_INFO(_T("Input file:\n"));
    MSG_INFO(_T("%s\n"), inputFileFullPath);
    MSG_INFO(_T("\n"));

    ////////////////////////////////////////////////// 检查轨道名称和输出文件路径 //////////////////////////////////////////////////

    MSG_INFO(_T("Output file(s):\n"));
    if (trackList.trackItemCount == 0) {
        // 未指定 track list, 正常输出

        for (int i = 0; i < modelInfo->outputCount; i++) {
            TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
            if (!_getOutputFilePath(outputFilePath, outputFilePathFormat,
                    modelInfo->outputCount, modelInfo->trackNames[i], inputFileFullPath)) {
                return EXIT_FAILURE;
            }

            MSG_INFO(_T("%s\n"), outputFilePath);

            if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
                return EXIT_FAILURE;
            }
        }
    } else {
        // 指定了 track list

        for (int i = 0; i < trackList.trackItemCount; i++) {
            TrackItem *trackItem = &trackList.trackItems[i];

            if (trackItem->sourceTrackItemCount == 0) {
                // 该 TrackItem 仅选择了一条已知的轨道，不涉及轨道更名或运算

                // 检查轨道名称是否存在
                if (!_checkSpleeterModelTrackName(modelInfo, trackItem->trackName)) {
                    return EXIT_FAILURE;
                }
            } else {
                // 该 TrackItem 指定了 source track list, 涉及轨道更名或运算

                // 检查源轨道名称是否存在
                for (int j = 0; j < trackItem->sourceTrackItemCount; j++) {
                    SourceTrackItem *sourceTrackItem = &trackItem->sourceTrackItems[j];
                    if (_tcscmp(sourceTrackItem->trackName, _T("input")) == 0) {
                        continue;
                    }
                    if (!_checkSpleeterModelTrackName(modelInfo, sourceTrackItem->trackName)) {
                        return EXIT_FAILURE;
                    }
                }
            }

            TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
            if (!_getOutputFilePath(outputFilePath, outputFilePathFormat,
                    trackList.trackItemCount, trackItem->trackName, inputFileFullPath)) {
                return EXIT_FAILURE;
            }

            MSG_INFO(_T("%s\n"), outputFilePath);

            if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
                return EXIT_FAILURE;
            }
        }
    }
    MSG_INFO(_T("\n"));

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

    AudioDataSource *audioDataSourceStereo = AudioFile_readAll(inputFileFullPath, &spleeterSampleType);

    // 使用 Spleeter 进行处理

    SpleeterProcessorResult *result = NULL;
    if (SpleeterProcessor_split(modelName, audioDataSourceStereo, &result) != 0) {
        MSG_ERROR(_T("Spleeter processor failed. Use --debug to get more information.\n"));
        return EXIT_FAILURE;
    }
    if (result == NULL) {
        MSG_ERROR(_T("Spleeter processor returns no result. Use --debug to get more information.\n"));
        return EXIT_FAILURE;
    }

    if (trackList.trackItemCount == 0) {
        // 未指定 track list, 正常输出

        for (int i = 0; i < result->trackCount; i++) {
            SpleeterProcessorResultTrack *track = &result->trackList[i];

            TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
            if (!_getOutputFilePath(outputFilePath, outputFilePathFormat,
                    result->trackCount, track->trackName, inputFileFullPath)) {
                return EXIT_FAILURE;
            }

            if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
                return EXIT_FAILURE;
            }

            if (!AudioFile_writeAll(outputFilePath, &outputAudioFileFormat, &spleeterSampleType,
                    (void *)track->audioDataSource->sampleValues, track->audioDataSource->sampleCountPerChannel)) {
                MSG_ERROR(_T("Failed to write output file \"%s\".\n"), outputFilePath);
                return EXIT_FAILURE;
            }

            Common_updateProgress(STAGE_AUDIO_FILE_WRITER, (i + 1), result->trackCount);
        }
    } else {
        // 指定了 track list

        for (int i = 0; i < trackList.trackItemCount; i++) {
            TrackItem *trackItem = &trackList.trackItems[i];

            if (trackItem->sourceTrackItemCount == 0) {
                // 该 TrackItem 仅选择了一条已知的轨道，不涉及轨道更名或运算

                SpleeterProcessorResultTrack *track = SpleeterProcessorResult_getTrack(result, trackItem->trackName);
                if (track == NULL) {
                    MSG_ERROR(_T("Track \"%s\" does not exist.\n"), trackItem->trackName);
                    return EXIT_FAILURE;
                }

                TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
                if (!_getOutputFilePath(outputFilePath, outputFilePathFormat,
                        trackList.trackItemCount, track->trackName, inputFileFullPath)) {
                    return EXIT_FAILURE;
                }

                if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
                    return EXIT_FAILURE;
                }

                if (!AudioFile_writeAll(outputFilePath, &outputAudioFileFormat, &spleeterSampleType,
                        (void *)track->audioDataSource->sampleValues, track->audioDataSource->sampleCountPerChannel)) {
                    MSG_ERROR(_T("Failed to write output file \"%s\".\n"), outputFilePath);
                    return EXIT_FAILURE;
                }

                Common_updateProgress(STAGE_AUDIO_FILE_WRITER, (i + 1), trackList.trackItemCount);
            } else {
                // 该 TrackItem 指定了 source track list, 涉及轨道更名或运算

                TCHAR outputFilePath[FILE_PATH_MAX_SIZE] = { _T('\0') };
                if (!_getOutputFilePath(outputFilePath, outputFilePathFormat,
                        trackList.trackItemCount, trackItem->trackName, inputFileFullPath)) {
                    return EXIT_FAILURE;
                }

                if (!_checkOutputFilePath(outputFilePath, overwriteFlag)) {
                    return EXIT_FAILURE;
                }

                AudioDataSource *audioDataSourceComputed = NULL;
                for (int j = 0; j < trackItem->sourceTrackItemCount; j++) {
                    SourceTrackItem *sourceTrackItem = &trackItem->sourceTrackItems[j];

                    AudioDataSource *sourceTrackAudioDataSource = NULL;
                    if (_tcscmp(sourceTrackItem->trackName, _T("input")) == 0) {
                        sourceTrackAudioDataSource = audioDataSourceStereo;
                    } else {
                        SpleeterProcessorResultTrack *sourceTrack = SpleeterProcessorResult_getTrack(result, sourceTrackItem->trackName);
                        if (sourceTrack == NULL) {
                            MSG_ERROR(_T("Track \"%s\" does not exist.\n"), sourceTrackItem->trackName);
                            return EXIT_FAILURE;
                        }

                        sourceTrackAudioDataSource = sourceTrack->audioDataSource;
                    }

                    if (audioDataSourceComputed == NULL) {
                        audioDataSourceComputed = AudioDataSource_createEmpty(sourceTrackAudioDataSource);
                    }

                    if (sourceTrackItem->toSubtract) {
                        AudioDataSource_subSamples(audioDataSourceComputed, sourceTrackAudioDataSource);
                    } else {
                        AudioDataSource_addSamples(audioDataSourceComputed, sourceTrackAudioDataSource);
                    }
                }

                if (!AudioFile_writeAll(outputFilePath, &outputAudioFileFormat, &spleeterSampleType,
                        (void *)audioDataSourceComputed->sampleValues, audioDataSourceComputed->sampleCountPerChannel)) {
                    MSG_ERROR(_T("Failed to write output file \"%s\".\n"), outputFilePath);
                    return EXIT_FAILURE;
                }

                Common_updateProgress(STAGE_AUDIO_FILE_WRITER, (i + 1), trackList.trackItemCount);

                AudioDataSource_free(&audioDataSourceComputed);
            }
        }
    }

    SpleeterProcessorResult_free(&result);

    MSG_INFO(_T("\n"));
    MSG_INFO(_T("Completed.\n"));

    return EXIT_SUCCESS;
}
