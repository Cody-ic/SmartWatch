@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
  echo Run from an MSVC Developer Command Prompt.
  exit /b 1
)
set "STMROOT=%~dp0..\.."
set "OUT=%TEMP%\smartwatch-lsm6dsox-regression"
if not exist "%OUT%" mkdir "%OUT%"
cd /d "%OUT%"
cl /nologo /utf-8 /std:c11 /W4 /I"%STMROOT%\tests\host\lsm6dsox_stubs" /I"%STMROOT%\Core\Inc" "%STMROOT%\tests\host\lsm6dsox_regression.c" "%STMROOT%\Core\Src\LSM6DSOX.c" /Felsm6dsox_regression.exe
if errorlevel 1 exit /b 1
lsm6dsox_regression.exe
exit /b %errorlevel%
