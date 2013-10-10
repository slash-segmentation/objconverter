@echo off
if exist %1 goto CONVERT
echo Need to specify a file on the command line
goto END

:CONVERT
set T=tmp%RANDOM%.obj
cd %~dp1
ren %~nx1 %T%

echo %TIME% Started Normalization
"%~dp0\objnormalize.exe" %T% %~nx1

echo %TIME% Started Compression
"%~dp0\objcompress.exe" -w %~nx1 %~n1.utf8 > %~n1.js
echo %TIME% Done
del %~nx1
ren %T% %~nx1

:END
pause
