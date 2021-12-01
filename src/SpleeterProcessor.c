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

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "tensorflow/c/c_api.h"
#include "Common.h"
#include "AudioFileCommon.h"
#include "AudioFileWriter.h"
#include "SpleeterProcessor.h"

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

static const int SLICE_LENGTH = SPLEETER_MODEL_AUDIO_SAMPLE_RATE * 30;

static const int EXTEND_LENGTH = SPLEETER_MODEL_AUDIO_SAMPLE_RATE * 5;

static const int LAST_SEGMENT_MIN_LENGTH = SPLEETER_MODEL_AUDIO_SAMPLE_RATE * 10;

static AudioDataSource *_createAudioDataSource(AudioSampleValue_t *sampleValues, int sampleCountPerChannel) {
    AudioDataSource *audioDataSource = AudioDataSource_create();

    audioDataSource->filenameUtf8 = NULL;
    audioDataSource->sampleRate = SPLEETER_MODEL_AUDIO_SAMPLE_RATE;
    audioDataSource->sampleValues = sampleValues;
    audioDataSource->sampleCountPerChannel = sampleCountPerChannel;
    audioDataSource->channelCount = SPLEETER_MODEL_AUDIO_CHANNEL_COUNT;

    return audioDataSource;
}

static void _noOpDeallocator(void *data, size_t a, void *b) {
}

static const char *_getModelPath(const TCHAR *modelName) {
    TCHAR modelPathBuffer[FILE_PATH_MAX_SIZE] = { 0 };

#if defined(_DEBUG) && 0
    _sntprintf(modelPathBuffer, FILE_PATH_MAX_SIZE, _T("%s%s"),
            _T("D:\\Projects\\SpleeterMsvcExe\\spleeter\\models\\"), modelName);
#else
    TCHAR programFolderPathBuffer[FILE_PATH_MAX_SIZE] = { 0 };

    // 获取程序可执行文件的完整路径
    if (GetModuleFileName(NULL, programFolderPathBuffer, FILE_PATH_MAX_SIZE) == 0) {
        DEBUG_ERROR("GetModuleFileName() failed\n");
        return NULL;
    }
    programFolderPathBuffer[FILE_PATH_MAX_SIZE - 1] = _T('\0');     // 确保安全

    // 去除文件名部分，留下目录部分的完整路径
    if (!PathRemoveFileSpec(programFolderPathBuffer)) {
        DEBUG_ERROR("PathRemoveFileSpec() failed\n");
        return NULL;
    }
    programFolderPathBuffer[FILE_PATH_MAX_SIZE - 1] = _T('\0');

    _sntprintf(modelPathBuffer, FILE_PATH_MAX_SIZE, _T("%s%s%s"),
            programFolderPathBuffer, _T("\\models\\"), modelName);
#endif

    modelPathBuffer[FILE_PATH_MAX_SIZE - 1] = _T('\0');

    if (!PathFileExists(modelPathBuffer)) {
        DEBUG_ERROR("Target model directory does not exist\n");
        return NULL;
    }

    const char *modelPath = AudioFileCommon_getUtf8StringFromUnicodeString(modelPathBuffer);

    return modelPath;
}

static const SpleeterModelInfo _modelInfoList[] = {
    {
        .name           = _T("2stems"),
        .outputCount    = 2,
        .outputNames    = { "output_vocals", "output_accompaniment" },
        .trackNames     = { _T("vocals"), _T("accompaniment") }
    },

    {
        .name           = _T("4stems"),
        .outputCount    = 4,
        .outputNames    = { "output_vocals", "output_drums", "output_bass", "output_other" },
        .trackNames     = { _T("vocals"), _T("drums"), _T("bass"), _T("other") }
    },

    {
        .name           = _T("5stems"),
        .outputCount    = 5,
        .outputNames    = { "output_vocals", "output_drums", "output_bass", "output_piano", "output_other" },
        .trackNames     = { _T("vocals"), _T("drums"), _T("bass"), _T("piano"), _T("other") }
    }
};

const SpleeterModelInfo *SpleeterProcessor_getModelInfo(const TCHAR *modelName) {
    for (int i = 0; i < (sizeof(_modelInfoList) / sizeof(_modelInfoList[0])); i++) {
        const SpleeterModelInfo *modelInfo = &_modelInfoList[i];

        if (_tcsstr(modelName, modelInfo->name) != NULL) {
            return modelInfo;
        }
    }

    return NULL;
}

int SpleeterProcessor_split(const TCHAR *modelName, AudioDataSource *audioDataSource, SpleeterProcessorResult **resultOut) {
    SpleeterProcessorResult *result = NULL;

    SpleeterModelAudioSampleValue_t *outputSampleValuesBufferList[SPLEETER_MODEL_MAX_OUTPUT_COUNT] = { 0 };

    TF_Status *status = NULL;
    TF_Graph *graph = NULL;
    TF_SessionOptions *sessionOptions = NULL;
    TF_Buffer *runOptions = NULL;
    TF_Buffer *metaGraphDef = NULL;
    TF_Session *session = NULL;

    //////////////////////////////// Check Input ////////////////////////////////

    const SpleeterModelInfo *modelInfo = SpleeterProcessor_getModelInfo(modelName);
    if (modelInfo == NULL) {
        DEBUG_ERROR("SpleeterProcessor_getModelInfo() failed\n");
        goto clean_up;
    }

    if (audioDataSource->channelCount != SPLEETER_MODEL_AUDIO_CHANNEL_COUNT) {
        DEBUG_ERROR("wrong channel count: %d\n", audioDataSource->channelCount);
        goto clean_up;
    }

    if (audioDataSource->sampleRate != SPLEETER_MODEL_AUDIO_SAMPLE_RATE) {
        DEBUG_ERROR("wrong sample rate: %d\n", audioDataSource->sampleRate);
        goto clean_up;
    }

    if (audioDataSource->sampleValues == NULL) {
        DEBUG_ERROR("audioDataSource->sampleValues is NULL\n");
        goto clean_up;
    }

    if (audioDataSource->sampleCountPerChannel <= 0) {
        DEBUG_ERROR("audioDataSource->sampleCountPerChannel is less than or equal to 0\n");
        goto clean_up;
    }

    //////////////////////////////// Prepare Input ////////////////////////////////

    SpleeterModelAudioSampleValue_t *intputSampleValuesInterlaced = audioDataSource->sampleValues;
    int inputSampleCountPerChannel = audioDataSource->sampleCountPerChannel;

    const char *modelPath = _getModelPath(modelInfo->name);
    if (modelPath == NULL) {
        DEBUG_ERROR("_getModelPath() failed\n");
        goto clean_up;
    }

    //////////////////////////////// Allocate Buffers ////////////////////////////////

    for (int i = 0; i < modelInfo->outputCount; i++) {
        outputSampleValuesBufferList[i] = MEMORY_ALLOC_ARRAY(SpleeterModelAudioSampleValue_t,
                (inputSampleCountPerChannel * SPLEETER_MODEL_AUDIO_CHANNEL_COUNT));
    }

    //////////////////////////////// Load Session ////////////////////////////////

    Common_updateProgress(STAGE_SPLEETER_PROCESSOR_LOAD_MODEL, 0, 1);

    TCHAR *tfCppMinLogLevelReadback_1 = _tgetenv(_T("TF_CPP_MIN_LOG_LEVEL"));
    DEBUG_INFO("now TF_CPP_MIN_LOG_LEVEL = %S\n", tfCppMinLogLevelReadback_1);

    /*
     * TF_CPP_MIN_LOG_LEVEL:
     * 0: all messages are logged (default behavior)
     * 1: INFO messages are not printed
     * 2: INFO and WARNING messages are not printed
     * 3: INFO, WARNING, and ERROR messages are not printed
     */
    const TCHAR *tfCppMinLogLevel;
    if (g_verboseMode) {
        tfCppMinLogLevel = _T("0");     // show all messages
    } else {
        tfCppMinLogLevel = _T("1");     // show warning and error messages
    }

    DEBUG_INFO("set TF_CPP_MIN_LOG_LEVEL = %S\n", tfCppMinLogLevel);
    _tputenv_s(_T("TF_CPP_MIN_LOG_LEVEL"), tfCppMinLogLevel);

    TCHAR *tfCppMinLogLevelReadback_2 = _tgetenv(_T("TF_CPP_MIN_LOG_LEVEL"));
    DEBUG_INFO("now TF_CPP_MIN_LOG_LEVEL = %S\n", tfCppMinLogLevelReadback_2);

    // Notice: the above log level setting only takes effect in release version of this program

    DEBUG_INFO("TensorFlow C library version %s\n", TF_Version());

    status = TF_NewStatus();

    graph = TF_NewGraph();
    sessionOptions = TF_NewSessionOptions();

    runOptions = TF_NewBuffer();
    metaGraphDef = TF_NewBuffer();

    const char *tags[] = { "serve" };

    session = TF_LoadSessionFromSavedModel(
        sessionOptions,     // session_options
        runOptions,         // run_options
        modelPath,          // export_dir
        tags, 1,            // tags, tags_len
        graph,              // graph
        metaGraphDef,       // meta_graph_def
        status              // status
    );

    if (TF_GetCode(status) != TF_OK) {
        DEBUG_ERROR("TF_LoadSessionFromSavedModel() failed: %s\n", TF_Message(status));
        goto clean_up;
    }

    Common_updateProgress(STAGE_SPLEETER_PROCESSOR_LOAD_MODEL, 1, 1);

    //////////////////////////////// Segmented Processing ////////////////////////////////

    int segmentCount = (inputSampleCountPerChannel + (SLICE_LENGTH - 1)) / SLICE_LENGTH;

    if (segmentCount >= 2) {
        int lastSegmentLength = inputSampleCountPerChannel - (SLICE_LENGTH * (segmentCount - 1));

        if (lastSegmentLength < LAST_SEGMENT_MIN_LENGTH) {
            segmentCount--;
        }
    }

    for (int segmentIndex = 0; segmentIndex < segmentCount; segmentIndex++) {
        int currentOffset = SLICE_LENGTH * segmentIndex;

        int extendLengthAtBegin = (segmentIndex == 0) ? 0 : EXTEND_LENGTH;
        int extendLengthAtEnd = (segmentIndex == (segmentCount - 1)) ? 0 : EXTEND_LENGTH;

        int regionUseLength = (segmentIndex == (segmentCount - 1)) ? (inputSampleCountPerChannel - currentOffset) : SLICE_LENGTH;
        int regionUseStart = extendLengthAtBegin;

        int regionWaveformOffset = currentOffset - extendLengthAtBegin;
        int regionWaveformLength = min((extendLengthAtBegin + regionUseLength + extendLengthAtEnd), (inputSampleCountPerChannel - regionWaveformOffset));

#if defined(_DEBUG) && 0
        DEBUG_INFO("Region: %3d, %3d; %3d, %3d\n",
                (regionWaveformOffset / SPLEETER_MODEL_AUDIO_SAMPLE_RATE),
                (regionWaveformLength / SPLEETER_MODEL_AUDIO_SAMPLE_RATE),
                (regionUseStart / SPLEETER_MODEL_AUDIO_SAMPLE_RATE),
                (regionUseLength / SPLEETER_MODEL_AUDIO_SAMPLE_RATE));
#endif

        //////////////////////////////// Input ////////////////////////////////

        TF_Output inputs[1] = { 0 };

        inputs[0].oper = TF_GraphOperationByName(graph, "input_waveform");
        inputs[0].index = 0;
        if (inputs[0].oper == NULL) {
            DEBUG_ERROR("TF_GraphOperationByName() failed\n");
            fprintf(stderr, "Error: Cannot find input tensor by name \"%s\".\n", "input_waveform");
            goto clean_up;
        }

        int64_t inputDims[] = { regionWaveformLength, SPLEETER_MODEL_AUDIO_CHANNEL_COUNT };

        void *inputData = intputSampleValuesInterlaced + (regionWaveformOffset * SPLEETER_MODEL_AUDIO_CHANNEL_COUNT);
        size_t inputDataLength = inputDims[0] * inputDims[1] * sizeof(SpleeterModelAudioSampleValue_t);

        TF_Tensor *inputTensors[1] = { NULL };
        inputTensors[0] = TF_NewTensor(
            TF_FLOAT,                       // data_type
            inputDims, 2,                   // dims, num_dims
            inputData, inputDataLength,     // data, len
            &_noOpDeallocator, NULL         // deallocator, deallocator_arg
        );

        //////////////////////////////// Output ////////////////////////////////

        TF_Output outputs[SPLEETER_MODEL_MAX_OUTPUT_COUNT] = { 0 };

        for (int i = 0; i < modelInfo->outputCount; i++) {
            outputs[i].oper = TF_GraphOperationByName(graph, modelInfo->outputNames[i]);
            outputs[i].index = 0;
            if (outputs[i].oper == NULL) {
                DEBUG_ERROR("TF_GraphOperationByName() failed\n");
                fprintf(stderr, "Error: Cannot find output tensor by name \"%s\".\n", modelInfo->outputNames[i]);
                goto clean_up;
            }
        }

        TF_Tensor *outputTensors[SPLEETER_MODEL_MAX_OUTPUT_COUNT] = { 0 };
        for (int i = 0; i < modelInfo->outputCount; i++) {
            outputTensors[i] = NULL;
        }

        //////////////////////////////// Run Session ////////////////////////////////

        TF_SessionRun(
            session,            // session
            NULL,               // run_options
            inputs, inputTensors, 1,                            // inputs, input_values, ninputs
            outputs, outputTensors, modelInfo->outputCount,     // outputs, output_values, noutputs
            NULL, 0,            // target_opers, ntargets
            NULL,               // run_metadata
            status              // output_status
        );

        if (TF_GetCode(status) != TF_OK) {
            DEBUG_ERROR("TF_SessionRun() failed: %s\n", TF_Message(status));
            goto clean_up;
        }

        //////////////////////////////// Process Result ////////////////////////////////

        for (int i = 0; i < modelInfo->outputCount; i++) {
            SpleeterModelAudioSampleValue_t *outputSampleValues = (SpleeterModelAudioSampleValue_t *)TF_TensorData(outputTensors[i]);
            memcpy((outputSampleValuesBufferList[i] + (currentOffset * SPLEETER_MODEL_AUDIO_CHANNEL_COUNT)), (outputSampleValues + (regionUseStart * SPLEETER_MODEL_AUDIO_CHANNEL_COUNT)),
                    (regionUseLength * SPLEETER_MODEL_AUDIO_CHANNEL_COUNT * sizeof(SpleeterModelAudioSampleValue_t)));
            TF_DeleteTensor(outputTensors[i]);
        }

        Common_updateProgress(STAGE_SPLEETER_PROCESSOR_PROCESS_SEGMENT, (currentOffset + regionUseLength), inputSampleCountPerChannel);
    }

    //////////////////////////////// Create Result ////////////////////////////////

    result = MEMORY_ALLOC_STRUCT(SpleeterProcessorResult);

    for (int i = 0; i < modelInfo->outputCount; i++) {
        result->trackList[i].trackName = _tcsdup(modelInfo->trackNames[i]);
        result->trackList[i].audioDataSource = _createAudioDataSource(outputSampleValuesBufferList[i], inputSampleCountPerChannel);
    }

    result->trackCount = modelInfo->outputCount;

    //////////////////////////////// Clean Up ////////////////////////////////

clean_up:

    if (session != NULL) {
        TF_CloseSession(session, status);
        if (TF_GetCode(status) != TF_OK) {
            DEBUG_ERROR("TF_CloseSession() failed: %s\n", TF_Message(status));
        }

        TF_DeleteSession(session, status);
        if (TF_GetCode(status) != TF_OK) {
            DEBUG_ERROR("TF_DeleteSession() failed: %s\n", TF_Message(status));
        }
    }

    if (sessionOptions != NULL) {
        TF_DeleteSessionOptions(sessionOptions);
    }
    if (graph != NULL) {
        TF_DeleteGraph(graph);
    }

    if (runOptions != NULL) {
        TF_DeleteBuffer(runOptions);
    }
    if (metaGraphDef != NULL) {
        TF_DeleteBuffer(metaGraphDef);
    }

    if (status != NULL) {
        TF_DeleteStatus(status);
    }

    if (result == NULL) {
        // 有错误产生
        return -1;
    }

    *resultOut = result;
    return 0;
}

void SpleeterProcessorResult_free(SpleeterProcessorResult **objPtr) {
    if (objPtr == NULL) {
        return;
    }

    SpleeterProcessorResult *obj = *objPtr;

    for (int i = 0; i < obj->trackCount; i++) {
        Memory_free(&obj->trackList[i].trackName);

        AudioDataSource_free(&obj->trackList[i].audioDataSource);
    }

    obj->trackCount = 0;

    Memory_free(objPtr);
}
