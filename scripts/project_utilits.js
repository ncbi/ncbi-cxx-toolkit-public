////////////////////////////////////////////////////////////////////////////////////
// Shared part of new_project.wsf and import_project.wsf

////////////////////////////////////////////////////////////////////////////////////
// Utility functions :
// create cmd, execute command in cmd and redirect output to console stdou2t
function execute(oShell, command)
{
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
    this.TreeRoot              = oShell.CurrentDirectory;
    this.CompilersBranch       = this.TreeRoot + "\\compilers\\msvc710_prj";
    this.CompilersBranchStatic = this.CompilersBranch + "\\static";
    this.BinPathStatic         = this.CompilersBranchStatic + "\\bin";
    this.CompilersBranchDll    = this.CompilersBranch + "\\dll";
    this.BinPathDll            = this.CompilersBranchDll + "\\bin";

    this.IncludeRootBranch     = this.TreeRoot + "\\include";
    this.IncludeConfig         = this.IncludeRootBranch + "\\corelib\\config";
    this.IncludeProjectBranch  = this.IncludeRootBranch + "\\" + oTask.ProjectName;

    this.SrcRootBranch         = this.TreeRoot + "\\src";
    this.SrcProjectBranch      = this.SrcRootBranch + "\\" + oTask.ProjectName;
}
// diagnostic dump of the tree object
function DumpTree(oTree)
{
    WScript.Echo(oTree.TreeRoot              );
    WScript.Echo(oTree.CompilersBranch       );
    WScript.Echo(oTree.CompilersBranchStatic );
    WScript.Echo(oTree.BinPathStatic         );
    WScript.Echo(oTree.CompilersBranchDll    );

    WScript.Echo(oTree.IncludeRootBranch     );
    WScript.Echo(oTree.IncludeProjectBranch  );

    WScript.Echo(oTree.SrcRootBranch         );
    WScript.Echo(oTree.SrcProjectBranch      );
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
        WScript.Echo("Creating folder: " + path);
        CreateFolderIfAbsent(oFso, oFso.GetParentFolderName(path));
        oFso.CreateFolder(path);
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

    // Fill-in infra-structure for build tree

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

    GetFileFromTree(oShell, oTree, oTask, "/include/corelib/config/ncbiconf_msvc_site.h",    oTree.IncludeConfig);
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
function RemoveTempFolder(oShell, oFso, oTree)
{
    var temp_dir = oTree.TreeRoot + "\\temp";
    if ( oFso.FolderExists(temp_dir) ) {
        execute(oShell, "rmdir /S /Q " + temp_dir);
    }
}
// copy project_tree_builder app to appropriate places of the local tree
function CopyPtb(oShell, oTree, oTask)
{
    var configs = GetConfigs(oTask);
    for(var config_i = 0; config_i < configs.length; config_i++) {
        var conf = configs[config_i];
        var target_path = oTree.BinPathStatic + "\\" + conf;
        execute(oShell, "copy /Y " + oTask.ToolkitPath + "\\bin\\project_tree_builder.exe " + target_path);
    }
}
// copy datatool app to appropriate places of the local tree
function CopyDatatool(oShell, oTree, oTask)
{
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
        execute(oShell, "copy /Y " + oTask.ToolkitPath + "\\bin\\datatool.exe " + target_path);
    }
}
// Collect files names with particular extension
function CollectFileNames(dir_path, ext)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    var dir_folder = oFso.GetFolder(dir_path);
    var dir_folder_contents = new Enumerator(dir_folder.files);
    var file_names = new Array;
    var names_i = 0;
    for( ; !dir_folder_contents.atEnd(); dir_folder_contents.moveNext()) {
        var file_path = dir_folder_contents.item();
        if (oFso.GetExtensionName(file_path) == ext) {
            file_names[names_i] = oFso.GetBaseName(file_path);
            names_i++;
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
    var file = oFso.OpenTextFile(oTree.CompilersBranch + "\\project_tree_builder.ini", 8)
        file.WriteLine("[CXX_Toolkit]");
    file.WriteLine("INCLUDE = " + EscapeBackSlashes(oTask.ToolkitPath + "\\include"));
    file.WriteLine("LIBPATH = ");

    file.WriteLine("LIB     = \\");
    var dll_names = CollectFileNames(oTask.ToolkitPath + "\\DebugDLL", "dll");
    // we'll add only dll libraries for these .lib is available
    var dll_libs = CollectDllLibs(oTask, dll_names);
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
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + "\\DebugDLL"));
    file.WriteLine("[CXX_Toolkit.release.ReleaseDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + "\\ReleaseDLL"));

    file.Close();       
}
// for static build
function AdjustLocalSiteStatic(oShell, oTree, oTask)
{
    var oFso = new ActiveXObject("Scripting.FileSystemObject");
    // open for appending
    var file = oFso.OpenTextFile(oTree.CompilersBranch + "\\project_tree_builder.ini", 8)
        file.WriteLine("[CXX_Toolkit]");
    file.WriteLine("INCLUDE = " + EscapeBackSlashes(oTask.ToolkitPath + "\\include"));
    file.WriteLine("LIBPATH = ");

    file.WriteLine("LIB     = \\");
    var static_libs = CollectFileNames(oTask.ToolkitPath + "\\Debug", "lib");
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
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + "\\Debug"));
    file.WriteLine("[CXX_Toolkit.debug.DebugDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + "\\DebugDLL"));
    file.WriteLine("[CXX_Toolkit.release.Release]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + "\\Release"));
    file.WriteLine("[CXX_Toolkit.release.ReleaseDLL]");
    file.WriteLine("LIBPATH = " + EscapeBackSlashes(oTask.ToolkitPath + "\\ReleaseDLL"));

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
        var configs = GetConfigs(oTask);
        for( var config_i = 0; config_i < configs.length; config_i++ ) {
            var config = configs[config_i];
            var dlls_bin_path  = oTask.ToolkitPath + "\\" + config;
            var local_bin_path = oTree.BinPathDll  + "\\" + config;

            execute(oShell, "copy /Y " + dlls_bin_path + "\\*.dll " + local_bin_path);
        }
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
        execute(oShell, "copy /Y temp\\*.* " + res_target_dir);
        RemoveTempFolder(oShell, oFso, oTree);
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

    // 1. Try to get file from pre-buit toolkit first
    var toolkit_file_path = BackSlashes(oTask.ToolkitPath + cvs_rel_path);
    if ( oFso.FileExists(toolkit_file_path) ) {
        oFso.CopyFile(toolkit_file_path, target_abs_dir + "\\", true);
        return;
    }

    // 2. Last attempt - get it from CVS tree.
    RemoveTempFolder(oShell, oFso, oTree);
    execute(oShell, "cvs checkout -d temp " + GetCvsTreeRoot()+ cvs_rel_path);
    execute(oShell, "copy /Y temp\\*.* " + target_abs_dir);
    RemoveTempFolder(oShell, oFso, oTree);
}


