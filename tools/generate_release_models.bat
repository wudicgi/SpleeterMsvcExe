@echo off

set ERRORLEVEL=0

echo ============ Download models.zip ============
echo.
if exist "models.zip" (
    echo File models.zip is already existed
    echo.
) else (
    echo URL: https://github.com/gvne/spleeterpp/releases/download/models-1.0/models.zip
    echo.
    wget -O "models.zip" "https://github.com/gvne/spleeterpp/releases/download/models-1.0/models.zip"
    if %ERRORLEVEL% neq 0 goto error_occurred
)

echo ============ Download models-16KHz.zip ============
echo.
if exist "models-16KHz.zip" (
    echo File models-16KHz.zip is already existed
    echo.
) else (
    echo URL: https://github.com/gvne/spleeterpp/releases/download/models-1.0/models-16KHz.zip
    echo.
    wget -O "models-16KHz.zip" "https://github.com/gvne/spleeterpp/releases/download/models-1.0/models-16KHz.zip"
    if %ERRORLEVEL% neq 0 goto error_occurred
)

echo ============ Extract models.zip ============
echo.
if exist "models" (
    echo Folder models is already existed
    echo.
) else (
    unzip -d models models.zip
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Extract models-16KHz.zip ============
echo.
if exist "models-16KHz" (
    echo Folder models-16KHz is already existed
    echo.
) else (
    unzip -d models-16KHz models-16KHz.zip
    if %ERRORLEVEL% neq 0 goto error_occurred
    echo.
)

echo ============ Generate model_files-16kHz ============
echo.
if exist "models\model_files-16kHz" (
    echo Folder models\model_files-16kHz is already existed
    echo.
) else (
    echo Create folder models\model_files-16kHz...
    mkdir models\model_files-16kHz
    echo.

    echo Copy saved_model.pb files...
    copy /b "models-16KHz\2stems\saved_model.pb" "models\model_files-16kHz\2stems-16kHz-saved_model.pb"
    copy /b "models-16KHz\4stems\saved_model.pb" "models\model_files-16kHz\4stems-16kHz-saved_model.pb"
    copy /b "models-16KHz\5stems\saved_model.pb" "models\model_files-16kHz\5stems-16kHz-saved_model.pb"
    echo.

    echo Copy generate_16kHz.bat file...
    copy /b template_generate_16kHz.bat models\generate_16kHz.bat
    echo.
)

echo ============ Update folder time ============
echo.
echo Update folder time...
FolderTimeUpdate.exe /cfg FolderTimeUpdate_models.cfg /BaseFolder ".\models" /stext ".\FolderTimeUpdate_logs.txt"
echo.

echo ============ Clean up ============
echo.
echo Delete FolderTimeUpdate_logs.txt file...
del FolderTimeUpdate_logs.txt
if exist "models-16KHz" (
    echo Delete files in models-16KHz...
    del models-16KHz\2stems\variables\variables.*
    del models-16KHz\4stems\variables\variables.*
    del models-16KHz\5stems\variables\variables.*
    del models-16KHz\2stems\saved_model.pb
    del models-16KHz\4stems\saved_model.pb
    del models-16KHz\5stems\saved_model.pb

    echo Delete empty folders in models-16KHz...
    rmdir models-16KHz\2stems\variables
    rmdir models-16KHz\4stems\variables
    rmdir models-16KHz\5stems\variables
    rmdir models-16KHz\2stems
    rmdir models-16KHz\4stems
    rmdir models-16KHz\5stems

    echo Delete empty folders models-16KHz...
    rmdir models-16KHz

    echo.
)

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
