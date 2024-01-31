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

#ifndef _AUDIO_FILE_WRITER_H_
#define _AUDIO_FILE_WRITER_H_

#include "Common.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "AudioFileCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * AudioFileWriter 的上下文数据
 *
 * 此处未通过不透明结构体 (opaque structure) 的方式隐藏 private 成员，目的是便于 debug
 */
typedef struct {
    // 以下部分为 public 成员，用于获取所指定的文件名和文件格式等信息，只读

    /** 文件名 (UTF-8 编码) */
    char                    *filenameUtf8;

    /** 文件格式配置参数 */
    AudioFileFormat         *fileFormat;

    /** 输入样本值的样本类型 */
    AudioSampleType         *inputSampleType;

    // 以下部分为 private 成员，仅内部使用

    /** 输出容器格式的 context */
    AVFormatContext         *_outputFormatContext;

    /** 是否已调用 avformat_write_header() 成功写入头信息 */
    bool                    _headerWritten;

    /** 音频流 */
    AVStream                *_audioStream;

    /** 编码器 */
    const AVCodec           *_audioEncoder;
    /** 编码器的 context */
    AVCodecContext          *_audioEncoderContext;

    /** 写入 frame 时使用的临时 packet */
    AVPacket                *_tempPacket;

    /** 重采样器的 context */
    SwrContext              *_resamplerContext;

    /** _bufferFrame 和 _bufferFrameResampled 所能容纳的每声道样本数 */
    int                     _bufferFrameSampleCountPerChannel;
    /** 用于存储原始样本值的缓冲 frame */
    AVFrame                 *_bufferFrame;
    /** 用于存储重采样 (仅调整采样格式) 后样本值的缓冲 frame */
    AVFrame                 *_bufferFrameResampled;

    /** 总的每声道已写入采样数 */
    int                     _totalWrittenSampleCount;
} AudioFileWriter;

/**
 * 打开要写入的音频文件，并返回所创建的 AudioFileWriter 对象
 *
 * @param   filename                要打开文件的文件名 (如果不存在，则会创建一个新的文件)
 * @param   fileFormat              要生成音频文件的格式
 * @param   inputSampleType         输入样本值的样本类型
 *
 * @return  成功时，返回指向已分配和初始化的 AudioFileWriter 对象的指针；
 *          失败时，返回 NULL
 */
AudioFileWriter *AudioFileWriter_open(const TCHAR *filename,
        const AudioFileFormat *fileFormat, const AudioSampleType *inputSampleType);

/**
 * 写入样本值
 *
 * @param   obj                     指向 AudioFileWriter 对象的指针
 * @param   sampleValues            指向存储有要写入样本值的数组的指针 (数据类型和存储方式由 sampleValueFormat 确定)
 * @param   sampleCountPerChannel   要写入样本值的数量 (单个声道)
 *
 * @return  成功时，返回实际写入样本值的数量 (单个声道)；
 *          失败时，返回小于 0 的错误码
 */
int AudioFileWriter_write(AudioFileWriter *obj, void *sampleValues, int sampleCountPerChannel);

/**
 * 关闭已打开的音频文件
 *
 * @param   objPtr          指向 AudioFileWriter 对象的指针的指针
 */
void AudioFileWriter_close(AudioFileWriter **objPtr);

#ifdef __cplusplus
}
#endif

#endif // _AUDIO_FILE_WRITER_H_
