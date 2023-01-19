@echo off

rem libstored, distributed debuggable data stores.
rem Copyright (C) 2020-2022  Jochem Rutgers
rem
rem This Source Code Form is subject to the terms of the Mozilla Public
rem License, v. 2.0. If a copy of the MPL was not distributed with this
rem file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
:have_choco

if not "%CI%"=="" goto skip_python
rem heatshrink2 is not released (yet) for 3.11 and later on pypi. Use 3.10 for now. Should be fixed.
choco install -y --no-progress python3 --version 3.10.8
if errorlevel 1 goto error
:skip_python

set pkg=git cmake ninja pkgconfiglite mingw doxygen.install plantuml
rem Ignore "restart required" (3010)
choco install -y --no-progress --ignore-package-exit-codes=3010 %pkg%
if errorlevel 1 goto error

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
