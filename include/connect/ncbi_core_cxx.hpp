#ifndef CONNECT___NCBI_CORE_CXX__H
#define CONNECT___NCBI_CORE_CXX__H

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
 * Author:  Anton Lavrentiev
 *
 * File description:
 *   C++->C conversion functions for basic CORE connect stuff:
 *     Registry
 *     Logging
 *     Locking
 *
 */

#include <connect/ncbi_core.h>
#include <corelib/ncbireg.hpp>


/** @addtogroup UtilityFunc
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// NB: now that registries are CObjects, any we "own" will be deleted
/// if and only if nothing else still holds a reference to them.
extern NCBI_XCONNECT_EXPORT REG     REG_cxx2c
(IRWRegistry* reg,
 bool         pass_ownership = false
 );


extern NCBI_XCONNECT_EXPORT LOG     LOG_cxx2c(void);


extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_cxx2c
(CRWLock*     lock = 0,
 bool         pass_ownership = false
 );


enum EConnectInitFlag {
    eConnectInit_OwnNothing  = 0,
    eConnectInit_OwnRegistry = 1,
    eConnectInit_OwnLock     = 2
};
typedef unsigned int TConnectInitFlags;  // bitwise OR of EConnectInitFlag


extern NCBI_XCONNECT_EXPORT void CONNECT_Init
(IRWRegistry*      reg  = 0,
 CRWLock*          lock = 0,
 TConnectInitFlags flag = eConnectInit_OwnNothing);


/////////////////////////////////////////////////////////////////////////////
///
/// Helper hook-up class that installs default logging/registry/locking
/// (but only if they have not yet been installed explicitly by user).
///

class NCBI_XCONNECT_EXPORT CConnIniter
{
protected:
    CConnIniter();
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeout/STimeout adapters
///

/// Convert CTimeout to STimeout.
///
/// @param cto
///   Timeout value to convert.
/// @param sto
///   Variable used to store numeric timeout value.
/// @return
///   A special constants kDefaultTimeout or kInfiniteTimeout, 
///   if timeout have default or infinite value accordingly.
///   A pointer to "sto" object, if timeout have numeric value. 
///   "sto" will be used to store numeric value.
/// @sa CTimeout, STimeout
const STimeout* g_CTimeoutToSTimeout(const CTimeout& cto, STimeout& sto);

/// Convert STimeout to CTimeout.
///
/// @sa CTimeout, STimeout
CTimeout g_STimeoutToCTimeout(const STimeout* sto);


inline 
const STimeout* g_CTimeoutToSTimeout(const CTimeout& cto, STimeout& sto)
{
    if ( cto.IsDefault() )
        return kDefaultTimeout;
    else if ( cto.IsInfinite() )
        return kInfiniteTimeout;
    else {
        cto.Get(&sto.sec, &sto.usec);
        return &sto;
    }
}

inline 
CTimeout g_STimeoutToCTimeout(const STimeout* sto)
{
    if ( sto == kDefaultTimeout )
        return CTimeout(CTimeout::eDefault);
    else if ( sto == kInfiniteTimeout )
        return CTimeout(CTimeout::eInfinite);
    return CTimeout(sto->sec, sto->usec);
}


END_NCBI_SCOPE


/* @} */

#endif  // CONNECT___NCBI_CORE_CXX__HPP
