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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   C++->C conversion functions for basic CORE connect stuff:
 *     Registry
 *     Logging
 *     Locking
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_ansi_ext.h"
#include "ncbi_core_cxxp.hpp"
#include "ncbi_priv.h"
#include <connect/ncbi_util.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE


/***********************************************************************
 *                              Registry                               *
 ***********************************************************************/

static void s_REG_Get(void* user_data,
                      const char* section, const char* name,
                      char* value, size_t value_size) THROWS_NONE
{
    try {
        string result = static_cast<IRegistry*> (user_data)->
            Get(section, name);

        if ( !result.empty() )
            strncpy0(value, result.c_str(), value_size - 1);
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("s_REG_Get() failed", e);
    }
    STD_CATCH_ALL("s_REG_Get() failed");
}


static void s_REG_Set(void* user_data,
                      const char* section, const char* name,
                      const char* value, EREG_Storage storage) THROWS_NONE
{
    try {
        static_cast<IRWRegistry*> (user_data)->
            Set(section, name, value,
                (storage == eREG_Persistent ? CNcbiRegistry::ePersistent : 0) |
                CNcbiRegistry::eOverride | CNcbiRegistry::eTruncate);
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("s_REG_Set() failed", e);
    }
    STD_CATCH_ALL("s_REG_Set() failed");
}


static void s_REG_Cleanup(void* user_data) THROWS_NONE
{
    try {
        static_cast<IRegistry*> (user_data)->RemoveReference();
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("s_REG_Cleanup() failed", e);
    }
    STD_CATCH_ALL("s_REG_Cleanup() failed");
}


extern REG REG_cxx2c(IRWRegistry* reg, bool pass_ownership)
{
    if (pass_ownership  &&  reg) {
        reg->AddReference();
    }
    return reg ? REG_Create
        (static_cast<void*> (reg),
         reinterpret_cast<FREG_Get> (s_REG_Get),
         reinterpret_cast<FREG_Set> (s_REG_Set),
         pass_ownership ? reinterpret_cast<FREG_Cleanup> (s_REG_Cleanup) : 0,
         0) : 0;
}


/***********************************************************************
 *                                Logger                               *
 ***********************************************************************/

static void s_LOG_Handler(void*         /*user_data*/,
                          SLOG_Handler*   call_data) THROWS_NONE
{
    try {
        EDiagSev level;
        switch ( call_data->level ) {
        case eLOG_Trace:
            level = eDiag_Trace;
            break;
        case eLOG_Note:
            level = eDiag_Info;
            break;
        case eLOG_Warning:
            level = eDiag_Warning;
            break;
        case eLOG_Error:
            level = eDiag_Error;
            break;
        case eLOG_Critical:
            level = eDiag_Critical;
            break;
        case eLOG_Fatal:
            /*fallthru*/
        default:
            level = eDiag_Fatal;
            break;
        }

        CNcbiDiag diag(level);
        if (call_data->file) {
            diag.SetFile(call_data->file);
        }
        if (call_data->line) {
            diag.SetLine(call_data->line);
        }
        diag << call_data->message;
        if ( call_data->raw_size ) {
            diag <<
                "\n#################### [BEGIN] Raw Data (" <<
                call_data->raw_size <<
                " byte" << (call_data->raw_size != 1 ? "s" : "") << ")\n" <<
                NStr::PrintableString
                (string(static_cast<const char*> (call_data->raw_data),
                        call_data->raw_size), NStr::eNewLine_Passthru) <<
                "\n#################### [END] Raw Data";
        }
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("s_LOG_Handler() failed", e);
    }
    STD_CATCH_ALL("s_LOG_Handler() failed");
}


extern LOG LOG_cxx2c(void)
{
    return LOG_Create(0,
                      reinterpret_cast<FLOG_Handler> (s_LOG_Handler),
                      0,
                      0);
}


/***********************************************************************
 *                               MT-Lock                               *
 ***********************************************************************/

static int/*bool*/ s_LOCK_Handler(void* user_data, EMT_Lock how) THROWS_NONE
{
    try {
        CRWLock* lock = static_cast<CRWLock*> (user_data);
        switch ( how ) {
        case eMT_Lock:
            lock->WriteLock();
            break;
        case eMT_LockRead:
            lock->ReadLock();
            break;
        case eMT_Unlock:
            lock->Unlock();
            break;
        default:
            NCBI_THROW(CCoreException, eCore,
                       "Used with op " + (unsigned int) how);
        }
        return 1/*true*/;

    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("s_LOCK_Handler() failed", e);
    }
    STD_CATCH_ALL("s_LOCK_Handler() failed");
    return 0/*false*/;
}


static void s_LOCK_Cleanup(void* user_data) THROWS_NONE
{
    try {
        delete static_cast<CRWLock*> (user_data);
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("s_LOCK_Cleanup() failed", e);
    }
    STD_CATCH_ALL("s_LOCK_Cleanup() failed");
}


extern MT_LOCK MT_LOCK_cxx2c(CRWLock* lock, bool pass_ownership)
{
    return MT_LOCK_Create(static_cast<void*> (lock ? lock : new CRWLock),
                          reinterpret_cast<FMT_LOCK_Handler> (s_LOCK_Handler),
                          !lock  ||  pass_ownership ?
                          reinterpret_cast<FMT_LOCK_Cleanup> (s_LOCK_Cleanup) :
                          0);
}


/***********************************************************************
 *                                 Init                                *
 ***********************************************************************/

DEFINE_STATIC_FAST_MUTEX(s_ConnectInitMutex);

static enum EConnectInit {
    eConnectInit_Weak = -1,
    eConnectInit_Intact = 0,
    eConnectInit_Explicit = 1
} s_ConnectInit = eConnectInit_Intact;


static void s_Init(IRWRegistry*      reg = 0,
                   CRWLock*          lock = 0,
                   FConnectInitFlags flags = 0,
                   EConnectInit      how = eConnectInit_Weak)
{
    CORE_SetLOCK(MT_LOCK_cxx2c(lock, flags & eConnectInit_OwnLock ? true : false));
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(reg, flags & eConnectInit_OwnRegistry));
    s_ConnectInit = how;
}


/* PUBLIC */
extern void CONNECT_Init(IRWRegistry*      reg,
                         CRWLock*          lock,
                         FConnectInitFlags flags)
{
    CFastMutexGuard guard(s_ConnectInitMutex);
    try {
        s_Init(reg, lock, flags, eConnectInit_Explicit);
    }
    STD_CATCH_ALL("CONNECT_Init() failed");
}


/* PRIVATE */
extern void CONNECT_InitInternal(void)
{
    CFastMutexGuard guard(s_ConnectInitMutex);
    if (!g_CORE_Registry  &&  !g_CORE_Log  &&  !g_CORE_MT_Lock) {
        try {
            if (s_ConnectInit == eConnectInit_Intact) {
                CNcbiApplication* theApp = CNcbiApplication::Instance();
                s_Init(theApp ? &theApp->GetConfig() : 0);
            }
        }
        STD_CATCH_ALL("CONNECT_InitInternal() failed");
    } else {
        s_ConnectInit = eConnectInit_Explicit;
    }
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.27  2005/04/22 17:44:28  vasilche
 * Use mutex guard to avoid possibility of abandoned mutex.
 *
 * Revision 6.26  2004/12/20 16:45:07  ucko
 * Accept any IRWRegistry rather than specifically requiring a CNcbiRegistry.
 * Even with ownership passed, don't delete registries that still have
 * other references.
 *
 * Revision 6.25  2004/10/08 14:48:01  kuznets
 * MSVC warning exterminated
 *
 * Revision 6.24  2004/09/23 12:25:50  lavr
 * CONNECT_InitInternal(): Do not use CORE_LOCK_READ/UNLOCK
 *
 * Revision 6.23  2004/09/22 18:35:11  ucko
 * Fix typo in previous revision.
 *
 * Revision 6.22  2004/09/22 16:40:45  lavr
 * Do not perform weak init if C level REG/LOG/LOCK seen as being used
 *
 * Revision 6.21  2004/09/09 16:45:30  lavr
 * Introduce CONNECT_InitInternal() [and locking against concurrent init]
 *
 * Revision 6.20  2004/07/14 15:46:08  lavr
 * +flags (=0) for CONNECT_Init()
 *
 * Revision 6.19  2004/07/08 14:28:34  lavr
 * Additional parameter lock for CONNET_Init()
 * Do not pass ownership of the registry in CONNECT_Init()
 *
 * Revision 6.18  2004/05/17 20:58:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.17  2003/05/14 03:52:50  lavr
 * Minor change: use of default's parameter default value removed in CNcbiDiag
 *
 * Revision 6.16  2003/05/05 20:17:59  lavr
 * s_LOG_Handler() to explicitly check data_size only
 *
 * Revision 6.15  2002/12/19 20:48:33  lavr
 * Fix double exception handling in both catch() and STD_CATCH_ALL()
 *
 * Revision 6.14  2002/12/19 17:33:47  lavr
 * More complete (and consistent with corelib) exception handling
 *
 * Revision 6.13  2002/10/28 15:42:57  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.12  2002/10/21 18:31:31  lavr
 * Take advantage of eNewLine_Passthru in data dumping
 *
 * Revision 6.11  2002/09/24 15:07:13  lavr
 * File description indented uniformly
 *
 * Revision 6.10  2002/06/10 19:50:02  lavr
 * +CONNECT_Init(). Log moved to the end
 *
 * Revision 6.9  2002/05/07 18:21:23  lavr
 * +#include <ncbidiag.hpp>
 *
 * Revision 6.8  2002/01/25 23:33:04  vakatov
 * s_LOCK_Handler() -- to match handler proto, return boolean value (not VOID!)
 *
 * Revision 6.7  2002/01/15 21:28:49  lavr
 * +MT_LOCK_cxx2c()
 *
 * Revision 6.6  2001/03/02 20:08:17  lavr
 * Typos fixed
 *
 * Revision 6.5  2001/01/25 17:03:46  lavr
 * s_LOG_Handler: user_data commented out as unused
 *
 * Revision 6.4  2001/01/23 23:08:06  lavr
 * LOG_cxx2c introduced
 *
 * Revision 6.3  2001/01/12 05:48:50  vakatov
 * Use reinterpret_cast<> rather than static_cast<> to cast functions.
 * Added more paranoia to catch ALL possible exceptions in the s_*** functions.
 *
 * Revision 6.2  2001/01/11 23:51:47  lavr
 * static_cast instead of linkage specification 'extern "C" {}'.
 * Reason: MSVC++ doesn't allow C-linkage of the funs compiled in C++ file.
 *
 * Revision 6.1  2001/01/11 23:08:16  lavr
 * Initial revision
 *
 * ===========================================================================
 */
