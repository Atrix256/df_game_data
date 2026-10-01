@echo off
setlocal EnableDelayedExpansion

REM ---- edit these ----
set "SLN=Examples/Examples.slnx"
set "DATA_SLN=df_game_data.slnx"
set "DATA_EXE_NAME=Editor.exe"
set "TESTS=1_Simple 2_HotReloading 3_Exhaustive"
set "DBROOTS=Examples\1_Simple\data\Items.dbroot Examples\2_HotReloading\data\main.dbroot Examples\3_Exhaustive\data\test.dbroot"
REM --------------------

REM Optional args: RunTests.bat [Debug|Release] [x86|x64]
set "CONFIGS=%~1"
if "%CONFIGS%"=="" set "CONFIGS=Debug Release"
set "PLATS=%~2"
if "%PLATS%"=="" set "PLATS=x86 x64"

REM Find MSBuild for whatever VS version is installed
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
  echo MSBuild not found
  exit /b 1
)

set "ROOT=%~dp0"

REM ---- build the data compiler (Release x64) and compile all data ----
echo.
echo ===== Building data compiler: %DATA_SLN% [Release x64] =====
"%MSBUILD%" "%ROOT%%DATA_SLN%" /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
  echo BUILD FAILED: %DATA_SLN% [Release x64]
  exit /b 1
)

set "DATA_EXE=%ROOT%x64\Release\%DATA_EXE_NAME%"
if not exist "%DATA_EXE%" (
  echo Data compiler exe not found at: %DATA_EXE%
  exit /b 1
)

for %%D in (%DBROOTS%) do (
  echo --- Compiling %%D ---
  "%DATA_EXE%" --compile "%ROOT%%%D"
  if errorlevel 1 (
    echo DATA COMPILE FAILED: %%D
    exit /b 1
  )
)

REM ---- build and run example tests ----
set FAILS=0
for %%C in (%CONFIGS%) do for %%P in (%PLATS%) do call :one %%C %%P

echo.
if %FAILS% neq 0 (
  echo %FAILS% FAILURE^(S^)
  exit /b 1
)
echo ALL PASSED
exit /b 0

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