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

rem The actual python version is not really relevant, but pyzmq have trouble
rem building with 3.9.0 at the moment.
choco install -y --no-progress python3 --version=3.8.6
if errorlevel 1 goto error

choco install -y --no-progress tortoisegit git cmake make pkgconfiglite mingw doxygen.install
if errorlevel 1 goto error

call refreshenv

python.exe -m ensurepip
if errorlevel 1 goto error

python.exe -m pip install --upgrade setuptools
if errorlevel 1 goto error

python.exe -m pip install wheel
if errorlevel 1 goto error

python.exe -m pip install textx jinja2 pyzmq pyside2 pyserial lognplot PyQt5 natsort crcmod
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

