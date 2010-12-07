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
 * Authors:  Melvin Quintos
 *
 * File Description:
 *  Provides implementation of NSnp class. See snp_extra.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/snputil/snp_utils.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objmgr/annot_selector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
bool  NSnp::IsSnp(const CMappedFeat &mapped_feat)
{
    bool isSnp = false;
    const CSeq_feat &feat = mapped_feat.GetOriginalFeature();

    if (feat.IsSetData()) {
        isSnp = (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_variation);
    }

    return isSnp;
}

CTime NSnp::GetCreateTime(const CMappedFeat &feat)
{
    CTime time;
    CSeq_annot_Handle h = feat.GetAnnot();

    if (h.Seq_annot_CanGetDesc()) {
        const CAnnot_descr &desc = h.Seq_annot_GetDesc();
        if (desc.CanGet()) {
            ITERATE( CAnnot_descr::Tdata, it, desc.Get() ) {
                const CRef<CAnnotdesc> &d = *it;
                if (d->IsCreate_date()) {
                    time = d->GetCreate_date().AsCTime();
                    break;
                }
            }
        }
    }

    return time;
}

int NSnp::GetRsid(const CMappedFeat &mapped_feat)
{
    return GetRsid(mapped_feat.GetOriginalFeature());
}

int NSnp::GetRsid(const CSeq_feat &feat)
{
    int rsid = 0;

    CConstRef<CDbtag> ref = feat.GetNamedDbxref("dbSNP");
    if (ref) {
        rsid = ref->GetTag().GetId();
    }

    return rsid;
}

int NSnp::GetLength(const CMappedFeat &mapped_feat)
{
    return GetLength(mapped_feat.GetOriginalFeature());
}

int NSnp::GetLength(const CSeq_feat &feat)
{
    int length = 0;

    if (feat.IsSetExt()) {
        CConstRef<CUser_field> field =
            feat.GetExt().GetFieldRef("Extra");
        if (field) {
            string s1, s2;
            const string &str = field->GetData().GetStr();
            if (NStr::SplitInTwo(str, "=", s1, s2)) {
                vector<string> v;

                NStr::Tokenize(str, ",", v);
                if (v.size()==4) {
                    int rc = NStr::StringToInt(v[3], NStr::fConvErr_NoThrow);
                    int lc = NStr::StringToInt(v[2], NStr::fConvErr_NoThrow);
                    length = rc + lc + 1;
                }
            }
        }
    }

    return length;
}

CSnpBitfield NSnp::GetBitfield(const CMappedFeat &mapped_feat)
{
    return GetBitfield(mapped_feat.GetOriginalFeature());
}

CSnpBitfield NSnp::GetBitfield(const CSeq_feat &feat)
{
    CSnpBitfield b;

    CConstRef<CDbtag> ref = feat.GetNamedDbxref("dbSNP");

    if (ref && feat.IsSetExt() ) {
        CConstRef<CUser_field> field =
            feat.GetExt().GetFieldRef("QualityCodes");
        if (field) {
            b = field->GetData().GetOs();
        }
    }

    return b;
}

bool NSnp::IsSnpKnown( CScope &scope, const CMappedFeat &private_snp, const string &allele)
{
    const CSeq_loc &loc = private_snp.GetLocation();
    return IsSnpKnown(scope, loc, allele);
}

bool NSnp::IsSnpKnown( CScope& scope, const CSeq_loc& loc, const string & allele)
{
    bool isKnown        = false;
    SAnnotSelector      sel;       // annotation selector

    // Prepare Annotation Selection to find the SNPs
    //sel = CSeqUtils::GetAnnotSelector(CSeqFeatData::eSubtype_variation);
    sel .SetOverlapTotalRange()
        .SetResolveAll()
        .AddNamedAnnots("SNP")
        .SetExcludeExternal(false)
        .ExcludeUnnamedAnnots()
        .SetAnnotType(CSeq_annot::TData::e_Ftable)
        .SetFeatSubtype(CSeqFeatData::eSubtype_variation)
        .SetMaxSize(100000);  // In case someone does something silly.

    CFeat_CI feat_it(scope, loc, sel);

    if (allele == kEmptyStr) {
        // Don't check for alleles
        // Existing of any returned SNP means there are known SNPs
        if (feat_it.GetSize()>0) {
            isKnown = true;
        }
    }
    else {
        // Check all the alleles for all the returned SNPs
        for (; feat_it && !isKnown; ++feat_it) {
            const CSeq_feat &       or_feat = feat_it->GetOriginalFeature();
            if (or_feat.CanGetQual()) {
                ITERATE (CSeq_feat::TQual, it, or_feat.GetQual()) {
                    const CRef<CGb_qual> &qual = *it;
                    if (qual->GetQual() == "replace" &&
                        qual->GetVal().find(allele) != string::npos) {
                        isKnown = true;
                        break;
                    }
                }
            }
        }
    }

    return isKnown;
}

END_NCBI_SCOPE
