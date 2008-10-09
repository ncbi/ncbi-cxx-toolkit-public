////////////////////////////////////////////////////////////////////////////////////
// Shared part of new_project.wsf and import_project.wsf

// global settings
var g_verbose       = false;
var g_usefilecopy   = true;
var g_make_solution = true;

var g_def_branch = "toolkit/trunk/internal/c++";
var g_branch     = "toolkit/trunk/internal/c++";

// valid:   "71", "80", "80x64"
var g_def_msvcver = "80";
var g_msvcver     = "80";

////////////////////////////////////////////////////////////////////////////////////
// Utility functions :
// create cmd, execute command in cmd and redirect output to console stdou2t
function execute(oShell, command)
{
    VerboseEcho("+  " + command);
    var oExec = oShell.Exec("cmd /c \"" + command + " 2>&1 \"");
    while( oExec.Status == 0 ) {
        while( !oExec.StdOut.AtEndOfStream ) {
            var line = oExec.StdOut.ReadLine();
            if (line.indexOf("Kerberos") >= 0 && line.indexOf("Authentication") >= 0) {
                WScript.Echo("========================= Authentication failed");
                WScript.Echo(line)
                WScript.Echo("Please, terminate the script and execute the following command:")
                WScript.Echo("svn list " + GetRepositoryRoot());
//                oExec.Terminate();
//                WScript.Quit(1);    
            }
            VerboseEcho(line);
        }
        WScript.Sleep(100);
    }
    while( !oExec.StdOut.AtEndOfStream ) {
        VerboseEcho(oExec.StdOut.ReadLine());
    }
    return oExec.ExitCode;
}

function silent_execute(oShell, command)
{
/*
    var oExec = oShell.Exec("cmd /c \"" + command + " 2>&1 \"");
    while( oExec.Status == 0 ) {
        while (!oExec.StdOut.AtEndOfStream) {
            oExec.StdOut.ReadLine();
        }
        WScript.Sleep(100);
    }
    while (!oExec.StdOut.AtEndOfStream) {
        oExec.StdOut.ReadLine();
    }
*/
    var oExec = oShell.Exec("cmd /c \"" + command + "\"");
    while( oExec.Status == 0 ) {
        while (!oExec.StdErr.AtEndOfStream) {
            VerboseEcho(oExec.StdErr.ReadLine());
        }
        while (!oExec.StdOut.AtEndOfStream) {
            oExec.StdOut.SkipLine();
        }
        WScript.Sleep(100);
    }
    while (!oExec.StdErr.AtEndOfStream) {
        VerboseEcho(oExec.StdErr.ReadLine());
    }
    while (!oExec.StdOut.AtEndOfStream) {
        oExec.StdOut.SkipLine();
    }
    return oExec.ExitCode;
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

    CreateFolderIfAbsent(oFso, oTree.IncludeRootBranch     );
    CreateFolderIfAbsent(oFso, oTree.IncludeConfig         );
//    CreateFolderIfAbsent(oFso, oTree.IncludeConfig + "\\msvc");
    CreateFolderIfAbsent(oFso, oTree.IncludeProjectBranch  );

    CreateFolderIfAbsent(oFso, oTree.SrcRootBranch         );
    CreateFolderIfAbsent(oFso, oTree.SrcDllBranch          );
    CreateFolderIfAbsent(oFso, oTree.SrcBuildSystemBranch  );
    CreateFolderIfAbsent(oFso, oTree.SrcProjectBranch      );
}

// fill-in tree structure
function FillTreeStructure(oShell, oTree)
{
    if (!GetMakeSolution()) {
        return;
    }
    var temp_dir = oTree.TreeRoot + "\\temp";
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    if (oTask.DllBuild) {
        GetSubtreeFromTree(oShell, oTree, oTask, "src/dll", oTree.SrcDllBranch);
    }
    // Fill-in infrastructure for the build tree
    var build_files = new Array (
        "Makefile.mk.in",
        "Makefile.mk.in.msvc",
        "project_tree_builder.ini",
        "project_tags.txt"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/src/build-system", build_files, oTree.SrcBuildSystemBranch);

    var compiler_files = new Array (
        "Makefile.*.msvc",
        "ncbi.rc",
        "ncbilogo.ico",
        "project_tree_builder.ini",
        "lock_ptb_config.bat",
        "asn_prebuild.bat",
        "ptb.bat"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/compilers/" + GetMsvcFolder(), compiler_files, oTree.CompilersBranch);

    var dll_files = new Array (
        "dll_main.cpp"
        );
    GetFilesFromTree(oShell, oTree, oTask,
        "/compilers/" + GetMsvcFolder() + "/dll", dll_files,  oTree.CompilersBranchDll);

    GetFilesFromTree(oShell, oTree, oTask,
        "/include/common/config", new Array("ncbiconf_msvc*.*"),
        oTree.IncludeConfig);
/*
    GetFilesFromTree(oShell, oTree, oTask,
        "/include/common/config/msvc", new Array("ncbiconf_msvc*.*"),
        oTree.IncludeConfig + "\\msvc");
*/
}

// check-out a subdir from CVS/SVN - oTree is supposed to have TreeRoot property
function CheckoutSubDir(oShell, oTree, sub_dir)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    var dir_local_path  = oTree.TreeRoot + "\\" + sub_dir;
    var repository_path = GetRepository(oShell, sub_dir);
    var dir_local_path_parent = oFso.GetParentFolderName(dir_local_path);
    var base_name = oFso.GetBaseName(dir_local_path);

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
}
// copy project_tree_builder app to appropriate places of the local tree
function CopyPtb(oShell, oTree, oTask)
{
    var remote_ptb_found = false;
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var configs = GetConfigs(oTask);

// look for prebuilt PTB
    var sysenv = oShell.Environment("PROCESS");
    var ptbexe = sysenv("PREBUILT_PTB_EXE");
    if (ptbexe.length != 0) {
        if (oFso.FileExists(ptbexe)) {
            oTask.RemotePtb = ptbexe;
            remote_ptb_found = true;
            WScript.Echo("Using PREBUILT_PTB_EXE: " + ptbexe);
        } else {
            WScript.Echo("WARNING: PREBUILT_PTB_EXE not found: " + ptbexe);
        }
    }

    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path;
        if (oTask.DllBuild) {
            target_path = oTree.BinPathDll;
        } else {
            target_path = oTree.BinPathStatic;
        }
        target_path += "\\" + conf;
        var source_file = oTask.ToolkitPath + "\\bin" + "\\project_tree_builder.exe";
        if (!oFso.FileExists(source_file)) {
            WScript.Echo("WARNING: File not found: " + source_file);
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
        if (!remote_ptb_found) {
            oTask.RemotePtb = source_file;
            remote_ptb_found = true;
        }
        execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_path + "\"");
        if (oTask.DllBuild) {
            source_file = oFso.GetParentFolderName( source_file) + "\\ncbi_core.dll";
            if (!oFso.FileExists(source_file)) {
                WScript.Echo("WARNING: File not found: " + source_file);
                continue;
            }
            execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_path + "\"");
        }
    }
}
// copy datatool app to appropriate places of the local tree
function CopyDatatool(oShell, oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var configs = GetConfigs(oTask);
    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path;
        if (oTask.DllBuild) {
            target_path = oTree.BinPathDll;
        } else {
            target_path = oTree.BinPathStatic;
        }
        target_path += "\\" + conf;
        var source_file = oTask.ToolkitPath + "\\bin" + "\\datatool.exe";
        if (!oFso.FileExists(source_file)) {
            WScript.Echo("WARNING: File not found: " + source_file);
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
        execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_path + "\"");
        if (oTask.DllBuild) {
            source_file = oFso.GetParentFolderName( source_file) + "\\ncbi_core.dll";
            if (!oFso.FileExists(source_file)) {
                WScript.Echo("WARNING: File not found: " + source_file);
                continue;
            }
            execute(oShell, "copy /Y \"" + source_file + "\" \"" + target_path + "\"");
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
    g_msvcver = GetFlaggedValue(oArgs, flag, g_msvcver);
    if (g_msvcver != "71" && g_msvcver != "80" && g_msvcver != "80x64") {
        g_msvcver = GetDefaultMsvcVer();
    }
}

function GetMsvcFolder()
{
    if (g_msvcver == "80" || g_msvcver == "80x64") {
        return "msvc800_prj";
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
function GetDefaultCXX_ToolkitFolder()
{
    var root = "\\\\snowman\\win-coremake\\Lib\\Ncbi\\CXX_Toolkit\\msvc"
    if (g_msvcver == "80") {
        root += "8";
    } else if (g_msvcver == "80x64") {
        root += "8.64";
    } else {
        root += "71";
    }
    return root;
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
            var local_bin_path = oTree.BinPathDll  + "\\" + config;

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
        var res_target_dir = oTree.SrcRootBranch + "\\gui\\res"
            CreateFolderIfAbsent(oFso, res_target_dir);
        execute(oShell, "svn checkout " + GetRepository(oShell,"src/gui/res") + " temp");
        execute(oShell, "copy /Y temp\\*.* \"" + res_target_dir + "\"");
        RemoveFolder(oShell, oFso, "temp");
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
    return (silent_execute(oShell, "svn list " + test_path) == 0);
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
function GetFilesFromTree(oShell, oTree, oTask, cvs_rel_path, files, target_abs_dir)
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
    RemoveFolder(oShell, oFso, "temp");
    var cvs_dir = GetRepository(oShell, cvs_rel_path);
    execute(oShell, "svn checkout -N " + cvs_dir + " temp");
    for (var i = 0; i < files.length; ++i) {
        execute(oShell, "copy /Y \"temp\\" + files[i] + "\" \""+ target_abs_dir + "\"");
    }
    RemoveFolder(oShell, oFso, "temp");
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
    RemoveFolder(oShell, oFso, "temp");
    var rel_dir = oFso.GetParentFolderName(cvs_rel_path);
    var cvs_dir = GetRepository(oShell, rel_dir);
    var cvs_file = oFso.GetFileName(cvs_rel_path);
    execute(oShell, "svn checkout -N " + cvs_dir + " temp");
    execute(oShell, "copy /Y \"temp\\" + cvs_file + "\" \""+ target_abs_dir + "\"");
    RemoveFolder(oShell, oFso, "temp");
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
    RemoveFolder(oShell, oFso, "temp");
    var cvs_path = GetRepository(oShell, cvs_rel_path);
    execute(oShell, "svn xxx checkout " + cvs_path + " temp");
	execute(oShell, "xcopy temp \"" + target_abs_dir + "\" /S /E /Y /C");
    RemoveFolder(oShell, oFso, "temp");
}
