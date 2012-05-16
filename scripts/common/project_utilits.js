////////////////////////////////////////////////////////////////////////////////////
// Shared part of new_project.wsf and import_project.wsf

// global settings
var g_verbose       = false;
var g_usefilecopy   = true;
var g_make_solution = true;
var g_open_solution = true;

var g_def_branch = "toolkit/trunk/internal/c++";
var g_branch     = "toolkit/trunk/internal/c++";

// valid:   "71", "80", "80x64", "90", "90x64", "100", "100x64"
var g_def_msvcver = "100";
var g_msvcver     = "100";

////////////////////////////////////////////////////////////////////////////////////
// Utility functions :
// create cmd, execute command in cmd and redirect output to console stdou2t
function read_stdout_line(oExec)
{
    var line = oExec.StdOut.ReadLine();
    if ((line.indexOf("Kerberos") >= 0 && line.indexOf("Authentication") >= 0) ||
         line.indexOf("authorization failed") >= 0) {
        WScript.Echo("========================= Authentication failed");
        WScript.Echo(line)
        WScript.Echo("To update your password, please, execute the following command:")
        WScript.Echo("svn list " + GetRepositoryRoot());
        WScript.Echo("terminating the script...")
        oExec.Terminate();
        while( oExec.Status == 0 ) {
            WScript.Sleep(100);
        }
        WScript.Quit(1);    
    }
    return line;
}

function execute(oShell, command)
{
    VerboseEcho("+  " + command);
    var oExec = oShell.Exec("cmd /c \"" + command + " 2>&1 \"");
    while( oExec.Status == 0 ) {
        while( !oExec.StdOut.AtEndOfStream ) {
            var line = read_stdout_line(oExec);
            VerboseEcho(line);
        }
        WScript.Sleep(100);
    }
    while( !oExec.StdOut.AtEndOfStream ) {
        var line = read_stdout_line(oExec);
        VerboseEcho(line);
    }
    return oExec.ExitCode;
}

function silent_execute(oShell, command)
{
    var oExec = oShell.Exec("cmd /c \"" + command + " 2>&1 \"");
    while( oExec.Status == 0 ) {
        while (!oExec.StdOut.AtEndOfStream) {
            read_stdout_line(oExec);
        }
        WScript.Sleep(100);
    }
    while (!oExec.StdOut.AtEndOfStream) {
        read_stdout_line(oExec);
    }
    return oExec.ExitCode;
}

function get_ncbiapp_version(oShell, ncbiapp)
{
    var ver = "";
    var oExec = oShell.Exec("cmd /c \"" + ncbiapp + " -version\"");
    while( oExec.Status == 0 ) {
        WScript.Sleep(100);
    }
    while (!oExec.StdOut.AtEndOfStream) {
        var line = oExec.StdOut.ReadLine();
        if (ver.length == 0) {
          var sep = line.indexOf(": ");
          if (sep > 0) {
            ver = line.substr(sep + 2);
            ver = ver.replace(/[.]/g,"");
          }
        }
    }
    return ver;
}

// convert all back-slashes to forward ones
function ForwardSlashes(str)
{
    var str_to_escape = str;
    return str_to_escape.replace(/\\/g, "/");
}
// convert all forward slashes to back ones
function BackSlashes(str)
{
    var str_to_escape = str;
    return str_to_escape.replace(/[/]/g, "\\");
}
// escape all back slashes ( for NCBI registry )
function EscapeBackSlashes(str)
{
    // need to re-define the string
    // looks like JScript bug
    var str_to_escape = str;
    return str_to_escape.replace(/\\/g, "\\\\");
}


////////////////////////////////////////////////////////////////////////////////////
// Re-usable framework functions

// tree object constructor
function Tree(oShell, oTask)
{
    this.TreeRoot              = oShell.CurrentDirectory + "\\" + oTask.ProjectFolder;
    this.CompilersBranch       = this.TreeRoot + "\\compilers\\" + GetMsvcFolder();
    this.CompilersBranchStatic = this.CompilersBranch + "\\static";
    this.BinPathStatic         = this.CompilersBranchStatic + "\\bin";
    this.CompilersBranchDll    = this.CompilersBranch + "\\dll";
    this.BinPathDll            = this.CompilersBranchDll + "\\bin";

    this.IncludeRootBranch     = this.TreeRoot + "\\include";
    this.IncludeConfig         = this.IncludeRootBranch + "\\common\\config";
    this.IncludeProjectBranch  = this.IncludeRootBranch + "\\" + BackSlashes(oTask.ProjectName);

    this.SrcRootBranch         = this.TreeRoot + "\\src";
    this.SrcDllBranch          = this.TreeRoot + "\\src\\dll";
    this.SrcBuildSystemBranch  = this.TreeRoot + "\\src\\build-system";
    this.SrcProjectBranch      = this.SrcRootBranch + "\\" + BackSlashes(oTask.ProjectName);
}
// diagnostic dump of the tree object
function DumpTree(oTree)
{
    VerboseEcho("TreeRoot              = " + oTree.TreeRoot);
    VerboseEcho("CompilersBranch       = " + oTree.CompilersBranch);
    VerboseEcho("CompilersBranchStatic = " + oTree.CompilersBranchStatic);
    VerboseEcho("BinPathStatic         = " + oTree.BinPathStatic);
    VerboseEcho("CompilersBranchDll    = " + oTree.CompilersBranchDll);
    VerboseEcho("BinPathDll            = " + oTree.BinPathDll);

    VerboseEcho("IncludeRootBranch     = " + oTree.IncludeRootBranch);
    VerboseEcho("IncludeConfig         = " + oTree.IncludeConfig);
    VerboseEcho("IncludeProjectBranch  = " + oTree.IncludeProjectBranch);

    VerboseEcho("SrcRootBranch         = " + oTree.SrcRootBranch);
    VerboseEcho("SrcDllBranch          = " + oTree.SrcDllBranch);
    VerboseEcho("SrcBuildSystemBranch  = " + oTree.SrcBuildSystemBranch);
    VerboseEcho("SrcProjectBranch      = " + oTree.SrcProjectBranch);
}

// build configurations -  object oTask is supposed to have DllBuild property
function GetConfigs(oTask)
{
    if (oTask.DllBuild) {
        var configs = new Array ("DebugDLL", "ReleaseDLL");
        return configs;
    } else {
        if (g_msvcver == "71") {
            var configs = new Array (
                "Debug",   "DebugMT",   "DebugDLL", 
                "Release", "ReleaseMT", "ReleaseDLL");
            return configs;
        } else {
            var configs = new Array (
                "DebugMT",   "DebugDLL", 
                "ReleaseMT", "ReleaseDLL");
            return configs;
        }
    }
}       
// recursive path creator - oFso is pre-created file system object
function CreateFolderIfAbsent(oFso, path)
{
    if ( !oFso.FolderExists(path) ) {
        CreateFolderIfAbsent(oFso, oFso.GetParentFolderName(path));
        VerboseEcho("Creating folder: " + path);
        oFso.CreateFolder(path);
    } else {
        VerboseEcho("Folder exists  : " + path);
    }
}
// create local build tree directories structure
function CreateTreeStructure(oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    // do not create tree root - it is cwd :-))
    CreateFolderIfAbsent(oFso, oTree.CompilersBranch       );
    CreateFolderIfAbsent(oFso, oTree.CompilersBranchStatic );
    CreateFolderIfAbsent(oFso, oTree.CompilersBranchStatic + "\\build");

    var configs = GetConfigs(oTask);
    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path = oTree.BinPathStatic + "\\" + conf;
        CreateFolderIfAbsent(oFso, target_path);
        if (oTask.DllBuild) {
            target_path = oTree.BinPathDll + "\\" + conf;
            CreateFolderIfAbsent(oFso, target_path);
        }
    }

    CreateFolderIfAbsent(oFso, oTree.CompilersBranchDll    );
    CreateFolderIfAbsent(oFso, oTree.CompilersBranchDll + "\\build");

    CreateFolderIfAbsent(oFso, oTree.IncludeRootBranch     );
    CreateFolderIfAbsent(oFso, oTree.IncludeConfig         );
//    CreateFolderIfAbsent(oFso, oTree.IncludeConfig + "\\msvc");
    CreateFolderIfAbsent(oFso, oTree.IncludeProjectBranch  );

    CreateFolderIfAbsent(oFso, oTree.SrcRootBranch         );
    CreateFolderIfAbsent(oFso, oTree.SrcDllBranch          );
    CreateFolderIfAbsent(oFso, oTree.SrcBuildSystemBranch  );
    CreateFolderIfAbsent(oFso, oTree.SrcBuildSystemBranch+ "\\project_tree_builder_gui\\bin");
    CreateFolderIfAbsent(oFso, oTree.SrcProjectBranch      );
}

// fill-in tree structure
function FillTreeStructure(oShell, oTree)
{
    if (!GetMakeSolution()) {
        return;
    }
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    if (oTask.DllBuild) {
        GetSubtreeFromTree(oShell, oTree, oTask, "src/dll", oTree.SrcDllBranch);
    }
    GetSubtreeFromTree(oShell, oTree, oTask, "src/build-system/project_tree_builder_gui/bin",
        oTree.SrcBuildSystemBranch+ "\\project_tree_builder_gui\\bin");
    // Fill-in infrastructure for the build tree
    var build_files = new Array (
        "Makefile.mk.in",
        "Makefile.mk.in.msvc",
        "project_tags.txt",
        "ptb_version.txt",
        "datatool_version.txt"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/src/build-system", build_files, oTree.SrcBuildSystemBranch, false);

    var tmp = g_usefilecopy;
    g_usefilecopy = true;
    GetFilesFromTree(oShell, oTree, oTask,
        "/src/build-system", new Array("Makefile.*.mk"), oTree.SrcBuildSystemBranch, true);
    g_usefilecopy = false;
    var build_files2 = new Array (
        "project_tree_builder.ini"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/src/build-system", build_files2, oTree.SrcBuildSystemBranch, true);
    g_usefilecopy = tmp;

    var compiler_files = new Array (
        "Makefile.*.msvc",
        "ncbi.rc",
        "ncbilogo.ico",
        "lock_ptb_config.bat",
        "ptb.bat",
        "datatool.bat",
        "msvcvars.bat",
        "configure.bat",
        "make.bat"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/compilers/" + GetMsvcFolder(), compiler_files, oTree.CompilersBranch, false);

    var dll_files = new Array (
        "dll_main.cpp"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/compilers/" + GetMsvcFolder() + "/dll", dll_files,  oTree.CompilersBranchDll, false);

    GetFilesFromTree(oShell, oTree, oTask,
        "/include/common/config", new Array("ncbiconf_msvc*.*"),
        oTree.IncludeConfig, false);
/*
    GetFilesFromTree(oShell, oTree, oTask,
        "/include/common/config/msvc", new Array("ncbiconf_msvc*.*"),
        oTree.IncludeConfig + "\\msvc", false);
*/
}

// check-out a subdir from CVS/SVN - oTree is supposed to have TreeRoot property
function CheckoutSubDir(oShell, oTree, sub_dir)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    var dir_local_path  = oTree.TreeRoot + "\\" + sub_dir;
    var repository_path = GetRepository(oShell, sub_dir);
    var dir_local_path_parent = oFso.GetParentFolderName(dir_local_path);
    var base_name = oFso.GetFileName(dir_local_path);

    oFso.DeleteFolder(dir_local_path, true);
    var cmd_checkout = "svn checkout " + ForwardSlashes(repository_path) + " " + base_name;
    execute(oShell, "cd " + BackSlashes(dir_local_path_parent) + " && " + cmd_checkout);
    execute(oShell, "cd " + oTree.TreeRoot);
}

// remove temporary dir ( used for get something for CVS/SVN ) 
function RemoveFolder(oShell, oFso, folder)
{
    if ( oFso.FolderExists(folder) ) {
        execute(oShell, "rmdir /S /Q \"" + folder + "\"");
    }
    if ( oFso.FolderExists(folder) ) {
        WScript.Sleep(500);
        execute(oShell, "rmdir /S /Q \"" + folder + "\"");
    }
}
// copy project_tree_builder app to appropriate places of the local tree
function CopyPtb(oShell, oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var release = GetDefaultPtbRelease(oFso);
    var release_found = oFso.FileExists(release);

    var sysenv = oShell.Environment("PROCESS");
    var prebuilt = sysenv("PREBUILT_PTB_EXE");
    if (prebuilt.length != 0) {
        if (oFso.FileExists(prebuilt)) {
            release = prebuilt;
            release_found = true;
            WScript.Echo("Using PREBUILT_PTB_EXE: " + prebuilt);
        } else {
            WScript.Echo("WARNING: PREBUILT_PTB_EXE not found: " + prebuilt);
        }
    }

    var configs = GetConfigs(oTask);
    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path;
        if (oTask.DllBuild) {
            target_path = oTree.BinPathDll;
        } else {
            target_path = oTree.BinPathStatic;
        }
        target_path += "\\" + conf + "\\";
        var target_file = target_path + "project_tree_builder.exe";

        var source_file = release;
        if (!release_found) {
            source_file = oTask.ToolkitPath;
            if (oTask.DllBuild) {
                source_file += "\\dll";
            } else {
                source_file += "\\static";
            }
            source_file += "\\bin"+ "\\" + conf + "\\project_tree_builder.exe";
            if (!oFso.FileExists(source_file)) {
                WScript.Echo("WARNING: File not found: " + source_file);
                continue;
            }
        }
        execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_file + "\"");
        if (oTask.DllBuild) {
            source_file = oFso.GetParentFolderName( source_file) + "\\ncbi_core.dll";
            if (oFso.FileExists(source_file)) {
                execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_path + "\"");
            }
        }
    }
}
// copy datatool app to appropriate places of the local tree
function CopyDatatool(oShell, oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var release = GetDefaultDatatoolRelease(oFso);
    var release_found = oFso.FileExists(release);

    var sysenv = oShell.Environment("PROCESS");
    var prebuilt = sysenv("PREBUILT_DATATOOL_EXE");
    if (prebuilt.length != 0) {
        if (oFso.FileExists(prebuilt)) {
            release = prebuilt;
            release_found = true;
            WScript.Echo("Using PREBUILT_DATATOOL_EXE: " + prebuilt);
        } else {
            WScript.Echo("WARNING: PREBUILT_DATATOOL_EXE not found: " + prebuilt);
        }
    }

    var configs = GetConfigs(oTask);
    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path;
        if (oTask.DllBuild) {
            target_path = oTree.BinPathDll;
        } else {
            target_path = oTree.BinPathStatic;
        }
        target_path += "\\" + conf + "\\";
        var target_file = target_path + "datatool.exe";

        var source_file = release;
        if (!release_found) {
            source_file = oTask.ToolkitPath;
            if (oTask.DllBuild) {
                source_file += "\\dll";
            } else {
                source_file += "\\static";
            }
            source_file += "\\bin"+ "\\" + conf + "\\datatool.exe";
            if (!oFso.FileExists(source_file)) {
                WScript.Echo("WARNING: File not found: " + source_file);
                continue;
            }
        }
        execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_file + "\"");
        if (oTask.DllBuild) {
            source_file = oFso.GetParentFolderName( source_file) + "\\ncbi_core.dll";
            if (oFso.FileExists(source_file)) {
                execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_path + "\"");
            }
        }
    }
}

function SetVerboseFlag(value)
{
    g_verbose = value;
}

function SetVerbose(oArgs, flag, default_val)
{
    g_verbose = GetFlagValue(oArgs, flag, default_val);
}

function GetVerbose()
{
    return g_verbose;
}

function SetMakeSolution(oArgs, flag, default_val)
{
    g_make_solution = !GetFlagValue(oArgs, flag, default_val);
}

function GetMakeSolution()
{
    return g_make_solution;
}

function SetOpenSolution(oArgs, flag, default_val)
{
    g_open_solution = !GetFlagValue(oArgs, flag, default_val);
}

function GetOpenSolution()
{
    return g_open_solution;
}

function SetBranch(oArgs, flag)
{
    var branch = GetFlaggedValue(oArgs, flag, "");
    if (branch.length == 0)
        return;
    g_branch = branch;
    g_usefilecopy = false;
}

function GetBranch()
{
    return g_branch;
}

function GetDefaultBranch()
{
    return g_def_branch;
}

function IsFileCopyAllowed()
{
    return g_usefilecopy;
}

function VerboseEcho(message)
{
    if (GetVerbose()) {
        WScript.Echo(message);
    }
}

function GetDefaultMsvcVer()
{
    return g_def_msvcver;
}

function SetMsvcVer(oArgs, flag)
{
    var msvcver = GetFlaggedValue(oArgs, flag, "");
    if (msvcver.length  != 0) {
        if (msvcver != "71" && msvcver != "80" &&  msvcver != "80x64"
                            && msvcver != "90" &&  msvcver != "90x64"
                            && msvcver != "100" && msvcver != "100x64") {
            WScript.Echo("ERROR: Unknown version of MSVC requested: " + msvcver);
            WScript.Quit(1);    
        }
        g_msvcver = msvcver;
    }
}

function GetMsvcFolder()
{
    if (g_msvcver == "80" || g_msvcver == "80x64") {
        return "msvc800_prj";
    }
    if (g_msvcver == "90" || g_msvcver == "90x64") {
        return "msvc900_prj";
    }
    if (g_msvcver == "100" || g_msvcver == "100x64") {
        return "msvc1000_prj";
    }
    return "msvc710_prj";
}

function GetFlaggedValue(oArgs, flag, default_val)
{
    for(var arg_i = 0; arg_i < oArgs.length; arg_i++) {
        if (oArgs.item(arg_i) == flag) {
            arg_i++;
            if (arg_i < oArgs.length) {
                return oArgs.item(arg_i);
            }
        }
    }
    return default_val;
}

// Get value of boolean argument set by command line flag
function GetFlagValue(oArgs, flag, default_val)
{
    for(var arg_i = 0; arg_i < oArgs.length; arg_i++) {
        if (oArgs.item(arg_i) == flag) {
            return true;
        }
    }
    return default_val;
}
// Position value must not be empty 
// and must not starts from '-' (otherwise it is flag)
function IsPositionalValue(str_value)
{
    if(str_value.length == 0)
        return false;
    if(str_value.charAt(0) == "-")
        return false;

    return true;
}
// Get value of positional argument 
function GetOptionalPositionalValue(oArgs, position, default_value)
{
    var pos_count = 0;
    for(var arg_i = 0; arg_i < oArgs.length; arg_i++) {
        var arg = oArgs.item(arg_i);
        if (IsPositionalValue(arg)) {
            if (pos_count == position) {
                return arg;
            }
            pos_count++;
        }
        else
        {
// flag values go last; if we see one, we know there is no more positional args
            break;
        }
    }
    return default_value;
}
function GetPositionalValue(oArgs, position)
{
    return GetOptionalPositionalValue(oArgs, position, "");
}

// Configuration of pre-built C++ toolkit
function GetDefaultSuffix()
{
    var s = "8";
    if (g_msvcver == "80") {
        s = "8";
    } else if (g_msvcver == "80x64") {
        s = "8.64";
    } else if (g_msvcver == "90") {
        s = "9";
    } else if (g_msvcver == "90x64") {
        s = "9.64";
    } else if (g_msvcver == "100") {
        s = "10";
    } else if (g_msvcver == "100x64") {
        s = "10.64";
    } else {
        s = "71";
    }
    return s;
}
function GetPtbTargetSolutionArgs(oShell, ptb)
{
    var ver = get_ncbiapp_version(oShell, ptb);
    var s = "";
    if (ver < 180) {
        return s;
    }
    if (g_msvcver == "80") {
        s = " -ide 800 -arch Win32";
    } else if (g_msvcver == "80x64") {
        s = " -ide 800 -arch x64";
    } else if (g_msvcver == "90") {
        s = " -ide 900 -arch Win32";
    } else if (g_msvcver == "90x64") {
        s = " -ide 900 -arch x64";
    } else if (g_msvcver == "100") {
        s = " -ide 1000 -arch Win32";
    } else if (g_msvcver == "100x64") {
        s = " -ide 1000 -arch x64";
    } else {
        s = " -ide 710 -arch Win32";
    }
    return s;
}
function GetTargetPlatform()
{
    if (g_msvcver == "80x64" || g_msvcver == "90x64" || g_msvcver == "100x64") {
        return "x64";
    }
    return "Win32";
}
function GetDefaultCXX_ToolkitFolder()
{
    var root = "\\\\snowman\\win-coremake\\Lib\\Ncbi\\CXX_Toolkit\\msvc"
    return root + GetDefaultSuffix();
}
function GetDefaultPtbRelease(oFso)
{
    var root = "\\\\snowman\\win-coremake\\App\\Ncbi\\cppcore\\ptb\\msvc"
    var ptb = root + "\\project_tree_builder.RELEASE";
    if (oFso.FileExists(ptb)) {
        return ptb;
    }
    return root + GetDefaultSuffix() + "\\project_tree_builder.RELEASE";
}
function GetDefaultDatatoolRelease(oFso)
{
    var root = "\\\\snowman\\win-coremake\\App\\Ncbi\\cppcore\\datatool\\msvc"
    return root + "\\datatool.RELEASE";
}
function GetDefaultLibFolder()
{
    return "\\\\snowman\\win-coremake\\Lib";
}
function GetDefaultCXX_ToolkitSubFolder()
{
    return "cxx.current";
}

// Copy pre-built C++ Toolkit DLLs'
function CopyDlls(oShell, oTree, oTask)
{
    if ( oTask.CopyDlls ) {
        var oFso = new ActiveXObject("Scripting.FileSystemObject");
        var configs = GetConfigs(oTask);
        for( var config_i = 0; config_i < configs.length; config_i++ ) {
            var config = configs[config_i];
            var dlls_bin_path  = oTask.ToolkitPath + "\\lib\\dll\\" + config;
            if (!oFso.FolderExists(dlls_bin_path)) {
                dlls_bin_path  = oTask.ToolkitPath + "\\" + config;
            }
            var local_bin_path = oTree.BinPathDll  + "\\" + config + "\\";

//            execute(oShell, "copy /Y \"" + dlls_bin_path + "\\*.dll\" \"" + local_bin_path + "\"");
            execute(oShell, "xcopy /Y /Q /C /K \"" + dlls_bin_path + "\\*.dll\" \"" + local_bin_path + "\"");
        }
    } else {
        VerboseEcho("CopyDlls:  skipped (not requested)");
    }
}
// Copy gui resources
function CopyRes(oShell, oTree, oTask)
{
    if ( oTask.CopyRes ) {
        var oFso = new ActiveXObject("Scripting.FileSystemObject");
        var tempname = oFso.GetTempName();
        var res_target_dir = oTree.SrcRootBranch + "\\gui\\res"
            CreateFolderIfAbsent(oFso, res_target_dir);
        execute(oShell, "svn checkout " + GetRepository(oShell,"src/gui/res") + " " + tempname);
        execute(oShell, "xcopy " + tempname + " \"" + res_target_dir + "\" /S /E /Y /C");
        RemoveFolder(oShell, oFso, tempname);
    } else {
        VerboseEcho("CopyRes:  skipped (not requested)");
    }
}
// SVN tree root
function GetSvnRepositoryRoot()
{
    return "https://svn.ncbi.nlm.nih.gov/repos/";
}

function GetRepositoryRoot()
{
    return GetSvnRepositoryRoot() + GetBranch();
}

function RepositoryExists(oShell,path)
{
    var path_array = path.split(" ");
    var test_path = path_array[ path_array.length - 1 ];
    return (silent_execute(oShell, "svn --non-interactive list " + test_path) == 0);
}

function SearchRepository(oShell, abs_path, rel_path)
{
    if (RepositoryExists(oShell,abs_path)) {
        return abs_path;
    }
    var rel_path_array = rel_path.split("/");
    var rel_path_size = rel_path_array.length;
    var path = abs_path;
    var i;
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    for (i=rel_path_size-1; i>=0; --i) {
        path = oFso.GetParentFolderName(path);
        if (!RepositoryExists(oShell,path)) {
            continue;
        }
        var externals = oShell.Exec("cmd /c \"svn pg svn:externals " + path + " 2>&1 \"");
        var line;
        var repo = "";
        while( repo.length == 0 ) {
            while (repo.length == 0 && !externals.StdOut.AtEndOfStream) {
                line = externals.StdOut.ReadLine();
                var test = "";
                var j;
                for (j=i; j<rel_path_size; ++j) {
                    test += rel_path_array[j];
                    if (line.indexOf(test + " ") == 0) {
                        repo = line.substr(test.length + 1);
                        for (j=j+1; j<rel_path_size; ++j) {
                            repo += "/" + rel_path_array[j];
                        }
                        break;
                    }
                    test += "/";
                }
            }
            if (repo.length != 0) {
                while (!externals.StdOut.AtEndOfStream) {
                    externals.StdOut.ReadLine();
                }
            }
            if (externals.Status == 0) {
                WScript.Sleep(100);
            } else {
                if (externals.StdOut.AtEndOfStream) {
                    break;
                }
            }
        }
        if (repo.length != 0) {
            while (repo.indexOf(" ") == 0) {
                repo = repo.substr(1);
            }
            WScript.Echo("External " + abs_path);
            WScript.Echo("found in " + repo);
            return SearchRepository(oShell, repo, rel_path)
//            return repo;
        }
    }
    WScript.Echo("WARNING: repository not found: " + abs_path);
    return "";
}

function GetRepository(oShell, relative_path)
{
    var rel_path = ForwardSlashes(relative_path);
    if (relative_path.indexOf("/") == 0) {
        rel_path = relative_path.substr(1);
    }
    VerboseEcho("Looking for " + rel_path);
    var abs_path = GetRepositoryRoot() + "/" + rel_path;
    var result = SearchRepository(oShell, abs_path, rel_path)
    if (result.length > 0) {
        return result;
    }
    abs_path = GetSvnRepositoryRoot() + GetDefaultBranch() + "/" + rel_path;
    result = SearchRepository(oShell, abs_path, rel_path)
    if (result.length > 0) {
        return result;
    }
    return abs_path;
}

// Get files from SVN tree
function GetFilesFromTree(oShell, oTree, oTask, cvs_rel_path, files, target_abs_dir, trycopy)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    // Try to get the file from the pre-built toolkit
    if (IsFileCopyAllowed()) {
        var folder = BackSlashes(oTask.ToolkitSrcPath + cvs_rel_path);
        if ( oFso.FolderExists(folder) ) {
            var dir = oFso.GetFolder(folder);
            var dir_files = new Enumerator(dir.files);
            if (!dir_files.atEnd()) {
                for (var i = 0; i < files.length; ++i) {
                    execute(oShell, "copy /Y \"" + folder + "\\" + files[i] + "\" \"" + target_abs_dir + "\"");
                }
                return;
            }
        }
    }

    // Get it from SVN
    var tempname = oFso.GetTempName();
    var cvs_dir = GetRepository(oShell, cvs_rel_path);
    var res = execute(oShell, "svn checkout -N " + cvs_dir + " " + tempname);
    for (var i = 0; i < files.length; ++i) {
        execute(oShell, "copy /Y \"" + tempname + "\\" + files[i] + "\" \""+ target_abs_dir + "\"");
    }
    RemoveFolder(oShell, oFso, tempname);

    // if SVN failed, still try to get the file
    if (res != 0 && trycopy) {
        var folder = BackSlashes(oTask.ToolkitSrcPath + cvs_rel_path);
        if ( oFso.FolderExists(folder) ) {
            var dir = oFso.GetFolder(folder);
            var dir_files = new Enumerator(dir.files);
            if (!dir_files.atEnd()) {
                for (var i = 0; i < files.length; ++i) {
                    execute(oShell, "copy /Y \"" + folder + "\\" + files[i] + "\" \"" + target_abs_dir + "\"");
                }
                return;
            }
        }
    }
}

function GetFileFromTree(oShell, oTree, oTask, cvs_rel_path, target_abs_dir)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    // Try to get the file from the pre-built toolkit
    if (IsFileCopyAllowed()) {
        var toolkit_file_path = BackSlashes(oTask.ToolkitSrcPath + cvs_rel_path);
        var folder = oFso.GetParentFolderName(toolkit_file_path);
        if ( oFso.FolderExists(folder) ) {
            var dir = oFso.GetFolder(folder);
            var dir_files = new Enumerator(dir.files);
            if (!dir_files.atEnd()) {
                execute(oShell, "copy /Y \"" + toolkit_file_path + "\" \"" + target_abs_dir + "\"");
                return;
            }
        }
    }

    // Get it from CVS
    var tempname = oFso.GetTempName();
    var rel_dir = oFso.GetParentFolderName(cvs_rel_path);
    var cvs_dir = GetRepository(oShell, rel_dir);
    var cvs_file = oFso.GetFileName(cvs_rel_path);
    execute(oShell, "svn checkout -N " + cvs_dir + " " + tempname);
    execute(oShell, "copy /Y \"" + tempname + "\\" + cvs_file + "\" \""+ target_abs_dir + "\"");
    RemoveFolder(oShell, oFso, tempname);
}

function GetSubtreeFromTree(oShell, oTree, oTask, cvs_rel_path, target_abs_dir)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    // Try to get the file from the pre-built toolkit
    if (IsFileCopyAllowed()) {
        var src_folder = BackSlashes(oTask.ToolkitSrcPath + "/" + cvs_rel_path);
        if ( oFso.FolderExists(src_folder) ) {
            execute(oShell, "xcopy \"" + src_folder + "\" \"" + target_abs_dir + "\" /S /E /Y /C /Q");
            return;
        }
    }

    // Get it from SVN (CVS not implemented!)
    var tempname = oFso.GetTempName();
    var cvs_path = GetRepository(oShell, cvs_rel_path);
    execute(oShell, "svn checkout " + cvs_path + " " + tempname);
    execute(oShell, "xcopy " + tempname + " \"" + target_abs_dir + "\" /S /E /Y /C");
    RemoveFolder(oShell, oFso, tempname);
}

function CheckNetworkDrive()
{
    var driveS = "s:";
    var url = GetDefaultLibFolder().toLowerCase();
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    if (oFso.DriveExists(driveS)) {
        var oNetwork = WScript.CreateObject("WScript.Network");
        var oDrives = oNetwork.EnumNetworkDrives();
        for(i = 0; i < oDrives.length; i += 2) {
            var drive = oDrives.Item(i).toLowerCase();
            if (drive == driveS) {
                var loc = oDrives.Item(i + 1).toLowerCase();
                if (loc == url) {
                    return "ok";
                }
            }
        }
        return "wrong";
    }
    return "absent";
}

function MapNetworkDrive()
{
    var driveS = "S:";
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    if (oFso.DriveExists(driveS)) {
        WScript.Echo("Drive " + driveS + " exists");
        return;
    }
    var oNetwork = WScript.CreateObject("WScript.Network");
    oNetwork.MapNetworkDrive (driveS, GetDefaultLibFolder());
    if (oFso.DriveExists(driveS)) {
        WScript.Echo("Drive " + driveS + " created: mapped to " + GetDefaultLibFolder());
    } else {
        WScript.Echo("ERROR: Failed to map network drive to " + GetDefaultLibFolder());
    }
}
