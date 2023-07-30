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
    AudioDataSource *audioDataSource = AudioDataSource_alloc();

    audioDataSource->filenameUtf8 = NULL;
    audioDataSource->sampleRate = SPLEETER_MODEL_AUDIO_SAMPLE_RATE;
    audioDataSource->sampleValues = sampleValues;
    audioDataSource->sampleCountPerChannel = sampleCountPerChannel;
    audioDataSource->channelCount = SPLEETER_MODEL_AUDIO_CHANNEL_COUNT;

    return audioDataSource;
}

static void _noOpDeallocator(void *data, size_t a, void *b) {
}

static bool _getModelFolderPath(const TCHAR *modelName, char **out_modelFolderPath_utf8, char **out_savedModelFileName) {
    // 获取程序可执行文件的完整路径
    TCHAR programFolderPath[FILE_PATH_MAX_SIZE] = { 0 };
    if (GetModuleFileName(NULL, programFolderPath, FILE_PATH_MAX_SIZE) == 0) {
        DEBUG_ERROR("GetModuleFileName() failed\n");
        return false;
    }
    programFolderPath[FILE_PATH_MAX_SIZE - 1] = _T('\0');       // 确保安全

    // 去除文件名部分，留下目录部分的完整路径
    if (!PathRemoveFileSpec(programFolderPath)) {
        DEBUG_ERROR("PathRemoveFileSpec() failed\n");
        return false;
    }
    programFolderPath[FILE_PATH_MAX_SIZE - 1] = _T('\0');

    TCHAR modelsFolderPath[FILE_PATH_MAX_SIZE] = { 0 };
    if (PathCombine(modelsFolderPath, programFolderPath, _T("models")) == NULL) {
        DEBUG_ERROR("PathCombine() failed\n");
        return false;
    }
    if (!PathFileExists(modelsFolderPath)) {
        DEBUG_ERROR("Folder \"" TSTRING_FORMAT_SPECIFIER "\" does not exist\n", modelsFolderPath);
        return false;
    }

    TCHAR modelFolderPath[FILE_PATH_MAX_SIZE] = { 0 };
    if (PathCombine(modelFolderPath, modelsFolderPath, modelName) == NULL) {
        DEBUG_ERROR("PathCombine() failed\n");
        return false;
    }

    if (!PathFileExists(modelFolderPath)) {
        TCHAR modelName_dup[FILE_PATH_MAX_SIZE] = { _T('\0') };
        _tcsncpy(modelName_dup, modelName, (FILE_PATH_MAX_SIZE - 1));
        modelName_dup[FILE_PATH_MAX_SIZE - 1] = _T('\0');

        TCHAR *charDash = _tcschr(modelName_dup, _T('-'));
        if (charDash != NULL) {
            *charDash = _T('\0');

            const TCHAR *modelFolderName = (const TCHAR *)modelName_dup;
            const TCHAR *variantSuffix = (const TCHAR *)(charDash + 1);

            memset(modelFolderPath, 0, sizeof(modelFolderPath));
            if (PathCombine(modelFolderPath, modelsFolderPath, modelFolderName) == NULL) {
                DEBUG_ERROR("PathCombine() failed\n");
                return false;
            }

            if (PathFileExists(modelFolderPath)) {
                TCHAR savedModelFileName[FILE_PATH_MAX_SIZE] = { 0 };
                _sntprintf(savedModelFileName, FILE_PATH_MAX_SIZE, _T("saved_model-%s.pb"), variantSuffix);
                savedModelFileName[FILE_PATH_MAX_SIZE - 1] = _T('\0');

                TCHAR savedModelFilePath[FILE_PATH_MAX_SIZE] = { 0 };
                if (PathCombine(savedModelFilePath, modelFolderPath, savedModelFileName) == NULL) {
                    DEBUG_ERROR("PathCombine() failed\n");
                    return false;
                }
                if (PathFileExists(savedModelFilePath)) {
                    *out_modelFolderPath_utf8 = AudioFileCommon_getUtf8StringFromUnicodeString(modelFolderPath);
                    *out_savedModelFileName = AudioFileCommon_getUtf8StringFromUnicodeString(savedModelFileName);

                    return true;
                } else {
                    DEBUG_ERROR("SavedModel .pb file \"" TSTRING_FORMAT_SPECIFIER "\" does not exist\n", savedModelFilePath);
                }
            } else {
                DEBUG_ERROR("Folder \"" TSTRING_FORMAT_SPECIFIER "\" does not exist\n", modelFolderPath);
            }
        } else {
            DEBUG_ERROR("Folder \"" TSTRING_FORMAT_SPECIFIER "\" does not exist\n", modelFolderPath);
        }

        return false;
    }

    *out_modelFolderPath_utf8 = AudioFileCommon_getUtf8StringFromUnicodeString(modelFolderPath);
    *out_savedModelFileName = NULL;

    return true;
}

static const SpleeterModelInfo _modelInfoList[] = {
    {
        .basicName      = _T("2stems"),
        .outputCount    = 2,
        .outputNames    = { "output_vocals", "output_accompaniment" },
        .trackNames     = { _T("vocals"), _T("accompaniment") }
    },

    {
        .basicName      = _T("4stems"),
        .outputCount    = 4,
        .outputNames    = { "output_vocals", "output_drums", "output_bass", "output_other" },
        .trackNames     = { _T("vocals"), _T("drums"), _T("bass"), _T("other") }
    },

    {
        .basicName      = _T("5stems"),
        .outputCount    = 5,
        .outputNames    = { "output_vocals", "output_drums", "output_bass", "output_piano", "output_other" },
        .trackNames     = { _T("vocals"), _T("drums"), _T("bass"), _T("piano"), _T("other") }
    }
};

const SpleeterModelInfo *SpleeterProcessor_getModelInfo(const TCHAR *modelName) {
    for (int i = 0; i < (sizeof(_modelInfoList) / sizeof(_modelInfoList[0])); i++) {
        const SpleeterModelInfo *modelInfo = &_modelInfoList[i];

        if (_tcsstr(modelName, modelInfo->basicName) != NULL) {
            return modelInfo;
        }
    }

    return NULL;
}

int SpleeterProcessor_split(const TCHAR *modelName, AudioDataSource *audioDataSource, SpleeterProcessorResult **resultOut) {
    SpleeterProcessorResult *result = NULL;

    char *modelFolderPath_utf8 = NULL;
    char *savedModelFileName_utf8 = NULL;

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

    if (!_getModelFolderPath(modelName, &modelFolderPath_utf8, &savedModelFileName_utf8)) {
        DEBUG_ERROR("_getModelFolderPath() failed\n");
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

    //////////////////////////////// Allocate Buffers ////////////////////////////////

    for (int i = 0; i < modelInfo->outputCount; i++) {
        outputSampleValuesBufferList[i] = MEMORY_ALLOC_ARRAY(SpleeterModelAudioSampleValue_t,
                (inputSampleCountPerChannel * SPLEETER_MODEL_AUDIO_CHANNEL_COUNT));
    }

    //////////////////////////////// Load Session ////////////////////////////////

    Common_updateProgress(STAGE_SPLEETER_PROCESSOR_LOAD_MODEL, 0, 1);

    /*
     * TF_CPP_MIN_LOG_LEVEL:
     * 0: all messages are logged (default behavior)
     * 1: INFO messages are not printed
     * 2: INFO and WARNING messages are not printed
     * 3: INFO, WARNING, and ERROR messages are not printed
     */
    const char *tfCppMinLogLevel;
    if (g_verboseMode) {
        tfCppMinLogLevel = "0";     // show all messages
    } else {
        tfCppMinLogLevel = "1";     // show warning and error messages
    }

    // Notice: Due to the behaviour of MSVC CRT, the getenv() call in tensorflow.dll cannot get the latest environment variable value in debug version
    // which uses /MDd compile parameter. So when using debug version, the above log level setting may not take effect.
    // But the release version which uses /MD compile parameter does not have this problem.
    DEBUG_INFO("set TF_CPP_MIN_LOG_LEVEL = %s\n", tfCppMinLogLevel);
    _putenv_s("TF_CPP_MIN_LOG_LEVEL", tfCppMinLogLevel);
    DEBUG_INFO("now TF_CPP_MIN_LOG_LEVEL = %s\n", getenv("TF_CPP_MIN_LOG_LEVEL"));

    if (savedModelFileName_utf8 != NULL) {
        // Our custom added code in tensorflow library uses GetEnvironmentVariableA() to get the value of this environment variable,
        // so there is no problem like the above TF_CPP_MIN_LOG_LEVEL.
        DEBUG_INFO("set TF_CPP_SAVED_MODEL_FILENAME_PB = %s\n", savedModelFileName_utf8);
        _putenv_s("TF_CPP_SAVED_MODEL_FILENAME_PB", savedModelFileName_utf8);   // "saved_model-测试.pb"
        DEBUG_INFO("now TF_CPP_SAVED_MODEL_FILENAME_PB = %s\n", getenv("TF_CPP_SAVED_MODEL_FILENAME_PB"));
    }

    const char *tensorFlowVersion = TF_Version();
    DEBUG_INFO("TensorFlow C library version %s\n", tensorFlowVersion);

    if ((savedModelFileName_utf8 != NULL)
            && (strstr(tensorFlowVersion, "-mod") == NULL)) {
        DEBUG_ERROR("The TensorFlow library loaded (%s) is not our mod version, not supporting change the filename of saved_model.pb.\n", tensorFlowVersion);
        DEBUG_ERROR("Please execute the batch script \"extract_16kHz_22kHz_models_into_separated_folders.bat\" in \"models\" folder.\n");
        goto clean_up;
    }

    status = TF_NewStatus();

    graph = TF_NewGraph();
    sessionOptions = TF_NewSessionOptions();

    runOptions = TF_NewBuffer();
    metaGraphDef = TF_NewBuffer();

    const char *tags[] = { "serve" };

    session = TF_LoadSessionFromSavedModel(
        sessionOptions,         // session_options
        runOptions,             // run_options
        modelFolderPath_utf8,   // export_dir
        tags, 1,                // tags, tags_len
        graph,                  // graph
        metaGraphDef,           // meta_graph_def
        status                  // status
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

    if (savedModelFileName_utf8 != NULL) {
        Memory_free(&savedModelFileName_utf8);
    }
    if (modelFolderPath_utf8 != NULL) {
        Memory_free(&modelFolderPath_utf8);
    }

    if (result == NULL) {
        // 有错误产生
        return -1;
    }

    *resultOut = result;
    return 0;
}

SpleeterProcessorResultTrack *SpleeterProcessorResult_getTrack(SpleeterProcessorResult *obj, const TCHAR *trackName) {
    for (int i = 0; i < obj->trackCount; i++) {
        SpleeterProcessorResultTrack *track = &obj->trackList[i];

        if (_tcscmp(track->trackName, trackName) == 0) {
            return track;
        }
    }

    return NULL;
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
