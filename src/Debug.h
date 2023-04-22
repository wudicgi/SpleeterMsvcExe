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

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_verboseMode;

extern bool g_debugMode;

#ifdef _UNICODE
#define TSTRING_FORMAT_SPECIFIER    "%ls"
#else
#define TSTRING_FORMAT_SPECIFIER    "%s"
#endif

#define DEBUG_INFO(fmt, ...)        debugPrintInfo(fmt, __VA_ARGS__)

#define DEBUG_ERROR(fmt, ...)       debugPrintError(fmt, __VA_ARGS__)

void debugPrintInfo(const char *format, ...);

void debugPrintError(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif // _DEBUG_H_
