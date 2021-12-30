@echo off

rem Run this script (without arguments) to build and test libstored
rem using different sets of configuration options.

set here=%~dp0

:run_all
call :config Debug test
if errorlevel 1 goto silent_error
call :config Debug dev test
if errorlevel 1 goto silent_error
call :config Debug nozmq test
if errorlevel 1 goto silent_error
call :config Debug test C++11
if errorlevel 1 goto silent_error
call :config Debug test C++14
if errorlevel 1 goto silent_error
call :config Release
if errorlevel 1 goto silent_error
call :config Release test
if errorlevel 1 goto silent_error
call :config Release nozmq test
if errorlevel 1 goto silent_error
call :config Release noexamples
if errorlevel 1 goto silent_error
call :config Release test C++11
if errorlevel 1 goto silent_error
call :config Release test C++14
if errorlevel 1 goto silent_error
call :config Debug dev test gcc
if errorlevel 1 goto silent_error
call :config Debug test gcc C++98
if errorlevel 1 goto silent_error
call :config Debug test gcc C++11
if errorlevel 1 goto silent_error
call :config Debug test gcc C++14
if errorlevel 1 goto silent_error
call :config Debug test gcc C++17
if errorlevel 1 goto silent_error
call :config Release test gcc
if errorlevel 1 goto silent_error
call :config Release test gcc C++98
if errorlevel 1 goto silent_error
call :config Release test gcc C++11
if errorlevel 1 goto silent_error

:done
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
exit /b 1

:config
echo.
echo.
echo ============================
echo == Running config: %*
echo.
if not exist "%here%build" goto build
rmdir /s /q "%here%build"
if errorlevel 1 goto error
:build
call "%here%build.cmd" %*
if errorlevel 1 goto silent_error
goto done
