@echo off

rem SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
rem
rem SPDX-License-Identifier: MPL-2.0


rem Call this script to prepare the runtime environment on Windows for building the project.

set "here=%~dp0"

set "PATH=%ChocolateyInstall%\lib\mingw\tools\install\mingw64\bin;%ChocolateyInstall%\lib\ninja\tools;%PATH%;%ChocolateyInstall%\bin"

echo Looking for cmake...
where /q cmake > NUL 2> NUL
if errorlevel 1 goto find_cmake
goto show_cmake
:find_cmake
set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
:show_cmake
where cmake 2> NUL | cmd /e /v /q /c"set/p.=&&echo(^!.^!"
if errorlevel 1 goto need_bootstrap
:have_cmake

echo Looking for git...
where git 2> NUL | cmd /e /v /q /c"set/p.=&&echo(^!.^!"
if errorlevel 1 goto need_bootstrap

echo Looking for gcc...
where gcc 2> NUL | cmd /e /v /q /c"set/p.=&&echo(^!.^!"
if errorlevel 1 goto need_bootstrap

echo Looking for ninja...
where ninja 2> NUL | cmd /e /v /q /c"set/p.=&&echo(^!.^!"
if errorlevel 1 goto need_bootstrap

echo Looking for python3...
where python 2> NUL | cmd /e /v /q /c"set/p.=&&echo(^!.^!"
if errorlevel 1 goto need_bootstrap

:done
exit /b 0

:need_bootstrap
echo Missing dependencies. Run bootstrap first.

:error
echo.
echo Error occurred, stopping
echo.
exit /b 1
