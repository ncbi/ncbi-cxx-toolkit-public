		////////////////////////////////////////////////////////////////////////////////////
		// Shared part of new_project.wsf and import_project.wsf
		
		////////////////////////////////////////////////////////////////////////////////////
		// Utility functions :
		// create cmd, execute command in cmd and redirect output to console stdout
		function execute(oShell, command)
		{
			var oExec = oShell.Exec("cmd /c " + command);
			while(!oExec.StdOut.AtEndOfStream)
				WScript.StdOut.WriteLine(oExec.StdOut.ReadLine());
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
		// check-out a subdir from CVS - oTree is supposed to have TreeRoot property
		function CheckoutSubDir(oShell, oTree, sub_dir)
		{
			var oFso = new ActiveXObject("Scripting.FileSystemObject");
			
			var dir_local_path  = oTree.TreeRoot + "\\" + sub_dir;
			var repository_path = "internal/c++/" + sub_dir;
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
		