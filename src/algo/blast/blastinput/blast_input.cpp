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
 * Author:  Jason Papadopoulos
 *
 */

/** @file algo/blast/blastinput/blast_input.cpp
 * Convert sources of sequence data into blast sequence input
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <objtools/readers/reader_exception.hpp> // for CObjReaderParseException

#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CBlastInputSourceConfig::CBlastInputSourceConfig
    (const SDataLoaderConfig& dlconfig,
     objects::ENa_strand strand         /* = objects::eNa_strand_other */,
     bool lowercase                     /* = false */,
     bool believe_defline               /* = false */,
     TSeqRange range                    /* = TSeqRange() */,
     bool retrieve_seq_data             /* = true */,
     int local_id_counter               /* = 1 */,
     unsigned int seqlen_thresh2guess   /* = numeric_limits<unsigned int>::max()*/,
     bool skip_seq_check                /* = false -RMH- */ )
: m_Strand(strand), m_LowerCaseMask(lowercase), 
  m_BelieveDeflines(believe_defline), 
  m_SkipSeqCheck(skip_seq_check), /* -RMH- */
  m_Range(range), m_DLConfig(dlconfig),
  m_RetrieveSeqData(retrieve_seq_data),
  m_LocalIdCounter(local_id_counter),
  m_SeqLenThreshold2Guess(seqlen_thresh2guess),
  m_GapsToNs(false)
{
    // Set an appropriate default for the strand
    if (m_Strand == eNa_strand_other) {
        m_Strand = (m_DLConfig.m_IsLoadingProteins)
            ? eNa_strand_unknown
            : eNa_strand_both;
    }
    SetQueryLocalIdMode();
}

CBlastInput::CBlastInput(const CBlastInput& rhs)
{
    do_copy(rhs);
}

CBlastInput&
CBlastInput::operator=(const CBlastInput& rhs)
{
    do_copy(rhs);
    return *this;
}

void
CBlastInput::do_copy(const CBlastInput& rhs)
{
    if (this != &rhs) {
        m_Source = rhs.m_Source;
        m_BatchSize = rhs.m_BatchSize;
    }
}

TSeqLocVector
CBlastInput::GetNextSeqLocBatch(CScope& scope)
{
    TSeqLocVector retval;
    TSeqPos size_read = 0;

    while (size_read < GetBatchSize()) {

        if (End())
            break;

        try { retval.push_back(m_Source->GetNextSSeqLoc(scope)); }
        catch (const CObjReaderParseException& e) {
            if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                break;
            }
            throw;
        }
        SSeqLoc& loc = retval.back();

        if (loc.seqloc->IsInt()) {
            size_read += sequence::GetLength(loc.seqloc->GetInt().GetId(), 
                                            loc.scope);
        } else if (loc.seqloc->IsWhole()) {
            size_read += sequence::GetLength(loc.seqloc->GetWhole(),
                                            loc.scope);
        } else {
            // programmer error, CBlastInputSource should only return Seq-locs
            // of type interval or whole
            abort();
        }
    }
    _TRACE("Read " << retval.size() << " queries");
    return retval;
}


CRef<CBlastQueryVector>
CBlastInput::GetNextSeqBatch(CScope& scope)
{
    CRef<CBlastQueryVector> retval(new CBlastQueryVector);
    TSeqPos size_read = 0;

    while (size_read < GetBatchSize()) {

        if (End())
            break;

        CRef<CBlastSearchQuery> q;
        try { q.Reset(m_Source->GetNextSequence(scope)); }
        catch (const CObjReaderParseException& e) {
            if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                break;
            }
            throw;
        }
        CConstRef<CSeq_loc> loc = q->GetQuerySeqLoc();

        if (loc->IsInt()) {
            size_read += sequence::GetLength(loc->GetInt().GetId(),
                                             q->GetScope());
        } else if (loc->IsWhole()) {
            size_read += sequence::GetLength(loc->GetWhole(), q->GetScope());
        } else {
            // programmer error, CBlastInputSource should only return Seq-locs
            // of type interval or whole
            abort();
        }

        retval->AddQuery(q);
    }
    m_NumSeqs +=retval->Size();
    m_TotalLength += size_read;
    _TRACE("Read " << retval->Size() << " queries");
    return retval;
}


TSeqLocVector
CBlastInput::GetAllSeqLocs(CScope& scope)
{
    TSeqLocVector retval;

    while (!End()) {
        try { retval.push_back(m_Source->GetNextSSeqLoc(scope)); }
        catch (const CObjReaderParseException& e) {
            if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                break;
            }
            throw;
        }
    }

    return retval;
}


CRef<CBlastQueryVector>
CBlastInput::GetAllSeqs(CScope& scope)
{
    CRef<CBlastQueryVector> retval(new CBlastQueryVector);

    while (!End()) {
        try { retval->AddQuery(m_Source->GetNextSequence(scope)); }
        catch (const CObjReaderParseException& e) {
            auto err = e.GetErrCode();
            if (err == CObjReaderParseException::eEOF) {
                break;
            } else if (err == CObjReaderParseException::eNoDefline) {
                CNcbiStrstream ss;
                ss << "Query input doesn't start with "
                    "a defline or comment, line " << e.GetPos() << ends;
                NCBI_THROW(CInputException, eInvalidInput, ss.str());
            }
            throw;
        }
    }

    return retval;
}

CRef<CBioseq> CBlastBioseqMaker::
        CreateBioseqFromId(CConstRef<CSeq_id> id, bool retrieve_seq_data)
{
    _ASSERT(m_scope.NotEmpty());

    // N.B.: this call fetches the Bioseq into the scope from its
    // data sources (should be BLAST DB first, then Genbank)
    TSeqPos len = sequence::GetLength(*id, m_scope);
    if (len == numeric_limits<TSeqPos>::max()) {
        NCBI_THROW(CInputException, eSeqIdNotFound,
                    "Sequence ID not found: '" + 
                    id->AsFastaString() + "'");
    }

    CBioseq_Handle bh = m_scope->GetBioseqHandle(*id);

    CRef<CBioseq> retval;
    if (retrieve_seq_data) {
        retval.Reset(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    } else {
        retval.Reset(new CBioseq());
        CRef<CSeq_id> idToStore(new CSeq_id);
        idToStore->Assign(*id);
        retval->SetId().push_back(idToStore);
        retval->SetInst().SetRepr(CSeq_inst::eRepr_raw);
        retval->SetInst().SetMol(bh.IsProtein() 
                                    ? CSeq_inst::eMol_aa
                                    : CSeq_inst::eMol_dna);
        retval->SetInst().SetLength(len);
    }
    return retval;
}

bool CBlastBioseqMaker::IsProtein(CConstRef<CSeq_id> id)
{
    _ASSERT(m_scope.NotEmpty());

    CBioseq_Handle bh = m_scope->GetBioseqHandle(*id);
    if (!bh)
    {
        NCBI_THROW(CInputException, eSeqIdNotFound,
                    "Sequence ID not found: '" + 
                    id->AsFastaString() + "'");
    }
    return bh.IsProtein();
}

bool CBlastBioseqMaker::HasSequence(CConstRef<CSeq_id> id)
{
      CBioseq_Handle bh = m_scope->GetBioseqHandle(*id);
      CSeqVector seq_vect = bh.GetSeqVector();
      CSeqVector_CI itr(seq_vect);
      if (itr.GetGapSizeForward() == seq_vect.size())
          return false;
      else
          return true;
}

bool
CBlastBioseqMaker::IsEmptyBioseq(const CBioseq& bioseq)
{
    if (bioseq.CanGetInst()) {
        const CSeq_inst& inst = bioseq.GetInst();
        return (inst.GetRepr() == CSeq_inst::eRepr_raw &&
                inst.CanGetMol() &&
                inst.CanGetLength() &&
                inst.CanGetSeq_data() == false);
    }
    return false;

}

CBlastInputOMF::CBlastInputOMF(CBlastInputSourceOMF* source,
                               TSeqPos batch_size)
    : m_Source(source),
      m_BatchSize(batch_size),
      m_MaxNumSequences(5000000),
      m_BioseqSet(new CBioseq_set)
    
{}

void
CBlastInputOMF::GetNextSeqBatch(CBioseq_set& bioseq_set)
{
    TSeqPos bases_added = 0;
    TSeqPos num_sequences = 0;
    while (bases_added < m_BatchSize && num_sequences < m_MaxNumSequences &&
           !m_Source->End()) {

        CBioseq_set one_seq;
        bases_added += m_Source->GetNextSequence(one_seq);

        for (auto it: one_seq.GetSeq_set()) {
            num_sequences++;
            bioseq_set.SetSeq_set().push_back(it);
        }
    }
}

CRef<CBioseq_set>
CBlastInputOMF::GetNextSeqBatch(void)
{
    CRef<CBioseq_set> bioseq_set(new CBioseq_set);
    GetNextSeqBatch(*bioseq_set);
    return bioseq_set;
}


END_SCOPE(blast)
END_NCBI_SCOPE
