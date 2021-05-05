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
#include "Common.h"
#include "Memory.h"
#include "AudioFileReader.h"
#include "AudioFileWriter.h"
#include "AudioFile.h"

AudioDataSource *AudioDataSource_create(void) {
    AudioDataSource *dataSource = MEMORY_ALLOC_STRUCT(AudioDataSource);

    return dataSource;
}

void AudioDataSource_free(AudioDataSource **objPtr) {
    if (objPtr == NULL) {
        return;
    }

    AudioDataSource *obj = *objPtr;

    if (obj->sampleValues != NULL) {
        Memory_free(&obj->sampleValues);
    }

    if (obj->filenameUtf8 != NULL) {
        Memory_free(&obj->filenameUtf8);
    }

    Memory_free(objPtr);
}

AudioDataSource *AudioFile_readAll(const TCHAR *filename, const AudioSampleType *outputSampleType) {
    AudioFileReader *obj = AudioFileReader_open(filename, outputSampleType);
    if (obj == NULL) {
        return NULL;
    }

    int expectingSamplesCountPerChannel = (int)ceil(obj->durationInSeconds * obj->outputSampleType->sampleRate);

    int samplesBufferSize = sizeof(AudioSampleValue_t) * (expectingSamplesCountPerChannel * obj->outputSampleType->channelCount);
    AudioSampleValue_t *samplesBuffer = (AudioSampleValue_t *)Memory_alloc(samplesBufferSize);

    int totalSamplesCountPerChannel = 0;
    int didReadSampleCountPerChannel = 0;
    AudioSampleValue_t *samplesBufferPtr = samplesBuffer;
    do {
        int samplesBufferRemainingSampleCountPerChannel = expectingSamplesCountPerChannel - totalSamplesCountPerChannel;

        didReadSampleCountPerChannel = AudioFileReader_read(obj, samplesBufferPtr, samplesBufferRemainingSampleCountPerChannel);
        if (didReadSampleCountPerChannel < 0) {
            break;
        }

        samplesBufferPtr += didReadSampleCountPerChannel * obj->outputSampleType->channelCount;

        totalSamplesCountPerChannel += didReadSampleCountPerChannel;
        if (totalSamplesCountPerChannel >= expectingSamplesCountPerChannel) {
            break;
        }

        Common_updateProgress(STAGE_AUDIO_FILE_READER, totalSamplesCountPerChannel, expectingSamplesCountPerChannel);
    } while (didReadSampleCountPerChannel >= 0);

    Common_updateProgress(STAGE_AUDIO_FILE_READER, totalSamplesCountPerChannel, totalSamplesCountPerChannel);

    size_t filenameUtf8ActualSizeInBytes = strlen(obj->filenameUtf8) + 1;
    char *filenameUtf8 = (char *)MEMORY_ALLOC_ARRAY(char, filenameUtf8ActualSizeInBytes);
    memcpy(filenameUtf8, obj->filenameUtf8, filenameUtf8ActualSizeInBytes);

    int sampleRate = obj->outputSampleType->sampleRate;
    int channelCount = obj->outputSampleType->channelCount;

    AudioFileReader_close(&obj);

    AudioDataSource *dataSource = AudioDataSource_create();

    dataSource->filenameUtf8 = filenameUtf8;
    dataSource->sampleRate = sampleRate;
    dataSource->sampleValues = samplesBuffer;
    dataSource->sampleCountPerChannel = totalSamplesCountPerChannel;
    dataSource->channelCount = channelCount;

    return dataSource;
}

bool AudioFile_writeAll(const TCHAR *filename, const AudioFileFormat *fileFormat, const AudioSampleType *inputSampleType,
        void *sampleValues, int sampleCountPerChannel) {
    AudioFileWriter *obj = AudioFileWriter_open(filename, fileFormat, inputSampleType);
    if (obj == NULL) {
        return false;
    }

    int writtenSampleCountPerChannel = AudioFileWriter_write(obj, sampleValues, sampleCountPerChannel);

    AudioFileWriter_close(&obj);

    if (writtenSampleCountPerChannel != sampleCountPerChannel) {
        return false;
    }

    return true;
}
