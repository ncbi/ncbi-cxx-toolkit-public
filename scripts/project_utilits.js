////////////////////////////////////////////////////////////////////////////////////
// Shared part of new_project.wsf and import_project.wsf

// global settings
var g_verbose = false;

////////////////////////////////////////////////////////////////////////////////////
// Utility functions :
// create cmd, execute command in cmd and redirect output to console stdou2t
function execute(oShell, command)
{
    VerboseEcho("+  " + command);
    var oExec = oShell.Exec("cmd /c " + command);
    while( oExec.Status == 0 && !oExec.StdOut.AtEndOfStream )
        WScript.StdOut.WriteLine(oExec.StdOut.ReadLine());

    while (oExec.Status == 0) // waiting for task to finish
        WScript.Sleep(100);

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
    this.CompilersBranch       = this.TreeRoot + "\\compilers\\msvc710_prj";
    this.CompilersBranchStatic = this.CompilersBranch + "\\static";
    this.BinPathStatic         = this.CompilersBranchStatic + "\\bin";
    this.CompilersBranchDll    = this.CompilersBranch + "\\dll";
    this.BinPathDll            = this.CompilersBranchDll + "\\bin";

    this.IncludeRootBranch     = this.TreeRoot + "\\include";
    this.IncludeConfig         = this.IncludeRootBranch + "\\corelib\\config";
    this.IncludeProjectBranch  = this.IncludeRootBranch + "\\" + BackSlashes(oTask.ProjectName);

    this.SrcRootBranch         = this.TreeRoot + "\\src";
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
    VerboseEcho("SrcProjectBranch      = " + oTree.SrcProjectBranch);
}

// build configurations -  object oTask is supposed to have DllBuild property
function GetConfigs(oTask)
{
    if (oTask.DllBuild) {
        var configs = new Array ("DebugDLL", 
                                 "ReleaseDLL");
        return configs;
    } else {
        var configs = new Array ("Debug", 
                                 "DebugMT", 
                                 "DebugDLL", 
                                 "Release", 
                                 "ReleaseMT", 
                                 "ReleaseDLL");
        return configs;
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
    CreateFolderIfAbsent(oFso, oTree.IncludeProjectBranch  );

    CreateFolderIfAbsent(oFso, oTree.SrcRootBranch         );
    CreateFolderIfAbsent(oFso, oTree.SrcProjectBranch      );
}

// fill-in tree structure
function FillTreeStructure(oShell, oTree)
{
    var temp_dir = oTree.TreeRoot + "\\temp";
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    // Fill-in infrastructure for the build tree
    GetFileFromTree(oShell, oTree, oTask, "/src/Makefile.in",                                oTree.SrcRootBranch);
    GetFileFromTree(oShell, oTree, oTask, "/src/Makefile.mk.in",                             oTree.SrcRootBranch);
    GetFileFromTree(oShell, oTree, oTask, "/src/Makefile.mk.in.msvc",                        oTree.SrcRootBranch);

    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/Makefile.FLTK.app.msvc",   oTree.CompilersBranch);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/ncbi.rc",                  oTree.CompilersBranch);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/ncbilogo.ico",             oTree.CompilersBranch);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/project_tree_builder.ini", oTree.CompilersBranch);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/winmain.cpp",              oTree.CompilersBranch);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/asn_prebuild.bat",         oTree.CompilersBranch);

    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/dll/dll_info.ini",         oTree.CompilersBranchDll);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/dll/dll_main.cpp",         oTree.CompilersBranchDll);
    GetFileFromTree(oShell, oTree, oTask, "/compilers/msvc710_prj/dll/Makefile.mk",          oTree.CompilersBranchDll);

    GetFileFromTree(oShell, oTree, oTask, "/include/corelib/config/ncbiconf_msvc_site.*",    oTree.IncludeConfig);
}

// check-out a subdir from CVS - oTree is supposed to have TreeRoot property
function CheckoutSubDir(oShell, oTree, sub_dir)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    var dir_local_path  = oTree.TreeRoot + "\\" + sub_dir;
    var repository_path = "" + GetCvsTreeRoot()+"/" + sub_dir;
    var dir_local_path_parent = oFso.GetParentFolderName(dir_local_path);
    var base_name = oFso.GetBaseName(dir_local_path);

    oFso.DeleteFolder(dir_local_path, true);
    execute(oShell, "cd " + BackSlashes(dir_local_path_parent) + " && " + "cvs checkout -d " + base_name + " " + ForwardSlashes(repository_path));
    execute(oShell, "cd " + oTree.TreeRoot);
}

// remove temporary dir ( used for get something for CVS ) 
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
    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path = oTree.BinPathStatic + "\\" + conf;
        var source_file = oTask.ToolkitPath + "\\bin" + "\\project_tree_builder.exe";
        if (!oFso.FileExists(source_file)) {
            source_file = oTask.ToolkitPath + "\\static\\bin"+ "\\" + conf + "\\project_tree_builder.exe";
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
/*
// Collect files names with particular extension
function CollectFileNames(dir_path, ext)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var file_names = new Array;
    if ( oFso.FolderExists(dir_path) ) {
        var dir_folder = oFso.GetFolder(dir_path);
        var dir_folder_contents = new Enumerator(dir_folder.files);
        var names_i = 0;
        for( ; !dir_folder_contents.atEnd(); dir_folder_contents.moveNext()) {
            var file_path = dir_folder_contents.item();
            if (oFso.GetExtensionName(file_path) == ext) {
                file_names[names_i] = oFso.GetBaseName(file_path);
                names_i++;
            }
        }
    }
    return file_names;
}
function CollectDllLibs(oTask, dll_names)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var dll_lib_names = new Array;
    var dll_lib_i = 0;
    for(var dll_i = 0; dll_i < dll_names.length; dll_i++) {
        var dll_base_name = dll_names[dll_i];
        var dll_lib_path = oTask.ToolkitPath + "\\DebugDLL\\" + dll_base_name + ".lib";
        if ( !oFso.FileExists(dll_lib_path) ) {
            dll_lib_path = oTask.ToolkitPath + "\\lib\\DebugDLL\\" + dll_base_name + ".lib";
        }
        if ( oFso.FileExists(dll_lib_path) ) {
            dll_lib_names[dll_lib_i] = dll_base_name;
            dll_lib_i++;
        }
    }
    return dll_lib_names;
}
// Local site should contain C++ Toolkit information as a third-party library
// this one is for dll build
function AdjustLocalSiteDll(oShell, oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    // open for appending
    var ptb_ini = oTree.CompilersBranch + "\\project_tree_builder.ini";
    VerboseEcho("Modifying (appending): " + ptb_ini);
    var file = oFso.OpenTextFile(ptb_ini, 8);
    file.WriteLine("[CXX_Toolkit]");
    var folder_root = oTask.ToolkitSrcPath;
    var folder_include = folder_root + "\\include";
    while (!oFso.FolderExists(folder_include)) {
        VerboseEcho("Folder not found: " + folder_include);
        folder_root = oFso.GetParentFolderName(folder_root)
        if (folder_root == "") {
            break;
        }
        folder_include = folder_root + "\\include";
    }
    file.WriteLine("INCLUDE = " + EscapeBackSlashes(folder_include));
    file.WriteLine("LIBPATH = ");

    var libpath_prefix = "";
    var dll_names = CollectFileNames(oTask.ToolkitPath + "\\DebugDLL", "dll");
    if (dll_names.length == 0) {
        libpath_prefix = "\\lib";
        dll_names = CollectFileNames(oTask.ToolkitPath + "\\lib\\DebugDLL", "dll");
    }
    // we'll add only dll libraries for these .lib is available
    var dll_libs = CollectDllLibs(oTask, dll_names);
    if (dll_libs.length > 0) {
        file.WriteLine("LIB     = \\");
    }
    for(var lib_i = 0; lib_i < dll_libs.length; lib_i++) {
        var lib_base_name = dll_libs[lib_i];
        if (lib_i != dll_libs.length-1) {
            file.WriteLine("        " + lib_base_name + ".lib \\");            
        } else {
            // the last line
            file.WriteLine("        " + lib_base_name + ".lib");            
        }
    }

    file.WriteLine("CONFS   = DebugDLL ReleaseDLL");
    file.WriteLine("[CXX_Toolkit.debug.DebugDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + libpath_prefix + "\\DebugDLL"));
    file.WriteLine("[CXX_Toolkit.release.ReleaseDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + libpath_prefix + "\\ReleaseDLL"));

    file.Close();       
}
// for static build
function AdjustLocalSiteStatic(oShell, oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    // open for appending
    var ptb_ini = oTree.CompilersBranch + "\\project_tree_builder.ini";
    VerboseEcho("Modifying (appending): " + ptb_ini);
    var file = oFso.OpenTextFile(ptb_ini, 8)
        file.WriteLine("[CXX_Toolkit]");
    var folder_root = oTask.ToolkitSrcPath;
    var folder_include = folder_root + "\\include";
    while (!oFso.FolderExists(folder_include)) {
        VerboseEcho("Folder not found: " + folder_include);
        folder_root = oFso.GetParentFolderName(folder_root)
        if (folder_root == "") {
            break;
        }
        folder_include = folder_root + "\\include";
    }
    file.WriteLine("INCLUDE = " + EscapeBackSlashes(folder_include));
    file.WriteLine("LIBPATH = ");

    var libpath_prefix = "";
    var static_libs = CollectFileNames(oTask.ToolkitPath + "\\Debug", "lib");
    if (static_libs.length == 0) {
        libpath_prefix = "\\lib";
        static_libs = CollectFileNames(oTask.ToolkitPath + "\\lib\\Debug", "lib");
    }
    if (static_libs.length > 0) {
        file.WriteLine("LIB     = \\");
    }
    for(var lib_i = 0; lib_i < static_libs.length; lib_i++) {
        var lib_base_name = static_libs[lib_i];
        if (lib_i != static_libs.length-1) {
            file.WriteLine("        " + lib_base_name + ".lib \\");            
        } else {
            // the last line
            file.WriteLine("        " + lib_base_name + ".lib");            
        }
    }

    file.WriteLine("CONFS   = Debug DebugDLL Release ReleaseDLL");
    file.WriteLine("[CXX_Toolkit.debug.Debug]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + libpath_prefix + "\\Debug"));
    file.WriteLine("[CXX_Toolkit.debug.DebugDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + libpath_prefix + "\\DebugDLL"));
    file.WriteLine("[CXX_Toolkit.release.Release]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + libpath_prefix + "\\Release"));
    file.WriteLine("[CXX_Toolkit.release.ReleaseDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + libpath_prefix + "\\ReleaseDLL"));

    file.Close();       
}
// Add C++ Toolkit to local site
function AdjustLocalSite(oShell, oTree, oTask)
{
    if ( oTask.DllBuild ) {
        AdjustLocalSiteDll(oShell, oTree, oTask);
    } else {
        AdjustLocalSiteStatic(oShell, oTree, oTask);
    }
}
*/
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

function VerboseEcho(message)
{
    if (GetVerbose()) {
        WScript.Echo(message);
    }
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
    return "\\\\snowman\\win-coremake\\Lib\\Ncbi\\CXX_Toolkit\\msvc71";
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
            var dlls_bin_path  = oTask.ToolkitPath + "\\" + config;
            if (!oFso.FolderExists(dlls_bin_path)) {
                dlls_bin_path  = oTask.ToolkitPath + "\\dll\\bin\\" + config;
            }
            var local_bin_path = oTree.BinPathDll  + "\\" + config;

            execute(oShell, "copy /Y \"" + dlls_bin_path + "\\*.dll\" \"" + local_bin_path + "\"");
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
        execute(oShell, "cvs checkout -d temp " + GetCvsTreeRoot()+"/src/gui/res");
        execute(oShell, "copy /Y temp\\*.* \"" + res_target_dir + "\"");
        RemoveFolder(oShell, oFso, "temp");
    } else {
        VerboseEcho("CopyRes:  skipped (not requested)");
    }
}
// CVS tree root
function GetCvsTreeRoot()
{
    return "internal/c++";
}
// Get file from CVS tree
function GetFileFromTree(oShell, oTree, oTask, cvs_rel_path, target_abs_dir)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");

    // Try to get the file from the pre-built toolkit
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

    // Get it from CVS
    RemoveFolder(oShell, oFso, "temp");
    var cvs_path = GetCvsTreeRoot() + cvs_rel_path;
    var cvs_dir = oFso.GetParentFolderName(cvs_path);
    var cvs_file = oFso.GetFileName(cvs_path);
    if (cvs_file.search(/\*/) != -1 || cvs_file.search(/\?/) != -1) {
        execute(oShell, "cvs checkout -l -d temp " + cvs_dir);
    } else {
        execute(oShell, "cvs checkout -d temp " + cvs_path);
    }
    execute(oShell, "copy /Y \"temp\\" + cvs_file + "\" \""+ target_abs_dir + "\"");
    RemoveFolder(oShell, oFso, "temp");
}
