@echo off
REM arcamini.bat - dispatches to arcalua, arcaqjs, or arcapy based on script suffix

setlocal
set "lastarg=%~nx1"
for %%A in (%*) do set "lastarg=%%A"

if "%lastarg:~-4%"==".lua" (
    arcalua %*
) else if "%lastarg:~-3%"==".js" (
    arcaqjs %*
) else if "%lastarg:~-3%"==".py" (
    arcapy %*
) else (
    echo Unknown script type: %lastarg% 1>&2
    exit /b 99
)
endlocal