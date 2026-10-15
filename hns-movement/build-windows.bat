@echo off
rem Builds hns-movement for Windows and installs it into a local CS2 server.
rem
rem Needs: Visual Studio 2022 (or Build Tools 2022) with "Desktop development with C++",
rem        Git for Windows, and Python 3 (python.org installer, "Add to PATH" ticked).
rem
rem Usage: double-click, or   build-windows.bat ["path\to\game\csgo"]
setlocal EnableExtensions

set "SRC=%~dp0"
set "SERVER=%~1"
if "%SERVER%"=="" set "SERVER=D:\cs2_hns\steamcmd\steamapps\common\Counter-Strike Global Offensive\game\csgo"
set "WORK=%SRC%_build\hns-movement"

echo === hns-movement Windows build ===
echo Server folder: %SERVER%
echo.

rem ---- Visual Studio C++ toolchain ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :novs
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || goto :novs
echo [ok] MSVC from %VSPATH%

rem ---- Git ----
where git >nul 2>nul || (echo [error] Git not found. Install Git for Windows: https://git-scm.com/download/win & goto :fail)
echo [ok] git

rem ---- Python ----
set "PY="
where python >nul 2>nul && set "PY=python"
if not defined PY (where py >nul 2>nul && set "PY=py -3")
if not defined PY (echo [error] Python 3 not found. Install from https://www.python.org/downloads/ with "Add python.exe to PATH". & goto :fail)
%PY% -c "import sys; assert sys.version_info >= (3, 8)" || (echo [error] Python 3.8+ required & goto :fail)
echo [ok] python

rem ---- AMBuild ----
%PY% -c "import ambuild2" >nul 2>nul
if errorlevel 1 (
	echo Installing AMBuild...
	%PY% -m pip install --user "git+https://github.com/alliedmodders/ambuild" || goto :fail
)
echo [ok] ambuild

rem ---- Source: clone from the bundle, then the SDK and Metamod submodules ----
if not exist "%WORK%\.git" (
	echo Cloning source from hns-movement.gitbundle...
	git clone "%SRC%hns-movement.gitbundle" "%WORK%" || goto :fail
) else (
	git -C "%WORK%" pull "%SRC%hns-movement.gitbundle" master || goto :fail
)
echo Fetching hl2sdk-cs2 and metamod-source (first run downloads ~1 GB)...
git -C "%WORK%" submodule update --init || goto :fail
git -C "%WORK%\metamod-source" submodule update --init --recursive || goto :fail

rem ---- Build ----
if not exist "%WORK%\build" mkdir "%WORK%\build"
pushd "%WORK%\build"
%PY% ..\configure.py --enable-optimize || (popd & goto :fail)
%PY% -c "import sys; from ambuild2.run import cli_run; sys.exit(cli_run())" || (popd & goto :fail)
popd
set "PKG=%WORK%\build\package"
if not exist "%PKG%\addons\hns-movement\bin\win64\hns-movement.dll" (echo [error] Build finished but the .dll is missing & goto :fail)
echo [ok] built %PKG%\addons\hns-movement\bin\win64\hns-movement.dll

rem ---- Install ----
if not exist "%SERVER%\addons\metamod" (echo [error] Metamod not found in "%SERVER%\addons" & goto :fail)
xcopy /E /I /Y "%PKG%\addons" "%SERVER%\addons" >nul || goto :fail
if not exist "%SERVER%\cfg\hns-movement" mkdir "%SERVER%\cfg\hns-movement"
rem Never overwrite a cfg you may have edited.
for %%f in ("%PKG%\cfg\hns-movement\*.cfg") do if not exist "%SERVER%\cfg\hns-movement\%%~nxf" copy "%%f" "%SERVER%\cfg\hns-movement\" >nul
echo [ok] installed into %SERVER%
echo.
echo Start the server and check with:  meta list   and   hns_status
pause
exit /b 0

:novs
echo [error] Visual Studio 2022 C++ build tools not found.
echo Install "Build Tools for Visual Studio 2022" from https://visualstudio.microsoft.com/downloads/
echo and tick the "Desktop development with C++" workload (it includes the Windows SDK).
:fail
echo.
echo Build/install FAILED.
pause
exit /b 1
