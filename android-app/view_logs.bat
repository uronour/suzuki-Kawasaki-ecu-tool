@echo off
set "ANDROID_HOME=D:\Android\Sdk"
set "PATH=%ANDROID_HOME%\platform-tools;%PATH%"

echo Waiting for device...
adb wait-for-device
echo Showing crashes and EcuToolApp logs...
adb logcat *:E | findstr /i "AndroidRuntime EcuToolApp"
pause
