@echo off

:: using xcopy to copy single file
:: https://superuser.com/questions/34303/xcopy-not-surpressing-file-directory-query/540671#540671
:: https://ss64.com/nt/xcopy.html (Copy a single file)

:: using "copy /b" instead of xcopy will display indent spaces

echo ========================= 2stems-16kHz =========================
if exist "2stems" (
    if not exist "2stems-16kHz" (
        mkdir 2stems-16kHz
        xcopy /i 2stems\variables 2stems-16kHz\variables
        xcopy model_files-16kHz\2stems-16kHz-saved_model.pb 2stems-16kHz\saved_model.pb*
    ) else (
        echo Folder 2stems-16kHz is already existed, skip
    )
) else (
    echo Folder 2stems does not exist, skip
)
echo.

echo ========================= 4stems-16kHz =========================
if exist "4stems" (
    if not exist "4stems-16kHz" (
        mkdir 4stems-16kHz
        xcopy /i 4stems\variables 4stems-16kHz\variables
        xcopy model_files-16kHz\4stems-16kHz-saved_model.pb 4stems-16kHz\saved_model.pb*
    ) else (
        echo Folder 4stems-16kHz is already existed, skip
    )
) else (
    echo Folder 4stems does not exist, skip
)
echo.

echo ========================= 5stems-16kHz =========================
if exist "5stems" (
    if not exist "5stems-16kHz" (
        mkdir 5stems-16kHz
        xcopy /i 5stems\variables 5stems-16kHz\variables
        xcopy model_files-16kHz\5stems-16kHz-saved_model.pb 5stems-16kHz\saved_model.pb*
    ) else (
        echo Folder 5stems-16kHz is already existed, skip
    )
) else (
    echo Folder 5stems does not exist, skip
)
echo.

echo Completed.
echo.
