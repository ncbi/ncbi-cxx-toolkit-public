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
* Authors:  Justin Foley
*
* File Description:
*
* ===========================================================================
*/

#ifndef _FEATURE_MOD_APPLY_HPP_
#define _FEATURE_MOD_APPLY_HPP_

#include <corelib/ncbistd.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq;
class CSeq_loc;
class CSeq_feat;
class CSeqFeatData;


class CFeatModApply
{
public:

    using TSkippedMods = CModAdder::TSkippedMods;
    using FReportError = CModAdder::FReportError;

    CFeatModApply(CBioseq& bioseq, FReportError fReportError, TSkippedMods& skipped_mods);
    virtual ~CFeatModApply(void);

    using TModEntry = CModHandler::TMods::value_type;
    bool Apply(const TModEntry& mod_entry);

private:
    CSeq_feat& x_SetProtein(void);

    bool x_TryProtRefMod(const TModEntry& mod_entry);

    static const string& x_GetModName(const TModEntry& mod_entry);
    static const string& x_GetModValue(const TModEntry& mod_entry);

    using FVerifyFeat = function<bool(const CSeq_feat&)>;
    using FCreateFeatData = function<CRef<CSeqFeatData>()>;

    CRef<CSeq_feat> x_FindSeqfeat(FVerifyFeat fVerifyFeat);

    CRef<CSeq_feat> x_CreateSeqfeat(
            FCreateFeatData fCreateFeatData,
            const CSeq_loc& feat_loc);

    CRef<CSeq_loc> x_GetWholeSeqLoc(void);
    CRef<CSeq_feat> m_pProtein;

    CBioseq& m_Bioseq;
    FReportError m_fReportError;
    TSkippedMods& m_SkippedMods;
};

END_SCOPE(objects)
END_NCBI_SCOPE


#endif // _FEATURE_MOD_APPLY_HPP_
