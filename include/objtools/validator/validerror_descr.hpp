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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_DESCR__HPP
#define VALIDATOR___VALIDERROR_DESCR__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objtools/validator/validerror_desc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

BEGIN_SCOPE(validator)

// ============================  Validate SeqDescr  ============================


class CValidError_descr : private CValidError_base
{
public:
    CValidError_descr(CValidError_imp& imp);
    ~CValidError_descr() override;

    void ValidateSeqDescr(const CSeq_descr& descr, const CSeq_entry& ctx);
    bool ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, bool report);
private:

    CValidError_desc m_DescValidator;
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_DESCR__HPP */
