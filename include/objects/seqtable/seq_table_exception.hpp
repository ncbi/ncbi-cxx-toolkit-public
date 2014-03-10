#ifndef SEQ_TABLE_EXCEPTION__HPP
#define SEQ_TABLE_EXCEPTION__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Exception class for Seq-table objects
*
*/

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


/// Seq-loc and seq-align mapper exceptions
class NCBI_SEQ_EXPORT CSeqTableException : public CException
{
public:
    enum EErrCode {
        eColumnNotFound,        ///< Requested column is missing
        eRowNotFound,           ///< Requested row is missing
        eIncompatibleRowType,   ///< Data cannot be converted to asked type
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSeqTableException, CException);
};


/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_TABLE_EXCEPTION__HPP
