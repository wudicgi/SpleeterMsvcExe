@echo off

echo --------------------------------------------------------------------------------------------------------------------
echo Before run this batch file, you need install spleeter package in Python, 
echo and modify the code in file "<PythonInstalledDir>\Lib\site-packages\spleeter\model\__init__.py"
echo.
echo in _build_manual_output_waveform(self, masked_stft) function, replace the line
echo     output_waveform[instrument] = self._inverse_stft(stft_data)
echo with
echo     output_waveform[instrument] = tf.identity(self._inverse_stft(stft_data), name = 'output_' + str(instrument))
echo --------------------------------------------------------------------------------------------------------------------
echo.

pause
echo.

set DIR_BASE=%~dp0
if "%DIR_BASE:~-1%" == "\" set DIR_BASE=%DIR_BASE:~0,-1%

set DIR_DOWNLOADED_FILES=%DIR_BASE%\01_downloaded_files
set DIR_PRETRAINED_MODELS=%DIR_BASE%\02_pretrained_models
set DIR_EXPORTED_MODELS=%DIR_BASE%\03_exported_models
set DIR_PACKED_MODELS=%DIR_BASE%\04_packed_models
set DIR_RELEASE_MODELS=%DIR_BASE%\05_release_models

echo DIR_BASE               = %DIR_BASE%
echo.
echo DIR_DOWNLOADED_FILES   = %DIR_DOWNLOADED_FILES%
echo DIR_PRETRAINED_MODELS  = %DIR_PRETRAINED_MODELS%
echo DIR_EXPORTED_MODELS    = %DIR_EXPORTED_MODELS%
echo DIR_PACKED_MODELS      = %DIR_PACKED_MODELS%
echo DIR_RELEASE_MODELS     = %DIR_RELEASE_MODELS%
echo.

set ERRORLEVEL=0

set PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python

echo -------------------- Download the official pretrained models --------------------
echo.

echo Create folder %DIR_DOWNLOADED_FILES%...
mkdir "%DIR_DOWNLOADED_FILES%"
echo.

echo ============ Download 2stems.tar.gz ============
echo.
if exist "%DIR_DOWNLOADED_FILES%\2stems.tar.gz" (
    echo File %DIR_DOWNLOADED_FILES%\2stems.tar.gz is already existed. Skip this step.
    echo.
) else (
    echo URL: https://github.com/deezer/spleeter/releases/download/v1.4.0/2stems.tar.gz
    echo.
    set ERRORLEVEL=0
    wget -O "%DIR_DOWNLOADED_FILES%\2stems.tar.gz" "https://github.com/deezer/spleeter/releases/download/v1.4.0/2stems.tar.gz"
    if %ERRORLEVEL% neq 0 goto error_occurred
)

echo ============ Download 4stems.tar.gz ============
echo.
if exist "%DIR_DOWNLOADED_FILES%\4stems.tar.gz" (
    echo File %DIR_DOWNLOADED_FILES%\4stems.tar.gz is already existed. Skip this step.
    echo.
) else (
    echo URL: https://github.com/deezer/spleeter/releases/download/v1.4.0/4stems.tar.gz
    echo.
    set ERRORLEVEL=0
    wget -O "%DIR_DOWNLOADED_FILES%\4stems.tar.gz" "https://github.com/deezer/spleeter/releases/download/v1.4.0/4stems.tar.gz"
    if %ERRORLEVEL% neq 0 goto error_occurred
)

echo ============ Download 5stems.tar.gz ============
echo.
if exist "%DIR_DOWNLOADED_FILES%\5stems.tar.gz" (
    echo File %DIR_DOWNLOADED_FILES%\5stems.tar.gz is already existed. Skip this step.
    echo.
) else (
    echo URL: https://github.com/deezer/spleeter/releases/download/v1.4.0/5stems.tar.gz
    echo.
    set ERRORLEVEL=0
    wget -O "%DIR_DOWNLOADED_FILES%\5stems.tar.gz" "https://github.com/deezer/spleeter/releases/download/v1.4.0/5stems.tar.gz"
    if %ERRORLEVEL% neq 0 goto error_occurred
)

echo -------------------- Extract the downloaded pretrained models --------------------
echo.

echo Create folder %DIR_PRETRAINED_MODELS%...
mkdir "%DIR_PRETRAINED_MODELS%"
echo.

echo ============ Extract 2stems.tar.gz ============
echo.
if exist "%DIR_PRETRAINED_MODELS%\2stems" (
    echo Folder %DIR_PRETRAINED_MODELS%\2stems is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    7za x -so "%DIR_DOWNLOADED_FILES%\2stems.tar.gz" | 7za x -si -ttar "-o%DIR_PRETRAINED_MODELS%\2stems"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Extract 4stems.tar.gz ============
echo.
if exist "%DIR_PRETRAINED_MODELS%\4stems" (
    echo Folder %DIR_PRETRAINED_MODELS%\4stems is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    7za x -so "%DIR_DOWNLOADED_FILES%\4stems.tar.gz" | 7za x -si -ttar "-o%DIR_PRETRAINED_MODELS%\4stems"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Extract 5stems.tar.gz ============
echo.
if exist "%DIR_PRETRAINED_MODELS%\5stems" (
    echo Folder %DIR_PRETRAINED_MODELS%\5stems is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    7za x -so "%DIR_DOWNLOADED_FILES%\5stems.tar.gz" | 7za x -si -ttar "-o%DIR_PRETRAINED_MODELS%\5stems"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo -------------------- Export TensorFlow saved models --------------------
echo.

echo Create folder %DIR_EXPORTED_MODELS%...
mkdir "%DIR_EXPORTED_MODELS%"
echo.

echo ============ Export 11kHz models ============
echo.
if exist "%DIR_EXPORTED_MODELS%\2stems" (
    echo Folder %DIR_EXPORTED_MODELS%\2stems is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    python export_spleeter_models.py "%DIR_PRETRAINED_MODELS%" "%DIR_EXPORTED_MODELS%" "11khz"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Export 16kHz models ============
echo.
if exist "%DIR_EXPORTED_MODELS%\2stems-16khz" (
    echo Folder %DIR_EXPORTED_MODELS%\2stems-16khz is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    python export_spleeter_models.py "%DIR_PRETRAINED_MODELS%" "%DIR_EXPORTED_MODELS%" "16khz"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Export 22kHz models ============
echo.
if exist "%DIR_EXPORTED_MODELS%\2stems-22khz" (
    echo Folder %DIR_EXPORTED_MODELS%\2stems-22khz is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    python export_spleeter_models.py "%DIR_PRETRAINED_MODELS%" "%DIR_EXPORTED_MODELS%" "22khz"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo -------------------- Pack models --------------------
echo.

echo Create folder %DIR_PACKED_MODELS%...
mkdir "%DIR_PACKED_MODELS%"
echo.

echo ============ Copy 11kHz models ============
echo.
xcopy "%DIR_EXPORTED_MODELS%\2stems" "%DIR_PACKED_MODELS%\2stems" /e /i
xcopy "%DIR_EXPORTED_MODELS%\4stems" "%DIR_PACKED_MODELS%\4stems" /e /i
xcopy "%DIR_EXPORTED_MODELS%\5stems" "%DIR_PACKED_MODELS%\5stems" /e /i
echo.

echo ============ Copy .pb file of 16kHz models ============
echo.
xcopy "%DIR_EXPORTED_MODELS%\2stems-16khz\saved_model.pb" "%DIR_PACKED_MODELS%\2stems\saved_model-16khz.pb*"
xcopy "%DIR_EXPORTED_MODELS%\4stems-16khz\saved_model.pb" "%DIR_PACKED_MODELS%\4stems\saved_model-16khz.pb*"
xcopy "%DIR_EXPORTED_MODELS%\5stems-16khz\saved_model.pb" "%DIR_PACKED_MODELS%\5stems\saved_model-16khz.pb*"
echo.

echo ============ Copy .pb file of 22kHz models ============
echo.
xcopy "%DIR_EXPORTED_MODELS%\2stems-22khz\saved_model.pb" "%DIR_PACKED_MODELS%\2stems\saved_model-22khz.pb*"
xcopy "%DIR_EXPORTED_MODELS%\4stems-22khz\saved_model.pb" "%DIR_PACKED_MODELS%\4stems\saved_model-22khz.pb*"
xcopy "%DIR_EXPORTED_MODELS%\5stems-22khz\saved_model.pb" "%DIR_PACKED_MODELS%\5stems\saved_model-22khz.pb*"
echo.

echo ============ Copy batch files ============
echo.
echo Copy extract_16kHz_22kHz_models_into_separated_folders.bat file...
xcopy "%DIR_BASE%\extract_16kHz_22kHz_models_into_separated_folders.bat" "%DIR_PACKED_MODELS%\extract_16kHz_22kHz_models_into_separated_folders.bat*"
echo.

echo ============ Update folder time ============
echo.
echo Update folder time...
FolderTimeUpdate.exe /cfg FolderTimeUpdate_models.cfg /BaseFolder "%DIR_PACKED_MODELS%" /stext ".\FolderTimeUpdate_logs.txt"
echo.

echo ============ Clean up ============
echo.
echo Delete FolderTimeUpdate_logs.txt file...
del FolderTimeUpdate_logs.txt
echo.

echo -------------------- Make release files --------------------
echo.

echo Create folder %DIR_RELEASE_MODELS%...
mkdir "%DIR_RELEASE_MODELS%"
echo.

echo ============ Make models-2stems-only.zip ============
echo.
if exist "%DIR_RELEASE_MODELS%\models-2stems-only.zip" (
    echo File %DIR_RELEASE_MODELS%\models-2stems-only.zip is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    7za a -tzip "%DIR_RELEASE_MODELS%\models-2stems-only.zip" "%DIR_PACKED_MODELS%\2stems" "%DIR_PACKED_MODELS%\generate_16kHz_22kHz.bat*"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Make models-4stems-only.zip ============
echo.
if exist "%DIR_RELEASE_MODELS%\models-4stems-only.zip" (
    echo File %DIR_RELEASE_MODELS%\models-4stems-only.zip is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    7za a -tzip "%DIR_RELEASE_MODELS%\models-4stems-only.zip" "%DIR_PACKED_MODELS%\4stems" "%DIR_PACKED_MODELS%\generate_16kHz_22kHz.bat*"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Make models-all.zip ============
echo.
if exist "%DIR_RELEASE_MODELS%\models-all.zip" (
    echo File %DIR_RELEASE_MODELS%\models-all.zip is already existed. Skip this step.
    echo.
) else (
    set ERRORLEVEL=0
    7za a -tzip "%DIR_RELEASE_MODELS%\models-all.zip" "%DIR_PACKED_MODELS%\2stems" "%DIR_PACKED_MODELS%\4stems" "%DIR_PACKED_MODELS%\5stems" "%DIR_PACKED_MODELS%\generate_16kHz_22kHz.bat*"
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo -------------------- Completed --------------------
echo.

echo ============ Completed ============
echo.
echo Completed.
echo.

pause
exit /b 0

:error_occurred
echo Error, return code = %ERRORLEVEL%
echo.

pause
exit /b 1
