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
* Author: Philip Johnson
*
* File Description:
*   CSplignSimple -- simple wrapper to splign that uses BLAST & the object
*   manager
*
* ---------------------------------------------------------------------------
*/

#include <algo/align/splign/splign_simple.hpp>
#include <algo/align/splign/splign_formatter.hpp>
#include <algo/align/nw_spliced_aligner16.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#ifdef _DEBUG
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CSplignObjMgrAccessor : public CSplignSeqAccessor {
public:
    CSplignObjMgrAccessor(const CSeq_id &id1, const CSeq_id &id2,
                          CScope& scope) :
        m_Handle1(scope.GetBioseqHandle(id1)),
        m_Handle2(scope.GetBioseqHandle(id2)),
        m_SeqVector1(m_Handle1.GetSeqVector(CBioseq_Handle::eCoding_Iupac)),
        m_SeqVector2(m_Handle2.GetSeqVector(CBioseq_Handle::eCoding_Iupac))
    {
    }

    virtual void Load(const string& id, vector<char> *seq,
                      size_t start, size_t finish)
    {
        CSeq_id seqId(id);

        if (!seq) {
            NCBI_THROW(CAlgoAlignException, eNotInitialized,
                       "CSplignObjMgrAccessor::Load passed NULL sequence "
                       "pointer.");
        }
        if (seqId.Which() == CSeq_id::e_not_set) {
            NCBI_THROW(CAlgoAlignException, eInternal,
                       "CSplignObjMgrAccessor::Load could not identify "
                       "sequence for '"+id+"'.");
        }

        CSeqVector *sv = NULL;
        if (m_Handle1.IsSynonym(seqId)) {
            sv = &m_SeqVector1;
        } else if (m_Handle2.IsSynonym(seqId)) {
            sv = &m_SeqVector2;
        } else {
            NCBI_THROW(CAlgoAlignException, eInternal,
                       "CSplignObjMgrAccessor::Load could not find the proper "
                       "sequence for '"+id+"'.");
        }

        if (finish == kMax_UInt) { //flag to return everything
            finish = sv->size() - 1;
        }

        seq->reserve(finish - start + 1);
        for (int i = start;  i <= finish;  ++i) {
            seq->push_back((*sv)[i]);
        }
    }

protected:
    CBioseq_Handle m_Handle1;
    CBioseq_Handle m_Handle2;
    CSeqVector m_SeqVector1;
    CSeqVector m_SeqVector2;
};

/*---------------------------------------------------------------------------*/
// PRE : transcript location, genomic location (maximum span), scope in
// which transcript & genomic sequence can be resolved
// POST: blast & splign initialized
CSplignSimple::CSplignSimple(const CSeq_loc &transcript,
                             const CSeq_loc &genomic,
                             CScope& scope) :
    m_Blast(blast::SSeqLoc(transcript, scope),
            blast::SSeqLoc(genomic, scope), blast::eMegablast)
{
    CRef<CSplicedAligner> aligner(new CSplicedAligner16);
    CRef<CSplignSeqAccessor> accessor
        (new CSplignObjMgrAccessor(sequence::GetId(transcript),
                                   sequence::GetId(genomic),
                                   scope));
    m_Splign.SetAligner(aligner);
    m_Splign.SetSeqAccessor(accessor);
}

/*---------------------------------------------------------------------------*/
// PRE : splign & blast objects initialized
// POST: split results
const CSplign::TResults& CSplignSimple::Run(void)
{
    string res;
    blast::TSeqAlignVector blastRes(m_Blast.Run());

    if (!blastRes.empty()  &&
        blastRes.front().NotEmpty()  &&
        !blastRes.front()->Get().empty()  &&
        !blastRes.front()->Get().front()->GetSegs().GetDisc().Get().empty()) {
        
        CSplign::THits hits;

        const CSeq_align_set::Tdata &sas =
            blastRes.front()->Get().front()->GetSegs().GetDisc().Get();
        ITERATE(CSeq_align_set::Tdata, saI, sas) {
            hits.push_back(CHit(**saI));
        }

        m_Splign.Run(&hits);

        const CSplign::TResults &splignRes = m_Splign.GetResult();        
        ITERATE(CSplign::TResults, resI, splignRes) {
            if (resI->m_error) {
                NCBI_THROW(CException, eUnknown, resI->m_msg);
            }
        }
    }

    return m_Splign.GetResult();
}

/*---------------------------------------------------------------------------*/
// PRE : splign run
// POST: splign results (if any) as Seq_align_set
CRef<objects::CSeq_align_set> CSplignSimple::GetResultsAsAln(void) const
{
    return CSplignFormatter(m_Splign).AsSeqAlignSet();
}

END_NCBI_SCOPE

/*===========================================================================
* $Log$
* Revision 1.2  2004/05/04 15:23:45  ucko
* Split splign code out of xalgoalign into new xalgosplign.
*
* Revision 1.1  2004/05/03 15:39:11  johnson
* initial revision
*
* ===========================================================================
*/
