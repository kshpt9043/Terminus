@echo off
chcp 65001 >nul
setlocal

REM ============================================================
REM  Terminus - Standalone 실행
REM
REM  PIE(에디터 안에서 재생)에서는 Steam OSS 가 초기화되지 않는다.
REM  세션/로비 관련 테스트는 반드시 이 배치로 실행할 것.
REM
REM  사용법:
REM    Run-Standalone.bat            일반 실행 (Steam 사용)
REM    Run-Standalone.bat nosteam    Steam 끄고 실행 (로컬 2인 접속 테스트용)
REM ============================================================

REM --- 설치 경로가 다르면 이 줄만 고치세요 ---
set "ENGINE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

REM --- 실행 옵션 ---
set "RESX=1280"
set "RESY=720"
set "MAXFPS=60"

REM 프로젝트 경로는 이 배치파일이 있는 폴더에서 자동으로 찾는다
set "PROJECT=%~dp0Terminus.uproject"

REM nosteam 인자를 주면 Steam OSS 를 끄고 IP 넷드라이버로 떨어진다
set "EXTRA="
if /I "%~1"=="nosteam" set "EXTRA=-nosteam"

if not exist "%ENGINE%" (
    echo [오류] 엔진을 찾을 수 없습니다.
    echo        %ENGINE%
    echo        위쪽 ENGINE 변수를 본인 설치 경로로 수정하세요.
    pause
    exit /b 1
)

if not exist "%PROJECT%" (
    echo [오류] 프로젝트를 찾을 수 없습니다: %PROJECT%
    pause
    exit /b 1
)

echo Terminus 실행: %RESX%x%RESY% / %MAXFPS%fps / 창모드 %EXTRA%
start "" "%ENGINE%" "%PROJECT%" -game -log -windowed -ResX=%RESX% -ResY=%RESY% -ExecCmds="t.MaxFPS %MAXFPS%" %EXTRA%

endlocal
