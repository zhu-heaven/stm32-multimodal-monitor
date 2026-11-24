@echo off
if not exist "%~dp0WeatherBridge.exe" call "%~dp0build.cmd"
if exist "%~dp0WeatherBridge.exe" start "" "%~dp0WeatherBridge.exe"
