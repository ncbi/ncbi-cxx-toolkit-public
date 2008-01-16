IF _%1%==_ GOTO USAGE
IF _%2%==_ GOTO USAGE
ECHO %CD%

:PREPARE
SET TREE_ROOT=%CD%\..\

SET PROJ_DIR=%TREE_ROOT%src\%1\
SET COMPILERS_DIR=%TREE_ROOT%compilers\
SET PTB_EXE_PATH=%COMPILERS_DIR%msvc710_prj\static\bin\debug\
SET PTB_INI_PATH=%COMPILERS_DIR%msvc710_prj\

IF EXIST %PTB_EXE_PATH%project_tree_builder.exe GOTO START_PTB
:BUILD_PTB
ECHO "Building project_tree_builder ..."
devenv %COMPILERS_DIR%msvc710_prj\static\build\app\project_tree_builder\project_tree_builder.sln /build Debug /project "-BUILD-ALL-" > Debug.log
IF ERRORLEVEL 1 GOTO ABORT
ECHO "Completed."
GOTO START_PTB

:START_PTB
ECHO "Creating requested solution ..."
%PTB_EXE_PATH%project_tree_builder.exe -logfile out.log -conffile %PTB_INI_PATH%project_tree_builder.ini %TREE_ROOT% src\%1 %PROJ_DIR%%2
IF ERRORLEVEL 1 GOTO ABORT
ECHO "Completed."
GOTO OPEN_SOLUTION

:OPEN_SOLUTION
devenv %PROJ_DIR%%2
GOTO EXIT

:USAGE
ECHO "bat file for starting a new project with MSVC 7.10"
ECHO "UNIX makefiles must be created first!"
ECHO "USAGE:"
ECHO "new_project_msvc7 <path-from-tree-root-to-your-project> <solution-name>"
ECHO "Example:"
ECHO "new_project_msvc7 internal\cppcore\test_stat test_stat.sln"
GOTO EXIT

:ABORT
ECHO "FAILED"
GOTO EXIT

:EXIT