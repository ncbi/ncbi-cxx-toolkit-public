@echo off

set OUTDIR=%~1%
shift
set CONFIGURATIONNAME=%~1%
shift
set SOLUTIONPATH=%~1%

:label_begin
shift
set PROJECT=%~1%
if "%PROJECT%"=="" goto label_done
if exist "%OUTDIR%\%PROJECT%" goto label_begin
devenv /build "%CONFIGURATIONNAME%" /project "%PROJECT%" "%SOLUTIONPATH%"
goto label_begin

:label_done


