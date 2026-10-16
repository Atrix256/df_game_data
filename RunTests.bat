@echo off
setlocal EnableDelayedExpansion
set "RC=0"

REM ---- edit these ----
set "SLN=Examples/Examples.slnx"
set "DATA_SLN=df_game_data.slnx"
set "DATA_EXE_NAME=Editor.exe"
set "DATA_REQUIRED_FILES=Editor.exe nfd.dll"
set "TESTS=1_Simple 1_Simple_C 2_HotReloading 2_HotReloading_C 3_Exhaustive 3_Exhaustive_C"
set "DBROOTS=Examples\1_Simple\data\Items.dbroot Examples\2_HotReloading\data\main.dbroot Examples\3_Exhaustive\data\test.dbroot"
REM --------------------

REM Optional args: RunTests.bat [Debug|Release] [x86|x64]
REM Set NOPAUSE=1 in the environment to skip the final pause (e.g. for CI).
set "CONFIGS=%~1"
if "%CONFIGS%"=="" set "CONFIGS=Debug Release"
set "PLATS=%~2"
if "%PLATS%"=="" set "PLATS=x86 x64"

REM Scratch folder for captured output; failures are collected for the final report
set "LOGDIR=%TEMP%\RunTests_%RANDOM%%RANDOM%"
mkdir "%LOGDIR%"
set "FAILLOG=%LOGDIR%\failures.txt"
set "FAILLIST=%LOGDIR%\failed_list.txt"

REM Find MSBuild for whatever VS version is installed
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
  echo MSBuild not found
  set "RC=1"
  goto :end
)

set "ROOT=%~dp0"

REM ---- build the data compiler (Release x64) and compile all data ----
REM NOTE: /m omitted deliberately here - parallel build was racing a DLL
REM copy step and producing an Editor.exe without nfd.dll.
echo.
echo ===== Building data compiler: %DATA_SLN% [Release x64] =====
REM Editor.exe now builds into the repo root (same folder as this script).
REM Strip the trailing backslash from ROOT so paths don't end up with "\\".
set "DATA_EXE_DIR=%ROOT:~0,-1%"

REM Clear any stale output from a previous build so a broken build can't
REM silently "succeed" by running against leftover artifacts.
for %%F in (%DATA_REQUIRED_FILES%) do (
  if exist "%DATA_EXE_DIR%\%%F" del /f /q "%DATA_EXE_DIR%\%%F"
)

"%MSBUILD%" "%ROOT%%DATA_SLN%" /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
  echo BUILD FAILED: %DATA_SLN% [Release x64]
  set "RC=1"
  goto :end
)

set "DATA_EXE=%DATA_EXE_DIR%\%DATA_EXE_NAME%"
for %%F in (%DATA_REQUIRED_FILES%) do (
  if not exist "%DATA_EXE_DIR%\%%F" (
    echo Required data-compiler file missing after build: %DATA_EXE_DIR%\%%F
    set "RC=1"
    goto :end
  )
)

pushd "%DATA_EXE_DIR%"
for %%D in (%DBROOTS%) do (
  echo --- Compiling %%D ---
  "%DATA_EXE%" --compile "%ROOT%%%D"
  set "CRC=!errorlevel!"
  if not "!CRC!"=="0" (
    echo DATA COMPILE FAILED: %%D exit code !CRC!
    popd
    set "RC=1"
    goto :end
  )
)
popd

REM ---- build and run example tests ----
set FAILS=0
for %%C in (%CONFIGS%) do for %%P in (%PLATS%) do call :one %%C %%P

echo.
if %FAILS% neq 0 (
  echo ################ FAILURE DETAILS ################
  type "%FAILLOG%"
  echo ################ SUMMARY ################
  echo Failed:
  type "%FAILLIST%"
  echo.
  echo %FAILS% FAILURE^(S^)
  set "RC=1"
) else (
  echo ALL PASSED
)
goto :end

:end
if exist "%LOGDIR%" rmdir /s /q "%LOGDIR%"
echo.
if not defined NOPAUSE pause
exit /b %RC%

:one
set "CFG=%~1"
set "PLAT=%~2"

REM x86 build output omits the platform folder; x64 includes it
if /i "%PLAT%"=="x86" (
  set "EXEDIR=%CFG%"
) else (
  set "EXEDIR=%PLAT%\%CFG%"
)

echo.
echo ===== %CFG% %PLAT% =====
set "BOUT=%LOGDIR%\build_%CFG%_%PLAT%.txt"
"%MSBUILD%" "%SLN%" /m /nologo /v:minimal /p:Configuration=%CFG% /p:Platform=%PLAT% > "%BOUT%" 2>&1
set "BRC=!errorlevel!"
type "%BOUT%"
if not "!BRC!"=="0" (
  echo BUILD FAILED: %CFG% %PLAT%
  set /a FAILS+=1
  >> "%FAILLOG%" echo ================================================================
  >> "%FAILLOG%" echo BUILD FAILED: %CFG% %PLAT%
  >> "%FAILLOG%" echo ================================================================
  >> "%FAILLOG%" type "%BOUT%"
  >> "%FAILLOG%" echo.
  >> "%FAILLIST%" echo   BUILD %CFG% %PLAT%
  exit /b 0
)

for %%T in (%TESTS%) do (
  echo --- %%T ---
  set "EXE=%ROOT%Examples\%EXEDIR%\%%T.exe"
  set "OUT=%LOGDIR%\%%T_%CFG%_%PLAT%.txt"
  pushd "%ROOT%Examples\%%T"
  "!EXE!" -test > "!OUT!" 2>&1
  set "TRC=!errorlevel!"
  popd
  type "!OUT!"
  if not "!TRC!"=="0" (
    echo TEST FAILED: %%T [%CFG% %PLAT%] exit code !TRC!
    set /a FAILS+=1
    >> "%FAILLOG%" echo ================================================================
    >> "%FAILLOG%" echo TEST FAILED: %%T [%CFG% %PLAT%] exit code !TRC!
    >> "%FAILLOG%" echo ================================================================
    >> "%FAILLOG%" type "!OUT!"
    >> "%FAILLOG%" echo.
    >> "%FAILLIST%" echo   %%T [%CFG% %PLAT%]
  )
)

exit /b 0
