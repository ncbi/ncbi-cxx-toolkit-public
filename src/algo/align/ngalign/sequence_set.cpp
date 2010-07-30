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
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
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

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);


CBlastDbSet::CBlastDbSet(const string& BlastDb) : m_BlastDb(BlastDb), m_Filter(-1)
{
    ;
}

void CBlastDbSet::SetNegativeGiList(const vector<int>& GiList)
{
    m_NegativeGiList.Reset(new CInputGiList);
    ITERATE(vector<int>, GiIter, GiList) {
        m_NegativeGiList->AppendGi(*GiIter);
    }
}


void CBlastDbSet::SetPositiveGiList(const vector<int>& GiList)
{
    m_PositiveGiList.Reset(new CInputGiList);
    ITERATE(vector<int>, GiIter, GiList) {
        m_PositiveGiList->AppendGi(*GiIter);
    }
}


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

    if(m_Filter != -1) {
        SearchDb->SetFilteringAlgorithm(m_Filter, DB_MASK_SOFT);
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

        int Gi;
        Gi = sequence::GetGiForId(**IdIter, Scope);
        if(Gi != 0) {
            GiList.push_back(Gi);
        }
    }
}



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
    } catch(...) {
        return CRef<CSeq_loc>();
    }

    CSymDustMasker DustMasker;

    try {
        Masks.reset((*SeqMasker)(Vector));
        DustMasks = DustMasker(Vector);
    } catch(CException& e) {
        ERR_POST(Info << "CSeqIdListSet::CreateQueryFactory Masking Failure: " << e.ReportAll());
        return CRef<CSeq_loc>();
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

        if(m_SeqMasker == NULL || !BlastOpts.GetMaskAtHash()) {
            CRef<CSeq_loc> WholeLoc(new CSeq_loc);
            WholeLoc->SetWhole().Assign(**IdIter);
            SSeqLoc WholeSLoc(*WholeLoc, Scope);
            FastaLocVec.push_back(WholeSLoc);
        } else {
            CRef<CSeq_loc> WholeLoc(new CSeq_loc), MaskLoc;
            WholeLoc->SetWhole().Assign(**IdIter);

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

        if(m_SeqMasker == NULL) {
            CRef<CSeq_loc> WholeLoc(new CSeq_loc);
            WholeLoc->SetWhole().Assign(**IdIter);
            SSeqLoc WholeSLoc(*WholeLoc, Scope);
            FastaLocVec.push_back(WholeSLoc);
        } else {

            CRef<CSeq_loc> WholeLoc(new CSeq_loc), MaskLoc;
            WholeLoc->SetWhole().Assign(**IdIter);

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



CFastaFileSet::CFastaFileSet(CNcbiIstream* FastaStream)
            : m_FastaStream(FastaStream), m_LowerCaseMasking(true)
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

