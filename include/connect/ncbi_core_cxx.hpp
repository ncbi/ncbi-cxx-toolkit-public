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
 * @file ncbi_core_cxx.hpp
 *   C++->C conversion functions for basic CORE connect stuff:
 *   - Registry,
 *   - Logging,
 *   - Locking.
 *
 */

#include <corelib/ncbireg.hpp>
#include <connect/ncbi_tls.h>


/** @addtogroup UtilityFunc
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Convert a C++ Toolkit registry object to a REG registry.
/// @note The C++ registries are CObjects, so any we "own" will be deleted
///       if and only if nothing else is still holding a reference to them.
/// @param reg
///  A C++ toolkit registry, on top of which new REG registry is to be created
/// @param pass_ownership
///  True if the ownership of "reg" gets passed to new REG
/// @return
///  New REG registry (or NULL on error)
/// @sa
///  REG_Create, CONNECT_Init
extern NCBI_XCONNECT_EXPORT REG     REG_cxx2c
(IRWRegistry* reg,
 bool         pass_ownership = false
 );


/// Convert a C++ Toolkit read-only registry object to a REG registry.
/// @note The returned registry won't have a set method available, and any
///       attempt to set a parameter will fail.
/// @note The C++ registries are CObjects, so any we "own" will be deleted
///       if and only if nothing else is still holding a reference to them.
/// @param reg
///  A C++ toolkit registry, on top of which new REG registry is to be created
/// @param pass_ownership
///  True if the ownership of "reg" gets passed to new REG
/// @return
///  New REG registry (or NULL on error)
/// @sa
///  REG_Create, CONNECT_Init
extern NCBI_XCONNECT_EXPORT REG     REG_cxx2c
(const IRWRegistry* reg,
 bool               pass_ownership = false
 );


/// Create LOG on top of C++ Toolkit CNcbiDiag.
/// @return
///  New LOG log (or NULL on error)
/// @sa
///  LOG_Create, CONNECT_Init
extern NCBI_XCONNECT_EXPORT LOG     LOG_cxx2c(void);


/// Convert a C++ Toolkit lock object to an MT_LOCK lock.
/// @param lock
///  Existing lock to convert (if NULL a new CRWLock will be used)
/// @param pass_ownership
///  True if the ownership of non-NULL lock gets passed to new MT_LOCK
/// @return
///  New MT_LOCK lock (or NULL on error)
/// @sa
///  MT_LOCK_Create, CONNECT_Init
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_cxx2c
(CRWLock* lock = 0,
 bool     pass_ownership = false
 );


/// CONNECT_Init flags:  which parameters to own / initialize.
/// @sa
///  CONNECT_Init
enum EConnectInitFlag {
    eConnectInit_OwnNothing  = 0,  ///< Original ownership gets retained
    eConnectInit_OwnRegistry = 1,  ///< Registry ownership gets passed
    eConnectInit_OwnLock     = 2,  ///< Lock ownership gets passed
    eConnectInit_NoSSL       = 4   ///< Do NOT init secure socket layer (SSL)
};
typedef unsigned int TConnectInitFlags;  ///< Bitwise OR of EConnectInitFlag


/// Init [X]CONNECT library with the specified "reg" and "lock" (ownership for
/// either or both can be detailed in the "flag" parameter).
/// @warning MUST be called in MT applications to make CONNECT MT-safe, or
///          alternatively, CConnIniter must be used as a base-class.
/// @param reg
///  Registry to use, non-modifiable (none if NULL)
/// @param lock
///  Lock to use (new lock will get created if NULL)
/// @param flag
///  Ownership / initialization control
/// @param ssl
///  TLS provider to use for SSL (ignored if eConnectInit_NoSSL;
///                               0 selects the default provider)
/// @note LOG will get created out of CNcbiDiag automatically.
/// @sa
///  EConnectInitFlag, CConnIniter, REG_cxx2c, LOG_cxx2c, MT_LOCK_cxx2c,
///  NcbiSetupTls, SOCK_SetupSSL,
///  CNcbiApplication, CRWLock, CNcbiRegistry, CNcbiDiag
extern NCBI_XCONNECT_EXPORT void CONNECT_Init
(const IRWRegistry* reg  = 0,
 CRWLock*           lock = 0,
 TConnectInitFlags  flag = eConnectInit_OwnNothing,
 FSSLSetup          ssl  = 0);


/////////////////////////////////////////////////////////////////////////////
///
/// Helper hook-up class that installs default logging/registry/locking (but
/// only if they have not yet been installed explicitly by the user) as if by
/// calling CONNECT_Init(&CNcbiApplication::GetConfig()) automagically.
/// @note  Derive your CONNECT-dependent classes from this class for MT safety:
///        @code
///        class CMyNetworkedObject : virtual protected CConnIniter
///                                   [, ... other bases]
///        @endcode
/// @note  You can also use this class for an inline initialization of some
///        library code (in a function body) like so:
///        @code
///        {{
///            class CInPlaceConnIniter : protected CConnIniter {
///            } conn_initer;
///        }}
///        @endcode
/// @sa
///  CONNECT_Init, CConn_IOStream, CONN_OpenURL
class NCBI_XCONNECT_EXPORT CConnIniter
{
protected:
    CConnIniter(void);
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
///   Variable to receive a numeric timeout value.
/// @return
///   A special constant kDefaultTimeout or kInfiniteTimeout, if the incoming
///   timeout is either a default or an infinite one, respectively.
///   A pointer to the "sto" variable, if the incoming timeout contains some
///   other (finite and numeric) value, and "sto" is used to receive the value.
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
    if ( cto.IsInfinite() )
        return kInfiniteTimeout;
    ::memset(&sto, 0, sizeof(sto));
    cto.Get(&sto.sec, &sto.usec);
    return &sto;
}

inline 
CTimeout g_STimeoutToCTimeout(const STimeout* sto)
{
    if ( sto == kDefaultTimeout )
        return CTimeout(CTimeout::eDefault);
    if ( sto == kInfiniteTimeout )
        return CTimeout(CTimeout::eInfinite);
    return CTimeout(sto->sec, sto->usec);
}


END_NCBI_SCOPE


/* @} */

#endif  // CONNECT___NCBI_CORE_CXX__HPP
