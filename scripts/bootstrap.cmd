@echo off
set here=%~dp0
pushd %here%\..

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

rem mingw (gcc 8.3.0) is broken:
rem https://github.com/msys2/MINGW-packages/issues/5006
rem version 8.2.0 is not available, reverting to 8.1.0
rem 8.1.0 seems to crash as well, probably SSE2 / AVX related, reverting to 7.3.0
choco install -y mingw --version 7.3.0
if errorlevel 1 goto error

choco install -y tortoisegit git cmake make python3 pip
if errorlevel 1 goto error

C:\Python38\Scripts\pip3 install textx jinja2 pyzmq pyside2 pyserial
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
pause
exit /b 1

