#ifndef VALIDATOR___VALIDERROR_SUPPRESS__HPP
#define VALIDATOR___VALIDERROR_SUPPRESS__HPP

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
 * Author:  Justin Foley, Colleen Bollin......
 *
 * File Description:
 *   Methods to facilitate runtime supression of validation errors
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <set>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_entry_Handle;
class CSeq_submit;
class CUser_object;
class CBioseq;

BEGIN_SCOPE(validator)


class NCBI_VALIDATOR_EXPORT CValidErrorSuppress 
{
public:

    using TErrCode = CValidErrItem::TErrIndex;
    using TCodes = set<TErrCode>;

    // for suppressing error collection during runtime
    static void SetSuppressedCodes(const CUser_object& user, TCodes& errCodes);
    static void SetSuppressedCodes(const CSeq_entry& se, TCodes& errCodes);
    static void SetSuppressedCodes(const CSeq_entry_Handle& se, TCodes& errCodes);
    static void SetSuppressedCodes(const CSeq_submit& ss, TCodes& errCodes);
    static void SetSuppressedCodes(const CBioseq& seq, TCodes& errCodes);
    static void AddSuppression(CUser_object& user, TErrCode errCode);
};



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_SUPPRESS__HPP */
