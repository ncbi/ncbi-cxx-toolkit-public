#ifndef MSVC_TRAITS_HEADER
#define MSVC_TRAITS_HEADER

#include <string>
#include <corelib/ncbienv.hpp>
USING_NCBI_SCOPE;

//traits for MSVC projects:

//------------------------------------------------------------------------------
// 1. RunTime library traits:
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
//------------------------------------------------------------------------------
// 2. Debug/Release traits:
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
};

struct SRelease
{
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
};
//------------------------------------------------------------------------------
// 3. Congiguration Type (Target type) traits:
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





#endif // MSVC_TRAITS_HEADER