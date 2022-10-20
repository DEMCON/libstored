@echo off

rem libstored, distributed debuggable data stores.
rem Copyright (C) 2020-2022  Jochem Rutgers
rem
rem This Source Code Form is subject to the terms of the Mozilla Public
rem License, v. 2.0. If a copy of the MPL was not distributed with this
rem file, You can obtain one at https://mozilla.org/MPL/2.0/.

setlocal EnableDelayedExpansion
set "here=%~dp0"

call %here%env.cmd > NUL

set "venv_dir=%here%..\.venv"
set in_venv=0
python "%here%..\common\check_venv.py"
if errorlevel 1 set in_venv=1

set python=
for /f "tokens=*" %%f in ('where python') do (
	if "!python!" == "" set "python=%%f"
)

set op=%1

if "%op%" == "" set op=check

if %op% == check (
	call :venv_check
	if errorlevel 2 goto silent_error
	if %in_venv% == 0 echo venv's python is: %venv_dir%\Scripts\python.exe
	if %in_venv% == 1 echo venv's python is: %python%
	if errorlevel 1 exit /b 1
	goto done
)
if %op% == install (
	call :venv_requirements
	if errorlevel 1 goto silent_error
	goto done
)
if %op% == clean (
	call :venv_clean
	if errorlevel 1 goto silent_error
	goto done
)
call :venv_help
goto done



:venv_install
if exist "%venv_dir%" goto :eof
if %in_venv% == 0 goto do_install

echo Preparing current venv with %python%...
"%python%" -m pip install --upgrade wheel pip
if errorlevel 1 goto error
mkdir "%venv_dir%"
if errorlevel 1 goto error
goto :eof

:do_install
echo Installing venv in "%venv_dir%"...
python -m venv "%venv_dir%"
if errorlevel 1 goto error
"%venv_dir%\Scripts\python.exe" -m pip install --prefer-binary --upgrade wheel pip
if errorlevel 1 goto error
goto :eof



:venv_just_activate
call :venv_install
if errorlevel 1 goto silent_error
if %in_venv% == 0 (
	call "%venv_dir%\Scripts\activate.bat"
	if errorlevel 1 goto error
	set "python=%venv_dir%\Scripts\python.exe"
)
goto :eof



:venv_requirements
call :venv_just_activate
if errorlevel 1 goto silent_error
echo Installing dependencies in %venv_dir% using %python%...
"%python%" -m pip install --prefer-binary --upgrade -r "%here%..\common\requirements.txt"
if errorlevel 1 goto error
goto :eof



:venv_check
if exist "%venv_dir%" goto :eof
call :venv_requirements
if errorlevel 1 goto silent_error
echo.
exit /b 1



:venv_activate
call :venv_check
if errorlevel 2 goto silent_error
if errorlevel 1 goto done
call :venv_just_activate
goto done



:venv_deactivate
if exist "%venv_dir%\Scripts\deactivate.bat" call "%venv_dir%\Scripts\deactivate.bat"
if errorlevel 1 goto error
goto :eof



:venv_clean
call :venv_deactivate
if errorlevel 1 goto silent_error
if exist "%venv_dir%" rmdir /s /q "%venv_dir%"
if errorlevel 1 goto error
goto :eof



:venv_help
echo Usage:
echo     venv.cmd install^|check^|clean
exit /b 2



:done
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
exit /b 2
