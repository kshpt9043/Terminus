@echo off
chcp 65001 >nul
setlocal

REM ============================================================
REM  Terminus - Standalone 실행 (언리얼 Run-Standalone.bat 자리)
REM
REM  유니티는 에디터를 게임 모드로 띄우는 기능이 없어서 빌드한 exe 를 띄운다.
REM  Steam 로비 / 초대 테스트는 이걸로 (에디터 Play 는 기본 IP 모드).
REM
REM  사용법:
REM    Run-Standalone.bat              실행 (Steam 사용. Steam 클라이언트가 켜져 있어야 함)
REM    Run-Standalone.bat nosteam      Steam 끄고 실행 (IP 직접 접속)
REM    Run-Standalone.bat 2            Steam 끄고 2개 실행 (같은 PC 2인 테스트. 1번이 방 만들기, 2번이 127.0.0.1 로 참가)
REM    Run-Standalone.bat build        빌드하고 실행 (유니티 에디터가 이 프로젝트를 열고 있으면 에디터 메뉴 Terminus/Build/Windows 로)
REM    Run-Standalone.bat build 2      빌드하고 2개 실행
REM ============================================================

REM --- 실행 옵션 ---
set "RESX=1280"
set "RESY=720"
set "MAXFPS=60"

set "ROOT=%~dp0"
set "EXE=%ROOT%Builds\Windows\Terminus.exe"

REM --- 인자 ---
set "DO_BUILD="
set "MODE=steam"
for %%A in (%*) do (
    if /I "%%A"=="build" set "DO_BUILD=1"
    if /I "%%A"=="nosteam" set "MODE=nosteam"
    if /I "%%A"=="2" set "MODE=two"
)

if defined DO_BUILD call :build || exit /b 1

if not exist "%EXE%" (
    echo [오류] 빌드가 없습니다: %EXE%
    echo        Run-Standalone.bat build  또는  유니티 에디터 메뉴 Terminus/Build/Windows
    pause
    exit /b 1
)

set "COMMON=-screen-fullscreen 0 -screen-width %RESX% -screen-height %RESY% -maxfps %MAXFPS%"

REM 두 번째 창은 2초 뒤 (timeout 은 Git Bash 에서 GNU timeout 으로 잡혀서 ping). 이름은 영문 (UTF-8 배치의 한글 명령줄이 깨짐)
if "%MODE%"=="two" (
    echo Terminus 2개 실행: %RESX%x%RESY% / %MAXFPS%fps / 창모드 / Steam 끔
    start "" "%EXE%" %COMMON% -nosteam -profile p1 -name Player1 -logFile "%ROOT%Logs\standalone_p1.log"
    ping -n 3 127.0.0.1 >nul
    start "" "%EXE%" %COMMON% -nosteam -profile p2 -name Player2 -logFile "%ROOT%Logs\standalone_p2.log"
    goto :eof
)

set "EXTRA="
if "%MODE%"=="nosteam" set "EXTRA=-nosteam"
echo Terminus 실행: %RESX%x%RESY% / %MAXFPS%fps / 창모드 %EXTRA%
start "" "%EXE%" %COMMON% %EXTRA% -logFile "%ROOT%Logs\standalone.log"
goto :eof

REM ------------------------------------------------------------ 빌드
:build
REM 유니티 에디터 위치: 환경 변수 UNITY_EDITOR (Unity.exe 경로) -> 유니티 허브 기본 경로 (ProjectVersion.txt 의 버전)
set "UNITY=%UNITY_EDITOR%"
if not defined UNITY (
    for /f "tokens=2" %%V in ('findstr /b "m_EditorVersion:" "%ROOT%ProjectSettings\ProjectVersion.txt"') do set "UNITY=C:\Program Files\Unity\Hub\Editor\%%V\Editor\Unity.exe"
)
if not exist "%UNITY%" (
    echo [오류] 유니티 에디터를 찾을 수 없습니다: %UNITY%
    echo        환경 변수 UNITY_EDITOR 에 Unity.exe 경로를 넣어 주세요.
    pause
    exit /b 1
)
if exist "%ROOT%Temp\UnityLockfile" (
    echo [안내] 유니티 에디터가 이 프로젝트를 열고 있어 여기서 빌드할 수 없습니다.
    echo        에디터 메뉴 Terminus/Build/Windows 로 빌드한 뒤 build 없이 다시 실행하세요.
    pause
    exit /b 1
)
if not exist "%ROOT%Logs" mkdir "%ROOT%Logs"
echo 빌드 중... (로그: Logs\build.log)
"%UNITY%" -batchmode -quit -projectPath "%ROOT%." -buildTarget Win64 -executeMethod Terminus.EditorTools.BuildMenu.BuildWindowsBatch -logFile "%ROOT%Logs\build.log"
if errorlevel 1 (
    echo [오류] 빌드 실패. Logs\build.log 를 확인하세요.
    pause
    exit /b 1
)
echo 빌드 완료: %EXE%
exit /b 0
