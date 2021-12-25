@echo off

setlocal
set "here=%~dp0"

call "%here%env.cmd"
if errorlevel 1 goto silent_error

echo.

set cmake_opts=
set support_test=1
set do_test=
set msvc=1

:parse_param

if "%1" == "" (
	goto do_build
)
if %1 == -h (
	goto show_help
)
if %1 == -? (
	goto show_help
)
if %1 == --help (
	goto show_help
)
if %1 == Debug (
	set cmake_opts=%cmake_opts% -DCMAKE_BUILD_TYPE=%1
	goto next_param
)
if %1 == RelWithDebInfo (
	set cmake_opts=%cmake_opts% -DCMAKE_BUILD_TYPE=%1
	goto next_param
)
if %1 == Release (
	set cmake_opts=%cmake_opts% -DCMAKE_BUILD_TYPE=%1
	goto next_param
)
if %1 == gcc (
	set cmake_opts=%cmake_opts% -GNinja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
	set msvc=0
	goto next_param
)
if %1 == C++98 (
	set cmake_opts=%cmake_opts% -DCMAKE_CXX_STANDARD=98 -DCMAKE_C_STANDARD=99
	set support_test=0
	goto next_param
)
if %1 == C++11 (
	set cmake_opts=%cmake_opts% -DCMAKE_CXX_STANDARD=11 -DCMAKE_C_STANDARD=11
	set support_test=0
	goto next_param
)
if %1 == C++14 (
	set cmake_opts=%cmake_opts% -DCMAKE_CXX_STANDARD=14 -DCMAKE_C_STANDARD=11
	goto next_param
)
if %1 == C++17 (
	set cmake_opts=%cmake_opts% -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_STANDARD=11
	goto next_param
)
if %1 == C++20 (
	set cmake_opts=%cmake_opts% -DCMAKE_CXX_STANDARD=20 -DCMAKE_C_STANDARD=11
	goto next_param
)
if %1 == dev (
	set cmake_opts=%cmake_opts% -DLIBSTORED_DEV=ON
	set do_test=1
	goto next_param
)
if %1 == nodev (
	set cmake_opts=%cmake_opts% -DLIBSTORED_DEV=OFF
	goto next_param
)
if %1 == test (
	set do_test=1
	goto next_param
)
if %1 == notest (
	set do_test=0
	goto next_param
)
if %1 == examples (
	set cmake_opts=%cmake_opts% -DLIBSTORED_EXAMPLES=ON
	goto next_param
)
if %1 == noexamples (
	set cmake_opts=%cmake_opts% -DLIBSTORED_EXAMPLES=OFF
	goto next_param
)
if %1 == zmq (
	set cmake_opts=%cmake_opts% -DLIBSTORED_HAVE_LIBZMQ=ON
	goto next_param
)
if %1 == nozmq (
	set cmake_opts=%cmake_opts% -DLIBSTORED_HAVE_LIBZMQ=OFF
	goto next_param
)
if %1 == -- (
	shift
	goto do_build
)

rem Unknown parameter, pass the remaining to cmake.
goto do_build

:next_param
shift
goto parse_param

:do_build

if %support_test% == 0 set do_test=
if "%do_test%" == "0" set cmake_opts=%cmake_opts% -DLIBSTORED_TESTS=OFF
if "%do_test%" == "1" set cmake_opts=%cmake_opts% -DLIBSTORED_TESTS=ON

set "builddir=%here%build"
if not exist "%builddir%" mkdir "%builddir%"
if errorlevel 1 goto error_nopopd

pushd "%builddir%"

set "cmake_here=%here:\=/%"
set "cmake_builddir=%builddir:\=/%"

if %msvc% == 1 (
	set par=
) else (
	set par=-- -j%NUMBER_OF_PROCESSORS%
)

cmake "-DCMAKE_MODULE_PATH=%cmake_here%../common" "-DCMAKE_INSTALL_PREFIX=%cmake_builddir%/deploy" %cmake_opts% ..\..\.. %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 1 goto error
cmake --build . %par%
if errorlevel 1 goto error
cmake --build . --target install %par%
if errorlevel 1 goto error

if not "%do_test%" == "1" goto done
set CTEST_OUTPUT_ON_FAILURE=1
if %msvc% == 1 (
	cmake --build . --target RUN_TESTS
	if errorlevel 1 goto error
) else (
	cmake --build . --target test
	if errorlevel 1 goto error
)

:done
popd
exit /b 0


:show_help
echo Usage: $0 [^<opt^>...] [--] [^<other cmake arguments^>]
echo.
echo where opt is:
echo   Debug RelWithDebInfo Release
echo         Set CMAKE_BUILD_TYPE to this value
echo   gcc   Use gcc instead of default compiler
echo   C++98 C++03 C++11 C++14 C++17 C++20
echo         Set the C++ standard
echo   dev   Enable development-related options
echo   test  Enable building and running tests
echo   zmq   Enable ZeroMQ integration
echo   nozmq Disable ZeroMQ integration
popd
exit /b 2
goto silent_error

:error
popd
:error_nopopd
echo.
echo Error occurred, stopping
echo.
:silent_error
exit /b 1
