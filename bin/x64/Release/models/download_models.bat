@echo off

set ERRORLEVEL=0

echo ============ Download models-all.zip ============
echo.
if exist "models-all.zip" (
    echo File models-all.zip is already existed
    echo.
) else (
    echo URL: https://github.com/wudicgi/SpleeterMsvcExe/releases/download/models-v1.0/models-all.zip
    echo.
    wget -O "models-all.zip" "https://github.com/wudicgi/SpleeterMsvcExe/releases/download/models-v1.0/models-all.zip"
    if %ERRORLEVEL% neq 0 goto error_occurred
)

echo ============ Extract models-all.zip ============
echo.
unzip models-all.zip
if %ERRORLEVEL% neq 0 goto error_occurred
echo.

echo ============ Run generate_16kHz.bat ============
echo.
call generate_16kHz.bat
if %ERRORLEVEL% neq 0 goto error_occurred
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
