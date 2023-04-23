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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/mathematics.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "Common.h"
#include "Memory.h"
#include "AudioFileCommon.h"
#include "AudioFileWriter.h"

#if defined(_DEBUG) && 0
static void _logPacket(const AVFormatContext *formatContext, const AVPacket *packet) {
    AVRational *timeBase = &formatContext->streams[packet->stream_index]->time_base;

    DEBUG_INFO("pts: %s (%s), dts: %s (%s), duration: %s (%s), stream_index: %d\n",
        av_ts2str(packet->pts), av_ts2timestr(packet->pts, timeBase),
        av_ts2str(packet->dts), av_ts2timestr(packet->dts, timeBase),
        av_ts2str(packet->duration), av_ts2timestr(packet->duration, timeBase),
        packet->stream_index);
}
#else
#define _logPacket(formatContext, packet)
#endif

/**
 * 写入一个 AVFrame
 *
 * @param   obj     指向 AudioFileWriter 对象的指针
 * @param   frame   指向要写入 AVFrame 的指针
 *
 * @return  成功时返回 1 或 0, 失败时返回 < 0 的值
 */
static int _writeFrame(AudioFileWriter *obj, AVFrame *frame, AVPacket *packet) {
    int ret;

    // 将该帧提供给编码器
    ret = avcodec_send_frame(obj->_audioEncoderContext, frame);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_send_frame() failed: %s\n", av_err2str(ret));
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(obj->_audioEncoderContext, packet);
        if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF)) {
            // AVERROR(EAGAIN):   output is not available in the current state - user must try to send input
            // AVERROR_EOF:       the encoder has been fully flushed, and there will be no more output packets
            break;
        } else if (ret < 0) {
            DEBUG_ERROR("avcodec_receive_packet() failed: %s\n", av_err2str(ret));
            return ret;
        }

        // Rescale output packet timestamp values from codec to stream timebase
        av_packet_rescale_ts(packet, obj->_audioEncoderContext->time_base, obj->_audioStream->time_base);
        packet->stream_index = obj->_audioStream->index;

        // Write the compressed frame to the media file.
        _logPacket(obj->_outputFormatContext, packet);
        ret = av_interleaved_write_frame(obj->_outputFormatContext, packet);
        // pkt is now blank (av_interleaved_write_frame() takes ownership of
        // its contents and resets pkt), so that no unreferencing is necessary.
        // This would be different if one used av_write_frame().
        if (ret < 0) {
            DEBUG_ERROR("av_interleaved_write_frame() failed: %s\n", av_err2str(ret));
            return ret;
        }
    }

    if (ret == AVERROR_EOF) {
        // the encoder has been fully flushed, and there will be no more output packets
        return 1;
    } else {
        return 0;
    }
}

/**
 * 添加一个音频流
 *
 * @param   obj     指向 AudioFileWriter 对象的指针
 *
 * @return  成功时返回 true, 失败时返回 false
 */
static bool _addAudioStream(AudioFileWriter *obj) {
    // 从输出容器格式中获取音频的 Codec ID
    enum AVCodecID audioCodecId = obj->_outputFormatContext->oformat->audio_codec;
    if (audioCodecId == AV_CODEC_ID_NONE) {
        DEBUG_ERROR("audio codec not found\n");
        return false;
    }

    // 查找可用的音频编码器
    obj->_audioEncoder = avcodec_find_encoder(audioCodecId);
    if (obj->_audioEncoder == NULL) {
        DEBUG_ERROR("cannot find encoder for '%s'\n", avcodec_get_name(audioCodecId));
        return false;
    }
    if (obj->_audioEncoder->type != AVMEDIA_TYPE_AUDIO) {
        DEBUG_ERROR("found audio encoder is not the type of AVMEDIA_TYPE_AUDIO\n");
        return false;
    }

    // 为编码器创建临时 packet
    obj->_tempPacket = av_packet_alloc();
    if (obj->_tempPacket == NULL) {
        DEBUG_ERROR("av_packet_alloc() failed\n");
        return false;
    }

    // 添加一个新的音频流
    obj->_audioStream = avformat_new_stream(obj->_outputFormatContext, NULL);
    if (obj->_audioStream == NULL) {
        DEBUG_ERROR("avformat_new_stream() failed\n");
        return false;
    }
    obj->_audioStream->id = obj->_outputFormatContext->nb_streams - 1;

    // 为音频编码器分配一个 AVCodecContext
    obj->_audioEncoderContext = avcodec_alloc_context3(obj->_audioEncoder);
    if (obj->_audioEncoderContext == NULL) {
        DEBUG_ERROR("avcodec_alloc_context3() failed\n");
        return false;
    }

    const AVCodec *encoder = obj->_audioEncoder;
    AVCodecContext *context = obj->_audioEncoderContext;

    int sampleRate = obj->inputSampleType->sampleRate;
    int64_t bitRate = obj->fileFormat->bitRate;
    AVChannelLayout channelLayout;
    if (obj->inputSampleType->channelCount == 1) {
        channelLayout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
    } else if (obj->inputSampleType->channelCount == 2) {
        channelLayout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    } else {
        DEBUG_ERROR("wrong channel count: %d\n", obj->inputSampleType->channelCount);
        return false;
    }

    // 设置采样格式
    if (encoder->sample_fmts != NULL) {
        context->sample_fmt = encoder->sample_fmts[0];
    } else {
        context->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }

    // 设置采样率
    if (encoder->supported_samplerates != NULL) {
        context->sample_rate = encoder->supported_samplerates[0];

        for (int i = 0; encoder->supported_samplerates[i]; i++) {
            if (encoder->supported_samplerates[i] == sampleRate) {
                context->sample_rate = sampleRate;
                break;
            }
        }
    } else {
        context->sample_rate = sampleRate;
    }

    // 设置比特率
    context->bit_rate = bitRate;

    // 设置声道布局
    if (encoder->ch_layouts != NULL) {
        av_channel_layout_copy(&context->ch_layout, &encoder->ch_layouts[0]);

        const AVChannelLayout *p = encoder->ch_layouts;
        while (p->nb_channels) {
            if (av_channel_layout_compare(p, &channelLayout) == 0) {
                av_channel_layout_copy(&context->ch_layout, &channelLayout);
                break;
            }

            p++;
        }
    } else {
        av_channel_layout_copy(&context->ch_layout, &channelLayout);
    }

    // 设置 flags (有些文件格式有单独的头信息)
    if (obj->_outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 设置时间的基本单位
    obj->_audioStream->time_base = (AVRational) {
        .num = 1,
        .den = context->sample_rate
    };

    return true;
}

/**
 * 分配一个音频帧
 *
 * @param   destFramePtr
 * @param   sampleFormat
 * @param   sampleRate
 * @param   channelLayout
 * @param   frameSampleCountPerChannel
 *
 * @return  成功时返回 true, 失败时返回 false
 */
static bool _allocAudioFrame(AVFrame **destFramePtr, enum AVSampleFormat sampleFormat,
        int sampleRate, const AVChannelLayout *channelLayout, int frameSampleCountPerChannel) {
    AVFrame *frame = av_frame_alloc();
    if (frame == NULL) {
        DEBUG_ERROR("av_frame_alloc() failed\n");
        return false;
    }

    frame->format = sampleFormat;
    frame->sample_rate = sampleRate;
    av_channel_layout_copy(&frame->ch_layout, channelLayout);
    frame->nb_samples = frameSampleCountPerChannel;

    if (frameSampleCountPerChannel != 0) {
        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            DEBUG_ERROR("av_frame_get_buffer() failed: %s\n", av_err2str(ret));
            return false;
        }
    }

    *destFramePtr = frame;

    return true;
}

/**
 * 初始化音频编码器，并分配必要的缓冲区
 *
 * @param   obj         指向 AudioFileWriter 对象的指针
 * @param   opt_arg     可选参数
 *
 * @return  成功时返回 true, 失败时返回 false
 */
static bool _openAudio(AudioFileWriter *obj, AVDictionary *opt_arg) {
    int ret;

    AVCodecContext *encoderContext = obj->_audioEncoderContext;

    // 初始化音频编码器的 context
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(encoderContext, obj->_audioEncoder, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_open2() failed: %s\n", av_err2str(ret));
        return false;
    }

    // 从编码器的 context 获取帧大小
    int frameSize;
    if (((encoderContext->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) != 0)
            || (encoderContext->frame_size == 0)) {
        frameSize = 1152;
    } else {
        frameSize = encoderContext->frame_size;
    }

    // 分配用于存储原始样本值数据的 bufferFrame
    obj->_bufferFrameSampleCountPerChannel = frameSize;
    obj->_bufferFrame = NULL;
    if (!_allocAudioFrame(&obj->_bufferFrame, AudioFileCommon_getAvSampleFormat(obj->inputSampleType->sampleValueFormat),
            encoderContext->sample_rate, &encoderContext->ch_layout, frameSize)) {
        return false;
    }

    // 分配用于存储重采样 (调整采样格式) 后的样本值的 resampledFrame
    obj->_bufferFrameResampled = NULL;
    if (!_allocAudioFrame(&obj->_bufferFrameResampled, encoderContext->sample_fmt,
            encoderContext->sample_rate, &encoderContext->ch_layout, frameSize)) {
        return false;
    }

    // 复制音频流的参数
    ret = avcodec_parameters_from_context(obj->_audioStream->codecpar, encoderContext);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_parameters_from_context() failed: %s\n", av_err2str(ret));
        return false;
    }

    // 获取输入样本值格式
    enum AVSampleFormat inputSampleFormat = AudioFileCommon_getAvSampleFormat(obj->inputSampleType->sampleValueFormat);
    if (inputSampleFormat == AV_SAMPLE_FMT_NONE) {
        DEBUG_ERROR("AudioFileCommon_getAvSampleFormat() failed\n");
        return false;
    }

    // 初始化 libswresample 重采样器的 context
    obj->_resamplerContext = NULL;
    ret = swr_alloc_set_opts2(
        &obj->_resamplerContext,        // swresample context
        &encoderContext->ch_layout,     // [output] channel layout
        encoderContext->sample_fmt,     // [output] sample format
        encoderContext->sample_rate,    // [output] sample rate
        &encoderContext->ch_layout,     // [input] channel layout
        inputSampleFormat,              // [input] sample format
        encoderContext->sample_rate,    // [input] sample rate
        0,                              // logging level offset
        NULL                            // parent logging context, can be NULL
    );
    if (ret < 0) {
        DEBUG_ERROR("swr_alloc_set_opts2() failed: %s\n", av_err2str(ret));
        return false;
    }
    if (obj->_resamplerContext == NULL) {
        DEBUG_ERROR("swr_alloc_set_opts2() failed\n");
        return false;
    }

    // 初始化 libswresample 重采样器
    ret = swr_init(obj->_resamplerContext);
    if (ret < 0) {
        DEBUG_ERROR("swr_init() failed: %s\n", av_err2str(ret));
        return false;
    }

    return true;
}

/**
 * 编码一个音频帧，并送入 muxer
 *
 * @param   obj     指向 AudioFileWriter 对象的指针
 *
 * @return  成功时如果编码完成，则返回 1, 否则返回 0；
 *          失败时返回 < 0 的值
 */
static int _writeBufferFrame(AudioFileWriter *obj) {
    int ret;

    assert(obj->_bufferFrame != NULL);

    // 计算当前 buffer frame 重采样后的每声道样本数
    int resampledFrameSampleCountPerChannel = (int)av_rescale_rnd(
        swr_get_delay(obj->_resamplerContext, obj->_audioEncoderContext->sample_rate) + obj->_bufferFrame->nb_samples,
        obj->_audioEncoderContext->sample_rate,
        obj->_audioEncoderContext->sample_rate,
        AV_ROUND_UP
    );
    // 因为重采样仅调整采样格式，采样率和声道数保持不变，
    // 计算所得的重采样后每声道样本数应该与重采样之前的相等
    assert(resampledFrameSampleCountPerChannel == obj->_bufferFrame->nb_samples);

    // When we pass a frame to the encoder, it may keep a reference to it internally.
    // Make sure we do not overwrite it here
    ret = av_frame_make_writable(obj->_bufferFrameResampled);
    if (ret < 0) {
        return ret;
    }

    // 进行重采样转换
    ret = swr_convert(
        obj->_resamplerContext,
        obj->_bufferFrameResampled->data,           // [out]
        resampledFrameSampleCountPerChannel,        // [out]
        (const uint8_t **)obj->_bufferFrame->data,  // [in]
        obj->_bufferFrame->nb_samples               // [in]
    );
    if (ret < 0) {
        DEBUG_ERROR("swr_convert() failed\n");
        return ret;
    }

    obj->_bufferFrameResampled->pts = av_rescale_q(
        obj->_totalWrittenSampleCount,
        (AVRational) { 1, obj->_audioEncoderContext->sample_rate },
        obj->_audioEncoderContext->time_base
    );
    obj->_totalWrittenSampleCount += resampledFrameSampleCountPerChannel;

    ret = _writeFrame(obj, obj->_bufferFrameResampled, obj->_tempPacket);
    if (ret < 0) {
        DEBUG_ERROR("_writeFrame() failed\n");
        return ret;
    }

    return ret;
}

AudioFileWriter *AudioFileWriter_open(const TCHAR *filename,
        const AudioFileFormat *fileFormat, const AudioSampleType *inputSampleType) {
    int ret;
    AVDictionary *opt = NULL;
    AudioFileWriter *obj = NULL;

    if ((filename == NULL) || (fileFormat == NULL) || (inputSampleType == NULL)) {
        goto err;
    }

    obj = MEMORY_ALLOC_STRUCT(AudioFileWriter);
    if (obj == NULL) {
        DEBUG_ERROR("allocating AudioFileWriter struct failed\n");
        goto err;
    }

    obj->filenameUtf8 = AudioFileCommon_getUtf8StringFromUnicodeString(filename);
    if (obj->filenameUtf8 == NULL) {
        DEBUG_ERROR("converting filename to UTF-8 encoding failed\n");
        goto err;
    }

    obj->fileFormat = MEMORY_ALLOC_STRUCT(AudioFileFormat);
    if (obj->fileFormat == NULL) {
        DEBUG_ERROR("allocating AudioFileFormat struct failed\n");
        goto err;
    }
    memcpy(obj->fileFormat, fileFormat, sizeof(AudioFileFormat));

    obj->inputSampleType = MEMORY_ALLOC_STRUCT(AudioSampleType);
    if (obj->inputSampleType == NULL) {
        DEBUG_ERROR("allocating AudioSampleType struct failed\n");
        goto err;
    }
    memcpy(obj->inputSampleType, inputSampleType, sizeof(AudioSampleType));

    // 为输出格式分配一个 AVFormatContext
    ret = avformat_alloc_output_context2(&obj->_outputFormatContext,
            NULL, obj->fileFormat->formatName, obj->filenameUtf8);
    if (ret < 0) {
        DEBUG_ERROR("avformat_alloc_output_context2() failed: %s\n", av_err2str(ret));
        goto err;
    }
    if (obj->_outputFormatContext == NULL) {
        DEBUG_ERROR("avformat_alloc_output_context2() failed, ctx is null\n");
        goto err;
    }

    // 检查输出容器格式的 flags 中的 AVFMT_NOFILE 标记位 (应为 0)
    if ((obj->_outputFormatContext->oformat->flags & AVFMT_NOFILE) != 0) {
        DEBUG_ERROR("specified format has AVFMT_NOFILE flag\n");
        goto err;
    }

    // 添加一个音频流 (使用格式默认的编码器)
    if (!_addAudioStream(obj)) {
        DEBUG_ERROR("_addAudioStream() failed\n");
        goto err;
    }

    // 初始化音频编码器，并分配必要的缓冲区
    if (!_openAudio(obj, opt)) {
        DEBUG_ERROR("_openAudio() failed\n");
        goto err;
    }

    if (g_verboseMode) {
        av_dump_format(obj->_outputFormatContext, 0, obj->filenameUtf8, 1);
    }

    // 打开输出文件
    ret = avio_open(&obj->_outputFormatContext->pb, obj->filenameUtf8, AVIO_FLAG_WRITE);
    if (ret < 0) {
        DEBUG_ERROR("avio_open() failed: %s\n", av_err2str(ret));
        goto err;
    }

    // 写入头信息
    ret = avformat_write_header(obj->_outputFormatContext, &opt);
    if (ret < 0) {
        DEBUG_ERROR("avformat_write_header() failed: %s\n", av_err2str(ret));
        goto err;
    }
    obj->_headerWritten = true;

    return obj;

err:
    if (obj != NULL) {
        AudioFileWriter_close(&obj);
    }

    return NULL;
}

int AudioFileWriter_write(AudioFileWriter *obj, void *sampleValues, int sampleCountPerChannel) {
    int sampleValueSize = AudioFileCommon_getSampleValueSize(obj->inputSampleType->sampleValueFormat);
    if (sampleValueSize == -1) {
        DEBUG_ERROR("AudioFileCommon_getSampleValueSize() failed\n");
        return 0;
    }

    uint8_t *sampleValuePtr = sampleValues;
    int totalWrittenSampleCountPerChannel = 0;
    while (totalWrittenSampleCountPerChannel < sampleCountPerChannel) {
        // 该行不能放到循环外执行
        uint8_t *bufferFrameData = obj->_bufferFrame->data[0];

        // 计算本次循环要向 buffer frame 复制的样本数
        int frameSampleCountPerChannel = min(obj->_bufferFrameSampleCountPerChannel, (sampleCountPerChannel - totalWrittenSampleCountPerChannel));

        // 复制样本数据到 buffer frame 中
        int frameSampleDataLengthInBytes = frameSampleCountPerChannel * obj->_audioEncoderContext->ch_layout.nb_channels * sampleValueSize;
        memcpy(bufferFrameData, sampleValuePtr, frameSampleDataLengthInBytes);
        sampleValuePtr += frameSampleDataLengthInBytes;

        // 设置 buffer frame 的每声道样本数为实际复制的数量
        obj->_bufferFrame->nb_samples = frameSampleCountPerChannel;

        // 写入 buffer frame
        int ret = _writeBufferFrame(obj);
        if (ret < 0) {
            DEBUG_ERROR("_writeBufferFrame() failed\n");
            return totalWrittenSampleCountPerChannel;
        }

        totalWrittenSampleCountPerChannel += frameSampleCountPerChannel;
    }

    return totalWrittenSampleCountPerChannel;
}

void AudioFileWriter_close(AudioFileWriter **objPtr) {
    if (objPtr == NULL) {
        return;
    }

    AudioFileWriter *obj = *objPtr;

    // 使编码器对已缓冲的 packet 做 flush 处理，并结束 stream
    {
        // 可参看 avcodec_send_frame() 函数对 frame 参数为 NULL 时的说明
        int ret = _writeFrame(obj, NULL, obj->_tempPacket);
        if (ret < 0) {
            DEBUG_ERROR("_writeFrame() failed: error occurred when try to flush packets\n");
        }
    }

    // 写入尾部信息
    if ((obj->_outputFormatContext != NULL) && obj->_headerWritten) {
        int ret = av_write_trailer(obj->_outputFormatContext);
        if (ret < 0) {
            DEBUG_ERROR("av_write_trailer() failed: %s\n", av_err2str(ret));
        }
    }

    // 释放 AVCodecContext
    if (obj->_audioEncoderContext != NULL) {
        avcodec_free_context(&obj->_audioEncoderContext);
    }

    // 释放缓冲 frame
    if (obj->_bufferFrameResampled != NULL) {
        av_frame_free(&obj->_bufferFrameResampled);
    }
    if (obj->_bufferFrame != NULL) {
        av_frame_free(&obj->_bufferFrame);
    }
    if (obj->_tempPacket != NULL) {
        av_packet_free(&obj->_tempPacket);
    }

    // 释放 libswresample 的 context
    if (obj->_resamplerContext != NULL) {
        swr_free(&obj->_resamplerContext);
    }

    // 释放 AVFormatContext
    if (obj->_outputFormatContext != NULL) {
        // 关闭输出文件
        if (((obj->_outputFormatContext->oformat->flags & AVFMT_NOFILE) == 0)
                && (obj->_outputFormatContext->pb != NULL)) {
            int ret = avio_closep(&obj->_outputFormatContext->pb);
            if (ret < 0) {
                DEBUG_ERROR("avio_closep() failed: %s\n", av_err2str(ret));
            }
        }

        avformat_free_context(obj->_outputFormatContext);
    }

    if (obj->inputSampleType != NULL) {
        Memory_free(&obj->inputSampleType);
    }

    if (obj->fileFormat != NULL) {
        Memory_free(&obj->fileFormat);
    }

    if (obj->filenameUtf8 != NULL) {
        Memory_free(&obj->filenameUtf8);
    }

    Memory_free(objPtr);
}
