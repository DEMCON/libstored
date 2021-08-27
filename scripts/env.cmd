@echo off
rem Call this script to prepare the runtime environment on Windows for building the project.

set here=%~dp0
pushd %here%\..

call refreshenv

echo Looking for cmake...
where /q cmake > NUL 2> NUL
if errorlevel 1 goto find_cmake
goto show_cmake
:find_cmake
set PATH=C:\Program Files\CMake\bin;%PATH%
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
popd
exit /b 0

:need_bootstrap
echo Missing dependencies. Run bootstrap first.

:error
echo.
echo Error occurred, stopping
echo.
popd
exit /b 1

