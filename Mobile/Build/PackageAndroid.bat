@echo off
REM PackageAndroid.bat
REM v7.2 — Windows batch for Android packaging

setlocal

set "PROJECT_ROOT=%~dp0..\.."
set "PROJECT_FILE=%PROJECT_ROOT%\StellarSystem.uproject"
set "UAT_PATH=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

echo ==========================================
echo   StellarSystem v7.2 — Android Packager
echo ==========================================
echo   Project: %PROJECT_FILE%
echo   UAT:     %UAT_PATH%
echo.

if not exist "%PROJECT_FILE%" (
    echo ERROR: Project file not found: %PROJECT_FILE%
    exit /b 1
)

if not exist "%UAT_PATH%" (
    echo ERROR: RunUAT.bat not found at %UAT_PATH%
    echo Set UE_ROOT environment variable to your UE5 root.
    exit /b 1
)

set "CONFIG=%1"
if "%CONFIG%"=="" set "CONFIG=Shipping"

set "TARGET=%2"
if "%TARGET%"=="" set "TARGET=StellarSystemAndroid"

echo Configuration: %CONFIG%
echo Target:        %TARGET%
echo.

call "%UAT_PATH%" BuildGame ^
    -project="%PROJECT_FILE%" ^
    -targetplatform=Android ^
    -configuration=%CONFIG% ^
    -target=%TARGET% ^
    -cookflavor=ARM64 ^
    -pak ^
    -compressed ^
    -distribution ^
    -nodebuginfo

echo.
echo ==========================================
echo   Android packaging complete!
echo   Output: Binaries\Android\StellarSystem-%CONFIG%-arm64.apk
echo ==========================================
endlocal
