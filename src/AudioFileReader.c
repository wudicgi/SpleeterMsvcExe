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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "Common.h"
#include "Memory.h"
#include "AudioFileCommon.h"
#include "AudioFileReader.h"

AudioFileReader *AudioFileReader_open(const TCHAR *filename, const AudioSampleType *outputSampleType) {
    int ret;
    AudioFileReader *obj = NULL;

    if ((filename == NULL) || (outputSampleType == NULL)) {
        goto err;
    }

    obj = MEMORY_ALLOC_STRUCT(AudioFileReader);
    if (obj == NULL) {
        DEBUG_ERROR("allocating AudioFileReader struct failed\n");
        goto err;
    }

    obj->filenameUtf8 = AudioFileCommon_getUtf8StringFromUnicodeString(filename);
    if (obj->filenameUtf8 == NULL) {
        DEBUG_ERROR("converting filename to UTF-8 encoding failed\n");
        goto err;
    }

    obj->outputSampleType = MEMORY_ALLOC_STRUCT(AudioSampleType);
    if (obj->outputSampleType == NULL) {
        DEBUG_ERROR("allocating AudioSampleType struct failed\n");
        goto err;
    }
    memcpy(obj->outputSampleType, outputSampleType, sizeof(AudioSampleType));

    // 分配一个空的 AVFormatContext
    obj->_inputFormatContext = avformat_alloc_context();
    if (obj->_inputFormatContext == NULL) {
        DEBUG_ERROR("avformat_alloc_context() failed\n");
        goto err;
    }

    // 打开输入文件，并读取头信息
    ret = avformat_open_input(&obj->_inputFormatContext, obj->filenameUtf8, NULL, NULL);
    if (ret < 0) {
        DEBUG_ERROR("avformat_open_input() failed: %s\n", av_err2str(ret));
        goto err;
    }

    // 读取文件中的 packets, 获取 stream 信息
    ret = avformat_find_stream_info(obj->_inputFormatContext, NULL);
    if (ret < 0) {
        DEBUG_ERROR("avformat_find_stream_info() failed: %s\n", av_err2str(ret));
        goto err;
    }

    // 计算总时长
    double durationInSeconds = (double)((double)obj->_inputFormatContext->duration / (double)AV_TIME_BASE);
    obj->durationInSeconds = durationInSeconds;

#if defined(_DEBUG) && 1
    av_dump_format(obj->_inputFormatContext, 0, obj->filenameUtf8, false);
    DEBUG_PRINTF("Duration: %lf seconds\n", obj->durationInSeconds);
#endif

    // 查找第一个音频流
    size_t audioStreamIndex = 0;
    while (audioStreamIndex < obj->_inputFormatContext->nb_streams) {
        if (obj->_inputFormatContext->streams[audioStreamIndex]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            break;
        }

        audioStreamIndex++;
    }
    if (audioStreamIndex == obj->_inputFormatContext->nb_streams) {
        DEBUG_ERROR("audio stream not found\n");
        goto err;
    }
    obj->_audioStreamIndex = audioStreamIndex;

    // 获取音频流的 codec context
    obj->_audioDecoderContext = obj->_inputFormatContext->streams[audioStreamIndex]->codec;
    if (obj->_audioDecoderContext == NULL) {
        DEBUG_ERROR("audio decoder context is null\n");
        goto err;
    }

    // TODO, from ffmpeg_decode.cpp
    if (obj->_audioDecoderContext->channel_layout == 0) {
        obj->_audioDecoderContext->channel_layout = AV_CH_LAYOUT_STEREO;
    }

    // 获取音频流的 decoder
    obj->_audioDecoder = avcodec_find_decoder(obj->_audioDecoderContext->codec_id);
    if (obj->_audioDecoder == NULL) {
        DEBUG_ERROR("audio decoder not found\n");
        goto err;
    }

    // 用找到的 decoder 初始化 codec context
    ret = avcodec_open2(obj->_audioDecoderContext, obj->_audioDecoder, NULL);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_open2() failed: %s\n", av_err2str(ret));
        goto err;
    }

    // 获取输出声道布局
    int64_t outputChannelLayout = AudioFileCommon_getChannelLayout(outputSampleType->channelCount);
    if (outputChannelLayout == -1) {
        DEBUG_ERROR("AudioFileCommon_getChannelLayout() failed\n");
        goto err;
    }

    // 获取输出样本值格式
    enum AVSampleFormat outputSampleFormat = AudioFileCommon_getAvSampleFormat(obj->outputSampleType->sampleValueFormat);
    if (outputSampleFormat == AV_SAMPLE_FMT_NONE) {
        DEBUG_ERROR("AudioFileCommon_getAvSampleFormat() failed\n");
        goto err;
    }

    // 初始化 libswresample 重采样器的 context
    obj->_resamplerContext = swr_alloc_set_opts(
        NULL,                                       // no existing swresample context
        outputChannelLayout,                        // [output] channel layout
        outputSampleFormat,                         // [output] sample format
        obj->outputSampleType->sampleRate,          // [output] sample rate
        obj->_audioDecoderContext->channel_layout,  // [input] channel layout
        obj->_audioDecoderContext->sample_fmt,      // [input] sample format
        obj->_audioDecoderContext->sample_rate,     // [input] sample rate
        0,                                          // logging level offset
        NULL                                        // parent logging context, can be NULL
    );
    if (obj->_resamplerContext == NULL) {
        DEBUG_ERROR("swr_alloc_set_opts() failed\n");
        goto err;
    }

    // 初始化 libswresample 重采样器
    ret = swr_init(obj->_resamplerContext);
    if (ret < 0) {
        DEBUG_ERROR("swr_init() failed: %s\n", av_err2str(ret));
        goto err;
    }

    // 为输入流创建临时 packet
    obj->_tempPacket = av_packet_alloc();
    if (obj->_tempPacket == NULL) {
        DEBUG_ERROR("av_packet_alloc() failed\n");
        goto err;
    }

    // 为 decoder 分配临时 frame
    obj->_tempFrame = av_frame_alloc();
    if (obj->_tempFrame == NULL) {
        DEBUG_ERROR("av_frame_alloc() failed\n");
        goto err;
    }

    // 设置 _resamplerOutputBuffer 的初始值为空
    obj->_resamplerOutputBufferSampleCountPerChannel = 0;
    obj->_resamplerOutputBufferSize = 0;
    obj->_resamplerOutputBuffer = NULL;

    return obj;

err:
    if (obj != NULL) {
        AudioFileReader_close(&obj);
    }

    return NULL;
}

int AudioFileReader_read(AudioFileReader *obj, void *destBuffer, int destBufferSampleCountPerChannel) {
    int ret;

    // 从输入流中读取下一个 frame 为 packet
    ret = av_read_frame(obj->_inputFormatContext, obj->_tempPacket);
    if (ret < 0) {
        return -1;
    }

    // 如果 packet 不是所找到音频流的，跳过
    if (obj->_tempPacket->stream_index != obj->_audioStreamIndex) {
        return 0;
    }

    // 解码 packet 为 frame
    int gotFrame = 0;
#pragma warning(suppress : 4996)    // 忽略 avcodec_decode_audio4() 函数被标记为已废弃的警告
    ret = avcodec_decode_audio4(obj->_audioDecoderContext, obj->_tempFrame, &gotFrame, obj->_tempPacket);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_decode_audio4() failed\n");
        return -1;
    }
    if (!gotFrame) {
        return 0;
    }

    // 获取输出样本值格式
    enum AVSampleFormat outputSampleFormat = AudioFileCommon_getAvSampleFormat(obj->outputSampleType->sampleValueFormat);

    // 分配输出 buffer 的空间
    int resamplerUpperBoundOutputSampleCountPerChannel = swr_get_out_samples(obj->_resamplerContext, obj->_tempFrame->nb_samples);
    if ((obj->_resamplerOutputBuffer == NULL)
            || (resamplerUpperBoundOutputSampleCountPerChannel > obj->_resamplerOutputBufferSampleCountPerChannel)) {
        // 如果是扩大 buffer 空间的情况，先释放原有空间
        if (obj->_resamplerOutputBuffer != NULL) {
            av_free(obj->_resamplerOutputBuffer);
            obj->_resamplerOutputBuffer = NULL;
        }

        obj->_resamplerOutputBufferSampleCountPerChannel = resamplerUpperBoundOutputSampleCountPerChannel;

        obj->_resamplerOutputBufferSize = av_samples_get_buffer_size(NULL,
                obj->outputSampleType->channelCount, resamplerUpperBoundOutputSampleCountPerChannel, outputSampleFormat, 1);

        obj->_resamplerOutputBuffer = (uint8_t*)av_malloc(obj->_resamplerOutputBufferSize);
        if (obj->_resamplerOutputBuffer == NULL) {
            DEBUG_ERROR("allocating resampler output buffer failed\n");
            return -1;
        }
    }

    // 转换 frame 到输出 buffer
    int outputSampleCountPerChannel = swr_convert(obj->_resamplerContext,
            &obj->_resamplerOutputBuffer, obj->_resamplerOutputBufferSampleCountPerChannel,
            (const uint8_t **)obj->_tempFrame->data, obj->_tempFrame->nb_samples);
    if (outputSampleCountPerChannel < 0) {
        DEBUG_ERROR("swr_convert() failed\n");
        return -1;
    }

    // 计算要复制到 destBuffer 的每声道样本数
    int copySampleCountPerChannel = outputSampleCountPerChannel;
    if (copySampleCountPerChannel > destBufferSampleCountPerChannel) {
        copySampleCountPerChannel = destBufferSampleCountPerChannel;
    }

    // 复制重采样结果到 destBuffer
    if (copySampleCountPerChannel > 0) {
        int copyDataLengthInBytes = av_samples_get_buffer_size(NULL,
                obj->outputSampleType->channelCount, copySampleCountPerChannel, outputSampleFormat, 1);
        assert(copyDataLengthInBytes <= obj->_resamplerOutputBufferSize);

        memcpy(destBuffer, obj->_resamplerOutputBuffer, copyDataLengthInBytes);
    }

    // 清扫临时 packet 的数据
    av_packet_unref(obj->_tempPacket);

    return copySampleCountPerChannel;
}

void AudioFileReader_close(AudioFileReader **objPtr) {
    if (objPtr == NULL) {
        return;
    }

    AudioFileReader *obj = *objPtr;

    if (obj->_resamplerOutputBuffer != NULL) {
        av_free(obj->_resamplerOutputBuffer);
        obj->_resamplerOutputBuffer = NULL;
    }

    if (obj->_tempFrame != NULL) {
        av_frame_free(&obj->_tempFrame);
    }

    if (obj->_tempPacket != NULL) {
        av_packet_free(&obj->_tempPacket);
    }

    if (obj->_resamplerContext != NULL) {
        swr_free(&obj->_resamplerContext);
    }

    if (obj->_audioDecoderContext != NULL) {
        avcodec_close(obj->_audioDecoderContext);
        obj->_audioDecoderContext = NULL;
    }

    if (obj->_inputFormatContext != NULL) {
        avformat_close_input(&obj->_inputFormatContext);
    }

    if (obj->outputSampleType != NULL) {
        Memory_free(&obj->outputSampleType);
    }

    if (obj->filenameUtf8 != NULL) {
        Memory_free(&obj->filenameUtf8);
    }

    Memory_free(objPtr);
}
