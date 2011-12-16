#ifndef UTIL_ICANCELED__HPP
#define UTIL_ICANCELED__HPP

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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   ICanceled -
 *      Interface for testing cancellation request in a long lasting operation.
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup StreamSupport
 *
 * @{
 */


/// Interface for testing cancellation request in a long lasting operation.
/// The implementation must have CObject as its first parent.
class ICanceled
{
public:
    virtual bool IsCanceled() const = 0;
    virtual ~ICanceled() {}
};



/* @} */


END_NCBI_SCOPE

#endif // UTIL_ICANCELED__HPP
