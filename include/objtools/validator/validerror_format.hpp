#ifndef VALIDATOR___VALIDERROR_FORMAT__HPP
#define VALIDATOR___VALIDERROR_FORMAT__HPP

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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objmgr/scope.hpp>

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_entry_Handle;
class CSeq_submit;
class CSeq_annot;
class CSeq_annot_Handle;
class CUser_object;
class CSeq_feat;
class CBioSource;
class CPubdesc;
class CBioseq;
class CSeqdesc;
class CObjectManager;
class CScope;

BEGIN_SCOPE(validator)


enum ESubmitterFormatErrorGroup {
    eSubmitterFormatErrorGroup_ConsensusSplice = 0,
    eSubmitterFormatErrorGroup_BadEcNumberFormat,
    eSubmitterFormatErrorGroup_BadEcNumberValue,
    eSubmitterFormatErrorGroup_BadEcNumberProblem,
    eSubmitterFormatErrorGroup_BadSpecificHost,
    eSubmitterFormatErrorGroup_BadInstitutionCode,
    eSubmitterFormatErrorGroup_LatLonCountry,
    eSubmitterFormatErrorGroup_Default
};


class NCBI_VALIDATOR_EXPORT CValidErrorFormat : public CObject 
{
public:

    // Constructor / Destructor
    CValidErrorFormat(CObjectManager& objmgr);
    ~CValidErrorFormat(void);

    ESubmitterFormatErrorGroup GetSubmitterFormatErrorGroup(CValidErrItem::TErrIndex err_code) const;
    string GetSubmitterFormatErrorGroupTitle(CValidErrItem::TErrIndex err_code) const;
    string FormatForSubmitterReport(const CValidErrItem& error, CScope& scope) const;
    string FormatForSubmitterReport(const CValidError& errors, CScope& scope, CValidErrItem::TErrIndex err_code) const;
    string FormatCategoryForSubmitterReport(const CValidError& errors, CScope& scope, ESubmitterFormatErrorGroup grp) const;
    vector<unsigned int> GetListOfErrorCodes(const CValidError& errors) const;
    vector<string> FormatCompleteSubmitterReport(const CValidError& errors, CScope& scope) const;

    // for formatting the objects as presented by the validator
    static string GetFeatureContentLabel (const CSeq_feat& feat, CRef<CScope> scope);
    static string GetFeatureIdLabel (const CFeat_id& feat_id);
    static string GetFeatureLabel(const CSeq_feat& ft, CRef<CScope> scope, bool suppress_context);
    static string GetDescriptorContent (const CSeqdesc& ds);
    static string GetDescriptorLabel(const CSeqdesc& ds, const CSeq_entry& ctx, CRef<CScope> scope, bool suppress_context);
    static string GetBioseqLabel (CBioseq_Handle bh);
    static string GetBioseqSetLabel(const CBioseq_set& st, CRef<CScope> scope, bool suppress_context);
    static string GetObjectLabel(const CObject& obj, const CSeq_entry& ctx, CRef<CScope> scope, bool suppress_context); 

    // for suppressing error collection during runtime
    static void SetSuppressionRules(const CUser_object& user, CValidError& errors);
    static void SetSuppressionRules(const CSeq_entry& se, CValidError& errors);
    static void SetSuppressionRules(const CSeq_entry_Handle& se, CValidError& errors);
    static void SetSuppressionRules(const CSeq_submit& ss, CValidError& errors);
    static void SetSuppressionRules(const CBioseq& seq, CValidError& errors);
    static void AddSuppression(CUser_object& user, unsigned int error_code);

private:
    // Prohibit copy constructor & assignment operator
    CValidErrorFormat(const CValidErrorFormat&);
    CValidErrorFormat& operator= (const CValidErrorFormat&);


    string x_FormatConsensusSpliceForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatECNumberForSubmitterReport(const CValidErrItem& error, CScope& scope) const;
    string x_FormatBadSpecificHostForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatBadInstCodeForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatLatLonCountryForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatPartialProblemForSubmitterReport(const CValidErrItem& error) const;

    CRef<CObjectManager>    m_ObjMgr;

};


// Inline Functions:


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_FORMAT__HPP */
