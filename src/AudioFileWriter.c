/*
 * MIT License
 *
 * Copyright (c) 2020-2021 Wudi <wudi@wudilabs.org>
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

    DEBUG_PRINTF("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
        av_ts2str(packet->pts), av_ts2timestr(packet->pts, timeBase),
        av_ts2str(packet->dts), av_ts2timestr(packet->dts, timeBase),
        av_ts2str(packet->duration), av_ts2timestr(packet->duration, timeBase),
        packet->stream_index);
}
#else
#define _logPacket(formatContext, packet)
#endif

/**
 * ???????????? AVFrame
 *
 * @param   obj     ?????? AudioFileWriter ???????????????
 * @param   frame   ??????????????? AVFrame ?????????
 *
 * @return  ??????????????? 1 ??? 0, ??????????????? < 0 ??????
 */
static int _writeFrame(AudioFileWriter *obj, AVFrame *frame) {
    int ret;

    // ???????????????????????????
    ret = avcodec_send_frame(obj->_audioEncoderContext, frame);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_send_frame() failed: %s\n", av_err2str(ret));
        return ret;
    }

    while (ret >= 0) {
        AVPacket packet = { 0 };

        ret = avcodec_receive_packet(obj->_audioEncoderContext, &packet);
        if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF)) {
            // AVERROR(EAGAIN):   output is not available in the current state - user must try to send input
            // AVERROR_EOF:       the encoder has been fully flushed, and there will be no more output packets
            break;
        } else if (ret < 0) {
            DEBUG_ERROR("avcodec_receive_packet() failed: %s\n", av_err2str(ret));
            return ret;
        }

        // Rescale output packet timestamp values from codec to stream timebase
        av_packet_rescale_ts(&packet, obj->_audioEncoderContext->time_base, obj->_audioStream->time_base);
        packet.stream_index = obj->_audioStream->index;

        // Write the compressed frame to the media file.
        _logPacket(obj->_outputFormatContext, &packet);
        ret = av_interleaved_write_frame(obj->_outputFormatContext, &packet);
        av_packet_unref(&packet);
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
 * ?????????????????????
 *
 * @param   obj     ?????? AudioFileWriter ???????????????
 *
 * @return  ??????????????? true, ??????????????? false
 */
static bool _addAudioStream(AudioFileWriter *obj) {
    // ??????????????????????????????????????? Codec ID
    enum AVCodecID audioCodecId = obj->_outputFormatContext->oformat->audio_codec;
    if (audioCodecId == AV_CODEC_ID_NONE) {
        DEBUG_ERROR("audio codec not found\n");
        return false;
    }

    // ??????????????????????????????
    obj->_audioEncoder = avcodec_find_encoder(audioCodecId);
    if (obj->_audioEncoder == NULL) {
        DEBUG_ERROR("cannot find encoder for '%s'\n", avcodec_get_name(audioCodecId));
        return false;
    }
    if (obj->_audioEncoder->type != AVMEDIA_TYPE_AUDIO) {
        DEBUG_ERROR("found audio encoder is not the type of AVMEDIA_TYPE_AUDIO\n");
        return false;
    }

    // ???????????????????????????
    obj->_audioStream = avformat_new_stream(obj->_outputFormatContext, NULL);
    if (obj->_audioStream == NULL) {
        DEBUG_ERROR("avformat_new_stream() failed\n");
        return false;
    }
    obj->_audioStream->id = obj->_outputFormatContext->nb_streams - 1;

    // ?????????????????????????????? AVCodecContext
    obj->_audioEncoderContext = avcodec_alloc_context3(obj->_audioEncoder);
    if (obj->_audioEncoderContext == NULL) {
        DEBUG_ERROR("avcodec_alloc_context3() failed\n");
        return false;
    }

    AVCodec *encoder = obj->_audioEncoder;
    AVCodecContext *context = obj->_audioEncoderContext;

    int sampleRate = obj->inputSampleType->sampleRate;
    int64_t bitRate = obj->fileFormat->bitRate;
    uint64_t channelLayout = (obj->inputSampleType->channelCount == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

    // ??????????????????
    if (encoder->sample_fmts != NULL) {
        context->sample_fmt = encoder->sample_fmts[0];
    } else {
        context->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }

    // ???????????????
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

    // ???????????????
    context->bit_rate = bitRate;

    // ??????????????????
    if (encoder->channel_layouts != NULL) {
        context->channel_layout = encoder->channel_layouts[0];

        for (int i = 0; encoder->channel_layouts[i]; i++) {
            if (encoder->channel_layouts[i] == channelLayout) {
                context->channel_layout = channelLayout;
                break;
            }
        }
    } else {
        context->channel_layout = channelLayout;
    }

    // ???????????????
    context->channels = av_get_channel_layout_nb_channels(context->channel_layout);

    // ?????? flags (???????????????????????????????????????)
    if (obj->_outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // ???????????????????????????
    obj->_audioStream->time_base = (AVRational) {
        .num = 1,
        .den = context->sample_rate
    };

    return true;
}

/**
 * ?????????????????????
 *
 * @param   destFramePtr
 * @param   sampleFormat
 * @param   sampleRate
 * @param   channelLayout
 * @param   frameSampleCountPerChannel
 *
 * @return  ??????????????? true, ??????????????? false
 */
static bool _allocAudioFrame(AVFrame **destFramePtr, enum AVSampleFormat sampleFormat,
        int sampleRate, uint64_t channelLayout, int frameSampleCountPerChannel) {
    AVFrame *frame = av_frame_alloc();
    if (frame == NULL) {
        DEBUG_ERROR("av_frame_alloc() failed\n");
        return false;
    }

    frame->format = sampleFormat;
    frame->sample_rate = sampleRate;
    frame->channel_layout = channelLayout;
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
 * ??????????????????????????????????????????????????????
 *
 * @param   obj         ?????? AudioFileWriter ???????????????
 * @param   opt_arg     ????????????
 *
 * @return  ??????????????? true, ??????????????? false
 */
static bool _openAudio(AudioFileWriter *obj, AVDictionary *opt_arg) {
    int ret;

    AVCodecContext *encoderContext = obj->_audioEncoderContext;

    // ??????????????????????????? context
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(encoderContext, obj->_audioEncoder, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_open2() failed: %s\n", av_err2str(ret));
        return false;
    }

    // ??????????????? context ???????????????
    int frameSize;
    if (((encoderContext->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) != 0)
            || (encoderContext->frame_size == 0)) {
        frameSize = 1152;
    } else {
        frameSize = encoderContext->frame_size;
    }

    // ?????????????????????????????????????????? bufferFrame
    obj->_bufferFrameSampleCountPerChannel = frameSize;
    obj->_bufferFrame = NULL;
    if (!_allocAudioFrame(&obj->_bufferFrame, AudioFileCommon_getAvSampleFormat(obj->inputSampleType->sampleValueFormat),
            encoderContext->sample_rate, encoderContext->channel_layout, frameSize)) {
        return false;
    }

    // ??????????????????????????? (??????????????????) ?????????????????? resampledFrame
    obj->_bufferFrameResampled = NULL;
    if (!_allocAudioFrame(&obj->_bufferFrameResampled, encoderContext->sample_fmt,
            encoderContext->sample_rate, encoderContext->channel_layout, frameSize)) {
        return false;
    }

    // ????????????????????????
    ret = avcodec_parameters_from_context(obj->_audioStream->codecpar, encoderContext);
    if (ret < 0) {
        DEBUG_ERROR("avcodec_parameters_from_context() failed: %s\n", av_err2str(ret));
        return false;
    }

    // ???????????????????????????
    enum AVSampleFormat inputSampleFormat = AudioFileCommon_getAvSampleFormat(obj->inputSampleType->sampleValueFormat);
    if (inputSampleFormat == AV_SAMPLE_FMT_NONE) {
        DEBUG_ERROR("AudioFileCommon_getAvSampleFormat() failed\n");
        return false;
    }

    // ?????? libswresample ??????????????? context
    obj->_resamplerContext = swr_alloc();
    if (obj->_resamplerContext == NULL) {
        DEBUG_ERROR("swr_alloc() failed\n");
        return false;
    }

    // ???????????????????????? (?????????????????????)
    av_opt_set_sample_fmt(obj->_resamplerContext,   "in_sample_fmt",        inputSampleFormat,              0);
    av_opt_set_int(obj->_resamplerContext,          "in_sample_rate",       encoderContext->sample_rate,    0);
    av_opt_set_int(obj->_resamplerContext,          "in_channel_count",     encoderContext->channels,       0);
    av_opt_set_sample_fmt(obj->_resamplerContext,   "out_sample_fmt",       encoderContext->sample_fmt,     0);
    av_opt_set_int(obj->_resamplerContext,          "out_sample_rate",      encoderContext->sample_rate,    0);
    av_opt_set_int(obj->_resamplerContext,          "out_channel_count",    encoderContext->channels,       0);

    // ????????? libswresample ????????????
    ret = swr_init(obj->_resamplerContext);
    if (ret < 0) {
        DEBUG_ERROR("swr_init() failed: %s\n", av_err2str(ret));
        return false;
    }

    return true;
}

/**
 * ????????????????????????????????? muxer
 *
 * @param   obj     ?????? AudioFileWriter ???????????????
 *
 * @return  ??????????????????????????????????????? 1, ???????????? 0???
 *          ??????????????? < 0 ??????
 */
static int _writeBufferFrame(AudioFileWriter *obj) {
    int ret;

    assert(obj->_bufferFrame != NULL);

    // ???????????? buffer frame ?????????????????????????????????
    int resampledFrameSampleCountPerChannel = (int)av_rescale_rnd(
        swr_get_delay(obj->_resamplerContext, obj->_audioEncoderContext->sample_rate) + obj->_bufferFrame->nb_samples,
        obj->_audioEncoderContext->sample_rate,
        obj->_audioEncoderContext->sample_rate,
        AV_ROUND_UP
    );
    // ???????????????????????????????????????????????????????????????????????????
    // ??????????????????????????????????????????????????????????????????????????????
    assert(resampledFrameSampleCountPerChannel == obj->_bufferFrame->nb_samples);

    // When we pass a frame to the encoder, it may keep a reference to it internally.
    // Make sure we do not overwrite it here
    ret = av_frame_make_writable(obj->_bufferFrameResampled);
    if (ret < 0) {
        return ret;
    }

    // ?????????????????????
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

    ret = _writeFrame(obj, obj->_bufferFrameResampled);
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

    // ??????????????????????????? AVFormatContext
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

    // ??????????????????????????? flags ?????? AVFMT_NOFILE ????????? (?????? 0)
    if ((obj->_outputFormatContext->oformat->flags & AVFMT_NOFILE) != 0) {
        DEBUG_ERROR("specified format has AVFMT_NOFILE flag\n");
        goto err;
    }

    // ????????????????????? (??????????????????????????????)
    if (!_addAudioStream(obj)) {
        DEBUG_ERROR("_addAudioStream() failed\n");
        goto err;
    }

    // ??????????????????????????????????????????????????????
    if (!_openAudio(obj, opt)) {
        DEBUG_ERROR("_openAudio() failed\n");
        goto err;
    }

#if defined(_DEBUG) && 1
    av_dump_format(obj->_outputFormatContext, 0, obj->filenameUtf8, 1);
#endif

    // ??????????????????
    ret = avio_open(&obj->_outputFormatContext->pb, obj->filenameUtf8, AVIO_FLAG_WRITE);
    if (ret < 0) {
        DEBUG_ERROR("avio_open() failed: %s\n", av_err2str(ret));
        goto err;
    }

    // ???????????????
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
        // ?????????????????????????????????
        uint8_t *bufferFrameData = obj->_bufferFrame->data[0];

        // ???????????????????????? buffer frame ??????????????????
        int frameSampleCountPerChannel = min(obj->_bufferFrameSampleCountPerChannel, (sampleCountPerChannel - totalWrittenSampleCountPerChannel));

        // ????????????????????? buffer frame ???
        int frameSampleDataLengthInBytes = frameSampleCountPerChannel * obj->_audioEncoderContext->channels * sampleValueSize;
        memcpy(bufferFrameData, sampleValuePtr, frameSampleDataLengthInBytes);
        sampleValuePtr += frameSampleDataLengthInBytes;

        // ?????? buffer frame ?????????????????????????????????????????????
        obj->_bufferFrame->nb_samples = frameSampleCountPerChannel;

        // ?????? buffer frame
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

    // ??????????????????
    if ((obj->_outputFormatContext != NULL) && obj->_headerWritten) {
        int ret = av_write_trailer(obj->_outputFormatContext);
        if (ret < 0) {
            DEBUG_ERROR("av_write_trailer() failed: %s\n", av_err2str(ret));
        }
    }

    // ?????? AVCodecContext
    if (obj->_audioEncoderContext != NULL) {
        avcodec_free_context(&obj->_audioEncoderContext);
    }

    // ???????????? frame
    if (obj->_bufferFrameResampled != NULL) {
        av_frame_free(&obj->_bufferFrameResampled);
    }
    if (obj->_bufferFrame != NULL) {
        av_frame_free(&obj->_bufferFrame);
    }

    // ?????? libswresample ??? context
    if (obj->_resamplerContext != NULL) {
        swr_free(&obj->_resamplerContext);
    }

    // ?????? AVFormatContext
    if (obj->_outputFormatContext != NULL) {
        // ??????????????????
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
