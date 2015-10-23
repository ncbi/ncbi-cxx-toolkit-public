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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <math.h>
#include <algo/align/ngalign/sequence_set.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/annot_ci.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <algo/align/util/score_builder.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>

#include <objtools/readers/fasta.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

#include <algo/dustmask/symdust.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);


CBlastDbSet::CBlastDbSet(const string& BlastDb) : m_BlastDb(BlastDb)
{
    ;
}

void CBlastDbSet::SetNegativeGiList(const vector<TGi>& GiList)
{
    m_NegativeGiList.Reset(new CInputGiList);
    ITERATE(vector<TGi>, GiIter, GiList) {
        m_NegativeGiList->AppendGi(*GiIter);
    }
}


void CBlastDbSet::SetPositiveGiList(const vector<TGi>& GiList)
{
    m_PositiveGiList.Reset(new CInputGiList);
    ITERATE(vector<TGi>, GiIter, GiList) {
        m_PositiveGiList->AppendGi(*GiIter);
    }
}


#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
void CBlastDbSet::SetNegativeGiList(const vector<int>& GiList)
{
    m_NegativeGiList.Reset(new CInputGiList);
    ITERATE(vector<int>, GiIter, GiList) {
        m_NegativeGiList->AppendGi(GI_FROM(int, *GiIter));
    }
}


void CBlastDbSet::SetPositiveGiList(const vector<int>& GiList)
{
    m_PositiveGiList.Reset(new CInputGiList);
    ITERATE(vector<int>, GiIter, GiList) {
        m_PositiveGiList->AppendGi(GI_FROM(int, *GiIter));
    }
}
#endif

CRef<IQueryFactory>
CBlastDbSet::CreateQueryFactory(CScope& Scope,
                                const CBlastOptionsHandle& BlastOpts)
{
    NCBI_THROW(CException, eInvalid,
               "CreateQueryFactory is not supported for type BlastDb");
    return CRef<IQueryFactory>();
}


CRef<blast::IQueryFactory>
CBlastDbSet::CreateQueryFactory(CScope& Scope,
                                const CBlastOptionsHandle& BlastOpts,
                                const CAlignResultsSet& Alignments, int Threshold)
{
    NCBI_THROW(CException, eInvalid,
               "CreateQueryFactory is not supported for type BlastDb");
    return CRef<IQueryFactory>();
}


CRef<CLocalDbAdapter>
CBlastDbSet::CreateLocalDbAdapter(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts)
{
    if(m_BlastDb.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CBLastDb::CreateLocalDbAdapter: BlastDb is empty.");
    }
    CRef<CSearchDatabase> SearchDb;
    SearchDb.Reset(new CSearchDatabase(m_BlastDb, CSearchDatabase::eBlastDbIsNucleotide));

    if(! m_Filter.empty()) {
        SearchDb->SetFilteringAlgorithm(m_Filter, eSoftSubjMasking);
    }

    if(!m_NegativeGiList.IsNull() && !m_NegativeGiList->NotEmpty()) {
        SearchDb->SetNegativeGiList(m_NegativeGiList);
    }

    if(!m_PositiveGiList.IsNull() && !m_PositiveGiList->NotEmpty()) {
        SearchDb->SetGiList(m_PositiveGiList);
    }

    CRef<CLocalDbAdapter> Result;
    Result.Reset(new CLocalDbAdapter(*SearchDb));
    return Result;
}



CSeqIdListSet::CSeqIdListSet() : m_SeqMasker(NULL)
{
    ;
}


list<CRef<CSeq_id> >& CSeqIdListSet::SetIdList()
{
    return m_SeqIdList;
}


void CSeqIdListSet::SetSeqMasker(CSeqMasker* SeqMasker)
{
    m_SeqMasker = SeqMasker;
}


void CSeqIdListSet::GetGiList(vector<TGi>& GiList, CScope& Scope,
                            const CAlignResultsSet& Alignments, int Threshold)
{
    ITERATE(list<CRef<CSeq_id> >, IdIter, m_SeqIdList) {

        if(Alignments.QueryExists(**IdIter)) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(**IdIter);
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                continue;
            }
        }

        TGi Gi;
        Gi = sequence::GetGiForId(**IdIter, Scope);
        if(Gi != ZERO_GI && Gi != INVALID_GI) {
			GiList.push_back(Gi);
        }
    }
}


#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
void CSeqIdListSet::GetGiList(vector<int>& GiList, CScope& Scope,
                            const CAlignResultsSet& Alignments, int Threshold)
{
    ITERATE(list<CRef<CSeq_id> >, IdIter, m_SeqIdList) {

        if(Alignments.QueryExists(**IdIter)) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(**IdIter);
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                continue;
            }
        }

        TGi Gi;
        Gi = sequence::GetGiForId(**IdIter, Scope);
        if(Gi != ZERO_GI && Gi != INVALID_GI) {
            GiList.push_back(GI_TO(int, Gi));
        }
    }
}
#endif


CRef<CSeq_loc> s_GetMaskLoc(const CSeq_id& Id,
                            CSeqMasker* SeqMasker,
                            CScope& Scope)
{
    auto_ptr<CSeqMasker::TMaskList> Masks, DustMasks;
    CBioseq_Handle Handle;
    CSeqVector Vector;

    try {
        Handle = Scope.GetBioseqHandle(Id);
        Vector = Handle.GetSeqVector(Handle.eCoding_Iupac, Handle.eStrand_Plus);
    } catch(CException& e) {
        ERR_POST(Error << "CSeqIdListSet::CreateQueryFactory GetSeqVector error: " << e.ReportAll());
        throw e;
    }

    CSymDustMasker DustMasker;

    try {
        Masks.reset((*SeqMasker)(Vector));
        DustMasks = DustMasker(Vector);
    } catch(CException& e) {
        ERR_POST(Error << "CSeqIdListSet::CreateQueryFactory Dust Masking Failure: " << e.ReportAll());
        throw e;
    }

    if(!DustMasks->empty()) {
        copy(DustMasks->begin(), DustMasks->end(),
            insert_iterator<CSeqMasker::TMaskList>(*Masks, Masks->end()));
    }

    if(Masks->empty()) {
        return CRef<CSeq_loc>();
    }

    CRef<CSeq_loc> MaskLoc(new CSeq_loc);
    ITERATE(CSeqMasker::TMaskList, IntIter, *Masks) {
        CSeq_loc IntLoc;
        IntLoc.SetInt().SetId().Assign(Id);
        IntLoc.SetInt().SetFrom() = IntIter->first;
        IntLoc.SetInt().SetTo() = IntIter->second;
        MaskLoc->Add(IntLoc);
    }

    MaskLoc = sequence::Seq_loc_Merge(*MaskLoc, CSeq_loc::fSortAndMerge_All,
                                      &Scope);
    MaskLoc->ChangeToPackedInt();

    return MaskLoc;
}


CRef<CSeq_loc> s_GetClipLoc(const CSeq_id& Id,
                            CScope& Scope)
{
    CBioseq_Handle Handle = Scope.GetBioseqHandle(Id);
	if(!Handle) {
		cerr << "s_GetClipLoc:  Could not get Handle for " << MSerial_AsnText << Id << endl;
		cerr << Id.GetSeqIdString(true) << endl;
		return CRef<CSeq_loc>();
	}

    // Extract the Seq-annot.locs, for the clip region.
    CRef<CSeq_loc> ClipLoc;

    SAnnotSelector Sel(CSeqFeatData::e_Region);
    CAnnot_CI AnnotIter(Handle, Sel);

    while(AnnotIter) {
        if (AnnotIter->IsFtable() && 
            AnnotIter->IsNamed()  &&
            AnnotIter->GetName() == "NCBI_GPIPE") {
            CConstRef<CSeq_annot> Annot = AnnotIter->GetCompleteSeq_annot();
            
            ITERATE(CSeq_annot::C_Data::TFtable, FeatIter,
                    Annot->GetData().GetFtable()) {
                CConstRef<CSeq_feat> Feat = *FeatIter;
                if (Feat->CanGetLocation() &&
                    Feat->CanGetData() && 
                    Feat->GetData().IsRegion() &&
                    (Feat->GetData().GetRegion() == "high_quality" ||
                     Feat->GetData().GetRegion() == "hight_quality") ) {
                    
                    ClipLoc.Reset(new CSeq_loc);
                    ClipLoc->Assign(Feat->GetLocation());
                }
            }
        }
        ++AnnotIter;
    }


    if(ClipLoc.IsNull() && Handle.HasAnnots() ) {
        CConstRef<CBioseq> Bioseq = Handle.GetCompleteBioseq();
        ITERATE(CBioseq::TAnnot, AnnotIter, Bioseq->GetAnnot()) {
            if( (*AnnotIter)->GetData().IsLocs() ) {
                ITERATE(CSeq_annot::C_Data::TLocs, 
                        LocIter, (*AnnotIter)->GetData().GetLocs()) {
                    if( (*LocIter)->IsInt() && 
                        (*LocIter)->GetInt().GetId().Equals(Id) ) {
                        ClipLoc.Reset(new CSeq_loc);
                        ClipLoc->Assign(**LocIter);
                    }
                }
            }
        }
    }
    

    return ClipLoc;
}


CRef<CSeq_loc> s_GetUngapLoc(const CSeq_id& Id,
                             CScope& Scope)
{
    CBioseq_Handle Handle = Scope.GetBioseqHandle(Id);
	if(!Handle) {
		cerr << "s_GetUngapLoc:  Could not get Handle for " << MSerial_AsnText << Id << endl;
		cerr << Id.GetSeqIdString(true) << endl;
		return CRef<CSeq_loc>();
	}

    // Extract the Seq-annot.locs, for the not-gap region.
    CRef<CSeq_loc> UngapLoc;
    
    if(!Handle.IsSetInst_Ext()) {
        CRef<CSeq_loc> WholeLoc(new CSeq_loc);
        WholeLoc->SetWhole().Assign(Id);
        return WholeLoc;
    }
    
    const CSeq_ext& Ext = Handle.GetInst_Ext();

    if(!Ext.IsDelta()) {
        CRef<CSeq_loc> WholeLoc(new CSeq_loc);
        WholeLoc->SetWhole().Assign(Id);
        return WholeLoc;
    }

    UngapLoc.Reset(new CSeq_loc);
    
    TSeqPos Curr = 0;
    ITERATE(CDelta_ext::Tdata, SeqIter, Ext.GetDelta().Get()) {
        const CDelta_seq& Seq = **SeqIter;
        
        if(Seq.IsLiteral() && Seq.GetLiteral().IsSetLength()) {
            Curr += Seq.GetLiteral().GetLength();
            continue;
        }
        else if(Seq.IsLoc() && Seq.GetLoc().IsInt()) {
            TSeqPos Length = Seq.GetLoc().GetInt().GetLength();

            CSeq_loc IntLoc;
            IntLoc.SetInt().SetId().Assign(Id);
            IntLoc.SetInt().SetFrom() = Curr;
            IntLoc.SetInt().SetTo() = Curr + Length - 1;
            UngapLoc->Add(IntLoc);
            
            Curr += Length;
        }
    }
   
    UngapLoc = sequence::Seq_loc_Merge(*UngapLoc, CSeq_loc::fSortAndMerge_All,
                                      &Scope);
    UngapLoc->ChangeToPackedInt();

    return UngapLoc;
}


CRef<IQueryFactory>
CSeqIdListSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts)
{
    if(m_SeqIdList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSeqIdListSet::CreateQueryFactory: Id List is empty.");
    }

    TSeqLocVector FastaLocVec;
    ITERATE(list<CRef<CSeq_id> >, IdIter, m_SeqIdList) {

        CRef<CSeq_loc> WholeLoc;
        WholeLoc = s_GetClipLoc(**IdIter, Scope);
        if(WholeLoc.IsNull()) {
            //WholeLoc = s_GetUngapLoc(**IdIter, Scope);
            WholeLoc.Reset(new CSeq_loc);
            WholeLoc->SetWhole().Assign(**IdIter);
        }
        string FilterStr = BlastOpts.GetFilterString();
        if(m_SeqMasker == NULL || FilterStr.find('m') == string::npos  ) {
            SSeqLoc WholeSLoc(*WholeLoc, Scope);
            FastaLocVec.push_back(WholeSLoc);
        } else {
            CRef<CSeq_loc> MaskLoc;
            MaskLoc = s_GetMaskLoc(**IdIter, m_SeqMasker, Scope);

            if(MaskLoc.IsNull() /* || Vec.size() < 100*/ ) {
                SSeqLoc WholeSLoc(*WholeLoc, Scope);
                FastaLocVec.push_back(WholeSLoc);
            } else {
                SSeqLoc MaskSLoc(*WholeLoc, Scope, *MaskLoc);
                FastaLocVec.push_back(MaskSLoc);
            }
        }
    }

    CRef<IQueryFactory> Result;
    if(!FastaLocVec.empty())
        Result.Reset(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<blast::IQueryFactory>
CSeqIdListSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts,
                                  const CAlignResultsSet& Alignments, int Threshold)
{
    if(m_SeqIdList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSeqIdListSet::CreateQueryFactory: Id List is empty.");
    }
    
	TSeqLocVector FastaLocVec;
    ITERATE(list<CRef<CSeq_id> >, IdIter, m_SeqIdList) {

        if(Alignments.QueryExists(**IdIter)) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(**IdIter);
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                continue;
            }
        }
    
        ERR_POST(Info << "Blast Including ID: " << (*IdIter)->GetSeqIdString(true));

		
        CRef<CSeq_loc> WholeLoc;
        WholeLoc = s_GetClipLoc(**IdIter, Scope);
        if(WholeLoc.IsNull()) {
            //WholeLoc = s_GetUngapLoc(**IdIter, Scope);
            WholeLoc.Reset(new CSeq_loc);
            WholeLoc->SetWhole().Assign(**IdIter);
        }
       
        string FilterStr = BlastOpts.GetFilterString();
        if(m_SeqMasker == NULL || FilterStr.find('m') == string::npos  ) {
            SSeqLoc WholeSLoc(*WholeLoc, Scope);
            FastaLocVec.push_back(WholeSLoc);
        } else {
            CRef<CSeq_loc> MaskLoc;
            MaskLoc = s_GetMaskLoc(**IdIter, m_SeqMasker, Scope);

            if(MaskLoc.IsNull()) {
                SSeqLoc WholeSLoc(*WholeLoc, Scope);
                FastaLocVec.push_back(WholeSLoc);
            } else {
                SSeqLoc MaskSLoc(*WholeLoc, Scope, *MaskLoc);
                FastaLocVec.push_back(MaskSLoc);
            }
        }
    }

    CRef<IQueryFactory> Result;
    if(!FastaLocVec.empty())
        Result.Reset(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}



CRef<CLocalDbAdapter>
CSeqIdListSet::CreateLocalDbAdapter(CScope& Scope,
                                    const CBlastOptionsHandle& BlastOpts)
{
    if(m_SeqIdList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSeqIdListSet::CreateLocalDbAdapter: Id List is empty.");
    }

    CRef<CLocalDbAdapter> Result;
    CRef<IQueryFactory> QueryFactory = CreateQueryFactory(Scope, BlastOpts);
    Result.Reset(new CLocalDbAdapter(QueryFactory,
                                     CConstRef<CBlastOptionsHandle>(&BlastOpts)));
    return Result;
}



CSeqLocListSet::CSeqLocListSet() : m_SeqMasker(NULL)
{
    ;
}


list<CRef<CSeq_loc> >& CSeqLocListSet::SetLocList()
{
    return m_SeqLocList;
}


void CSeqLocListSet::SetSeqMasker(CSeqMasker* SeqMasker)
{
    m_SeqMasker = SeqMasker;
}


void CSeqLocListSet::GetGiList(vector<TGi>& GiList, CScope& Scope,
                            const CAlignResultsSet& Alignments, int Threshold)
{
    ITERATE(list<CRef<CSeq_loc> >, LocIter, m_SeqLocList) {

        const CSeq_id* Id = (*LocIter)->GetId();

        if(Id == NULL)
            continue;

        if(Alignments.QueryExists(*Id)) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(*Id);
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                continue;
            }
        }

        TGi Gi = INVALID_GI;
        try {
            Gi = sequence::GetGiForId(*Id, Scope);
        } catch(...) { Gi = INVALID_GI; }
        if(Gi != ZERO_GI && Gi != INVALID_GI) {
			GiList.push_back(Gi);
        }
    }
}


#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
void CSeqLocListSet::GetGiList(vector<int>& GiList, CScope& Scope,
                            const CAlignResultsSet& Alignments, int Threshold)
{
    ITERATE(list<CRef<CSeq_loc> >, LocIter, m_SeqLocList) {

        const CSeq_id* Id = (*LocIter)->GetId();

        if(Id == NULL)
            continue;

        if(Alignments.QueryExists(*Id)) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(*Id);
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                continue;
            }
        }

        TGi Gi = INVALID_GI;
        try {
            Gi = sequence::GetGiForId(*Id, Scope);
        } catch(...) { Gi = INVALID_GI; }
        if(Gi != ZERO_GI && Gi != INVALID_GI) {
            GiList.push_back(GI_TO(int, Gi));
        }
    }
}
#endif

CRef<IQueryFactory>
CSeqLocListSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts)
{
    if(m_SeqLocList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSeqLocListSet::CreateQueryFactory: Loc List is empty.");
    }

    TSeqLocVector FastaLocVec;
    ITERATE(list<CRef<CSeq_loc> >, LocIter, m_SeqLocList) {
        const CSeq_id* Id = (*LocIter)->GetId();
        if(Id == NULL)
            continue;

        ERR_POST(Info << "Blast Including Loc: " << Id->AsFastaString() << "  " << (*LocIter)->GetTotalRange() );
        
        CRef<CSeq_loc> BaseLoc(new CSeq_loc);
        BaseLoc->Assign(**LocIter);
        CRef<CSeq_loc> ClipLoc = s_GetClipLoc(*Id, Scope);
        if(!ClipLoc.IsNull()) {
            CRef<CSeq_loc> Inters;
            Inters = BaseLoc->Intersect(*ClipLoc, CSeq_loc::fSortAndMerge_All, NULL);
            if(!Inters.IsNull()) {
                BaseLoc->Assign(*Inters);
            }
        }
        
        string FilterStr = BlastOpts.GetFilterString();
        if(m_SeqMasker == NULL || FilterStr.find('m') == string::npos  ) {
            SSeqLoc BaseSLoc(*BaseLoc, Scope);
            FastaLocVec.push_back(BaseSLoc);
        } else {
            CRef<CSeq_loc> MaskLoc;
            MaskLoc = s_GetMaskLoc(*Id, m_SeqMasker, Scope);

            if(MaskLoc.IsNull() /* || Vec.size() < 100*/ ) {
                SSeqLoc BaseSLoc(*BaseLoc, Scope);
                FastaLocVec.push_back(BaseSLoc);
            } else {
                SSeqLoc MaskSLoc(*BaseLoc, Scope, *MaskLoc);
                FastaLocVec.push_back(MaskSLoc);
            }
        }
    }

    CRef<IQueryFactory> Result;
    if(!FastaLocVec.empty())
        Result.Reset(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<blast::IQueryFactory>
CSeqLocListSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts,
                                  const CAlignResultsSet& Alignments, int Threshold)
{
    if(m_SeqLocList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSeqLocListSet::CreateQueryFactory: Loc List is empty.");
    }
    
	TSeqLocVector FastaLocVec;
    ITERATE(list<CRef<CSeq_loc> >, LocIter, m_SeqLocList) {
        const CSeq_id* Id = (*LocIter)->GetId();
        if(Id == NULL)
            continue;

        if(Alignments.QueryExists(*Id)) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(*Id);
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                continue;
            }
        }
    
        ERR_POST(Info << "Blast Including Loc: " << Id->AsFastaString() << "  " << (*LocIter)->GetTotalRange() );

		
        CRef<CSeq_loc> BaseLoc(new CSeq_loc), ClipLoc;
        BaseLoc->Assign(**LocIter);
        ClipLoc = s_GetClipLoc(*Id, Scope);
        if(!ClipLoc.IsNull()) {
            CRef<CSeq_loc> Inters;
            Inters = BaseLoc->Intersect(*ClipLoc, CSeq_loc::fSortAndMerge_All, NULL);
            if(!Inters.IsNull()) {
                BaseLoc->Assign(*Inters);
            }
        }
  
        string FilterStr = BlastOpts.GetFilterString();
        if(m_SeqMasker == NULL || FilterStr.find('m') == string::npos  ) {
            SSeqLoc BaseSLoc(*BaseLoc, Scope);
            FastaLocVec.push_back(BaseSLoc);
        } else {
            CRef<CSeq_loc> MaskLoc;
            MaskLoc = s_GetMaskLoc(*Id, m_SeqMasker, Scope);

            if(MaskLoc.IsNull()) {
                SSeqLoc BaseSLoc(*BaseLoc, Scope);
                FastaLocVec.push_back(BaseSLoc);
            } else {
                SSeqLoc MaskSLoc(*BaseLoc, Scope, *MaskLoc);
                FastaLocVec.push_back(MaskSLoc);
            }
        }
        
    }

    CRef<IQueryFactory> Result;
    if(!FastaLocVec.empty())
        Result.Reset(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<CLocalDbAdapter>
CSeqLocListSet::CreateLocalDbAdapter(CScope& Scope,
                                    const CBlastOptionsHandle& BlastOpts)
{
    if(m_SeqLocList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSeqLocListSet::CreateLocalDbAdapter: Loc List is empty.");
    }

    CRef<CLocalDbAdapter> Result;
    CRef<IQueryFactory> QueryFactory = CreateQueryFactory(Scope, BlastOpts);
    Result.Reset(new CLocalDbAdapter(QueryFactory,
                                     CConstRef<CBlastOptionsHandle>(&BlastOpts)));
    return Result;
}



CFastaFileSet::CFastaFileSet(CNcbiIstream* FastaStream)
            : m_FastaStream(FastaStream), m_LowerCaseMasking(true),
              m_Start(-1), m_Count(-1)
{
    ;
}


CFastaFileSet::CFastaFileSet(CNcbiIstream* FastaStream, int Start, int Count)
            : m_FastaStream(FastaStream), m_LowerCaseMasking(true),
              m_Start(Start), m_Count(Count)
{
    ;
}


void CFastaFileSet::EnableLowerCaseMasking(bool LowerCaseMasking)
{
    m_LowerCaseMasking = LowerCaseMasking;
}


CRef<IQueryFactory>
CFastaFileSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts)
{
    if(m_FastaStream == NULL) {
        NCBI_THROW(CException, eInvalid,
                   "CFastaFileSet::CreateQueryFactory: Fasta Stream is NULL.");
    }

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);
    CFastaReader FastaReader(*m_FastaStream);
    Scope.AddTopLevelSeqEntry(*(FastaReader.ReadSet()));

    SDataLoaderConfig LoaderConfig(false);
    CBlastInputSourceConfig InputConfig(LoaderConfig);
    InputConfig.SetLowercaseMask(m_LowerCaseMasking);
    InputConfig.SetBelieveDeflines(true);

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);
    CBlastFastaInputSource FastaSource(*m_FastaStream, InputConfig);
    const EProgram kProgram = eBlastn;
    CBlastInput Input(&FastaSource, GetQueryBatchSize(kProgram));

    TSeqLocVector FastaLocVec = Input.GetAllSeqLocs(Scope);
    //ITERATE(TSeqLocVector, LocIter, FastaLocVec) {
    //    cerr << *LocIter->seqloc->GetId() << endl;
    //}

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);

    CRef<IQueryFactory> Result(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<blast::IQueryFactory>
CFastaFileSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts,
                                  const CAlignResultsSet& Alignments, int Threshold)
{
    if(m_FastaStream == NULL) {
        NCBI_THROW(CException, eInvalid,
                   "CFastaFileSet::CreateQueryFactory: Fasta Stream is NULL.");
    }

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);
    CFastaReader FastaReader(*m_FastaStream);
    CRef<CSeq_entry> Entry = FastaReader.ReadSet();
    try {
        bool PreExisting = false;
        if(Entry->IsSet() && Entry->GetSet().GetSeq_set().front()->GetSeq().GetFirstId() != NULL) {
            const CSeq_id& Id = *Entry->GetSet().GetSeq_set().front()->GetSeq().GetFirstId();
            CBioseq_Handle Handle = Scope.GetBioseqHandle(Id);
            if(Handle)
                PreExisting = true;
        }
        if(!PreExisting)
            Scope.AddTopLevelSeqEntry(*Entry);
    } catch(...) {
        ERR_POST(Info << "Eating the Scope Fasta Dup Insert Exception");
    }
    SDataLoaderConfig LoaderConfig(false);
    CBlastInputSourceConfig InputConfig(LoaderConfig);
    InputConfig.SetLowercaseMask(m_LowerCaseMasking);
    InputConfig.SetBelieveDeflines(true);

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);
    CBlastFastaInputSource FastaSource(*m_FastaStream, InputConfig);
    const EProgram kProgram = eBlastn;
    CBlastInput Input(&FastaSource, GetQueryBatchSize(kProgram));

    TSeqLocVector FastaLocVec = Input.GetAllSeqLocs(Scope);
 
    if(m_Count > 0) {
        int i = 0;
        TSeqLocVector::iterator Curr;
        for(Curr = FastaLocVec.begin(); Curr != FastaLocVec.end(); ) {
            //cerr << " * " << Curr->seqloc->GetId()->AsFastaString() << " * " << endl;
            if( i < m_Start) {
                Curr = FastaLocVec.erase(Curr);
                i++;
                continue;
            }
            else if( i > m_Start + m_Count) {
                Curr = FastaLocVec.erase(Curr);
                i++;
                continue;
            }
            else {
                ++Curr;
                i++;
                continue;
            }
        }
        m_Start += m_Count;
    }


    TSeqLocVector::iterator Curr;
    for(Curr = FastaLocVec.begin(); Curr != FastaLocVec.end(); ) {
        if(Alignments.QueryExists(*Curr->seqloc->GetId())) {
            CConstRef<CQuerySet> QuerySet = Alignments.GetQuerySet(*Curr->seqloc->GetId());
            int BestRank = QuerySet->GetBestRank();
            if(BestRank != -1 && BestRank <= Threshold) {
                Curr = FastaLocVec.erase(Curr);
                continue;
            }
        }
        ++Curr;
    }
   

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);

    if(FastaLocVec.empty()) 
        return CRef<IQueryFactory>();

    CRef<IQueryFactory> Result(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<CLocalDbAdapter>
CFastaFileSet::CreateLocalDbAdapter(CScope& Scope,
                                    const CBlastOptionsHandle& BlastOpts)
{
    CRef<CLocalDbAdapter> Result;
    CRef<IQueryFactory> QueryFactory = CreateQueryFactory(Scope, BlastOpts);
    Result.Reset(new CLocalDbAdapter(QueryFactory, CConstRef<CBlastOptionsHandle>(&BlastOpts)));
    return Result;
}


END_SCOPE(ncbi)

