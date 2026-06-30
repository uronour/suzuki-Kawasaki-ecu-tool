@echo off
set "ANDROID_HOME=D:\Android\Sdk"
set "ANDROID_SDK_ROOT=D:\Android\Sdk"
set "ANDROID_AVD_HOME=D:\Android\.android\avd"
set "GRADLE_USER_HOME=D:\.gradle"
set "PATH=%ANDROID_HOME%\emulator;%ANDROID_HOME%\platform-tools;%ANDROID_HOME%\cmdline-tools\latest\bin;%PATH%"

echo Starting Samsung Galaxy S21 5G Emulator (S21_5G)...
emulator -avd S21_5G -no-snapshot-load
pause
