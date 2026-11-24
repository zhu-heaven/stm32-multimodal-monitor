@echo off
setlocal
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%CSC%" (echo Install .NET Framework 4.8 build tools first.& exit /b 1)
"%CSC%" /nologo /target:winexe /out:"%~dp0WeatherBridge.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Net.Http.dll /reference:System.Web.Extensions.dll "%~dp0WeatherBridge.cs"
exit /b %ERRORLEVEL%
