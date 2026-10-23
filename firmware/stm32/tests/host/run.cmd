@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
  echo Run from an MSVC Developer Command Prompt.
  exit /b 1
)
set "STMROOT=%~dp0..\.."
set "CHROOT=%~1"
set "APPROOT=%~2"
if not defined CHROOT set "CHROOT=%STMROOT%\..\ch582m"
if not defined APPROOT set "APPROOT=%STMROOT%\..\..\app\flutter_app"
set "OUT=%TEMP%\smartwatch-host-regression"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
cl /nologo /utf-8 /std:c11 /I"%STMROOT%\tests\host\stubs" /I"%STMROOT%\Core\Inc" /I"%STMROOT%\AI" "%STMROOT%\tests\host\firmware_regression.c" /Fefirmware_regression.exe
if errorlevel 1 goto failed
firmware_regression.exe "%APPROOT%\test\fixtures\firmware_backup.bin" "%APPROOT%\test\fixtures\dart_restore.bin"
if errorlevel 1 goto failed
cl /nologo /utf-8 /std:c11 /I"%CHROOT%\src\app_drv_fifo" "%STMROOT%\tests\host\ch582_fifo_regression.c" /Fech582_fifo_regression.exe
if errorlevel 1 goto failed
ch582_fifo_regression.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
