(*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Vlad Lebedev
 *
 * File Description:
 * xCode Project Generator Script
 *
 * Know issues:
 * 1) Script build phase should be changed for "gui_project". Use better sed command-line options.
 *)
property ret : "
" -- For some reason standard return does not work.

(* Project starts here *)
global newProject

(* external globals *)
global TheNCBIPath, TheFLTKPath, TheBDBPath, TheSQLPath, ThePCREPath, TheOUTPath
global libTypeDLL, cpuOptimization, zeroLink, fixContinue

(* Hold keys and values for object dictionary of the project *)
global objValues
global objKeys

(* file count: file indexes and references will use this number*)
global srcFileCount

(* all targets go here, as a dependencie for master target: Build All *)
global allDepList, libDepList, appDepList

(* Build settings: You could target older versions of Mac OS X with this settings*)
property buildSettings10_1 : {|MACOSX_DEPLOYMENT_TARGET|:"10.1", |SDKROOT|:"/Developer/SDKs/MacOSX10.1.5.sdk"}
property buildSettings10_2 : {|MACOSX_DEPLOYMENT_TARGET|:"10.2", |SDKROOT|:"/Developer/SDKs/MacOSX10.2.8.sdk"}

(* Build settings for the project *)

property buildSettingsCommon : {|WARNING_CFLAGS|:"-Wno-long-double", |GCC_MODEL_CPU|:"None", |GCC_MODEL_TUNING|:"None", |LIBRARY_SEARCH_PATHS|:"", |GCC_ALTIVEC_EXTENSIONS|:"NO", |PREBINDING|:"NO", |HEADER_SEARCH_PATHS|:"", |ZERO_LINK|:"NO", |GCC_PRECOMPILE_PREFIX_HEADER|:"YES", |OTHER_CPLUSPLUSFLAGS|:"", |GCC_PREFIX_HEADER|:"", |DEAD_CODE_STRIPPING|:"YES", |OBJROOT|:""}
property buildSettingsDevelopment : buildSettingsCommon & {|COPY_PHASE_STRIP|:"NO", |DEBUGGING_SYMBOLS|:"YES", |GCC_DYNAMIC_NO_PIC|:"NO", |GCC_ENABLE_FIX_AND_CONTINUE|:"NO", |GCC_GENERATE_DEBUGGING_SYMBOLS|:"YES", |GCC_OPTIMIZATION_LEVEL|:"0", |OPTIMIZATION_CFLAGS|:"-O0", |GCC_PREPROCESSOR_DEFINITIONS|:"NCBI_XCODE_BUILD _DEBUG _MT"}
property buildSettingsDeployment : buildSettingsCommon & {|COPY_PHASE_STRIP|:"YES", |GCC_ENABLE_FIX_AND_CONTINUE|:"NO", |DEPLOYMENT_POSTPROCESSING|:"YES", |GCC_PREPROCESSOR_DEFINITIONS|:"NCBI_XCODE_BUILD _MT"}

(* Build styles for the project *)
property buildStyleDevelopment : {isa:"PBXBuildStyle", |name|:"Development", |buildRules|:{}, |buildSettings|:buildSettingsDevelopment}
property buildStyleDeployment : {isa:"PBXBuildStyle", |name|:"Deployment", |buildRules|:{}, |buildSettings|:buildSettingsDeployment}
property projectBuildStyles : {"BUILDSTYLE__Development", "BUILDSTYLE__Deployment"}


(* Root Objects, project and main group *)
property rootObject : {isa:"PBXProject", |hasScannedForEncodings|:"1", |mainGroup|:"MAINGROUP", |targets|:{}, |buildSettings|:{|name|:"NCBI"}, |buildStyles|:{}}
property mainGroup : {isa:"PBXGroup", children:{"HEADERS", "SOURCES", "FRAMEWORKS"}, |name|:"NCBI C++ Toolkit", |refType|:"4"}
property emptyProject : {|rootObject|:"ROOT_OBJECT", |archiveVersion|:"1", |objectVersion|:"39", objects:{}}

(* Convinience Groups *)
property sources : {isa:"PBXGroup", children:{}, |name|:"Sources", |refType|:"4"}
property fworks : {isa:"PBXGroup", children:{}, |name|:"External Frameworks", |refType|:"4"}

(* Empty templates for the variouse things in the project *)
property toolProduct : {isa:"PBXExecutableFileReference", |explicitFileType|:"compiled.mach-o.executable", |refType|:"3"}
property libProduct : {isa:"PBXLibraryReference", |explicitFileType|:"compiled.mach-o.dylib", |refType|:"3"}
property appProduct : {isa:"PBXFileReference", |explicitFileType|:"wrapper.application", |refType|:"3"}




script ProjBuilder
	on Initialize()
		
		set libraryPath to ""
		set headerPath to TheNCBIPath & "/include/util/regexp" -- for local pcre library
		set headerPath to headerPath & " " & TheNCBIPath & "/include"
		
		set pack to {TheFLTKPath, TheBDBPath, TheSQLPath, ThePCREPath}
		
		repeat with p in pack -- add all paths to headers
			set headerPath to headerPath & " " & p & "/include"
			set libraryPath to libraryPath & " " & p & "/lib"
		end repeat
		
		set libraryPath to libraryPath & " " & TheOUTPath & "/lib"
		
		set |HEADER_SEARCH_PATHS| of buildSettingsDevelopment to headerPath
		set |HEADER_SEARCH_PATHS| of buildSettingsDeployment to headerPath
		
		set |LIBRARY_SEARCH_PATHS| of buildSettingsDevelopment to libraryPath
		set |LIBRARY_SEARCH_PATHS| of buildSettingsDeployment to libraryPath
		
		set PCH to x_Replace(TheNCBIPath, ":", "/") & "/include/ncbi_pch.hpp"
		set |GCC_PREFIX_HEADER| of buildSettingsDevelopment to PCH
		set |GCC_PREFIX_HEADER| of buildSettingsDeployment to PCH
		
		(* no-permissive flag for GCC *)
		set |OTHER_CPLUSPLUSFLAGS| of buildSettingsDevelopment to "-fno-permissive"
		set |OTHER_CPLUSPLUSFLAGS| of buildSettingsDevelopment to "-fno-permissive"
		
		(* Output directories and intermidiate files (works staring xCode 1.5) *)
		set |OBJROOT| of buildSettingsDevelopment to TheOUTPath
		set |OBJROOT| of buildSettingsDeployment to TheOUTPath
		
		(* Set other options *)
		if zeroLink then
			set |ZERO_LINK| of buildSettingsDevelopment to "YES"
		end if
		
		if fixContinue then
			set |GCC_ENABLE_FIX_AND_CONTINUE| of buildSettingsDevelopment to "YES"
		end if
		
		if cpuOptimization then
			log "Getting CPU type"
			set cpuType to do shell script "/usr/sbin/ioreg | grep PowerPC | awk '{print $3}'| cut -c 9-10"
			if cpuType contains "G5" then
				set |GCC_MODEL_TUNING| of buildSettingsDevelopment to "G5"
				set |GCC_MODEL_CPU| of buildSettingsDevelopment to "G5"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsDevelopment to "YES"
				set |GCC_MODEL_TUNING| of buildSettingsDeployment to "G5"
				set |GCC_MODEL_CPU| of buildSettingsDeployment to "G5"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsDeployment to "YES"
			else if cpuType contains "G4" then
				set |GCC_MODEL_TUNING| of buildSettingsDevelopment to "G4"
				set |GCC_MODEL_CPU| of buildSettingsDevelopment to "G4"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsDevelopment to "YES"
				set |GCC_MODEL_TUNING| of buildSettingsDeployment to "G4"
				set |GCC_MODEL_CPU| of buildSettingsDeployment to "G4"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsDeployment to "YES"
			else
				set |GCC_MODEL_TUNING| of buildSettingsDevelopment to "G3"
				set |GCC_MODEL_CPU| of buildSettingsDevelopment to "G3"
				set |GCC_MODEL_TUNING| of buildSettingsDeployment to "G3"
				set |GCC_MODEL_CPU| of buildSettingsDeployment to "G3"
			end if
			log cpuType
		end if
		
		set newProject to emptyProject
		set objValues to {}
		set objKeys to {}
		
		set srcFileCount to 1
		set allDepList to {}
		set libDepList to {}
		set appDepList to {}
		
		set |buildStyles| of rootObject to projectBuildStyles
		
		addPair(mainGroup, "MAINGROUP")
		addPair(buildStyleDevelopment, "BUILDSTYLE__Development")
		addPair(buildStyleDeployment, "BUILDSTYLE__Deployment")
		log "Done initialize ProjBuilder"
	end Initialize
	
	
	on MakeNewTarget(target_info, src_files, aTarget, aProduct, aType)
		set tgName to name of target_info
		set fullTargetName to tgName
		if aType is equal to 0 then set fullTargetName to "lib" & tgName -- Library
		set targetName to "TARGET__" & tgName
		set targetProxy to "PROXY__" & tgName
		set buildPhaseName to "BUILDPHASE__" & tgName
		set prodRefName to "PRODUCT__" & tgName
		set depName to "DEPENDENCE__" & tgName
		
		set targetDepList to {} -- dependencies for this target
		try -- set dependencies (if any)
			set depString to dep of target_info
			set depList to x_Str2List(depString)
			
			repeat with d in depList
				if d is "datatool" then copy "DEPENDENCE__" & d to the end of targetDepList
			end repeat
		end try
		
		copy depName to the end of allDepList -- store a dependency for use in master target: Build All
		if aType is equal to 0 then copy depName to the end of libDepList -- Library
		if aType is equal to 1 then copy depName to the end of appDepList -- Application
		--if aType equals 2 then copy depName to the end of testDepList -- Tests
		
		-- Add to proper lists
		copy targetName to the end of |targets| of rootObject
		copy tgName to the end of children of sources
		
		set libDepend to {isa:"PBXTargetDependency", |target|:targetName} --, |targetProxy|:targetProxy}
		--set aProxy to {isa:"PBXContainerItemProxy", |proxyType|:"1", |containerPortal|:"ROOT_OBJECT", |remoteInfo|:fullTargetName} --|remoteGlobalIDString|:targetName}
		
		set buildFileRefs to {}
		set libFileRefs to {}
		repeat with f in src_files
			set nameRef to "FILE" & srcFileCount
			set nameBuild to "REF_FILE" & srcFileCount
			
			set filePath to f --"/" & x_Replace(f, ":", "/") -- f will contain something like "users:vlad:c++:src:corelib:ncbicore.cpp"
			set fileName to x_FileNameFromPath(f)
			if fileName ends with ".cpp" then
				set fileType to "sourcecode.cpp.cpp"
			else
				set fileType to "sourcecode.c.c"
			end if
			
			set fileRef to {isa:"PBXFileReference", |lastKnownFileType|:fileType, |name|:fileName, |path|:filePath, |sourceTree|:"<absolute>"}
			set fileBuild to {isa:"PBXBuildFile", |fileRef|:nameRef}
			
			addPair(fileRef, nameRef)
			addPair(fileBuild, nameBuild)
			copy nameBuild to the end of buildFileRefs
			copy nameRef to the end of libFileRefs
			
			set srcFileCount to srcFileCount + 1
			--if srcFileCount = 3 then exit repeat
			--log f
		end repeat
		
		
		set libGroup to {isa:"PBXGroup", |name|:tgName, children:libFileRefs, |refType|:"4"}
		set aBuildPhase to {isa:"PBXSourcesBuildPhase", |files|:buildFileRefs}
		
		set |productReference| of aTarget to prodRefName
		set dependencies of aTarget to targetDepList
		
		-- Go through the lis of libraries and chech ASN1 dependency. Add a script phase and generate a datatool dep for each one of it.
		try -- generated from ASN?
			set subLibs to the libs of target_info
			set needDatatoolDep to false
			set ASNPaths to {}
			set ASNNames to {}
			
			repeat with theSubLib in subLibs
				try
					set asn1 to asn1 of theSubLib
					set needDatatoolDep to true
					set oneAsnPath to path of theSubLib
					set oneAsnName to last word of oneAsnPath
					try
						set oneAsnName to asn1Name of theSubLib -- have a special ASN name?
					end try
					
					copy oneAsnPath to the end of ASNPaths -- store all paths to asn files
					copy oneAsnName to the end of ASNNames -- store all names of asn files; Use either folder name or ASN1Name if provided
				end try
			end repeat
			
			if needDatatoolDep then -- add dependency to a data tool (when asn1 is true for at least sublibrary)
				copy "DEPENDENCE__datatool" to the end of dependencies of aTarget
				
				set shellScript to x_GenerateDatatoolScript(ASNPaths, ASNNames)
				--log shellScript
				
				-- Now add a new script build phase to regenerate dataobjects
				set scriptPhaseName to "SCRIPTPHASE__" & tgName
				set aScriptPhase to {isa:"PBXShellScriptBuildPhase", |files|:{}, |inputPaths|:{}, |outputPaths|:{}, |shellPath|:"/bin/sh", |shellScript|:shellScript}
				
				copy scriptPhaseName to the beginning of |buildPhases| of aTarget -- shell script phase goes first (before compiling)
				addPair(aScriptPhase, scriptPhaseName)
			end if
		end try
		
		
		(* Create a shell script phase to copy GBENCH Resources here *)
		try
			set tmp to gbench of target_info
			set shellScript to x_CopyGBENCHResourses()
			set scriptPhaseName to "SCRIPTPHASE__" & tgName
			set aScriptPhase to {isa:"PBXShellScriptBuildPhase", |files|:{}, |inputPaths|:{}, |outputPaths|:{}, |shellPath|:"/bin/sh", |shellScript|:shellScript}
			
			copy scriptPhaseName to the end of |buildPhases| of aTarget -- shell script phase goes first (before compiling)
			addPair(aScriptPhase, scriptPhaseName)
		end try -- Create a GBENCH Resources
		
		
		-- add to main object list
		addPair(aTarget, targetName)
		--addPair(aProxy, targetProxy)
		addPair(libDepend, depName)
		addPair(aProduct, prodRefName)
		addPair(aBuildPhase, buildPhaseName)
		
		addPair(libGroup, tgName)
	end MakeNewTarget
	
	(*
    TOOL="$INSTALL_PATH/app_datatool"
echo Updating $PRODUCT_NAME
$TOOL -m /Users/lebedev/tmp/access.asn -M "" -oA -of /Users/lebedev/tmp/access.files  -oc access -odi -od /Users/lebedev/tmp/access.def
*)
	
	on MakeNewLibraryTarget(lib_info, src_files)
		set libName to name of lib_info
		set targetName to "TARGET__" & libName
		set buildPhaseName to "BUILDPHASE__" & libName
		
		set installPath to TheOUTPath & "/lib" --"/Users/lebedev/Projects/tmp3"
		set linkerFlags to "-flat_namespace -undefined suppress" -- warning -- additional liker flags (like -lxncbi)
		set symRoot to TheOUTPath & "/lib"
		
		-- build DLLs by default
		set libraryStyle to "DYNAMIC"
		set fullLibName to "lib" & libName
		set libProdType to "com.apple.product-type.library.dynamic"
		
		set isBundle to false
		try -- are we building a loadable module?
			if bundle of lib_info then set libraryStyle to "BUNDLE"
			set linkerFlags to "" -- do not suppress undefined symbols. Bundles should be fully resolved
			--set linkerFlags to "-framework Carbon -framework AGL -framework OpenGL"
			set symRoot to TheOUTPath & "/bin/Genome Workbench.app/Contents/MacOS/plugins"
			set isBundle to true
		end try
		
		if libTypeDLL is false and isBundle is false then -- build as static			
			set libraryStyle to "STATIC"
			set fullLibName to libName
			set libProdType to "com.apple.product-type.library.static"
		end if
		
		
		set linkerFlags to linkerFlags & x_CreateLinkerFlags(lib_info) -- additional liker flags (like -lxncbi)
		
		set buildSettings to {|LIB_COMPATIBILITY_VERSION|:"1", |DYLIB_CURRENT_VERSION|:"1", |INSTALL_PATH|:installPath, |LIBRARY_STYLE|:libraryStyle, |PRODUCT_NAME|:fullLibName, |OTHER_LDFLAGS|:linkerFlags, |SYMROOT|:symRoot}
		set libTarget to {isa:"PBXNativeTarget", |buildPhases|:{buildPhaseName}, |buildSettings|:buildSettings, |name|:fullLibName, |productReference|:"", |productType|:libProdType, dependencies:{}}
		
		my MakeNewTarget(lib_info, src_files, libTarget, libProduct, 0) -- 0 is library
	end MakeNewLibraryTarget
	
	
	on MakeNewToolTarget(tool_info, src_files)
		set toolName to name of tool_info
		set targetName to "TARGET__" & toolName
		set buildPhaseName to "BUILDPHASE__" & toolName
		set fullToolName to toolName -- "app_" & toolName
		
		set linkerFlags to x_CreateLinkerFlags(tool_info) -- additional liker flags (like -lxncbi)
		
		set symRoot to TheOUTPath & "/bin"
		if toolName is "gbench_plugin_scan" then set symRoot to TheOUTPath & "/bin/Genome Workbench.app/Contents/MacOS"
		
		set buildSettings to {|PRODUCT_NAME|:fullToolName, |OTHER_LDFLAGS|:linkerFlags, |SYMROOT|:symRoot}
		set toolTarget to {isa:"PBXNativeTarget", |buildPhases|:{buildPhaseName}, |buildSettings|:buildSettings, |name|:fullToolName, |productReference|:"", |productType|:"com.apple.product-type.tool", dependencies:{}}
		
		my MakeNewTarget(tool_info, src_files, toolTarget, toolProduct, 2) -- is a tool
	end MakeNewToolTarget
	
	
	
	on MakeNewAppTarget(app_info, src_files)
		set appName to name of app_info
		set targetName to "TARGET__" & appName
		set buildPhaseName to "BUILDPHASE__" & appName
		--set fullAppName to "app_" & appName
		
		set linkerFlags to x_CreateLinkerFlags(app_info) -- additional liker flags (like -lxncbi)
		
		set symRoot to TheOUTPath & "/bin"
		set buildSettings to {|PRODUCT_NAME|:appName, |OTHER_LDFLAGS|:linkerFlags, |REZ_EXECUTABLE|:"YES", |INFOPLIST_FILE|:"", |SYMROOT|:symRoot}
		set appTarget to {isa:"PBXNativeTarget", |buildPhases|:{buildPhaseName}, |buildSettings|:buildSettings, |name|:appName, |productReference|:"", |productType|:"com.apple.product-type.application", dependencies:{}}
		
		my MakeNewTarget(app_info, src_files, appTarget, appProduct, 1) -- 1 is application
	end MakeNewAppTarget
	
	
	
	
	(* Save everything *)
	on SaveProjectFile()
		(* Target: Build Everything *)
		copy "TARGET__BUILD_APP" to the beginning of |targets| of rootObject
		addPair({isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{none:""}, dependencies:appDepList, |name|:"Build All Applications"}, "TARGET__BUILD_APP")
		copy "TARGET__BUILD_LIB" to the beginning of |targets| of rootObject
		addPair({isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{none:""}, dependencies:libDepList, |name|:"Build All Libraries"}, "TARGET__BUILD_LIB")
		copy "TARGET__BUILD_ALL" to the beginning of |targets| of rootObject
		addPair({isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{none:""}, dependencies:allDepList, |name|:"Build All"}, "TARGET__BUILD_ALL")
		
		(* add frameworks*)
		-- Carbon
		copy "FW_CARBON" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"Carbon.framework", |path|:"/System/Library/Frameworks/Carbon.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_CARBON")
		-- AGL
		copy "FW_AGL" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"AGL.framework", |path|:"/System/Library/Frameworks/AGL.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_AGL")
		-- OpenGL
		copy "FW_OpenGL" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"OpenGL.framework", |path|:"/System/Library/Frameworks/OpenGL.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_OpenGL")
		-- AppServices
		copy "FW_AppServices" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"ApplicationServices.framework", |path|:"/System/Library/Frameworks/ApplicationServices.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_AppServices")
		
		(* Add ROOT objects and groups *)
		addPair(rootObject, "ROOT_OBJECT")
		addPair(sources, "SOURCES")
		addPair(fworks, "FRAMEWORKS")
		
		
		(* Create a record from two lists *)
		set objects of newProject to CreateRecordFromList(objValues, objKeys)
		
		try -- create some folders
			set shScript to "if test ! -d " & TheOUTPath & "/NCBI.xcode ; then mkdir " & TheOUTPath & "/NCBI.xcode ; fi"
			do shell script shScript
		end try
		
		set fullProjName to (TheOUTPath & "/NCBI.xcode/project.pbxproj") as string
		(* Call NSDictionary method to save data as XML property list *)
		--call method "writeToFile:atomically:" of newProject with parameters {"/Users/lebedev/111.txt", "YES"}
		--call method "writeToFile:atomically:" of newProject with parameters {"/Users/lebedev/!test.xcode/project.pbxproj", "YES"}
		
		call method "writeToFile:atomically:" of newProject with parameters {fullProjName, "YES"}
		(*set the_file to open for access "users:lebedev:111.txt"
		set hhh to read the_file as string
		log hhh
		close access the_file*)
	end SaveProjectFile
	
	
	(* Convinience method *)
	on addPair(aObj, aKey)
		copy aObj to the end of objValues
		copy aKey to the end of objKeys
	end addPair
	
	(* Workaround the AppleScript lack of variable property labels *)
	on CreateRecordFromList(objs, keys)
		return call method "dictionaryWithObjects:forKeys:" of class "NSDictionary" with parameters {objs, keys}
	end CreateRecordFromList
	
	(*  Replace all occurances of "old" in the aString with "new" *)
	on x_Replace(aString, old, new)
		set OldDelims to AppleScript's text item delimiters
		set AppleScript's text item delimiters to old
		set newText to text items of aString
		set AppleScript's text item delimiters to new
		set finalText to newText as text
		set AppleScript's text item delimiters to OldDelims
		
		return finalText
	end x_Replace
	
	
	(* convert a space-separated "aString" to a list *)
	on x_Str2List(aString)
		set OldDelims to AppleScript's text item delimiters
		set AppleScript's text item delimiters to " "
		set aList to every text item in aString
		set AppleScript's text item delimiters to OldDelims
		return aList
	end x_Str2List
	
	
	(* Get file name and extension from the full file path *)
	on x_FileNameFromPath(fileNameWithPath)
		set OldDelims to AppleScript's text item delimiters
		set AppleScript's text item delimiters to "/"
		set partsList to text items of fileNameWithPath
		set AppleScript's text item delimiters to OldDelims
		return the last item of partsList as string
	end x_FileNameFromPath
	
	
	(* Create a linker flags string based on dependencies and libs to link *)
	on x_CreateLinkerFlags(info)
		set linkFlags to ""
		try -- set dependencies (if any)
			set depString to dep of info
			set depList to x_Str2List(depString)
			
			repeat with d in depList
				set linkFlags to linkFlags & " -l" & d
			end repeat
		end try
		
		try -- set libraries to link (if any)
			set libList to lib2link of info
			repeat with d in libList
				set linkFlags to linkFlags & " -l" & d
			end repeat
		end try
		
		set linkFlags to linkFlags & " -framework Carbon -framework AGL -framework OpenGL"
		return linkFlags
	end x_CreateLinkerFlags
	
	
	(* Creates a shell script to regenerate ASN files *)
	on x_GenerateDatatoolScript(thePaths, theNames)
		set theScript to "echo Updating $PRODUCT_NAME" & ret
		set theScript to theScript & "" & ret
		set idx to 1
		repeat with aPath in thePaths
			set asnName to item idx of theNames
			set posixPath to x_Replace(aPath, ":", "/")
			
			set fullPath to TheNCBIPath & "/src/" & posixPath
			
			set theScript to theScript & "echo Working in: " & fullPath & ret
			set theScript to theScript & "cd " & fullPath & ret
			
			set theScript to theScript & "if ! test -e " & asnName & ".files || find . -newer " & asnName & ".files | grep '.asn'; then" & ret
			set theScript to theScript & "  m=\"" & asnName & "\"" & ret
			
			set theScript to theScript & "  echo Running Datatool" & ret
			if asnName is "gui_project" or asnName is "plugin" then -- Should use sed properly here (but how?)
				set theScript to theScript & "  M=\"$(grep ^MODULE_IMPORT $m.module | sed 's/^.*= *//' | sed 's/\\([/a-z0-9_]*\\)/\\1.asn/g')\"" & ret
			else
				set theScript to theScript & "  M=\"$(grep ^MODULE_IMPORT $m.module | sed 's/^.*= *//' | sed 's/\\(objects[/a-z0-9]*\\)/\\1.asn/g')\"" & ret
			end if
			
			set theScript to theScript & "  " & TheOUTPath & "/bin/datatool -oR " & TheNCBIPath
			(*if asnName is "gui_project" then
				set theScript to theScript & " -opm " & TheNCBIPath & "/src  -m \"$m.asn\" -M \"$M\" -oA -of \"$m.files\" -or \"gui/core\" -oc \"$m\" -oex '' -ocvs -odi -od \"$m.def\"" & ret
			else if asnName is "gui_dlg_seq_feat_edit" then
				set theScript to theScript & " -opm " & TheNCBIPath & "/src  -m \"$m.asn\" -M \"$M\" -oA -of \"$m.files\" -or \"gui/dialogs/edit/feature\" -oc \"$m\" -oex '' -ocvs -odi -od \"$m.def\"" & ret
			else *)
			set theScript to theScript & " -opm " & TheNCBIPath & "/src  -m \"$m.asn\" -M \"$M\" -oA -of \"$m.files\" -or \"" & posixPath & "\" -oc \"$m\" -oex '' -ocvs -odi -od \"$m.def\"" & ret
			--end if
			set theScript to theScript & "else" & ret
			set theScript to theScript & "  echo ASN files are up to date" & ret
			set theScript to theScript & "fi" & ret & ret
			
			set idx to idx + 1
		end repeat
		
		return theScript
	end x_GenerateDatatoolScript
	
	
	(* Creates a shell script to copy some additional files into GBENCH package *)
	(* Can be replaced with Copy Files Build phase *)
	on x_CopyGBENCHResourses()
		set theScript to ""
		set theScript to theScript & "echo Running GBench Plugin Scan" & ret
		set theScript to theScript & "if test ! -e " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/plugins/plugin-cache ; then" & ret
		set theScript to theScript & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/gbench_plugin_scan " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/plugins" & ret
		set theScript to theScript & "fi" & ret
		
		set theScript to theScript & "echo Copying GBench resources" & ret
		
		-- Create etc directory
		set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc ; then" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		set theScript to theScript & "fi" & ret
		
		-- Create share directory
		set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/share/gbench ; then" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/share" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/share/gbench" & ret
		set theScript to theScript & "fi" & ret
		
		-- Create executables directory
		set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/executables ; then" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/executables" & ret
		set theScript to theScript & "fi" & ret
		
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/plugins/algo/executables/* " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/executables" & ret
		
		-- copy png images
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/res/share/gbench/* " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/share/gbench" & ret
		
		set theScript to theScript & "cp -r " & TheNCBIPath & "/src/gui/plugins/algo/executables " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/executables" & ret
		
		set theScript to theScript & "cp -r " & TheNCBIPath & "/src/gui/res/etc/* " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		--set theScript to theScript & "cp -r " & TheNCBIPath & "/src/gui/gbench/patterns/ " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc/patterns" & ret
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/gbench/news.ini " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/gbench/gbench.ini " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/gbench/algo_urls " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		
		return theScript
	end x_CopyGBENCHResourses
end script


(*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2005/03/10 14:22:55  lebedev
 * Link warnings fixed (no more pcre multiple defined symbols)
 *
 * Revision 1.21  2005/03/08 14:34:57  lebedev
 * A default -fno-permissive GCC flag added
 *
 * Revision 1.20  2004/12/21 13:21:56  lebedev
 * Options for multithreaded builds added
 *
 * Revision 1.19  2004/11/24 16:30:09  lebedev
 * gbench_plugin_scan moved inside Genome Workbench package
 *
 * Revision 1.18  2004/11/12 16:51:34  lebedev
 * New location of resources for GBench
 *
 * Revision 1.17  2004/11/04 12:37:37  lebedev
 * Datatool phase twicked: ignore underscopes in asn files names
 *
 * Revision 1.16  2004/09/30 17:42:53  lebedev
 * _DEBUG flag added for Development build
 *
 * Revision 1.15  2004/09/30 12:59:01  lebedev
 * Renamed: gbench -> Genome Workbench
 *
 * Revision 1.14  2004/09/24 12:26:30  lebedev
 * ASN generation build phase improvements and cleanup.
 *
 * Revision 1.13  2004/09/13 11:46:15  lebedev
 * Allways specify frameworks in the lnk path (A Framework Build Phase sometimes does not work)
 *
 * Revision 1.12  2004/09/03 14:35:21  lebedev
 * Explicitly link frameworks for bundles (gbench plugins)
 *
 * Revision 1.11  2004/08/13 11:41:43  lebedev
 * Changes to upgrade to xCode 1.5 and support for G5 CPU specific options
 *
 * Revision 1.10  2004/08/10 15:22:07  lebedev
 * Simplify target dependencies for xCode 1.5
 *
 * Revision 1.9  2004/08/06 15:26:59  lebedev
 * Handle ncbicfg.c.in correctly
 *
 * Revision 1.8  2004/07/29 13:16:04  lebedev
 * demo for cross alignment added
 *
 * Revision 1.7  2004/07/26 18:15:37  lebedev
 * Use proper extension for projects (xcode instead of xCode)
 *
 * Revision 1.6  2004/07/08 18:43:47  lebedev
 * Set the required lastKnownFileType for source files
 *
 * Revision 1.5  2004/07/07 18:34:26  lebedev
 * Datatool script build phase clean-up
 *
 * Revision 1.4  2004/07/06 15:31:36  lebedev
 * Datatool script build phase fixed for gui/plugin
 *
 * Revision 1.3  2004/06/25 15:11:52  lebedev
 * DEPLOYMENT_POSTPROCESSING set to YES in Deployment build. Missing space before -opm datatool flag added
 *
 * Revision 1.2  2004/06/24 15:32:51  lebedev
 * datatool generation parameters changed for gui_project
 *
 * Revision 1.1  2004/06/23 17:09:52  lebedev
 * Initial revision
 * ===========================================================================
 *)