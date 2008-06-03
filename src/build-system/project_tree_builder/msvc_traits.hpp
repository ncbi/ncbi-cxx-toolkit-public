#ifndef PROJECT_TREE_BUILDER__MSVC_TRAITS__HPP
#define PROJECT_TREE_BUILDER__MSVC_TRAITS__HPP

/* $Id$
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
 * Author:  Viatcheslav Gorelenkov
 *
 */


#include <string>
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


// These can be found in VCProjectEngine.dll type library
// See OLE/COM Object Viewer, Type Libraries
// Microsoft Development Environment VC++ Project System Engine 7.0 Type Library (Ver 1.0)

/// Traits for MSVC projects:


/// RunTime library traits.
#if 0
// Quota from MDE VC++ system engine typelib:
typedef enum {
    rtMultiThreaded = 0,
    rtMultiThreadedDebug = 1,
    rtMultiThreadedDLL = 2,
    rtMultiThreadedDebugDLL = 3,
    rtSingleThreaded = 4,
    rtSingleThreadedDebug = 5
} runtimeLibraryOption;
#endif

struct SCrtMultiThreaded
{
    static string RuntimeLibrary(void)
    {
	    return "0";
    }
};


struct SCrtMultiThreadedDebug
{
    static string RuntimeLibrary(void)
    {
	    return "1";
    }
};


struct SCrtMultiThreadedDLL
{
    static string RuntimeLibrary(void)
    {
        return "2";
    }
};


struct SCrtMultiThreadedDebugDLL
{
    static string RuntimeLibrary(void)
    {
	    return "3";
    }
};


struct SCrtSingleThreaded
{
    static string RuntimeLibrary(void)
    {
	    return "4";
    }
};


struct SCrtSingleThreadedDebug
{
    static string RuntimeLibrary(void)
    {
	    return "5";
    }
};


/// Debug/Release traits.
#if 0
typedef enum {
    debugDisabled = 0,
    debugOldStyleInfo = 1,
    debugLineInfoOnly = 2,
    debugEnabled = 3,
    debugEditAndContinue = 4
} debugOption;
typedef enum {
    expandDisable = 0,
    expandOnlyInline = 1,
    expandAnySuitable = 2
} inlineExpansionOption;
typedef enum {
    optReferencesDefault = 0,
    optNoReferences = 1,
    optReferences = 2
} optRefType;
typedef enum {
    optFoldingDefault = 0,
    optNoFolding = 1,
    optFolding = 2
} optFoldingType;
#endif


struct SDebug
{
    static bool debug(void)
    {
        return true;
    }
    static string Optimization(void)
    {
	    return "0";
    }
    static string PreprocessorDefinitions(void)
    {
	    return "_DEBUG;";
    }
    static string BasicRuntimeChecks(void)
    {
        return "3";
    }
    static string DebugInformationFormat(void)
    {
	    return "1";
    }
    static string InlineFunctionExpansion(void)
    {
	    return "";
    }
    static string OmitFramePointers(void)
    {
	    return "";
    }
    static string StringPooling(void)
    {
	    return "";
    }
    static string EnableFunctionLevelLinking(void)
    {
	    return "";
    }
    static string GenerateDebugInformation(void)
    {
	    return "TRUE";
    }
    static string OptimizeReferences(void)
    {
	    return "";
    }
    static string EnableCOMDATFolding(void)
    {
	    return "";
    }

    static string GlobalOptimizations(void)
    {
	    return "FALSE";
    }
    static string FavorSizeOrSpeed(void)
    {
	    return "0";
    }
    static string BrowseInformation(void)
    {
	    return "1";
    }
};


struct SRelease
{
    static bool debug(void)
    {
        return false;
    }
    static string Optimization(void)
    {
	    return "2"; //VG: MaxSpeed
    }
    static string PreprocessorDefinitions(void)
    {
	    return "NDEBUG;";
    }
    static string BasicRuntimeChecks(void)
    {
        return "0";
    }
    static string DebugInformationFormat(void)
    {
	    return "0";
    }
    static string InlineFunctionExpansion(void)
    {
	    return "1";
    }
    static string OmitFramePointers(void)
    {
	    return "FALSE";
    }
    static string StringPooling(void)
    {
	    return "TRUE";
    }
    static string EnableFunctionLevelLinking(void)
    {
	    return "TRUE";
    }
    static string GenerateDebugInformation(void)
    {
	    return "FALSE";
    }
    static string OptimizeReferences(void)
    {
	    return "2";
    }
    static string EnableCOMDATFolding(void)
    {
	    return "2";
    }

    static string GlobalOptimizations(void)
    {
	    return "TRUE";
    }
    static string FavorSizeOrSpeed(void)
    {
	    return "1";
    }
    static string BrowseInformation(void)
    {
	    return "0";
    }
};


/// Congiguration Type (Target type) traits.
#if 0
typedef enum {
    typeUnknown = 0,
    typeApplication = 1,
    typeDynamicLibrary = 2,
    typeStaticLibrary = 4,
    typeGeneric = 10
} ConfigurationTypes;
#endif


struct SApp
{
    static string ConfigurationType(void)
    {
	    return "1";
    }
    static string PreprocessorDefinitions(void)
    {
	    return "WIN32;_CONSOLE;";
    }
    static bool IsDll(void)
    {
	    return false;
    }
    static string TargetExtension(void)
    {
	    return ".exe";
    }
    static string SubSystem(void)
    {
	    return "1"; //console
    }
};


struct SLib
{
    static string ConfigurationType(void)
    {
	    return "4";
    }
    static string PreprocessorDefinitions(void)
    {
	    return "WIN32;_LIB;";
    }
    static bool IsDll(void)
    {
	    return false;
    }
    static string TargetExtension(void)
    {
	    return ".lib";
    }
    static string SubSystem(void)
    {
	    return "1"; //console
    }
};


struct SDll
{
    static string ConfigurationType(void)
    {
	    return "2";
    }
    static string PreprocessorDefinitions(void)
    {
	    return "WIN32;_WINDOWS;_USRDLL;";
    }
    static bool IsDll(void)
    {
	    return true;
    }
    static string TargetExtension(void)
    {
	    return ".dll";
    }
    static string SubSystem(void)
    {
	    return "2"; //windows
    }
};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__MSVC_TRAITS__HPP
