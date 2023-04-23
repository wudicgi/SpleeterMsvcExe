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

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "Common.h"

static const double _MIN_CHANGE = 1.0;

static void _processProgress(double basePercentage, double currentStagePercentage, int stageProgress, int stageTotal,
        const char *printProgressFormat) {
    static double lastProgress = 0.0;

    double progress = basePercentage + ((double)stageProgress / (double)stageTotal) * currentStagePercentage;

    if ((stageProgress == 0)
            || (stageProgress == stageTotal)
            || (fabs(progress - lastProgress) >= _MIN_CHANGE)) {
        printf("[%6.2f%%] ", progress);

        /*
        // 调试时使用
        time_t rawtime;         time(&rawtime);
        struct tm *timeinfo;    timeinfo = localtime(&rawtime);
        char timebuffer[32];    strftime(timebuffer, sizeof(timebuffer), "%H:%M:%S", timeinfo);
        printf("[%s] ", timebuffer);
        */

        printf(printProgressFormat, stageProgress, stageTotal);
        printf("\n");
        fflush(stdout);

        lastProgress = progress;
    }
}

#define PROCESS_PROGRESS(basePercentage, currentStagePercentage, printProgressFormat)       do {                    \
    _processProgress(basePercentage, currentStagePercentage, stageProgress, stageTotal, printProgressFormat);       \
} while (0)

void Common_updateProgress(Stage stage, int stageProgress, int stageTotal) {
    switch (stage) {
        case STAGE_AUDIO_FILE_READER:
            PROCESS_PROGRESS(0.0,   2.0,    "Reading audio samples, %d/%d");
            break;

        case STAGE_SPLEETER_PROCESSOR_LOAD_MODEL:
            PROCESS_PROGRESS(2.0,   3.0,    "Loading spleeter model, %d/%d");
            break;

        case STAGE_SPLEETER_PROCESSOR_PROCESS_SEGMENT:
            PROCESS_PROGRESS(5.0,   85.0,   "Processing segment, %d/%d");
            break;

        case STAGE_AUDIO_FILE_WRITER:
            PROCESS_PROGRESS(90.0,  10.0,   "Writing output file, %d/%d");
            break;

        default:
            break;
    }
}
