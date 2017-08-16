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

#ifndef VALIDATOR___VALIDERROR_DESC__HPP
#define VALIDATOR___VALIDERROR_DESC__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objects/valid/Comment_rule.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeqdesc;
class CMolInfo;
class CUser_object;

BEGIN_SCOPE(validator)

// =============================  Validate SeqDesc  ============================

class NCBI_VALIDATOR_EXPORT CValidError_desc : private CValidError_base
{
public:
    CValidError_desc(CValidError_imp& imp);
    virtual ~CValidError_desc(void);

    void ValidateSeqDesc(const CSeqdesc& desc, const CSeq_entry& ctx);

    bool ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, bool report = true);
    bool ValidateDblink(const CUser_object& usr, const CSeqdesc& desc, bool report = true);

    void ResetModifCounters(void);
private:

    void ValidateComment(const string& comment, const CSeqdesc& desc);
    void ValidateTitle(const string& title, const CSeqdesc& desc, const CSeq_entry& ctx);
    bool ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, const CComment_rule& rule, bool report);
    bool ValidateStructuredCommentGeneric(const CUser_object& usr, const CSeqdesc& desc, bool report);
    void x_ReportStructuredCommentErrors(const CSeqdesc& desc, const CComment_rule::TErrorList& errors);
    void ValidateUser(const CUser_object& usr, const CSeqdesc& desc);
    void ValidateMolInfo(const CMolInfo& minfo, const CSeqdesc& desc);

    CConstRef<CSeq_entry> m_Ctx;
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_DESC__HPP */
