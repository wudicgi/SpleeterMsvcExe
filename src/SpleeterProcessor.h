/*
 * MIT License
 *
 * Copyright (c) 2020 Wudi <wudi@wudilabs.org>
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

#ifndef _SPLEETER_PROCESSOR_H_
#define _SPLEETER_PROCESSOR_H_

#include "Common.h"
#include "AudioFile.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Spleeter 模型的音频采样值数据类型 */
typedef float SpleeterModelAudioSampleValue_t;

/** Spleeter 模型的音频声道数 */
#define SPLEETER_MODEL_AUDIO_CHANNEL_COUNT      2

/** Spleeter 模型的音频采样率 */
#define SPLEETER_MODEL_AUDIO_SAMPLE_RATE        44100

/** Spleeter 模型的最大输出数量 (音轨数) */
#define SPLEETER_MODEL_MAX_OUTPUT_COUNT         5

typedef struct {
    const TCHAR     *basicName;
    int             outputCount;
    const char      *outputNames[SPLEETER_MODEL_MAX_OUTPUT_COUNT];
    const TCHAR     *trackNames[SPLEETER_MODEL_MAX_OUTPUT_COUNT];
} SpleeterModelInfo;

/** Spleeter 单个音轨处理结果 */
typedef struct {
    /** 音轨名称 (vocals, accompaniment, drums 等) */
    TCHAR               *trackName;

    /** 音频数据 */
    AudioDataSource     *audioDataSource;
} SpleeterProcessorResultTrack;

/** Spleeter 所有处理结果 */
typedef struct {
    /** 音轨列表 */
    SpleeterProcessorResultTrack    trackList[SPLEETER_MODEL_MAX_OUTPUT_COUNT];

    /** 音轨数量 */
    int                             trackCount;
} SpleeterProcessorResult;

/**
 * 根据 Spleeter 模型的目录名称获取模型信息
 *
 * @param   modelName           Spleeter 模型的目录名称 ("2stems", "4stems", "5stems-16khz" 等)
 *
 * @return  如果找到模型信息则返回指向 SpleeterModelInfo 结构体的指针，未找到则返回 NULL
 */
const SpleeterModelInfo *SpleeterProcessor_getModelInfo(const TCHAR *modelName);

/**
 * 使用 Spleeter 模型对音频进行分离
 *
 * @param   modelName           要使用的模型名称 ("2stems", "4stems", "5stems-16khz" 等)
 * @param   audioDataSource     输入音频数据源
 * @param   resultOut           分离结果
 *
 * @return  成功时返回 0, 失败时返回小于 0 的错误码
 */
int SpleeterProcessor_split(const TCHAR *modelName, AudioDataSource *audioDataSource, SpleeterProcessorResult **resultOut);

/**
 * 根据轨道名称获取 SpleeterProcessorResult 中的轨道
 *
 * @param   obj                 指向 SpleeterProcessorResult 结构体的指针
 * @param   trackName           轨道名称
 *
 * @return  成功时返回指向 SpleeterProcessorResultTrack 结构体的指针，失败时返回 NULL
 */
SpleeterProcessorResultTrack *SpleeterProcessorResult_getTrack(SpleeterProcessorResult *obj, const TCHAR *trackName);

/**
 * 释放 SpleeterProcessorResult 结构体和其中的音轨名称、音频数据所占用的内存空间
 *
 * @param   objPtr              指向 SpleeterProcessorResult 结构体的指针的指针
 */
void SpleeterProcessorResult_free(SpleeterProcessorResult **objPtr);

#ifdef __cplusplus
}
#endif

#endif // _SPLEETER_PROCESSOR_H_
