@echo ========================= 2stems-16kHz =========================
mkdir 2stems-16kHz
xcopy /i 2stems\variables 2stems-16kHz\variables
copy /b model_files-16kHz\2stems-16kHz-saved_model.pb 2stems-16kHz\saved_model.pb
@echo.

@echo ========================= 4stems-16kHz =========================
mkdir 4stems-16kHz
xcopy /i 4stems\variables 4stems-16kHz\variables
copy /b model_files-16kHz\4stems-16kHz-saved_model.pb 4stems-16kHz\saved_model.pb
@echo.

@echo ========================= 5stems-16kHz =========================
mkdir 5stems-16kHz
xcopy /i 5stems\variables 5stems-16kHz\variables
copy /b model_files-16kHz\5stems-16kHz-saved_model.pb 5stems-16kHz\saved_model.pb
@echo.

@echo Completed.
@echo.
