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

property upper_alphabet : "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
property ret : "
" -- For some reason standard return does not work.

(* Project starts here *)
global newProject

(* external globals *)
global TheNCBIPath, TheFLTKPath, TheBDBPath, ThePCREPath, TheOUTPath
global libTypeDLL, cpuOptimization, zeroLink, fixContinue, xcodeVersion, projFile

(**)

(* Hold keys and values for object dictionary of the project *)
global objValues
global objKeys

(* file count: file indexes and references will use this number*)
global refFileCount

(* all targets go here, as a dependencie for master target: Build All *)
global allDepList, libDepList, appDepList

(* Build settings: You could target older versions of Mac OS X with this settings*)
property buildSettings10_1 : {|MACOSX_DEPLOYMENT_TARGET|:"10.1", |SDKROOT|:"/Developer/SDKs/MacOSX10.1.5.sdk"}
property buildSettings10_2 : {|MACOSX_DEPLOYMENT_TARGET|:"10.2", |SDKROOT|:"/Developer/SDKs/MacOSX10.2.8.sdk"}

(* Build settings for the project *)

property buildSettingsCommon : {|GCC_MODEL_CPU|:"|none|", |GCC_MODEL_TUNING|:"|none|", |FRAMEWORK_SEARCH_PATHS|:"/System/Library/Frameworks/CoreServices.framework/Frameworks", |LIBRARY_SEARCH_PATHS|:"", |GCC_ALTIVEC_EXTENSIONS|:"NO", |PREBINDING|:"NO", |HEADER_SEARCH_PATHS|:"", |ZERO_LINK|:"NO", |GCC_PRECOMPILE_PREFIX_HEADER|:"YES", |OTHER_CPLUSPLUSFLAGS|:"", |GCC_PREFIX_HEADER|:"", |STRIP_INSTALLED_PRODUCT|:"NO", |DEAD_CODE_STRIPPING|:"YES", |OBJROOT|:""}
property buildSettingsDebug : buildSettingsCommon & {|COPY_PHASE_STRIP|:"NO", |DEBUGGING_SYMBOLS|:"YES", |GCC_DYNAMIC_NO_PIC|:"NO", |GCC_ENABLE_FIX_AND_CONTINUE|:"NO", |GCC_GENERATE_DEBUGGING_SYMBOLS|:"YES", |GCC_OPTIMIZATION_LEVEL|:"0", |OPTIMIZATION_CFLAGS|:"-O0", |GCC_PREPROCESSOR_DEFINITIONS|:"NCBI_XCODE_BUILD _DEBUG _MT"}
property buildSettingsRelease : buildSettingsCommon & {|COPY_PHASE_STRIP|:"YES", |GCC_ENABLE_FIX_AND_CONTINUE|:"NO", |DEPLOYMENT_POSTPROCESSING|:"YES", |GCC_PREPROCESSOR_DEFINITIONS|:"NCBI_XCODE_BUILD _MT NDEBUG"}

(* Build styles for the project *)
property buildStyleDebug : {isa:"PBXBuildStyle", |name|:"Debug", |buildRules|:{}, |buildSettings|:buildSettingsDebug}
property buildStyleRelease : {isa:"PBXBuildStyle", |name|:"Release", |buildRules|:{}, |buildSettings|:buildSettingsRelease}
property projectBuildStyles : {"BUILDSTYLE__Development", "BUILDSTYLE__Deployment"}


(* Root Objects, project and main group *)
property rootObject : {isa:"PBXProject", |hasScannedForEncodings|:"1", |mainGroup|:"MAINGROUP", |targets|:{}, |buildSettings|:{|name|:"NCBI"}, |buildStyles|:{}}
property mainGroup : {isa:"PBXGroup", children:{"HEADERS", "SOURCES", "FRAMEWORKS"}, |name|:"NCBI C++ Toolkit", |refType|:"4"}
property emptyProject : {|rootObject|:"ROOT_OBJECT", |archiveVersion|:"1", |objectVersion|:"39", objects:{}}

(* Convinience Groups *)
property headers : {isa:"PBXGroup", children:{}, |name|:"Headers", |refType|:"4"}
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
		
		set pack to {TheFLTKPath, TheBDBPath, ThePCREPath}
		
		repeat with p in pack -- add all paths to headers
			set headerPath to headerPath & " " & p & "/include"
			set libraryPath to libraryPath & " " & p & "/lib"
		end repeat
		
		set libraryPath to libraryPath & " " & TheOUTPath & "/lib"
		
		set |HEADER_SEARCH_PATHS| of buildSettingsDebug to headerPath
		set |HEADER_SEARCH_PATHS| of buildSettingsRelease to headerPath
		
		set |LIBRARY_SEARCH_PATHS| of buildSettingsDebug to libraryPath
		set |LIBRARY_SEARCH_PATHS| of buildSettingsRelease to libraryPath
		
		set PCH to x_Replace(TheNCBIPath, ":", "/") & "/include/ncbi_pch.hpp"
		set |GCC_PREFIX_HEADER| of buildSettingsDebug to PCH
		set |GCC_PREFIX_HEADER| of buildSettingsRelease to PCH
		
		(* no-permissive flag for GCC *)
		set |OTHER_CPLUSPLUSFLAGS| of buildSettingsDebug to "-fno-permissive"
		set |OTHER_CPLUSPLUSFLAGS| of buildSettingsDebug to "-fno-permissive"
		
		(* Output directories and intermidiate files (works staring xCode 1.5) *)
		set |OBJROOT| of buildSettingsDebug to TheOUTPath
		set |OBJROOT| of buildSettingsRelease to TheOUTPath
		
		(* Set other options *)
		if zeroLink then
			set |ZERO_LINK| of buildSettingsDebug to "YES"
		end if
		
		if fixContinue then
			set |GCC_ENABLE_FIX_AND_CONTINUE| of buildSettingsDebug to "YES"
		end if
		
		if cpuOptimization then
			log "Getting CPU type"
			set cpuType to do shell script "/usr/sbin/ioreg | grep PowerPC | awk '{print $3}'| cut -c 9-10"
			
			if cpuType contains "G5" then
				set |GCC_MODEL_TUNING| of buildSettingsDebug to "G5"
				set |GCC_MODEL_CPU| of buildSettingsDebug to "G5"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsDebug to "YES"
				set |GCC_MODEL_TUNING| of buildSettingsRelease to "G5"
				set |GCC_MODEL_CPU| of buildSettingsRelease to "G5"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsRelease to "YES"
			else if cpuType contains "G4" then
				set |GCC_MODEL_TUNING| of buildSettingsDebug to "G4"
				set |GCC_MODEL_CPU| of buildSettingsDebug to "G4"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsDebug to "YES"
				set |GCC_MODEL_TUNING| of buildSettingsRelease to "G4"
				set |GCC_MODEL_CPU| of buildSettingsRelease to "G4"
				set |GCC_ALTIVEC_EXTENSIONS| of buildSettingsRelease to "YES"
			else
				set |GCC_MODEL_TUNING| of buildSettingsDebug to "G3"
				set |GCC_MODEL_CPU| of buildSettingsDebug to "G3"
				set |GCC_MODEL_TUNING| of buildSettingsRelease to "G3"
				set |GCC_MODEL_CPU| of buildSettingsRelease to "G3"
			end if
			log cpuType
		end if
		
		set newProject to emptyProject
		set objValues to {}
		set objKeys to {}
		
		set refFileCount to 1
		set allDepList to {}
		set libDepList to {}
		set appDepList to {}
		
		set |buildStyles| of rootObject to projectBuildStyles
		
		addPair(mainGroup, "MAINGROUP")
		addPair(buildStyleDebug, "BUILDSTYLE__Development")
		addPair(buildStyleRelease, "BUILDSTYLE__Deployment")
		
		if xcodeVersion is 1 then
			set |objectVersion| of newProject to "39"
		else if xcodeVersion is 2 then
			set |objectVersion| of newProject to "42"
		end if
		log "Done initialize ProjBuilder"
	end Initialize
	
	
	on MakeNewTarget(target_info, src_files, hdr_files, aTarget, aProduct, aType)
		set tgName to name of target_info
		set fullTargetName to tgName
		if aType is equal to 0 then set fullTargetName to "lib" & tgName -- Library
		set targetName to "TARGET__" & tgName
		set targetProxy to "PROXY__" & tgName
		set buildPhaseName to "BUILDPHASE__" & tgName
		set prodRefName to "PRODUCT__" & tgName
		set depName to "DEPENDENCE__" & tgName
		set tgNameH to tgName & "H"
		
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
		copy tgNameH to the end of children of headers
		
		set libDepend to {isa:"PBXTargetDependency", |target|:targetName} --, |targetProxy|:targetProxy}
		--set aProxy to {isa:"PBXContainerItemProxy", |proxyType|:"1", |containerPortal|:"ROOT_OBJECT", |remoteInfo|:fullTargetName} --|remoteGlobalIDString|:targetName}
		
		set buildFileRefs to {}
		set libFileRefs to {}
		-- Add Source Files
		repeat with F in src_files
			set nameRef to "FILE" & refFileCount
			set nameBuild to "REF_FILE" & refFileCount
			
			set filePath to F --"/" & x_Replace(f, ":", "/") -- f will contain something like "users:vlad:c++:src:corelib:ncbicore.cpp"
			set fileName to x_FileNameFromPath(F)
			
			--set first_char to first character of fileName
			--considering case
			--set valid_idx to offset of first_char in upper_alphabet
			--end considering
			
			--if valid_idx is 0 then -- no capital first letters!
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
			
			set refFileCount to refFileCount + 1
			--end if
			--if refFileCount = 3 then exit repeat
			--log f
		end repeat
		
		
		-- Add Header Files
		set hdrFileRefs to {}
		repeat with F in hdr_files
			set nameRef to "FILE" & refFileCount
			set filePath to F
			set fileName to x_FileNameFromPath(F)
			if fileName ends with ".hpp" then
				set fileType to "sourcecode.hpp.hpp"
			else
				set fileType to "sourcecode.h.h"
			end if
			
			set fileRef to {isa:"PBXFileReference", |lastKnownFileType|:fileType, |name|:fileName, |path|:filePath, |sourceTree|:"<absolute>"}
			addPair(fileRef, nameRef)
			copy nameRef to the end of hdrFileRefs
			set refFileCount to refFileCount + 1
		end repeat
		
		set libGroup to {isa:"PBXGroup", |name|:tgName, children:libFileRefs, |refType|:"4"}
		set hdrGroup to {isa:"PBXGroup", |name|:tgName, children:hdrFileRefs, |refType|:"4"}
		
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
					--set oneAsnName to last word of oneAsnPath
					set oneAsnName to do shell script "ruby -e \"s='" & oneAsnPath & "'; puts s.split(':')[1]\""
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
		
		
		(* Create a shell script phase to add resource fork for gbench_feedback app *)
		if tgName is "gbench_feedback_agent" then
			set shellScript to "cd \"$TARGET_BUILD_DIR\"" & ret & "$SYSTEM_DEVELOPER_TOOLS/Rez -t APPL " & TheFLTKPath & "/include/FL/mac.r -o $EXECUTABLE_NAME"
			set scriptPhaseName to "SCRIPTPHASE__" & tgName
			set aScriptPhase to {isa:"PBXShellScriptBuildPhase", |files|:{}, |inputPaths|:{}, |outputPaths|:{}, |shellPath|:"/bin/sh", |shellScript|:shellScript}
			copy scriptPhaseName to the end of |buildPhases| of aTarget -- shell script phase goes first (before compiling)
			addPair(aScriptPhase, scriptPhaseName)
		end if -- Create a Resources Fork
		
		
		-- add to main object list
		addPair(aTarget, targetName)
		--addPair(aProxy, targetProxy)
		addPair(libDepend, depName)
		addPair(aProduct, prodRefName)
		addPair(aBuildPhase, buildPhaseName)
		
		addPair(libGroup, tgName)
		addPair(hdrGroup, tgNameH)
	end MakeNewTarget
	
	
	on MakeNewLibraryTarget(lib_info, src_files, hdr_files)
		set libName to name of lib_info
		set targetName to "TARGET__" & libName
		set buildPhaseName to "BUILDPHASE__" & libName
		
		set installPath to TheOUTPath & "/lib" --"/Users/lebedev/Projects/tmp3"
		set linkerFlags to "" --  -flat_namespace -undefined suppress" -- warning -- additional liker flags (like -lxncbi)
		set symRoot to TheOUTPath & "/lib"
		
		-- build DLLs by default
		set libraryStyle to "DYNAMIC"
		set fullLibName to "lib" & libName
		set libProdType to "com.apple.product-type.library.dynamic"
		
		set isBundle to false
		try -- are we building a loadable module?
			if bundle of lib_info then set libraryStyle to "DYNAMIC" --"BUNDLE"
			set linkerFlags to "" -- do not suppress undefined symbols. Bundles should be fully resolved
			--set linkerFlags to "-framework Carbon -framework AGL -framework OpenGL"
			set symRoot to TheOUTPath & "/bin/$(CONFIGURATION)/Genome Workbench.app/Contents/MacOS/plugins"
			set isBundle to true
		end try
		
		if libTypeDLL is false and isBundle is false then -- build as static			
			set libraryStyle to "STATIC"
			set fullLibName to libName
			set libProdType to "com.apple.product-type.library.static"
		end if
		
		
		set linkerFlags to linkerFlags & x_CreateLinkerFlags(lib_info) -- additional liker flags (like -lxncbi)
		
		set buildSettings to {|LIB_COMPATIBILITY_VERSION|:"1", |DYLIB_CURRENT_VERSION|:"1", |INSTALL_PATH|:installPath, |LIBRARY_STYLE|:libraryStyle, |PRODUCT_NAME|:fullLibName, |OTHER_LDFLAGS|:linkerFlags, |SYMROOT|:symRoot, |TARGET_BUILD_DIR|:symRoot}
		set libTarget to {isa:"PBXNativeTarget", |buildPhases|:{buildPhaseName}, |buildSettings|:buildSettings, |name|:fullLibName, |productReference|:"", |productType|:libProdType, dependencies:{}}
		
		my MakeNewTarget(lib_info, src_files, hdr_files, libTarget, libProduct, 0) -- 0 is library
	end MakeNewLibraryTarget
	
	
	on MakeNewToolTarget(tool_info, src_files, hdr_files)
		set toolName to name of tool_info
		set targetName to "TARGET__" & toolName
		set buildPhaseName to "BUILDPHASE__" & toolName
		set fullToolName to toolName -- "app_" & toolName
		
		set linkerFlags to x_CreateLinkerFlags(tool_info) -- additional liker flags (like -lxncbi)
		
		set symRoot to TheOUTPath & "/bin"
		set buildSettings to {|PRODUCT_NAME|:fullToolName, |OTHER_LDFLAGS|:linkerFlags, |SYMROOT|:symRoot}
		if toolName is "gbench_plugin_scan" or toolName is "gbench_monitor" or toolName is "gbench_feedback_agent" or toolName is "gbench_cache_agent" then
			set symRoot to TheOUTPath & "/bin/$(CONFIGURATION)/Genome Workbench.app/Contents/MacOS"
			set |SYMROOT| of buildSettings to symRoot
			set buildSettings to buildSettings & {|TARGET_BUILD_DIR|:symRoot}
		end if
		set toolTarget to {isa:"PBXNativeTarget", |buildPhases|:{buildPhaseName}, |buildSettings|:buildSettings, |name|:fullToolName, |productReference|:"", |productType|:"com.apple.product-type.tool", dependencies:{}}
		
		my MakeNewTarget(tool_info, src_files, hdr_files, toolTarget, toolProduct, 2) -- is a tool
	end MakeNewToolTarget
	
	
	
	on MakeNewAppTarget(app_info, src_files, hdr_files)
		set appName to name of app_info
		set targetName to "TARGET__" & appName
		set buildPhaseName to "BUILDPHASE__" & appName
		--set fullAppName to "app_" & appName
		
		set linkerFlags to x_CreateLinkerFlags(app_info) -- additional liker flags (like -lxncbi)
		
		set symRoot to TheOUTPath & "/bin"
		set buildSettings to {|PRODUCT_NAME|:appName, |OTHER_LDFLAGS|:linkerFlags, |REZ_EXECUTABLE|:"YES", |INFOPLIST_FILE|:"", |SYMROOT|:symRoot, |KEEP_PRIVATE_EXTERNS|:"YES", |GENERATE_MASTER_OBJECT_FILE|:"YES"}
		set appTarget to {isa:"PBXNativeTarget", |buildPhases|:{buildPhaseName}, |buildSettings|:buildSettings, |name|:appName, |productReference|:"", |productType|:"com.apple.product-type.application", dependencies:{}}
		
		my MakeNewTarget(app_info, src_files, hdr_files, appTarget, appProduct, 1) -- 1 is application
	end MakeNewAppTarget
	
	
	
	
	(* Save everything *)
	on SaveProjectFile()
		(* Genome Workbench Disk Image *)
		(* Add a shell script only target to create a standalone disk image for distribution *)
		set shellScript to do shell script "cat " & TheNCBIPath & "/compilers/xCode/diskimage.tmpl"
		
		copy "TARGET__GBENCH_DISK" to the end of |targets| of rootObject
		set aScriptPhase to {isa:"PBXShellScriptBuildPhase", |files|:{}, |inputPaths|:{}, |outputPaths|:{}, |runOnlyForDeploymentPostprocessing|:1, |shellPath|:"/bin/sh", |shellScript|:shellScript}
		
		set theTarget to {isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{|PRODUCT_NAME|:"Genome Workbench Disk Image", |none|:""}, dependencies:{}, |name|:"Genome Workbench Disk Image"}
		copy "SCRIPTPHASE__GBENCH_DISK" to the beginning of |buildPhases| of theTarget
		addPair(aScriptPhase, "SCRIPTPHASE__GBENCH_DISK")
		addPair(theTarget, "TARGET__GBENCH_DISK")
		addPair({isa:"PBXTargetDependency", |target|:"TARGET__GBENCH_DISK"}, "DEPENDENCE__GBENCH_DISK")
		copy "DEPENDENCE__GBENCH_DISK" to the end of allDepList
		
		
		(* Target: Build Everything *)
		copy "TARGET__BUILD_APP" to the beginning of |targets| of rootObject
		addPair({isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{|PRODUCT_NAME|:"Build All Applications", |none|:""}, dependencies:appDepList, |name|:"Build All Applications"}, "TARGET__BUILD_APP")
		copy "TARGET__BUILD_LIB" to the beginning of |targets| of rootObject
		addPair({isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{|PRODUCT_NAME|:"Build All Libraries", |none|:""}, dependencies:libDepList, |name|:"Build All Libraries"}, "TARGET__BUILD_LIB")
		copy "TARGET__BUILD_ALL" to the beginning of |targets| of rootObject
		addPair({isa:"PBXAggregateTarget", |buildPhases|:{}, |buildSettings|:{|PRODUCT_NAME|:"Build All", |none|:""}, dependencies:allDepList, |name|:"Build All"}, "TARGET__BUILD_ALL")
		
		
		(* add frameworks*)
		-- Carbon
		copy "FW_CARBON" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"Carbon.framework", |path|:"/System/Library/Frameworks/Carbon.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_CARBON")
		-- OpenGL
		copy "FW_OpenGL" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"OpenGL.framework", |path|:"/System/Library/Frameworks/OpenGL.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_OpenGL")
		copy "FW_CORESERVICES" to the end of children of fworks
		addPair({isa:"PBXFileReference", |lastKnownFileType|:"wrapper.framework", |name|:"CoreServices.framework", |path|:"/System/Library/Frameworks/CoreServices.framework", |refType|:"0", |sourceTree|:"<absolute>"}, "FW_CORESERVICES")
		
		(* Add ROOT objects and groups *)
		addPair(rootObject, "ROOT_OBJECT")
		addPair(headers, "HEADERS")
		addPair(sources, "SOURCES")
		addPair(fworks, "FRAMEWORKS")
		
		
		(* Create a record from two lists *)
		set objects of newProject to CreateRecordFromList(objValues, objKeys)
		
		
		
		try -- create some folders
			set shScript to "if test ! -d " & TheOUTPath & "/" & projFile & " ; then mkdir " & TheOUTPath & "/" & projFile & " ; fi"
			do shell script shScript
		end try
		
		set fullProjName to (TheOUTPath & "/" & projFile & "/project.pbxproj") as string
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
		
		try -- set frameworks
			set fwString to fworks of info
			set fwList to x_Str2List(fwString)
			repeat with d in fwList
				set linkFlags to linkFlags & " -framework " & d
			end repeat
		end try
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
			if asnName is "gui_project" or asnName is "data_handle" or asnName is "plugin" or asnName is "gbench_svc" or asnName is "seqalign_ext" then -- Should use sed properly here (but how?)
				set theScript to theScript & "  M=\"$(grep ^MODULE_IMPORT $m.module | sed 's/^.*= *//' | sed 's/\\([/a-z0-9_]*\\)/\\1.asn/g')\"" & ret
			else
				set theScript to theScript & "  M=\"\"" & ret
				set theScript to theScript & "  if test -e $m.module; then" & ret
				set theScript to theScript & "    M=\"$(grep ^MODULE_IMPORT $m.module | sed 's/^.*= *//' | sed 's/\\(objects[/a-z0-9]*\\)/\\1.asn/g')\"" & ret
				set theScript to theScript & "  fi" & ret
			end if
			
			set theScript to theScript & "  " & TheOUTPath & "/bin/$CONFIGURATION/datatool -oR " & TheNCBIPath
			
			set theScript to theScript & " -opm " & TheNCBIPath & "/src  -m \"$m.asn\" -M \"$M\" -oA -of \"$m.files\" -or \"" & posixPath & "\" -oc \"$m\" -oex '' -ocvs -odi -od \"$m.def\"" & ret
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
		set theScript to theScript & "if test ! -e " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/plugins/plugin-cache ; then" & ret
		set theScript to theScript & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/gbench_plugin_scan " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/plugins" & ret
		set theScript to theScript & "fi" & ret
		
		set theScript to theScript & "echo Copying GBench resources" & ret
		
		-- Create etc directory
		set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/etc ; then" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		set theScript to theScript & "fi" & ret
		
		-- Create Resources directory
		set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/Resources ; then" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/Resources" & ret
		set theScript to theScript & "fi" & ret
		
		-- Create share directory
		set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/share/gbench ; then" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/share" & ret
		set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/share/gbench" & ret
		set theScript to theScript & "fi" & ret
		
		-- Create executables directory
		--set theScript to theScript & "if test ! -d " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/executables ; then" & ret
		--set theScript to theScript & "  mkdir " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/executables" & ret
		--set theScript to theScript & "fi" & ret
		
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/plugins/algo/executables/* " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/executables" & ret
		
		-- copy png images
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/res/share/gbench/* " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/share/gbench" & ret
		
		-- copy Info.plist file
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/res/share/gbench/Info.plist " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents" & ret
		
		-- copy Icon file
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/res/share/gbench/gbench.icns " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/Resources/Genome\\ Workbench.icns" & ret
		
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/res/share/gbench/gbench_workspace.icns " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/Resources/gbench_workspace.icns" & ret
		set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/res/share/gbench/gbench_project.icns " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/Resources/gbench_project.icns" & ret
		
		----set theScript to theScript & "cp -r " & TheNCBIPath & "/src/gui/plugins/algo/executables " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/executables" & ret
		set theScript to theScript & "rm -rf " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/etc/*" & ret
		set theScript to theScript & "cp -r " & TheNCBIPath & "/src/gui/res/etc/* " & TheOUTPath & "/bin/$CONFIGURATION/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		--set theScript to theScript & "cp -r " & TheNCBIPath & "/src/gui/gbench/patterns/ " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc/patterns" & ret
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/gbench/news.ini " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/gbench/gbench.ini " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		--set theScript to theScript & "cp " & TheNCBIPath & "/src/gui/gbench/algo_urls " & TheOUTPath & "/bin/Genome\\ Workbench.app/Contents/MacOS/etc" & ret
		
		return theScript
	end x_CopyGBENCHResourses
end script
