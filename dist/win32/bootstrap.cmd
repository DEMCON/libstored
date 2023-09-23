@echo off

rem SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
rem
rem SPDX-License-Identifier: MPL-2.0

net session > NUL 2> NUL
if errorlevel 1 goto no_admin
goto is_admin
:no_admin
echo Run this script as Administrator.
goto error
:is_admin

where /q choco > NUL 2> NUL
if errorlevel 1 goto no_choco
goto have_choco
:no_choco
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command " [System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
:choco_again
where /q choco > NUL 2> NUL
if errorlevel 1 goto still_no_choco
goto have_choco
:still_no_choco
echo Chocolatey not installed. Install from here: https://chocolatey.org/docs/installation
goto error

rem call :choco_install pkg [binary]
:choco_install
setlocal
if "%2"=="" goto do_choco_install
where /q "%2" > NUL 2>NUL
if errorlevel 1 goto do_choco_install
goto :eof
:do_choco_install
rem Ignore "restart required" (3010)
choco install -y --no-progress --ignore-package-exit-codes=3010 %1
endlocal
goto :eof

:have_choco
if not "%CI%"=="" goto skip_python
call :choco_install python3
if errorlevel 1 goto error
:skip_python
call :choco_install git git
if errorlevel 1 goto error
call :choco_install cmake cmake
if errorlevel 1 goto error
call :choco_install ninja ninja
if errorlevel 1 goto error
call :choco_install mingw g++
if errorlevel 1 goto error
call :choco_install plantuml plantuml
if errorlevel 1 goto error

rem SourceForge is a bit flaky. These packages often fail.
rem Without doxygen, documentation is not available.
rem Without pkgconfiglite, ZeroMQ is probably always built from source.
set optpkg=doxygen.install pkgconfiglite
choco install -y --no-progress --ignore-package-exit-codes=3010 %optpkg%
if errorlevel 1 echo Failed to install less essential packages. Ignored, but you may retry.

where /q libcairo-2.dll > NUL 2> NUL
if errorlevel 1 goto no_cairo
goto have_cairo
:no_cairo
echo To build the documentation, you need the GTK+ runtime. Install it from here:
echo https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer/releases/download/2021-04-29/gtk3-runtime-3.24.29-2021-04-29-ts-win64.exe
rem sleep 5, to show the user this message
ping -n 6 127.0.0.1 > NUL
:have_cairo

call refreshenv

python.exe -m ensurepip
if errorlevel 1 goto error


:done
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
pause
exit /b 1
