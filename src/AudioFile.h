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

#ifndef _AUDIO_FILE_H_
#define _AUDIO_FILE_H_

#include <tchar.h>
#include "AudioFileReader.h"
#include "AudioFileWriter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 音频样本值类型
 */
typedef float AudioSampleValue_t;

/**
 * 音频数据源
 */
typedef struct {
    /** 文件名 (UTF-8 编码) */
    char                    *filenameUtf8;

    /** 采样率 */
    int                     sampleRate;

    /** 样本值数组 */
    AudioSampleValue_t      *sampleValues;

    /** 每声道样本数 */
    int                     sampleCountPerChannel;

    /** 声道数 */
    int                     channelCount;
} AudioDataSource;

/**
 * 分配内存空间，创建一个空的 AudioDataSource 结构体
 *
 * @return  返回所创建的 AudioDataSource 结构体
 */
AudioDataSource *AudioDataSource_alloc(void);

/**
 * 释放 AudioDataSource 结构体和其中的文件名、音频数据所占用的内存空间
 *
 * @param   objPtr                  指向 AudioDataSource 结构体的指针的指针
 */
void AudioDataSource_free(AudioDataSource **objPtr);

/**
 * 创建一个与 objRef 的信息相同，但样本值都为零的 AudioDataSource
 *
 * @param   objRef                  指向参考 AudioDataSource 结构体的指针
 *
 * @return  返回所创建的 AudioDataSource 结构体
 */
AudioDataSource *AudioDataSource_createEmpty(AudioDataSource *objRef);

/**
 * 将两个 AudioDataSource 中的样本值相加 (obj = obj + obj2)
 *
 * @param   obj                     指向第 1 个 AudioDataSource 结构体的指针
 * @param   obj2                    指向第 2 个 AudioDataSource 结构体的指针
 */
void AudioDataSource_addSamples(AudioDataSource *obj, AudioDataSource *obj2);

/**
 * 将两个 AudioDataSource 中的样本值相减 (obj = obj - obj2)
 *
 * @param   obj                     指向第 1 个 AudioDataSource 结构体的指针
 * @param   obj2                    指向第 2 个 AudioDataSource 结构体的指针
 */
void AudioDataSource_subSamples(AudioDataSource *obj, AudioDataSource *obj2);

/**
 * 打开一个音频文件，并读取所有样本值
 *
 * @param   filename                要读取音频文件的文件名
 * @param   outputSampleType        输出样本类型
 *
 * @return  成功时返回包含所读取到样本值的 AudioDataSource 对象，失败时返回 NULL
 */
AudioDataSource *AudioFile_readAll(const TCHAR *filename, const AudioSampleType *outputSampleType);

/**
 * 创建一个音频文件，并写入指定的样本值
 *
 * @param   filename                要创建音频文件的文件名
 * @param   fileFormat              要生成音频文件的格式
 * @param   inputSampleType         样本值的样本类型
 * @param   sampleValues            存储有要写入样本值的数组 (样本值的数据类型和存储格式由 sampleValueFormat 确定)
 * @param   sampleCountPerChannel   样本值的数量 (单个声道)
 *
 * @return  成功时返回 true, 失败时返回 false
 */
bool AudioFile_writeAll(const TCHAR *filename, const AudioFileFormat *fileFormat, const AudioSampleType *inputSampleType,
        void *sampleValues, int sampleCountPerChannel);

#ifdef __cplusplus
}
#endif

#endif // _AUDIO_FILE_H_
