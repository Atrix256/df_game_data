@echo off
setlocal EnableDelayedExpansion
set "RC=0"

REM ---- edit these ----
set "SLN=Examples/Examples.slnx"
set "DATA_SLN=df_game_data.slnx"
set "DATA_EXE_NAME=Editor.exe"
set "DATA_REQUIRED_FILES=Editor.exe nfd.dll"
set "TESTS=1_Simple 2_HotReloading 3_Exhaustive"
set "DBROOTS=Examples\1_Simple\data\Items.dbroot Examples\2_HotReloading\data\main.dbroot Examples\3_Exhaustive\data\test.dbroot"
REM --------------------

REM Optional args: RunTests.bat [Debug|Release] [x86|x64]
REM Set NOPAUSE=1 in the environment to skip the final pause (e.g. for CI).
set "CONFIGS=%~1"
if "%CONFIGS%"=="" set "CONFIGS=Debug Release"
set "PLATS=%~2"
if "%PLATS%"=="" set "PLATS=x86 x64"

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
REM copy step and silently producing an Editor.exe without nfd.dll.
echo.
echo ===== Building data compiler: %DATA_SLN% [Release x64] =====
set "DATA_EXE_DIR=%ROOT%x64\Release"

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
  if errorlevel 1 (
    echo DATA COMPILE FAILED: %%D
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
  echo %FAILS% FAILURE^(S^)
  set "RC=1"
) else (
  echo ALL PASSED
)
goto :end

:end
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
"%MSBUILD%" "%SLN%" /m /nologo /v:minimal /p:Configuration=%CFG% /p:Platform=%PLAT%
if errorlevel 1 (
  echo BUILD FAILED: %CFG% %PLAT%
  set /a FAILS+=1
  exit /b 0
)
for %%T in (%TESTS%) do (
  echo --- %%T ---
  set "EXE=%ROOT%Examples\%EXEDIR%\%%T.exe"
  pushd "%ROOT%Examples\%%T"
  "!EXE!" -test
  if errorlevel 1 (
    echo TEST FAILED: %%T [%CFG% %PLAT%]
    set /a FAILS+=1
  )
  popd
)

exit /b 0