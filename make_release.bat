@echo off
setlocal

rem make_release.bat
rem Builds Release x64 of the solution and puts the files needed to run it
rem into .\release

set "SOLUTION=df_game_data.slnx"
set "PLATFORM=x64"

rem Files that make up the package (from the build output folder)
set "FILES=Editor.exe nfd.dll"

pushd "%~dp0"

rem --- Find MSBuild via vswhere ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found - is Visual Studio installed?
    goto :fail
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo ERROR: MSBuild.exe not found
    goto :fail
)
echo Using MSBuild: %MSBUILD%

rem --- Clean the release folder ---
set "DEST=%CD%\release"
set "STAGE=%CD%\release_build"
if exist "%DEST%" rmdir /s /q "%DEST%"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%DEST%"
mkdir "%STAGE%"

rem --- Build ---
echo.
echo === Building Release^|%PLATFORM% ===

rem The doubled trailing backslash keeps the quoting happy.
"%MSBUILD%" "%SOLUTION%" /m /nologo /verbosity:minimal /t:Build /p:Configuration=Release /p:Platform=%PLATFORM% "/p:OutDir=%STAGE%\\"
if errorlevel 1 (
    echo ERROR: build failed
    goto :fail
)

rem --- Collect the package files ---
for %%f in (%FILES%) do (
    if not exist "%STAGE%\%%f" (
        echo ERROR: %%f was not found in the build output
        goto :fail
    )
    copy /y "%STAGE%\%%f" "%DEST%\" >nul
    if errorlevel 1 (
        echo ERROR: failed to copy %%f
        goto :fail
    )
)

rmdir /s /q "%STAGE%"

rem --- Copy the font and its licenses from the source tree into release\FontAwesome ---
for %%f in (fontawesome-webfont.ttf licence.txt LICENSE.txt) do (
    if not exist "Editor\FontAwesome\%%f" (
        echo ERROR: Editor\FontAwesome\%%f not found
        goto :fail
    )
    xcopy /y /i /q "Editor\FontAwesome\%%f" "%DEST%\FontAwesome\" >nul
    if errorlevel 1 (
        echo ERROR: failed to copy Editor\FontAwesome\%%f
        goto :fail
    )
)

rem --- Copy the example data folders, keeping their relative paths ---
rem Robocopy exit codes below 8 mean success.
for %%d in (Examples\1_Simple\data Examples\2_HotReloading\data Examples\3_Exhaustive\data) do (
    if not exist "%%d\" (
        echo ERROR: %%d not found
        goto :fail
    )
    robocopy "%%d" "%DEST%\%%d" /E /NFL /NDL /NJH /NJS /NP >nul
    if errorlevel 8 (
        echo ERROR: failed to copy %%d
        goto :fail
    )
)

echo.
echo Release package ready: %DEST%
dir /b /s "%DEST%"
popd
endlocal
exit /b 0

:fail
echo.
echo FAILED
popd
endlocal
exit /b 1
