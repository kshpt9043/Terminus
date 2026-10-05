@echo off
chcp 65001 >nul
setlocal

REM ============================================================
REM  Terminus - 에디터 빌드 (sln 안 열고 C++ 컴파일)
REM
REM  에디터가 켜져 있으면 Live Coding 때문에 빌드가 막힌다. 에디터를 끄고 실행할 것.
REM  헤더(UPROPERTY, UFUNCTION 등)를 고쳤으면 Ctrl+Alt+F11 대신 이걸로.
REM
REM  사용법:
REM    Build-Editor.bat           빌드
REM    Build-Editor.bat clean     정리하고 처음부터 다시 빌드
REM ============================================================

REM --- 엔진 위치. 아래 순서로 처음 찾은 곳을 쓴다 ---
REM 1. 환경 변수 UE_ENGINE_DIR (예: E:\UE_5.8\Engine)
REM 2. 에픽 런처 기본 설치 경로
REM 3. E:\UE_5.8
set "ENGINE_DIR="
if defined UE_ENGINE_DIR if exist "%UE_ENGINE_DIR%\Build\BatchFiles\Build.bat" set "ENGINE_DIR=%UE_ENGINE_DIR%"
if not defined ENGINE_DIR if exist "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8\Engine"
if not defined ENGINE_DIR if exist "E:\UE_5.8\Engine\Build\BatchFiles\Build.bat" set "ENGINE_DIR=E:\UE_5.8\Engine"

if not defined ENGINE_DIR (
    echo [오류] UE 5.8 엔진을 찾을 수 없습니다.
    echo        환경 변수 UE_ENGINE_DIR 에 엔진 폴더를 넣어 주세요. 예: E:\UE_5.8\Engine
    pause
    exit /b 1
)

REM 프로젝트 경로는 이 배치파일이 있는 폴더에서 자동으로 찾는다
set "PROJECT=%~dp0Terminus.uproject"

if not exist "%PROJECT%" (
    echo [오류] 프로젝트를 찾을 수 없습니다: %PROJECT%
    pause
    exit /b 1
)

REM 에디터가 떠 있으면 UBT 가 Live Coding 오류로 바로 실패하니 미리 알려줌
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo [오류] 언리얼 에디터가 켜져 있습니다. 에디터를 끄고 다시 실행하세요.
    pause
    exit /b 1
)

set "BUILD_BAT=%ENGINE_DIR%\Build\BatchFiles\Build.bat"
if /I "%~1"=="clean" set "BUILD_BAT=%ENGINE_DIR%\Build\BatchFiles\Rebuild.bat"

echo 엔진: %ENGINE_DIR%
echo 빌드: TerminusEditor Win64 Development %~1
echo.

call "%BUILD_BAT%" TerminusEditor Win64 Development "-Project=%PROJECT%" -WaitMutex -FromMsBuild
set "RESULT=%ERRORLEVEL%"

echo.
if "%RESULT%"=="0" (
    echo ===== 빌드 성공 =====
) else (
    echo ===== 빌드 실패 ^(코드 %RESULT%^) =====
    echo 자세한 로그: %LOCALAPPDATA%\UnrealBuildTool\Log.txt
)

pause
endlocal & exit /b %RESULT%
