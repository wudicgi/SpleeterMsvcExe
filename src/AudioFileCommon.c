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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <Windows.h>
#include "libavformat/avformat.h"
#include "Common.h"
#include "AudioFileCommon.h"

char *AudioFileCommon_getUtf8StringFromUnicodeString(const wchar_t *unicodeString) {
    // 先获取转换结果的长度
    int length = WideCharToMultiByte(CP_UTF8, 0, unicodeString, -1, NULL, 0, NULL, NULL);

    // 为转换结果分配空间 (长度 length 中已包含结尾 '\0' 字符)
    char *utf8String = (char *)MEMORY_ALLOC_ARRAY(char, length);

    // 进行实际的转换
    WideCharToMultiByte(CP_UTF8, 0, unicodeString, -1, utf8String, length, NULL, NULL);

    return utf8String;
}

enum AVSampleFormat AudioFileCommon_getAvSampleFormat(AudioSampleValueFormat sampleValueFormat) {
    switch (sampleValueFormat) {
        case AUDIO_SAMPLE_VALUE_FORMAT_INT16_INTERLACED:
            return AV_SAMPLE_FMT_S16;

        case AUDIO_SAMPLE_VALUE_FORMAT_FLOAT_INTERLACED:
            return AV_SAMPLE_FMT_FLT;

        default:
            return AV_SAMPLE_FMT_NONE;
    }
}

int AudioFileCommon_getSampleValueSize(AudioSampleValueFormat sampleValueFormat) {
    switch (sampleValueFormat) {
        case AUDIO_SAMPLE_VALUE_FORMAT_INT16_INTERLACED:
            return (int)sizeof(int16_t);

        case AUDIO_SAMPLE_VALUE_FORMAT_FLOAT_INTERLACED:
            return (int)sizeof(float);

        default:
            return -1;
    }
}
