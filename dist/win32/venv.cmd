@echo off

setlocal EnableDelayedExpansion
set "here=%~dp0"

call %here%\env.cmd

set "venv_dir=%here%\..\.venv"
set in_venv=0
python "%here%\..\common\check_venv.py"
if errorlevel 1 set in_venv=1

if "%1" == "" (
	call :venv_check
	goto done
)
if %1 == minimal (
	call :venv_just_activate
	goto done
)
if %1 == install (
	call :venv_requirements
	goto done
)
if %1 == activate (
	call :venv_activate
	goto done
)
if %1 == deactivate (
	call :venv_deactivate
	goto done
)
if %1 == check (
	call :venv_check
	goto done
)
if %1 == clean (
	call :venv_clean
	goto done
)
call :venv_help
goto done



:venv_install
set python=
for /f "tokens=* USEBACKQ" %%f in (`where python`) do (
	if "!python!" == "" set "python=%%f"
)

if %in_venv% == 0 goto do_install
if exist "%venv_dir%" goto :eof

echo Preparing current venv with %python%...
"%python%" -m pip install --prefer-binary --upgrade wheel pip
if errorlevel 1 goto error
mkdir "%venv_dir%"
if errorlevel 1 goto error
goto :eof

:do_install
echo Installing venv in "%venv_dir%"...
python -m venv "%venv_dir%"
if errorlevel 1 goto error
"%venv_dir%\Scripts\python" -m pip install --prefer-binary --upgrade wheel pip
if errorlevel 1 goto error
goto :eof



:venv_just_activate
call :venv_install
if %in_venv% == 0 (
	"%venv_dir%\Scripts\activate"
	if errorlevel 1 goto error
	set "python=%venv_dir%\Scripts\python"
)
goto :eof



:venv_requirements
call :venv_just_activate
echo Installing dependencies in %venv_dir% using %python%...
"%python%" -m pip install --prefer-binary --upgrade -r "%here%\..\common\requirements.txt"
if errorlevel 1 goto error
goto :eof



:venv_check
if exist "%venv_dir%" goto :eof
call :venv_requirements
exit /b 1



:venv_activate
call :venv_check
if errorlevel 1 goto :eof
call :venv_just_activate
goto :eof



:venv_deactivate
"%venv_dir%\bin\deactivate"
goto :eof



:venv_clean
call :venv_deactivate
if exist "%venv_dir%" rmdir /s "%venv_dir%"
goto :eof



:venv_help
echo Usage:
echo     venv.cmd install^|check^|clean
echo     source venv.cmd activate^|deactivate
exit /b 2



:done
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
pause
exit /b 2
