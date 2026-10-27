@echo off
setlocal
if "%~1"=="" goto usage
for %%p in (html\index.htm html\sample.svg) do (
  echo sending %%p
  curl.exe -fsS -T "%%p" "http://%~1/upload/%%~nxp" || exit /b 1
)
exit /b 0
:usage
echo usage: upload.bat BOARD
exit /b 64
