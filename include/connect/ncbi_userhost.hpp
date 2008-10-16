#ifndef CONNECT___NCBI_USERHOST__HPP
#define CONNECT___NCBI_USERHOST__HPP

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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   User and host setup for NCBI Diag
 *
 */

#include <corelib/ncbistd.hpp>
#include <connect/connect_export.h>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Flags for SetDiagUserAndHost()
enum EDiagUserAndHost {
    fDiag_AddUser          = 1 << 0, ///< Add username to diag context
    fDiag_AddHost          = 1 << 1, ///< Add hostname to diag context
    fDiag_OverrideExisting = 1 << 2  ///< Set current user and host even if
                                     ///< they are already set.
};
typedef int TDiagUserAndHost;


/// Set username and hostname properties for the diag context.
/// Do not update existing properties if fDiag_OverrideExisting is not set.
NCBI_XCONNECT_EXPORT void
SetDiagUserAndHost(TDiagUserAndHost flags = fDiag_AddUser | fDiag_AddHost);


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_USERHOST__HPP */
