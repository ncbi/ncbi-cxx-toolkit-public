/*  $Id$
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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbidll.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/error_codes.hpp>

#include <windows.h>
#include <winnt.h>
#include <dbghelp.h>


#define NCBI_USE_ERRCODE_X   Corelib_Stack


BEGIN_NCBI_SCOPE


#define lenof(a) (sizeof(a) / sizeof((a)[0]))
#define MAXNAMELEN 1024 // max name length for found symbols
#define IMGSYMLEN (sizeof IMAGEHLP_SYMBOL64)
#define TTBUFLEN 65535


struct SModuleEntry
{
    std::string imageName;
    std::string moduleName;
    DWORD64 baseAddress;
    DWORD size;
};

typedef vector<SModuleEntry> TModules;
typedef TModules::iterator ModuleListIter;


// declarations for PSAPI
// this won't be present on all systems, so we include them here

typedef struct _MODULEINFO {
    LPVOID lpBaseOfDll;
    DWORD SizeOfImage;
    LPVOID EntryPoint;
} MODULEINFO, *LPMODULEINFO;


static bool s_FillModuleListPSAPI(TModules& mods, DWORD pid, HANDLE hProcess)
{
    try {
        // EnumProcessModules()
        typedef BOOL (__stdcall *FEnumProcessModules)(HANDLE hProcess,
                                                        HMODULE *lphModule,
                                                        DWORD cb,
                                                        LPDWORD lpcbNeeded);
        // GetModuleFileNameEx()
        typedef DWORD (__stdcall *FGetModuleFileNameEx)(HANDLE hProcess,
                                                        HMODULE hModule,
                                                        LPSTR lpFilename,
                                                        DWORD nSize);
        // GetModuleBaseName() -- redundant, as GMFNE() has the same prototype, but who cares?
        typedef DWORD (__stdcall *FGetModuleBaseName)(HANDLE hProcess,
                                                      HMODULE hModule,
                                                      LPSTR lpFilename,
                                                      DWORD nSize);
        // GetModuleInformation()
        typedef BOOL (__stdcall *FGetModuleInformation)(HANDLE hProcess,
                                                        HMODULE hModule,
                                                        LPMODULEINFO pmi,
                                                        DWORD nSize);

        FEnumProcessModules EnumProcessModules;
        FGetModuleFileNameEx GetModuleFileNameEx;
        FGetModuleBaseName GetModuleBaseName;
        FGetModuleInformation GetModuleInformation;

        mods.clear();
        CDll dll("psapi.dll", CDll::eLoadNow, CDll::eAutoUnload);

        EnumProcessModules =
            dll.GetEntryPoint_Func("EnumProcessModules",
                                   &EnumProcessModules);
        GetModuleFileNameEx =
            dll.GetEntryPoint_Func("GetModuleFileNameExA",
                                   &GetModuleFileNameEx);
        GetModuleBaseName =
            dll.GetEntryPoint_Func("GetModuleBaseNameA",
                                   &GetModuleBaseName);
        GetModuleInformation =
            dll.GetEntryPoint_Func("GetModuleInformation",
                                   &GetModuleInformation);

        if ( !EnumProcessModules  ||
             !GetModuleFileNameEx  ||
             !GetModuleBaseName  ||
             !GetModuleInformation ) {
            return false;
        }

        vector<HMODULE> modules;
        modules.resize(4096);

        string tmp;
        DWORD needed;
        if ( !EnumProcessModules(hProcess,
                                 &modules[0],
                                 DWORD(modules.size()*sizeof(HMODULE)),
                                 &needed) ) {
            NCBI_THROW(CCoreException, eCore,
                       "EnumProcessModules() failed");
        }

        if ( needed > modules.size() * sizeof(HMODULE)) {
            NCBI_THROW(CCoreException, eCore,
                       string("More than ") +
                       NStr::Int8ToString(modules.size()) + " modules");
        }


        needed /= sizeof(HMODULE);
        for (size_t i = 0;  i < needed;  ++i) {
            // for each module, get:
            // base address, size
            MODULEINFO mi;
            GetModuleInformation(hProcess, modules[i], &mi, sizeof(mi));

            SModuleEntry e;
            e.baseAddress = (DWORD64)mi.lpBaseOfDll;
            e.size = mi.SizeOfImage;

            // image file name
            char tt[2048];
            tt[0] = '\0';
            GetModuleFileNameEx(hProcess, modules[i], tt, sizeof(tt));
            e.imageName = tt;

            // module name
            tt[0] = '\0';
            GetModuleBaseName(hProcess, modules[i], tt, sizeof(tt));
            e.moduleName = tt;

            mods.push_back(e);
        }

        return true;
    }
    catch (exception& e) {
        ERR_POST_X(3, Error << "Error getting PSAPI symbols: " << e.what());
    }
    catch (...) {
    }

    return false;
}


static bool s_FillModuleList(TModules& modules, DWORD pid, HANDLE hProcess)
{
    return s_FillModuleListPSAPI(modules, pid, hProcess);
}


class CSymbolGuard
{
public:
    CSymbolGuard(void);
    ~CSymbolGuard(void);

    void UpdateSymbols(void);
private:
    // Remember loaded modules
    typedef set<string> TLoadedModules;

    TLoadedModules m_Loaded;
};


CSymbolGuard::CSymbolGuard(void)
{
    // our current process and thread within that process
    HANDLE curr_proc = GetCurrentProcess();

    try {
        string search_path(CDir::GetCwd());
        string tmp;
        tmp.resize(2048);
        if (GetModuleFileNameA(0, const_cast<char*>(tmp.data()),
            DWORD(tmp.length()))) {
            string::size_type pos = tmp.find_last_of("\\/");
            if (pos != string::npos) {
                tmp.erase(pos);
            }
            search_path = tmp + ';' + search_path;
        }

        const char* ptr = getenv("_NT_SYMBOL_PATH");
        if (ptr) {
            string tmp(ptr);
            search_path = tmp + ';' + search_path;
        }
        ptr = getenv("_NT_ALTERNATE_SYMBOL_PATH");
        if (ptr) {
            string tmp(ptr);
            search_path = tmp + ';' + search_path;
        }
        ptr = getenv("SYSTEMROOT");
        if (ptr) {
            string tmp(ptr);
            search_path = tmp + ';' + search_path;
        }

        // init symbol handler stuff (SymInitialize())
        if ( !SymInitialize(curr_proc,
                            const_cast<char*>(search_path.c_str()),
                            false) ) {
            NCBI_THROW(CCoreException, eCore, "SymInitialize() failed");
        }

        // set up default options
        DWORD symOptions = SymGetOptions();
        symOptions &= ~SYMOPT_UNDNAME;
        symOptions |= SYMOPT_LOAD_LINES;
        SymSetOptions(symOptions);

        // pre-load our symbols
        UpdateSymbols();
    }
    catch (exception& e) {
        ERR_POST_X(4, Error << "Error loading symbols for stack trace: "
            << e.what());
    }
    catch (...) {
        ERR_POST_X(5, Error
            << "Unknown error initializing symbols for stack trace.");
    }
}


CSymbolGuard::~CSymbolGuard(void)
{
    SymCleanup(GetCurrentProcess());
}


void CSymbolGuard::UpdateSymbols(void)
{
    HANDLE proc = GetCurrentProcess();
    DWORD pid = GetCurrentProcessId();

    // Enumerate modules and tell dbghelp.dll about them.
    // On NT, this is not necessary, but it won't hurt.
    TModules modules;

    // fill in module list
    s_FillModuleList(modules, pid, proc);

    NON_CONST_ITERATE (TModules, it, modules) {
        TLoadedModules::const_iterator module = m_Loaded.find(it->moduleName);
        if (module != m_Loaded.end()) {
            continue;
        }
        DWORD64 module_addr = SymLoadModule64(proc, 0,
            const_cast<char*>(it->imageName.c_str()),
            const_cast<char*>(it->moduleName.c_str()),
            it->baseAddress, it->size);
        if ( !module_addr ) {
            ERR_POST_X(6, Error << "Error loading symbols for module: "
                                << it->moduleName);
        } else {
            _TRACE("Loaded symbols from " << it->moduleName);
            m_Loaded.insert(it->moduleName);
        }
    }
}


static CSafeStaticPtr<CSymbolGuard> s_SymbolGuard;


class CStackTraceImpl
{
public:
    CStackTraceImpl(void);
    ~CStackTraceImpl(void);

    void Expand(CStackTrace::TStack& stack);

private:
    typedef STACKFRAME64 TStackFrame;
    typedef vector<TStackFrame> TStack;

    TStack m_Stack;
};


CStackTraceImpl::CStackTraceImpl(void)
{
    s_SymbolGuard->UpdateSymbols();

    HANDLE curr_proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    // we could use ImageNtHeader() here instead
    DWORD img_type = IMAGE_FILE_MACHINE_AMD64;

    // init CONTEXT record so we know where to start the stackwalk
    CONTEXT c;
    RtlCaptureContext(&c);

    // current stack frame
    STACKFRAME64 s;
    memset(&s, 0, sizeof s);
    s.AddrPC.Offset    = c.Rip;
    s.AddrPC.Mode      = AddrModeFlat;
    s.AddrFrame.Offset = c.Rbp;
    s.AddrFrame.Mode   = AddrModeFlat;
    s.AddrStack.Offset = c.Rsp;
    s.AddrStack.Mode   = AddrModeFlat;

    try {
        unsigned int max_depth = CStackTrace::s_GetStackTraceMaxDepth();
        for (size_t frame = 0; frame <= max_depth; ++frame) {
            // get next stack frame
            if ( !StackWalk64(img_type, curr_proc, thread, &s, &c, NULL,
                            SymFunctionTableAccess64,
                            SymGetModuleBase64,
                            NULL) ) {
                break;
            }

            // Discard the top frames describing current function
            if (frame < 1) {
                continue;
            }

            // Try to skip bad frames - not very reliable.
            if ( !s.AddrPC.Offset  ||  !s.AddrReturn.Offset ) {
                continue;
            }
            if (s.AddrPC.Offset == s.AddrReturn.Offset) {
                continue;
            }

            m_Stack.push_back(s);
        }
    }
    catch (exception& e) {
        ERR_POST_X(7, Error << "Error getting stack trace: " << e.what());
    }
    catch (...) {
        ERR_POST_X(8, Error << "Unknown error getting stack trace");
    }
}


CStackTraceImpl::~CStackTraceImpl(void)
{
}


void CStackTraceImpl::Expand(CStackTrace::TStack& stack)
{
    if ( m_Stack.empty() ) {
        return;
    }

    s_SymbolGuard->UpdateSymbols();

    HANDLE curr_proc = GetCurrentProcess();

    IMAGEHLP_SYMBOL64 *pSym = NULL;
    pSym = (IMAGEHLP_SYMBOL64 *) malloc(IMGSYMLEN + MAXNAMELEN);
    memset(pSym, 0, IMGSYMLEN + MAXNAMELEN);
    pSym->SizeOfStruct = IMGSYMLEN;
    pSym->MaxNameLength = MAXNAMELEN;

    IMAGEHLP_LINE64 Line;
    memset(&Line, 0, sizeof Line);
    Line.SizeOfStruct = sizeof(Line);

    IMAGEHLP_MODULE64 Module;
    memset(&Module, 0, sizeof Module);
    Module.SizeOfStruct = sizeof(Module);

    DWORD64 offs64 = 0;
    DWORD offs32 = 0;

    try {
        ITERATE(TStack, it, m_Stack) {
            CStackTrace::SStackFrameInfo sf_info;
            sf_info.func = "<cannot get function name for this address>";

            if ( !SymGetSymFromAddr64(curr_proc, it->AddrPC.Offset, &offs64, pSym) ) {
                // Showing unresolvable frames may produce thousands of them
                // per one stacktrace on win64.
                //stack.push_back(sf_info);
                continue;
            }
            sf_info.offs = offs64;

            // retrieve function names, if we can
            //char undName[MAXNAMELEN];
            char undFullName[MAXNAMELEN];
            //UnDecorateSymbolName(pSym->Name, undName,
            //                     MAXNAMELEN, UNDNAME_NAME_ONLY);
            UnDecorateSymbolName(pSym->Name, undFullName,
                                    MAXNAMELEN, UNDNAME_COMPLETE);

            sf_info.func = undFullName;

            // retrieve file and line number info
            if (SymGetLineFromAddr(curr_proc,
                                    it->AddrPC.Offset,
                                    &offs32,
                                    &Line)) {
                sf_info.file = Line.FileName;
                sf_info.line = Line.LineNumber;
            } else {
                _TRACE("failed to get line number for " << sf_info.func);
            }

            // get library info, if it is available
            if ( !SymGetModuleInfo(curr_proc,
                                    it->AddrPC.Offset,
                                    &Module) ) {
                // There are too many fails on win64, lower the severity.
                ERR_POST_X(10, Info << "failed to get module info for "
                    << sf_info.func);
            } else {
                sf_info.module = Module.ModuleName;
                sf_info.module += "[";
                sf_info.module += Module.ImageName;
                sf_info.module += "]";
            }

            stack.push_back(sf_info);
        }
    }
    catch (exception& e) {
        ERR_POST_X(11, Error << "Error getting stack trace: " << e.what());
    }
    catch (...) {
        ERR_POST_X(12, Error << "Unknown error getting stack trace");
    }

    free(pSym);
}


END_NCBI_SCOPE
