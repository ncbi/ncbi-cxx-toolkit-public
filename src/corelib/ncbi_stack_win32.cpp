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

#include <windows.h>

// imagehlp.h must be compiled with packing to eight-byte-boundaries,
// but does nothing to enforce that.
#pragma pack(push, before_imagehlp, 8)
#include <imagehlp.h>
#pragma pack(pop, before_imagehlp)


BEGIN_NCBI_SCOPE

//
// statics
//
// these are function local, and specific to Win32
//

// SymCleanup()
typedef BOOL (__stdcall *FSymCleanup)(IN HANDLE hProcess);
FSymCleanup SymCleanup = NULL;

// SymFunctionTableAccess()
typedef PVOID (__stdcall *FSymFunctionTableAccess)(HANDLE hProcess,
                                                   DWORD AddrBase);
FSymFunctionTableAccess SymFunctionTableAccess = NULL;

// SymGetLineFromAddr()
typedef BOOL (__stdcall *FSymGetLineFromAddr)(IN HANDLE hProcess,
                                              IN DWORD dwAddr,
                                              OUT PDWORD pdwDisplacement,
                                              OUT PIMAGEHLP_LINE Line);
FSymGetLineFromAddr SymGetLineFromAddr = NULL;

// SymGetModuleBase()
typedef DWORD (__stdcall *FSymGetModuleBase)(IN HANDLE hProcess,
                                             IN DWORD dwAddr);
FSymGetModuleBase SymGetModuleBase = NULL;

// SymGetModuleInfo()
typedef BOOL (__stdcall *FSymGetModuleInfo)(IN HANDLE hProcess,
                                            IN DWORD dwAddr,
                                            OUT PIMAGEHLP_MODULE ModuleInfo);
FSymGetModuleInfo SymGetModuleInfo = NULL;

// SymGetOptions()
typedef DWORD (__stdcall *FSymGetOptions)(VOID);
FSymGetOptions SymGetOptions = NULL;

// SymGetSymFromAddr()
typedef BOOL (__stdcall *FSymGetSymFromAddr)(IN HANDLE hProcess,
                                             IN DWORD dwAddr,
                                             OUT PDWORD pdwDisplacement,
                                             OUT PIMAGEHLP_SYMBOL Symbol);
FSymGetSymFromAddr SymGetSymFromAddr = NULL;

// SymInitialize()
typedef BOOL (__stdcall *FSymInitialize)(IN HANDLE hProcess,
                                         IN PSTR UserSearchPath,
                                         IN BOOL fInvadeProcess);
FSymInitialize SymInitialize = NULL;

// SymLoadModule()
typedef DWORD (__stdcall *FSymLoadModule)(IN HANDLE hProcess,
                                          IN HANDLE hFile,
                                          IN PSTR ImageName,
                                          IN PSTR ModuleName,
                                          IN DWORD BaseOfDll,
                                          IN DWORD SizeOfDll);
FSymLoadModule SymLoadModule = NULL;

// SymSetOptions()
typedef DWORD (__stdcall *FSymSetOptions)(IN DWORD SymOptions);
FSymSetOptions SymSetOptions = NULL;

// StackWalk()
typedef BOOL (__stdcall *FStackWalk)(DWORD MachineType,
                                     HANDLE hProcess,
                                     HANDLE thread,
                                     LPSTACKFRAME StackFrame,
                                     PVOID ContextRecord,
                                     PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
                                     PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
                                     PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
                                     PTRANSLATE_ADDRESS_ROUTINE TranslateAddress);
FStackWalk StackWalk = NULL;

// UnDecorateSymbolName()
typedef DWORD (__stdcall WINAPI *FUnDecorateSymbolName)(PCSTR DecoratedName,
                                                        PSTR UnDecoratedName,
                                                        DWORD UndecoratedLength,
                                                        DWORD Flags);
FUnDecorateSymbolName UnDecorateSymbolName = NULL;


#define lenof(a) (sizeof(a) / sizeof((a)[0]))
#define MAXNAMELEN 1024 // max name length for found symbols
#define IMGSYMLEN (sizeof IMAGEHLP_SYMBOL)
#define TTBUFLEN 65535


struct SModuleEntry
{
    std::string imageName;
    std::string moduleName;
    DWORD baseAddress;
    DWORD size;
};

typedef vector<SModuleEntry> TModules;
typedef TModules::iterator ModuleListIter;


// miscellaneous toolhelp32 declarations; we cannot #include the header
// because not all systems may have it
#define MAX_MODULE_NAME32 255
#define TH32CS_SNAPMODULE 0x00000008
#pragma pack(push, 8)
typedef struct tagMODULEENTRY32
{
    DWORD   dwSize;
    DWORD   th32ModuleID;       // This module
    DWORD   th32ProcessID;      // owning process
    DWORD   GlblcntUsage;       // Global usage count on the module
    DWORD   ProccntUsage;       // Module usage count in th32ProcessID's context
    BYTE*   modBaseAddr;        // Base address of module in th32ProcessID's context
    DWORD   modBaseSize;        // Size in bytes of module starting at modBaseAddr
    HMODULE hModule;            // The hModule of this module in th32ProcessID's context
    char    szModule[MAX_MODULE_NAME32 + 1];
    char    szExePath[MAX_PATH];
} MODULEENTRY32;

typedef MODULEENTRY32*  PMODULEENTRY32;
typedef MODULEENTRY32*  LPMODULEENTRY32;
#pragma pack(pop)


static bool s_FillModuleListTH32(TModules& modules, DWORD pid)
{
    // CreateToolhelp32Snapshot()
    typedef HANDLE (__stdcall *FCreateToolhelp32Snapshot)(WORD dwFlags,
                                                          DWORD th32ProcessID);
    // Module32First()
    typedef BOOL (__stdcall *FModule32First)(HANDLE hSnapshot,
                                             LPMODULEENTRY32 lpme);
    // Module32Next()
    typedef BOOL (__stdcall *FModule32Next)(HANDLE hSnapshot,
                                            LPMODULEENTRY32 lpme);

    // I think the DLL is called tlhelp32.dll on Win9X, so we try both
    FCreateToolhelp32Snapshot CreateToolhelp32Snapshot;
    FModule32First Module32First;
    FModule32Next Module32Next;

    try {
        const char *dllname[] = {
            "kernel32.dll",
            "tlhelp32.dll",
            NULL
        };

        const char* libname = dllname[0];
        auto_ptr<CDll> dll;
        while (libname) {
            dll.reset(new CDll(libname, CDll::eLoadNow, CDll::eAutoUnload));

            CreateToolhelp32Snapshot =
                dll->GetEntryPoint_Func("CreateToolhelp32Snapshot",
                                        &CreateToolhelp32Snapshot);
            Module32First =
                dll->GetEntryPoint_Func("Module32First", &Module32First);
            Module32Next =
                dll->GetEntryPoint_Func("Module32Next", &Module32Next);

            if (CreateToolhelp32Snapshot && Module32First && Module32Next) {
                break;
            }
        }

        if ( !CreateToolhelp32Snapshot ) {
            NCBI_THROW(CCoreException, eCore,
                       "toolhelp32 functions not available");
        }

        HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (handle == (HANDLE) -1) {
            NCBI_THROW(CCoreException, eCore,
                       "failed to create toolhelp32 snapshot");
        }

        MODULEENTRY32 me;
        me.dwSize = sizeof(MODULEENTRY32);
        bool done = !Module32First(handle, &me) ? true : false;
        while ( !done ) {
            // here, we have a filled-in MODULEENTRY32
            SModuleEntry e;
            e.imageName   = me.szExePath;
            e.moduleName  = me.szModule;
            e.baseAddress = (DWORD) me.modBaseAddr;
            e.size        = me.modBaseSize;
            modules.push_back(e);

            done = !Module32Next(handle, &me) ? true : false;
        }

        CloseHandle(handle);

        return modules.size() != 0;
    }
    catch (exception& e) {
        ERR_POST(Error << "Error retrieving toolhelp32 symbols: " << e.what());
    }
    catch (...) {
        ERR_POST(Error << "Unknown error retrieving toolhelp32 symbols");
    }

    return false;
}


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
        typedef DWORD (__stdcall *FGetModuleFileNameExA)(HANDLE hProcess,
                                                         HMODULE hModule,
                                                         LPSTR lpFilename,
                                                         DWORD nSize);
        // GetModuleBaseName() -- redundant, as GMFNE() has the same prototype, but who cares?
        typedef DWORD (__stdcall *FGetModuleBaseNameA)(HANDLE hProcess,
                                                       HMODULE hModule,
                                                       LPSTR lpFilename,
                                                       DWORD nSize);
        // GetModuleInformation()
        typedef BOOL (__stdcall *FGetModuleInformation)(HANDLE hProcess,
                                                        HMODULE hModule,
                                                        LPMODULEINFO pmi,
                                                        DWORD nSize);

        FEnumProcessModules EnumProcessModules;
        FGetModuleFileNameExA GetModuleFileNameExA;
        FGetModuleBaseNameA GetModuleBaseNameA;
        FGetModuleInformation GetModuleInformation;

        mods.clear();
        CDll dll("psapi.dll", CDll::eLoadNow, CDll::eAutoUnload);

        EnumProcessModules =
            dll.GetEntryPoint_Func("EnumProcessModules",
                                   &EnumProcessModules);
        GetModuleFileNameExA =
            dll.GetEntryPoint_Func("GetModuleFileNameExA",
                                   &GetModuleFileNameExA);
        GetModuleBaseNameA =
            dll.GetEntryPoint_Func("GetModuleBaseNameA",
                                   &GetModuleBaseNameA);
        GetModuleInformation =
            dll.GetEntryPoint_Func("GetModuleInformation",
                                   &GetModuleInformation);

        if ( !EnumProcessModules  ||
             !GetModuleFileNameExA  ||
             !GetModuleBaseNameA  ||
             !GetModuleInformation ) {
            return false;
        }

        vector<HMODULE> modules;
        modules.resize(4096);

        string tmp;
        DWORD needed;
        if ( !EnumProcessModules(hProcess,
                                 &modules[0],
                                 modules.size()*sizeof(HMODULE),
                                 &needed) ) {
            NCBI_THROW(CCoreException, eCore,
                       "EnumProcessModules() failed");
        }

        if ( needed > modules.size() * sizeof(HMODULE)) {
            NCBI_THROW(CCoreException, eCore,
                       string("More than ") +
                       NStr::IntToString(modules.size()) + " modules");
        }


        needed /= sizeof(HMODULE);
        for (size_t i = 0;  i < needed;  ++i) {
            // for each module, get:
            // base address, size
            MODULEINFO mi;
            GetModuleInformation(hProcess, modules[i], &mi, sizeof(mi));

            SModuleEntry e;
            e.baseAddress = (DWORD) mi.lpBaseOfDll;
            e.size = mi.SizeOfImage;

            // image file name
            char tt[2048];
            tt[0] = '\0';
            GetModuleFileNameExA(hProcess, modules[i], tt, sizeof(tt));
            e.imageName = tt;

            // module name
            tt[0] = '\0';
            GetModuleBaseNameA(hProcess, modules[i], tt, sizeof(tt));
            e.moduleName = tt;

            mods.push_back(e);
        }

        return true;
    }
    catch (exception& e) {
        ERR_POST(Error << "Error getting PSAPI symbols: " << e.what());
    }
    catch (...) {
    }

    return false;
}


static bool s_FillModuleList(TModules& modules, DWORD pid, HANDLE hProcess)
{
    // try toolhelp32 first
    if (s_FillModuleListTH32(modules, pid)) {
        return true;
    }
    // nope? try psapi, then
    return s_FillModuleListPSAPI(modules, pid, hProcess);
}


static void s_IterateSymbols(HANDLE proc, DWORD pid)
{

    // Enumerate modules and tell imagehlp.dll about them.
    // On NT, this is not necessary, but it won't hurt.
    TModules modules;

    // fill in module list
    s_FillModuleList(modules, pid, proc);

    NON_CONST_ITERATE (TModules, it, modules) {
        if ( !SymLoadModule(proc, 0,
                            const_cast<char*>(it->imageName.c_str()),
                            const_cast<char*>(it->moduleName.c_str()),
                            it->baseAddress, it->size) ) {
            ERR_POST(Error << "Error loading symbols for module: "
                     << it->moduleName);
        } else {
            _TRACE("Loaded symbols from " << it->moduleName);
        }
    }
}

static bool s_SymbolInit()
{
    if (StackWalk) {
        return true;
    }

    // we load imagehlp.dll dynamically because the NT4-version does not
    // offer all the functions that are in the NT5 lib
    try {
        CDll dll("imagehlp.dll", CDll::eLoadNow, CDll::eNoAutoUnload);

        SymCleanup =
            dll.GetEntryPoint_Func("SymCleanup",
                                   &SymCleanup);
        SymFunctionTableAccess =
            dll.GetEntryPoint_Func("SymFunctionTableAccess",
                                   &SymFunctionTableAccess);
        SymGetLineFromAddr =
            dll.GetEntryPoint_Func("SymGetLineFromAddr",
                                   &SymGetLineFromAddr);
        SymGetModuleBase =
            dll.GetEntryPoint_Func("SymGetModuleBase",
                                   &SymGetModuleBase);
        SymGetModuleInfo =
            dll.GetEntryPoint_Func("SymGetModuleInfo",
                                   &SymGetModuleInfo);
        SymGetOptions =
            dll.GetEntryPoint_Func("SymGetOptions",
                                   &SymGetOptions);
        SymGetSymFromAddr =
            dll.GetEntryPoint_Func("SymGetSymFromAddr",
                                   &SymGetSymFromAddr);
        SymInitialize =
            dll.GetEntryPoint_Func("SymInitialize",
                                   &SymInitialize);
        SymSetOptions =
            dll.GetEntryPoint_Func("SymSetOptions",
                                   &SymSetOptions);
        StackWalk =
            dll.GetEntryPoint_Func("StackWalk",
                                   &StackWalk);
        UnDecorateSymbolName =
            dll.GetEntryPoint_Func("UnDecorateSymbolName",
                                   &UnDecorateSymbolName);
        SymLoadModule =
            dll.GetEntryPoint_Func("SymLoadModule",
                                   &SymLoadModule);

        return true;
    }
    catch (CException& e) {
        ERR_POST(Error << "Error opening imagehlp.dll: " << e.what());
        return false;
    }
    catch (...) {
        ERR_POST(Error << "Unknown error opening imagehlp.dll");
        return false;
    }
}


#define GET_CURRENT_CONTEXT(c, contextFlags) \
    do { \
        memset(&c, 0, sizeof(CONTEXT)); \
        c.ContextFlags = contextFlags; \
        __asm    call x \
        __asm x: pop eax \
        __asm    mov c.Eip, eax \
        __asm    mov c.Ebp, ebp \
        __asm    mov c.Esp, esp \
    } while(0)


void CStackTrace::GetStackTrace(TStack& stack_trace)
{
    if ( !s_SymbolInit() ) {
        ERR_POST(Error << "Error initializing stack trace: "
                 "No stack trace retrieved.");
        return;
    }

    if ( !SymInitialize  ||  !StackWalk ) {
        ERR_POST(Error << "Error initializing stack trace: "
                 "No stack trace retrieved.");
        return;
    }

    IMAGEHLP_SYMBOL *pSym = NULL;
    // our current process and thread within that process
    HANDLE curr_proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    try {
        string search_path(CDir::GetCwd());
        {{
             string tmp;
             tmp.resize(2048);
             if (GetModuleFileName(0, const_cast<char*>(tmp.data()),
                                   tmp.length())) {
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
        }}

        // init symbol handler stuff (SymInitialize())
        if ( !SymInitialize(curr_proc,
                            const_cast<char*>(search_path.c_str()),
                            false) ) {
            NCBI_THROW(CCoreException, eCore,
                       "SymInitialize() failed");
        }

        // set up default options
        //SymGetOptions()
        DWORD symOptions = SymGetOptions();
        symOptions |= SYMOPT_LOAD_LINES;
        symOptions &= ~SYMOPT_UNDNAME;
        SymSetOptions(symOptions);

        // pre-load our symbols
        s_IterateSymbols(curr_proc, GetCurrentProcessId());

        // we could use ImageNtHeader() here instead
        DWORD img_type = IMAGE_FILE_MACHINE_I386;

        // init CONTEXT record so we know where to start the stackwalk
        CONTEXT c;
        GET_CURRENT_CONTEXT(c, CONTEXT_FULL);

        // current stack frame
        STACKFRAME s;
        memset(&s, '\0', sizeof s);
        s.AddrPC.Offset    = c.Eip;
        s.AddrPC.Mode      = AddrModeFlat;
        s.AddrFrame.Offset = c.Ebp;
        s.AddrFrame.Mode   = AddrModeFlat;
        s.AddrStack.Offset = c.Esp;
        s.AddrStack.Mode   = AddrModeFlat;

        pSym = (IMAGEHLP_SYMBOL *) malloc(IMGSYMLEN + MAXNAMELEN);
        memset(pSym, '\0', IMGSYMLEN + MAXNAMELEN);
        pSym->SizeOfStruct = IMGSYMLEN;
        pSym->MaxNameLength = MAXNAMELEN;

        IMAGEHLP_LINE Line;
        memset(&Line, '\0', sizeof Line);
        Line.SizeOfStruct = sizeof(Line);

        IMAGEHLP_MODULE Module;
        memset(&Module, '\0', sizeof Module);
        Module.SizeOfStruct = sizeof(Module);

        DWORD offs = 0;

        for (size_t frame = 0;  ;  ++frame) {
            // get next stack frame
            if ( !StackWalk(img_type, curr_proc, thread, &s, &c, NULL,
                            SymFunctionTableAccess,
                            SymGetModuleBase,
                            NULL) ) {
                break;
            }

            // Discard the top frame describing the GetStackTrace() call
            if (frame < 1) {
                continue;
            }

            if ( !s.AddrPC.Offset ) {
                continue;
            }

            SStackFrameInfo sf_info;
            sf_info.func = "???";
            sf_info.file = "???";
            sf_info.offs = 0;
            sf_info.line = 0;

            // show procedure info (SymGetSymFromAddr())
            if ( !SymGetSymFromAddr ) {
                continue;
            }

            if ( !SymGetSymFromAddr(curr_proc,
                                    s.AddrPC.Offset,
                                    &offs,
                                    pSym) ) {
                ERR_POST(Error << "failed to get symbol for address: "
                    << s.AddrPC.Offset);
            } else {

                // retrieve function names, if we can
                if (UnDecorateSymbolName) {
                    char undName[MAXNAMELEN];
                    char undFullName[MAXNAMELEN];
                    UnDecorateSymbolName(pSym->Name, undName,
                                         MAXNAMELEN, UNDNAME_NAME_ONLY);
                    UnDecorateSymbolName(pSym->Name, undFullName,
                                         MAXNAMELEN, UNDNAME_COMPLETE);

                    sf_info.func = undFullName;
                    sf_info.offs = offs;
                }

                // retrieve file and line number info
                if (SymGetLineFromAddr) {
                    if (SymGetLineFromAddr(curr_proc,
                                           s.AddrPC.Offset,
                                           &offs,
                                           &Line) ) {
                        sf_info.file = Line.FileName;
                        sf_info.line = Line.LineNumber;
                    } else {
                        _TRACE("failed to get line number for "
                               << sf_info.func);
                    }
                }

                // get library info, if it is available
                if (SymGetModuleInfo) {
                    if ( !SymGetModuleInfo(curr_proc,
                                           s.AddrPC.Offset,
                                           &Module) ) {
                        ERR_POST(Error << "failed to get module info for "
                                 << sf_info.func);
                    } else {
                        sf_info.module = Module.ModuleName;
                        sf_info.module += "[";
                        sf_info.module += Module.ImageName;
                        sf_info.module += "]";
                    }
                }
            }

            stack_trace.push_back(sf_info);
        }
    }
    catch (exception& e) {
        ERR_POST(Error << "Error getting stack trace: " << e.what());
    }
    catch (...) {
        ERR_POST(Error << "Unknown error getting stack trace");
    }

    // return to our caller after cleaning up
    ResumeThread(thread);
    if (pSym) {
        free(pSym);
    }
    SymCleanup(curr_proc);
}


END_NCBI_SCOPE
