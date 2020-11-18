@echo off

rem Run this script (without arguments) to build and test libstored
rem using different sets of configuration options.

set here=%~dp0
pushd %here%\..

if "%1." == "." goto run_all
rem Run the given configuration
call scripts\build.cmd %*
if errorlevel 1 goto error
set CTEST_OUTPUT_ON_FAILURE=1
make -C build test
if errorlevel 1 goto error
goto done

:run_all
call tests\test_config.cmd Debug "-DLIBSTORED_HAVE_LIBZMQ=ON" "-DLIBSTORED_HAVE_HEATSHRINK=ON" "-DCMAKE_CXX_STANDARD=14"
if errorlevel 1 goto error
call tests\test_config.cmd Debug "-DLIBSTORED_HAVE_LIBZMQ=OFF" "-DLIBSTORED_HAVE_HEATSHRINK=OFF" "-DCMAKE_CXX_STANDARD=11"
if errorlevel 1 goto error
call tests\test_config.cmd Release "-DLIBSTORED_HAVE_LIBZMQ=ON" "-DLIBSTORED_HAVE_HEATSHRINK=ON" "-DCMAKE_CXX_STANDARD=14"
if errorlevel 1 goto error
call tests\test_config.cmd Release "-DLIBSTORED_HAVE_LIBZMQ=OFF" "-DLIBSTORED_HAVE_HEATSHRINK=OFF" "-DCMAKE_CXX_STANDARD=11"
if errorlevel 1 goto error

:done
popd
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
popd
exit /b 1

