@echo off
setlocal enabledelayedexpansion

:: Gather release files for UE4SS
:: Usage: gather_release.bat [build_dir] [config]
:: Example: gather_release.bat build_cmake_Game__Shipping__Win64 Game__Shipping__Win64

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."
pushd "%PROJECT_ROOT%"
set "PROJECT_ROOT=%CD%"
popd

:: Default values
set "BUILD_DIR=%~1"
set "CONFIG=%~2"

if "%BUILD_DIR%"=="" set "BUILD_DIR=build_cmake_Game__Shipping__Win64"
if "%CONFIG%"=="" set "CONFIG=Game__Shipping__Win64"

set "RELEASE_DIR=%PROJECT_ROOT%\release"

:: Convert to absolute path if relative
if not "%BUILD_DIR:~0,1%"=="\" if not "%BUILD_DIR:~1,1%"==":" (
    set "BUILD_DIR=%PROJECT_ROOT%\%BUILD_DIR%"
)

:: Determine bin directory path
set "BIN_DIR=%BUILD_DIR%\%CONFIG%\bin"

:: Check if bin directory exists, if not try without config subfolder
if not exist "%BIN_DIR%" (
    set "BIN_DIR=%BUILD_DIR%\bin"
)

:: If still not found, search for UE4SS.dll
if not exist "%BIN_DIR%\UE4SS.dll" (
    for /r "%BUILD_DIR%" %%f in (UE4SS.dll) do (
        set "BIN_DIR=%%~dpf"
        goto :found_bin
    )
    echo Error: Could not find UE4SS.dll in build directory
    echo Searched in: %BUILD_DIR%
    exit /b 1
)
:found_bin

:: Remove trailing backslash if present
if "%BIN_DIR:~-1%"=="\" set "BIN_DIR=%BIN_DIR:~0,-1%"

echo Project root: %PROJECT_ROOT%
echo Build dir: %BUILD_DIR%
echo Bin dir: %BIN_DIR%
echo Release dir: %RELEASE_DIR%
echo.

:: Verify required files exist
if not exist "%BIN_DIR%\UE4SS.dll" (
    echo Error: UE4SS.dll not found in %BIN_DIR%
    exit /b 1
)

if not exist "%BIN_DIR%\dwmapi.dll" (
    echo Error: dwmapi.dll not found in %BIN_DIR%
    exit /b 1
)

:: Clean and create release directory
echo Creating release directory...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%\ue4ss"

:: Copy dwmapi.dll to release root
echo Copying dwmapi.dll...
copy /y "%BIN_DIR%\dwmapi.dll" "%RELEASE_DIR%\" >nul

:: Copy UE4SS.dll to ue4ss folder
echo Copying UE4SS.dll...
copy /y "%BIN_DIR%\UE4SS.dll" "%RELEASE_DIR%\ue4ss\" >nul

:: Copy UE4SS.pdb if it exists (for dev builds)
if exist "%BIN_DIR%\UE4SS.pdb" (
    echo Copying UE4SS.pdb...
    copy /y "%BIN_DIR%\UE4SS.pdb" "%RELEASE_DIR%\ue4ss\" >nul
)

:: Copy assets
echo Copying assets...
set "ASSETS_DIR=%PROJECT_ROOT%\assets"

:: Copy Mods folder
if exist "%ASSETS_DIR%\Mods" (
    xcopy /e /i /q /y "%ASSETS_DIR%\Mods" "%RELEASE_DIR%\ue4ss\Mods" >nul
)

:: Copy settings file
if exist "%ASSETS_DIR%\UE4SS-settings.ini" (
    copy /y "%ASSETS_DIR%\UE4SS-settings.ini" "%RELEASE_DIR%\ue4ss\" >nul
)

:: Copy CustomGameConfigs
if exist "%ASSETS_DIR%\CustomGameConfigs" (
    xcopy /e /i /q /y "%ASSETS_DIR%\CustomGameConfigs" "%RELEASE_DIR%\ue4ss\CustomGameConfigs" >nul
)

:: Copy other useful assets
if exist "%ASSETS_DIR%\UE4SS_Signatures" (
    xcopy /e /i /q /y "%ASSETS_DIR%\UE4SS_Signatures" "%RELEASE_DIR%\ue4ss\UE4SS_Signatures" >nul
)
if exist "%ASSETS_DIR%\VTableLayoutTemplates" (
    xcopy /e /i /q /y "%ASSETS_DIR%\VTableLayoutTemplates" "%RELEASE_DIR%\ue4ss\VTableLayoutTemplates" >nul
)
if exist "%ASSETS_DIR%\MemberVarLayoutTemplates" (
    xcopy /e /i /q /y "%ASSETS_DIR%\MemberVarLayoutTemplates" "%RELEASE_DIR%\ue4ss\MemberVarLayoutTemplates" >nul
)
if exist "%ASSETS_DIR%\Changelog.md" (
    copy /y "%ASSETS_DIR%\Changelog.md" "%RELEASE_DIR%\ue4ss\" >nul
)

:: Copy LICENSE
if exist "%PROJECT_ROOT%\LICENSE" (
    copy /y "%PROJECT_ROOT%\LICENSE" "%RELEASE_DIR%\ue4ss\" >nul
)

:: Copy C++ mods if they exist
echo Looking for C++ mods...
for /r "%BUILD_DIR%" %%f in (KismetDebuggerMod.dll) do (
    echo Copying KismetDebuggerMod...
    mkdir "%RELEASE_DIR%\ue4ss\Mods\KismetDebuggerMod\dlls" 2>nul
    copy /y "%%f" "%RELEASE_DIR%\ue4ss\Mods\KismetDebuggerMod\dlls\main.dll" >nul
    if exist "%%~dpnf.pdb" (
        copy /y "%%~dpnf.pdb" "%RELEASE_DIR%\ue4ss\Mods\KismetDebuggerMod\dlls\main.pdb" >nul
    )
    goto :done_cpp_mods
)
:done_cpp_mods

echo.
echo ========================================
echo Release files gathered successfully!
echo ========================================
echo.
echo Release structure:
echo   release\
echo   +-- dwmapi.dll
echo   +-- ue4ss\
echo       +-- UE4SS.dll
if exist "%RELEASE_DIR%\ue4ss\UE4SS.pdb" echo       +-- UE4SS.pdb
echo       +-- UE4SS-settings.ini
echo       +-- Mods\
echo       +-- CustomGameConfigs\
echo       +-- ...
echo.
echo To install: Copy dwmapi.dll and ue4ss\ folder to your game's Win64 directory

endlocal