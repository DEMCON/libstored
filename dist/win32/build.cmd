@echo off

rem Usage: build.cmd [CMAKE_BUILD_TYPE <other args for cmake>...]


set here=%~dp0
pushd %here%\..
call scripts\env.cmd
if errorlevel 1 goto silent_error

git submodule update --init --recursive
if errorlevel 1 goto error

if exist build goto have_build
mkdir build
if errorlevel 1 goto error

set CMAKE_BUILD_TYPE=%1
if "%CMAKE_BUILD_TYPE%." == "." set CMAKE_BUILD_TYPE=Debug

pushd build
rem Build with explicit build type
shift
cmake -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% "-GNinja" -DCMAKE_INSTALL_PREFIX=dist .. %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 1 goto error_popd
popd
goto build

:have_build
set CMAKE_BUILD_TYPE=%1
if "%CMAKE_BUILD_TYPE%." == "." goto build

pushd build
rem Override previous build type
shift
cmake -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% .. %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 1 goto error_popd
popd

:build
pushd build
cmake --build . --target install -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto error_popd
popd

:done
popd
exit /b 0

:error_popd
popd
:error
echo.
echo Error occurred, stopping
echo.
:silent_error
popd
exit /b 1

