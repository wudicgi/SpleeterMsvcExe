@echo off

echo Only for GPU version, which uses the the offical TensorFlow library,
echo this batch script is needed to execute.
echo.

set choice=Y
set /p choice=Are you sure to continue? [Y/n]: 
echo.
if not '%choice%'=='Y' goto cancel

:: using xcopy to copy single file
:: https://superuser.com/questions/34303/xcopy-not-surpressing-file-directory-query/540671#540671
:: https://ss64.com/nt/xcopy.html (Copy a single file)

:: using "copy /b" instead of xcopy will display indent spaces

if exist "2stems" (
    echo ========================= 2stems-16khz =========================
    if not exist "2stems-16khz" (
        mkdir 2stems-16khz
        xcopy 2stems\variables 2stems-16khz\variables /i
        xcopy 2stems\saved_model-16khz.pb 2stems-16khz\saved_model.pb*
    ) else (
        echo Folder 2stems-16khz is already existed, skip
    )
    echo.

    echo ========================= 2stems-22khz =========================
    if not exist "2stems-22khz" (
        mkdir 2stems-22khz
        xcopy 2stems\variables 2stems-22khz\variables /i
        xcopy 2stems\saved_model-22khz.pb 2stems-22khz\saved_model.pb*
    ) else (
        echo Folder 2stems-22khz is already existed, skip
    )
    echo.
) else (
    echo Folder 2stems does not exist, skip
    echo.
)

if exist "4stems" (
    echo ========================= 4stems-16khz =========================
    if not exist "4stems-16khz" (
        mkdir 4stems-16khz
        xcopy 4stems\variables 4stems-16khz\variables /i
        xcopy 4stems\saved_model-16khz.pb 4stems-16khz\saved_model.pb*
    ) else (
        echo Folder 4stems-16khz is already existed, skip
    )
    echo.

    echo ========================= 4stems-22khz =========================
    if not exist "4stems-22khz" (
        mkdir 4stems-22khz
        xcopy 4stems\variables 4stems-22khz\variables /i
        xcopy 4stems\saved_model-22khz.pb 4stems-22khz\saved_model.pb*
    ) else (
        echo Folder 4stems-22khz is already existed, skip
    )
    echo.
) else (
    echo Folder 4stems does not exist, skip
    echo.
)

if exist "5stems" (
    echo ========================= 5stems-16khz =========================
    if not exist "5stems-16khz" (
        mkdir 5stems-16khz
        xcopy 5stems\variables 5stems-16khz\variables /i
        xcopy 5stems\saved_model-16khz.pb 5stems-16khz\saved_model.pb*
    ) else (
        echo Folder 5stems-16khz is already existed, skip
    )
    echo.

    echo ========================= 5stems-22khz =========================
    if not exist "5stems-22khz" (
        mkdir 5stems-22khz
        xcopy 5stems\variables 5stems-22khz\variables /i
        xcopy 5stems\saved_model-22khz.pb 5stems-22khz\saved_model.pb*
    ) else (
        echo Folder 5stems-22khz is already existed, skip
    )
    echo.
) else (
    echo Folder 5stems does not exist, skip
    echo.
)

echo Completed.
echo.

echo Press any key to exit...
pause >nul
exit /b 0

:cancel

echo Operation has been cancelled.
echo.

echo Press any key to exit...
pause >nul
exit /b 1
