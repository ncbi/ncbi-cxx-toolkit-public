#ifndef NCBI_WIN_HOOK__HPP
#define NCBI_WIN_HOOK__HPP

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
 * Author:  Sergey Sikorskiy
 *
 * File Description:  Windows DLL function hooking
 *
 */


/** @addtogroup WinHook
 *
 * @{
 */

#if defined(NCBI_OS_MSWIN)

#include <corelib/ncbi_safe_static.hpp>
#include <process.h>
#include <Tlhelp32.h>
#include <vector>

BEGIN_NCBI_SCOPE

namespace NWinHook
{

    ///////////////////////////////////////////////////////////////////////////////
    class NCBI_DBAPIDRIVER_EXPORT CWinHookException : public CCoreException
    {
    public:
        enum EErrCode {
            eDbghelp,
            eDisabled
        };

        /// Translate from the error code value to its string representation.
        virtual const char* GetErrCodeString(void) const;

        // Standard exception boilerplate code.
        NCBI_EXCEPTION_DEFAULT(CWinHookException, CCoreException);
    };


    ///////////////////////////////////////////////////////////////////////////////
    /// class CHookedFunctions
    ///
    class CHookedFunction;

    // !!! Not thred-safe class !!!
    class CHookedFunctions
    {
    public:
        CHookedFunctions(void);
        ~CHookedFunctions(void);

    public:
        /// Return the address of an CHookedFunction object
        CRef<CHookedFunction> GetHookedFunction(
            PCSTR pszCalleeModName,
            PCSTR pszFuncName
            ) const;

        /// Return the address of an CHookedFunction object
        CRef<CHookedFunction> GetHookedFunction(
            HMODULE hmod,
            PCSTR   pszFuncName
            ) const;

        /// Add a new object to the container
        BOOL AddHook(const CRef<CHookedFunction> pHook);
        /// Remove exising object pointer from the container
        BOOL RemoveHook(const CRef<CHookedFunction> pHook);

        void UnHookAllFuncs(void);

        bool HaveHookedFunctions(void) const
        {
//             return(m_FunctionList.size() > 0);
            size_t num = 0;

            ITERATE(TModuleList, it, m_ModuleList) {
                num += it->second.size();
            }

            return (num > 0);
        }

    private:
        /// Return the name of the function from EAT by its ordinal value
        BOOL x_GetFunctionNameFromExportSection(
            HMODULE hmodOriginal,
            DWORD   dwFuncOrdinalNum,
            PSTR    pszFuncName
            ) const;
        /// Return the name of the function by its ordinal value
        void x_GetFunctionNameByOrdinal(
            PCSTR   pszCalleeModName,
            DWORD   dwFuncOrdinalNum,
            PSTR    pszFuncName
            ) const;
        void x_GetFunctionNameByOrdinal(
            HMODULE hmodOriginal,
            DWORD   dwFuncOrdinalNum,
            PSTR    pszFuncName
            ) const;

    private:
        struct SNocaseCmp {
            bool operator()(const string& x, const string& y) const {
                size_t sx = x.size();
                size_t sy = y.size();
                if (sx < sy)
                    return true;
                if (sx > sy)
                    return false;

                const char* dx = x.data();
                const char* dy = y.data();
                for (; sx != 0 /* &&  sy != 0 */; ++dx, --sx, ++dy /*, --sy*/) {
                    char cx = *dx;
                    char cy = *dy;
                    if (cx >= 'A'  &&  cx <= 'Z')
                        cx += 'a' - 'A';
                    if (cy >= 'A'  &&  cy <= 'Z')
                        cy += 'a' - 'A';
                    if (cx < cy)
                        return true;
                    if (cx > cy)
                        return false;
                }
                /*if (sy == 0)
                    return false;
                return true;*/
                return false;
            }
        };
        typedef map<string, CRef<CHookedFunction>, SNocaseCmp> TFunctionList;
        typedef map<void*, TFunctionList> TModuleList;
        typedef map<string, TFunctionList, SNocaseCmp> TModuleNameList;

        // TFunctionList m_FunctionList;
        TModuleList     m_ModuleList;
        TModuleNameList m_ModuleNameList;

        // Because of CApiHookMgr::HackModuleOnLoad
        friend class CApiHookMgr;
    };


    ///////////////////////////////////////////////////////////////////////////////
    /// class CApiHookMgr
    ///
    class CApiHookMgr {
    private:
        CApiHookMgr(void);
        ~CApiHookMgr(void);
        void operator =(const CApiHookMgr&);

    public:
        static CApiHookMgr& GetInstance(void);

        /// Hook up an API
        BOOL HookImport(PCSTR pszCalleeModName,
                        PCSTR pszFuncName,
                        PROC  pfnHook
                        );

        /// Restore hooked up API function
        BOOL UnHookImport(PCSTR pszCalleeModName,
                          PCSTR pszFuncName
                          );

        /// Used when a DLL is newly loaded after hooking a function
        void WINAPI HackModuleOnLoad(HMODULE hmod,
                                     DWORD   dwFlags
                                     );

        /// Return the address of an CHookedFunction object
        /// Protected version.
        CRef<CHookedFunction> GetHookedFunction(HMODULE hmod,
                                                PCSTR   pszFuncName
                                                ) const;

        /// Indicates whether there is hooked function
        bool HaveHookedFunctions(void) const;

    private:
        /// Hook all needed system functions in order to trap loading libraries
        BOOL x_HookSystemFuncs(void);

        /// Unhook all functions and restore original ones
        void x_UnHookAllFuncs(void);

        /// Used to trap events when DLLs are loaded
        static HMODULE WINAPI MyLoadLibraryA(PCSTR  pszModuleName);
        /// Used to trap events when DLLs are loaded
        static HMODULE WINAPI MyLoadLibraryW(PCWSTR pszModuleName);
        /// Used to trap events when DLLs are loaded
        static HMODULE WINAPI MyLoadLibraryExA(PCSTR  pszModuleName,
                                               HANDLE hFile,
                                               DWORD  dwFlags
                                               );
        /// Used to trap events when DLLs are loaded
        static HMODULE WINAPI MyLoadLibraryExW(PCWSTR pszModuleName,
                                               HANDLE hFile,
                                               DWORD  dwFlags
                                               );
        /// Returns address of replacement function if hooked function is
        /// requested
        static FARPROC WINAPI MyGetProcAddress(HMODULE hmod,
                                               PCSTR   pszProcName
                                               );

        /// Returns original address of the API function
        static FARPROC WINAPI xs_GetProcAddressWindows(
            HMODULE hmod,
            PCSTR   pszProcName
            );

        /// Add a newly intercepted function to the container
        BOOL x_AddHook(PCSTR  pszCalleeModName,
                       PCSTR  pszFuncName,
                       PROC   pfnOrig,
                       PROC   pfnHook
                       );

        /// Remove intercepted function from the container
        BOOL x_RemoveHook(PCSTR pszCalleeModName,
                          PCSTR pszFuncName
                          );

        mutable CFastMutex m_Mutex;
        /// Container keeps track of all hacked functions
        CHookedFunctions m_pHookedFunctions;
        /// Determines whether all system functions has been successfuly hacked
        BOOL m_bSystemFuncsHooked;

        friend class CSafeStaticPtr<CApiHookMgr>;
    };


    class NCBI_DBAPIDRIVER_EXPORT COnExitProcess
    {
    public:
        typedef void (*TFunct) (void);

        static COnExitProcess& Instance(void);

        // Return true in case of success.
        bool Add(TFunct funct);
        void Remove(TFunct funct);
        void ClearAll(void);

    private:
        COnExitProcess(void);
        ~COnExitProcess(void);

        // Hook function prototype
        static void WINAPI xs_ExitProcess(UINT uExitCode);

    private:
        typedef vector<TFunct> TRegistry;

        CFastMutex  m_Mutex;
        TRegistry   m_Registry;
        bool        m_Hooked;

        friend class CSafeStaticPtr<COnExitProcess>;
    };
}

END_NCBI_SCOPE

#endif // NCBI_OS_MSWIN

/* @} */

#endif  // NCBI_WIN_HOOK__HPP
