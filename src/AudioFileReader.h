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

#ifndef _AUDIO_FILE_READER_H_
#define _AUDIO_FILE_READER_H_

#include <tchar.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "AudioFileCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * AudioFileReader 的上下文数据
 *
 * 此处未通过不透明结构体 (opaque structure) 的方式隐藏 private 成员，目的是便于 debug
 */
typedef struct {
    // 以下部分为 public 成员，用于获取所读取音频文件相关信息，只读

    /** 文件名 (UTF-8 编码) */
    char                *filenameUtf8;

    /** 音频总时长 (以秒为单位) */
    double              durationInSeconds;

    /** 输出样本值的样本类型 */
    AudioSampleType     *outputSampleType;

    // 以下部分为 private 成员，仅内部使用

    /** 输入容器格式的 context */
    AVFormatContext     *_inputFormatContext;
    /** 解码器 */
    const AVCodec       *_audioDecoder;
    /** 解码器的 context */
    AVCodecContext      *_audioDecoderContext;
    /** 重采样器 */
    SwrContext          *_resamplerContext;

    /** 所找到音频流的 index */
    size_t              _audioStreamIndex;

    /** 从文件读取 packet 时使用 */
    AVPacket            *_tempPacket;

    /** 解码 packet 时使用的空 frame */
    AVFrame             *_tempFrame;

    /** 存储重采样结果的 buffer 所能容纳的每声道样本数 */
    int                 _resamplerOutputBufferSampleCountPerChannel;
    /** 存储重采样结果的 buffer 大小 (以字节为单位) */
    int                 _resamplerOutputBufferSize;
    /** 存储重采样结果的 buffer */
    uint8_t             *_resamplerOutputBuffer;
} AudioFileReader;

/**
 * 打开要读取的音频文件，并初始化 AudioFileReader 对象
 *
 * @param   filename            要打开文件的文件名
 * @param   outputSampleType    输出样本值的样本类型
 *
 * @return  成功时，返回指向已分配和初始化的 AudioFileReader 对象的指针；
 *          失败时，返回 NULL
 */
AudioFileReader *AudioFileReader_open(const TCHAR *filename, const AudioSampleType *outputSampleType);

/**
 * 读取样本值
 *
 * @param   obj                                 指向 AudioFileReader 对象的指针
 * @param   destBuffer                          指向目标缓冲区的指针，所读取的样本值将存储在该缓冲区中
 * @param   destBufferSampleCountPerChannel     destBuffer 所能容纳的每声道样本数
 *
 * @return  成功时，返回实际读取并写入到 destBuffer 的每声道样本数；
 *          失败时，返回小于 0 的错误码
 */
int AudioFileReader_read(AudioFileReader *obj, void *destBuffer, int destBufferSampleCountPerChannel);

/**
 * 关闭已打开的音频文件
 *
 * @param   objPtr          指向 AudioFileReader 对象的指针的指针
 */
void AudioFileReader_close(AudioFileReader **objPtr);

#ifdef __cplusplus
}
#endif

#endif // _AUDIO_FILE_READER_H_
