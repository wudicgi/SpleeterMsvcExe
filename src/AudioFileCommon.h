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

#ifndef _AUDIO_FILE_COMMON_H_
#define _AUDIO_FILE_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 音频样本值存储格式 (数据类型，和是否交错)
 */
typedef enum {
    /** 16 位有符号整数 (交错) */
    AUDIO_SAMPLE_VALUE_FORMAT_INT16_INTERLACED,

    /** 32 位浮点数 (交错) */
    AUDIO_SAMPLE_VALUE_FORMAT_FLOAT_INTERLACED
} AudioSampleValueFormat;

/**
 * 音频样本类型
 */
typedef struct {
    /** 采样率 */
    int                         sampleRate;

    /** 声道数 */
    int                         channelCount;

    /** 样本值格式 */
    AudioSampleValueFormat      sampleValueFormat;
} AudioSampleType;

/**
 * 音频文件格式
 */
typedef struct {
    /** 文件格式名称 (如 mp3, aac, flac 等，为 NULL 时根据文件名确定) */
    const char                  *formatName;

    /** 比特率 */
    int64_t                     bitRate;
} AudioFileFormat;

/**
 * 将 Unicode 编码的字符串转换为 UTF-8 编码
 *
 * @param   unicodeString   Unicode 编码的字符串
 *
 * @return  UTF-8 编码的字符串
 */
char *AudioFileCommon_getUtf8StringFromUnicodeString(const wchar_t *unicodeString);

/**
 * 获取指定样本值格式对应的 AVSampleFormat 枚举值
 *
 * @param   sampleValueFormat   样本值格式
 *
 * @return  成功时返回 AVSampleFormat 枚举值, 失败时返回 AV_SAMPLE_FMT_NONE
 */
enum AVSampleFormat AudioFileCommon_getAvSampleFormat(AudioSampleValueFormat sampleValueFormat);

/**
 * 获取指定样本值格式对应的单个样本值大小
 *
 * @param   sampleValueFormat   样本值格式
 *
 * @return  成功时返回单个样本值大小 (以字节为单位), 失败时返回 -1
 */
int AudioFileCommon_getSampleValueSize(AudioSampleValueFormat sampleValueFormat);

#ifdef __cplusplus
}
#endif

#endif // _AUDIO_FILE_H_
