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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <tchar.h>
#include "Memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG
    #define DEBUG_PRINTF(fmt, ...)  printf(fmt, __VA_ARGS__)
#else
    #define DEBUG_PRINTF(fmt, ...)
#endif

#ifdef _DEBUG
    #define DEBUG_ERROR(fmt, ...)   fprintf(stderr, "Error: " fmt, __VA_ARGS__)
#else
    #define DEBUG_ERROR(fmt, ...)
#endif

/*
In the Windows API (with some exceptions discussed in the following paragraphs), the maximum length for a path is MAX_PATH,
which is defined as 260 characters. A local path is structured in the following order: drive letter, colon, backslash,
name components separated by backslashes, and a terminating null character. For example, the maximum path on drive D
is "D:\some 256-character path string<NUL>" where "<NUL>" represents the invisible terminating null character for
the current system codepage. (The characters < > are used here for visual clarity and cannot be part of a valid path string.)

MAX_PATH 的 260 字节长度包含结尾的 '\0' 字符，因此声明数组时直接使用 MAX_PATH, 判断字符串长度时使用 (MAX_PATH - 1)
*/
#define FILE_PATH_MAX_SIZE      MAX_PATH

typedef enum {
    STAGE_AUDIO_FILE_READER,
    STAGE_SPLEETER_PROCESSOR_LOAD_MODEL,
    STAGE_SPLEETER_PROCESSOR_PROCESS_SEGMENT,
    STAGE_AUDIO_FILE_WRITER
} Stage;

void Common_updateProgress(Stage stage, int stageProgress, int stageTotal);

#ifdef __cplusplus
}
#endif

#endif // _COMMON_H_
