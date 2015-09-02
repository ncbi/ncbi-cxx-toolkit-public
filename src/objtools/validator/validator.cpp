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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/validator/validator.hpp>
#include <util/static_map.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/cached_taxon3.hpp>

#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/validerror_format.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
USING_SCOPE(sequence);


// *********************** CValidator implementation **********************


CValidator::CValidator(CObjectManager& objmgr,
    AutoPtr<ITaxon3> taxon) :
    m_ObjMgr(&objmgr),
    m_PrgCallback(0),
    m_UserData(0)
{
    if (taxon.get() == NULL) {
        AutoPtr<ITaxon3> taxon(new CTaxon3);
        taxon->Init();
        m_Taxon.reset(CCachedTaxon3::CreateUnSafe(taxon));
    } else {
        m_Taxon = taxon;
    }
    m_Taxon->Init();
}


CValidator::~CValidator(void)
{
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_entry& se,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&se));
    CValidErrorFormat::SetSuppressionRules(se, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.Validate(se, 0, scope) ) {
        errors.Reset();
    }
    return errors;
}

CConstRef<CValidError> CValidator::Validate
(const CSeq_entry_Handle& seh,
 Uint4 options)
{
    static unsigned int num_e = 0, mult = 0;

    num_e++;
    if (num_e % 200 == 0) {
        num_e = 0;
        mult++;
    }

    CRef<CValidError> errors(new CValidError(&*seh.GetCompleteSeq_entry()));
    CValidErrorFormat::SetSuppressionRules(seh, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.Validate(seh, 0) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::GetTSANStretchErrors(const CSeq_entry_Handle& se)
{
    CRef<CValidError> errors(new CValidError(&*se.GetCompleteSeq_entry()));
    CValidErrorFormat::SetSuppressionRules(se, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), 0);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.GetTSANStretchErrors(se) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::GetTSACDSOnMinusStrandErrors (const CSeq_entry_Handle& se)
{
    CRef<CValidError> errors(new CValidError(&*se.GetCompleteSeq_entry()));
    CValidErrorFormat::SetSuppressionRules(se, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), 0);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.GetTSACDSOnMinusStrandErrors(se) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::GetTSAConflictingBiomolTechErrors (const CSeq_entry_Handle& se)
{
    CRef<CValidError> errors(new CValidError(&*se.GetCompleteSeq_entry()));
    CValidErrorFormat::SetSuppressionRules(se, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), 0);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.GetTSAConflictingBiomolTechErrors(se) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::GetTSANStretchErrors(const CBioseq& seq)
{

    CRef<CValidError> errors(new CValidError(&seq));
    CValidErrorFormat::SetSuppressionRules(seq, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), 0);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.GetTSANStretchErrors(seq) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::GetTSACDSOnMinusStrandErrors (const CSeq_feat& f, const CBioseq& seq)
{
    CRef<CValidError> errors(new CValidError(&f));
    CValidErrorFormat::SetSuppressionRules(seq, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), 0);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.GetTSACDSOnMinusStrandErrors(f, seq) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::GetTSAConflictingBiomolTechErrors (const CBioseq& seq)
{
    CRef<CValidError> errors(new CValidError(&seq));
    CValidErrorFormat::SetSuppressionRules(seq, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), 0);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.GetTSAConflictingBiomolTechErrors(seq) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_submit& ss,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&ss));
    CValidErrorFormat::SetSuppressionRules(ss, *errors);
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.Validate(ss, scope);
    if (ss.IsSetSub() && ss.GetSub().IsSetContact() && ss.GetSub().GetContact().IsSetContact()
        && ss.GetSub().GetContact().GetContact().IsSetAffil()
        && ss.GetSub().GetContact().GetContact().GetAffil().IsStd()) {
        imp.ValidateSubAffil(ss.GetSub().GetContact().GetContact().GetAffil().GetStd(),
                             ss, 0);
    }
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_annot_Handle& sah,
 Uint4 options)
{
    CConstRef<CSeq_annot> sar = sah.GetCompleteSeq_annot();
    CRef<CValidError> errors(new CValidError(&*sar));
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.Validate(sah);
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_feat& feat, 
 CScope *scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&feat));
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.Validate(feat, scope);
    return errors;
}

CConstRef<CValidError> CValidator::Validate
(const CSeq_feat& feat, 
 Uint4 options)
{
    return Validate(feat, NULL, options);
}

CConstRef<CValidError> CValidator::Validate
(const CBioSource& src, 
 CScope *scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&src));
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.Validate(src, scope);
    return errors;
}

CConstRef<CValidError> CValidator::Validate
(const CBioSource& src, 
 Uint4 options)
{
    return Validate(src, NULL, options);
}

CConstRef<CValidError> CValidator::Validate
(const CPubdesc& pubdesc, 
 CScope *scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&pubdesc));
    CValidError_imp imp(*m_ObjMgr, &(*errors), m_Taxon.get(), options);
    imp.Validate(pubdesc, scope);
    return errors;
}

CConstRef<CValidError> CValidator::Validate
(const CPubdesc& pubdesc, 
 Uint4 options)
{
    return Validate(pubdesc, NULL, options);
}

void CValidator::SetProgressCallback(TProgressCallback callback, void* user_data)
{
    m_PrgCallback = callback;
    m_UserData = user_data;
}


bool CValidator::BadCharsInAuthorName(const string& str, bool allowcomma, bool allowperiod, bool last)
{
    if (NStr::IsBlank(str)) {
        return false;
    }


    size_t stp = string::npos;
    if (last) {
        if (NStr::StartsWith(str, "St.")) {
            stp = 2;
        }
        else if (NStr::StartsWith(str, "de M.")) {
            stp = 4;
        }
    }

    size_t pos = 0;
    const char *ptr = str.c_str();

    while (*ptr != 0) {
        if (isalpha(*ptr)
            || *ptr == '-'
            || *ptr == '\''
            || *ptr == ' '
            || (*ptr == ',' && allowcomma)
            || (*ptr == '.' && (allowperiod || pos == stp))) {
            // all these are ok
            ptr++;
            pos++;
        }
        else {
            return true;
        }
    }
    return false;
}


bool CValidator::BadCharsInAuthorLastName(const string& str)
{
    if (NStr::EqualNocase(str, "et al.")) {
        // this is ok
        return false;
    } else {
        return BadCharsInAuthorName(str, false, false, true);
    }
}

bool CValidator::BadCharsInAuthorFirstName(const string& str)
{
    return BadCharsInAuthorName(str, false, true, false);
}


bool CValidator::BadCharsInAuthorInitials(const string& str)
{
    return BadCharsInAuthorName(str, false, true, false);
}

string CValidator::BadCharsInAuthor(const CName_std& author, bool& last_is_bad)
{
    string badauthor = kEmptyStr;
    last_is_bad = false;

    if (author.IsSetLast() && BadCharsInAuthorLastName(author.GetLast())) {
        last_is_bad = true;
        badauthor = author.GetLast();
    } else if (author.IsSetFirst() && BadCharsInAuthorFirstName(author.GetFirst())) {
        badauthor = author.GetFirst();
    }
    else if (author.IsSetInitials() && BadCharsInAuthorInitials(author.GetInitials())) {
        badauthor = author.GetInitials();
    }
    return badauthor;
}


string CValidator::BadCharsInAuthor(const CAuthor& author, bool& last_is_bad)
{
    last_is_bad = false;
    if (author.IsSetName() && author.GetName().IsName()) {
        return BadCharsInAuthor(author.GetName().GetName(), last_is_bad);
    } else {
        return kEmptyStr;
    }
}


typedef bool(*CompareConsecutiveIntervalProc) (const CSeq_interval& int1, const CSeq_interval& int2, CScope *scope);

bool x_CompareConsecutiveIntervals
(const CPacked_seqint& packed_int,
CConstRef<CSeq_interval>& int_cur,
CConstRef<CSeq_interval>& int_prv,
CScope* scope,
CompareConsecutiveIntervalProc compar)
{
    bool ok = true;
    ITERATE(CPacked_seqint::Tdata, it, packed_int.Get()) {
        int_cur = (*it);
        if (int_prv && !compar(*int_cur, *int_prv, scope)) {
            ok = false;
            break;
        }

        int_prv = int_cur;
    }
    return ok;
}


bool CheckConsecutiveIntervals(const CSeq_loc& loc, CScope& scope, CompareConsecutiveIntervalProc compar)
{
    bool ok = true;
    const CSeq_interval *int_cur = 0, *int_prv = 0;

    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    for (; lit && ok; ++lit) {
        CSeq_loc::E_Choice loc_choice = lit->Which();
        switch (loc_choice) {
        case CSeq_loc::e_Int:
            {{
            int_cur = &lit->GetInt();
            if (int_prv) {
                ok = compar(*int_cur, *int_prv, &scope);
            }
            int_prv = int_cur;
            }}
            break;
        case CSeq_loc::e_Pnt:
            int_prv = 0;
            break;
        case CSeq_loc::e_Packed_pnt:
            int_prv = 0;
            break;
        case CSeq_loc::e_Packed_int:
        {{
            CConstRef<CSeq_interval> this_int_cur(int_cur);
            CConstRef<CSeq_interval> this_int_prv(int_prv);
            ok = x_CompareConsecutiveIntervals
                (lit->GetPacked_int(), this_int_cur, this_int_prv, &scope, compar);
            }}
            break;
        case CSeq_loc::e_Null:
            break;
        default:
            int_prv = 0;
            break;
        }

    }
    return ok;
}



bool x_IsCorrectlyOrdered
(const CSeq_interval& int_cur,
 const CSeq_interval& int_prv,
 CScope* scope)
{
    ENa_strand strand_cur = int_cur.IsSetStrand() ?
        int_cur.GetStrand() : eNa_strand_unknown;

    if (IsSameBioseq(int_prv.GetId(), int_cur.GetId(), scope)) {
        if (strand_cur == eNa_strand_minus) {
            if (int_prv.GetTo() < int_cur.GetTo()) {
                return false;
            }
        }
        else {
            if (int_prv.GetTo() > int_cur.GetTo()) {
                return false;
            }
        }
    }
    return true;
}


bool CValidator::IsSeqLocCorrectlyOrdered(const CSeq_loc& loc, CScope& scope)
{
    bool circular = false;
    CBioseq_Handle seq;
    try {
        CBioseq_Handle seq = scope.GetBioseqHandle(loc);
    } catch (const exception& e) {
        // no way to tell
        return true;
    }
    if (seq  &&  seq.GetInst_Topology() == CSeq_inst::eTopology_circular) {
        // no way to check if topology is circular
        return true;
    }

    return CheckConsecutiveIntervals(loc, scope, x_IsCorrectlyOrdered);
}


bool x_IsNotAdjacent
(const CSeq_interval& int_cur,
const CSeq_interval& int_prv,
CScope* scope)
{
    ENa_strand strand_cur = int_cur.IsSetStrand() ?
        int_cur.GetStrand() : eNa_strand_unknown;

    bool ok = true;
    if (IsSameBioseq(int_prv.GetId(), int_cur.GetId(), scope)) {
        if (strand_cur == eNa_strand_minus) {
            if (int_cur.GetTo() + 1 == int_prv.GetFrom()) {
                ok = false;
            }
        }
        else {
            if (int_prv.GetTo() + 1 == int_cur.GetFrom()) {
                ok = false;
            }
        }
    }
    return ok;
}


bool CValidator::DoesSeqLocContainAdjacentIntervals
(const CSeq_loc& loc, CScope &scope)
{
    return !CheckConsecutiveIntervals(loc, scope, x_IsNotAdjacent);
}


bool x_SameStrand(const CSeq_interval& int1, const CSeq_interval& int2)
{
    ENa_strand strand1 = int1.IsSetStrand() ?
        int1.GetStrand() : eNa_strand_unknown;
    ENa_strand strand2 = int2.IsSetStrand() ?
        int2.GetStrand() : eNa_strand_unknown;
    return (strand1 == strand2);
}


bool IsNotDuplicateInterval(const CSeq_interval& int1, const CSeq_interval& int2, CScope* scope)
{
    if (IsSameBioseq(int1.GetId(), int2.GetId(), scope) &&
        x_SameStrand(int1, int2) &&
        int1.GetFrom() == int2.GetFrom() &&
        int1.GetTo() == int2.GetTo()) {            
        return false;
    }
    return true;
}

bool CValidator::DoesSeqLocContainDuplicateIntervals(const CSeq_loc& loc, CScope& scope)
{
    return !CheckConsecutiveIntervals(loc, scope, IsNotDuplicateInterval);
}


CCache::CCache(void)
{
    m_impl.reset(new CCacheImpl);
}

CRef<CCache>
CValidator::MakeEmptyCache(void)
{
    return Ref(new CCache);
}

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
