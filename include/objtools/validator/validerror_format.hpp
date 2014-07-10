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

private:
    // Prohibit copy constructor & assignment operator
    CValidErrorFormat(const CValidErrorFormat&);
    CValidErrorFormat& operator= (const CValidErrorFormat&);


    string x_FormatConsensusSpliceForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatECNumberForSubmitterReport(const CValidErrItem& error, CScope& scope) const;
    string x_FormatBadSpecificHostForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatBadInstCodeForSubmitterReport(const CValidErrItem& error) const;
    string x_FormatLatLonCountryForSubmitterReport(const CValidErrItem& error) const;

    CRef<CObjectManager>    m_ObjMgr;

};


// Inline Functions:


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_FORMAT__HPP */
