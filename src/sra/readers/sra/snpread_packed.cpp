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
 *   Access to SNP files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/impl/snpread_packed.hpp>
#include <sra/error_codes.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <unordered_map>

BEGIN_STD_NAMESPACE;

template<>
struct hash<ncbi::CTempString>
{
    size_t operator()(ncbi::CTempString val) const
        {
            unsigned long __h = 5381;
            for ( auto c : val ) {
                __h = __h*17 + c;
            }
            return size_t(__h);
        }
};

END_STD_NAMESPACE;

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   SNPReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(SNPDbPacked);


/*
static const TSeqPos kPageSize = 5000;
static const TSeqPos kMaxSNPLength = 256;
static const TSeqPos kOverviewZoom = kPageSize;
static const TSeqPos kCoverageZoom = 100;
*/
static const size_t kMax_AlleleLength  = 32;
static const char kDefaultAnnotName[] = "SNP";
/*

static const char kFeatSubtypesToChars[] = "U-VSMLDIR";

static const bool kPreloadSeqList = false;
static const bool kPage2FeatErrorWorkaround = true;
*/


BEGIN_LOCAL_NAMESPACE;

inline
void x_AdjustRange(CRange<TSeqPos>& range,
                   const CSNPDbSeqIterator& it)
{
    range = range.IntersectionWith(it.GetSNPRange());
}


inline
void x_InitSNP_Info(SSNP_Info& info)
{
    info.m_Flags = info.fQualityCodesOs | info.fAlleleReplace;
    info.m_CommentIndex = info.kNo_CommentIndex;
    info.m_Weight = 0;
    info.m_ExtraIndex = info.kNo_ExtraIndex;
}


inline
bool x_ParseSNP_Info(SSNP_Info& info,
                     const CSNPDbFeatIterator& it,
                     CSeq_annot_SNP_Info& packed)
{
    TSeqPos len = it.GetSNPLength();
    if ( len > info.kMax_PositionDelta+1 ) {
        return false;
    }
    info.m_PositionDelta = len-1;
    info.m_ToPosition = it.GetSNPPosition()+len-1;

    CSNPDbFeatIterator::TExtraRange range = it.GetExtraRange();
    if ( range.second > info.kMax_AllelesCount ) {
        return false;
    }
    size_t index = 0;
    for ( ; index < range.second; ++index ) {
        CTempString allele = it.GetAllele(range, index);
        if ( allele.size() > kMax_AlleleLength ) {
            return false;
        }
        SSNP_Info::TAlleleIndex a_index = packed.x_GetAlleleIndex(allele);
        if ( a_index == info.kNo_AlleleIndex ) {
            return false;
        }
        info.m_AllelesIndices[index] = a_index;
    }
    for ( ; index < info.kMax_AllelesCount; ++index ) {
        info.m_AllelesIndices[index] = info.kNo_AlleleIndex;
    }

    vector<char> os;
    it.GetBitfieldOS(os);
    info.m_QualityCodesIndex = packed.x_GetQualityCodesIndex(os);
    if ( info.m_QualityCodesIndex == info.kNo_QualityCodesIndex ) {
        return false;
    }

    auto feat_id = it.GetFeatId();
    if ( feat_id > kMax_Int ) {
        NCBI_THROW(CSraException, eDataError,
                   "CSNPDbSeqIterator: FEAT_ID doesn't fit into table SNPId");
    }
    info.m_SNP_Id = SSNP_Info::TSNPId(feat_id);

    packed.x_AddSNP(info);
    return true;
}        


CRef<CSeq_annot> x_NewAnnot(const string& annot_name = kDefaultAnnotName)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetNameDesc(annot_name);
    return annot;
}


END_LOCAL_NAMESPACE;


TPackedAnnot GetPackedFeatAnnot(const CSNPDbSeqIterator& seq,
                                CRange<TSeqPos> range,
                                const CSNPDbSeqIterator::SFilter& filter,
                                CSNPDbSeqIterator::TFlags flags)
{
    x_AdjustRange(range, seq);
    CRef<CSeq_annot> annot = x_NewAnnot();
    CRef<CSeq_annot_SNP_Info> packed(new CSeq_annot_SNP_Info);
    CSeq_annot::TData::TFtable& feats = annot->SetData().SetFtable();

    SSNP_Info info;
    x_InitSNP_Info(info);
    CSNPDbSeqIterator::SSelector sel(CSNPDbSeqIterator::eSearchByStart, filter);
    for ( CSNPDbFeatIterator it(seq, range, sel); it; ++it ) {
        if ( !x_ParseSNP_Info(info, it, *packed) ) {
            feats.push_back(it.GetSeq_feat());
        }
    }
    if ( packed->empty() ) {
        packed = null;
        if ( feats.empty() ) {
            annot = null;
        }
    }
    else {
        packed->SetSeq_id(*seq.GetSeqId());
    }
    return TPackedAnnot(annot, packed);
}


TPackedAnnot GetPackedFeatAnnot(const CSNPDbSeqIterator& seq,
                                CRange<TSeqPos> range,
                                CSNPDbSeqIterator::TFlags flags)
{
    return GetPackedFeatAnnot(seq, range, seq.GetFilter(), flags);
}

END_NAMESPACE(SNPDbPacked);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
