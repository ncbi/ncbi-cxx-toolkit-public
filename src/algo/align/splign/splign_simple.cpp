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

#include <ncbi_pch.hpp>
#include "messages.hpp"

#include <algo/align/splign/splign_simple.hpp>
#include <algo/align/splign/splign_formatter.hpp>
#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/align_exception.hpp>
#include <algo/align/util/blast_tabular.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
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


/*---------------------------------------------------------------------------*/
// PRE : transcript location, genomic location (maximum span), scope in
// which transcript & genomic sequence can be resolved
// POST: blast & splign initialized
CSplignSimple::CSplignSimple(const CSeq_loc &transcript,
                             const CSeq_loc &genomic,
                             CScope& scope) :
    m_Splign(new CSplign),
    m_Blast(new blast::CBl2Seq(blast::SSeqLoc(transcript, scope),
                               blast::SSeqLoc(genomic, scope),
                               blast::eMegablast)),
    m_TranscriptId(&sequence::GetId(transcript, &scope)),
    m_GenomicId   (&sequence::GetId(genomic, &scope))
{    
    m_Splign->SetAligner() = CSplign::s_CreateDefaultAligner(true);
    m_Splign->SetScope().Reset(&scope);
    m_Splign->PreserveScope();
}


CRef<CSplign>& CSplignSimple::SetSplign(void) 
{
    return m_Splign;
}

CConstRef<CSplign> CSplignSimple::GetSplign(void) const
{
    return m_Splign;
}

CRef<blast::CBl2Seq>& CSplignSimple::SetBlast(void)
{
    return m_Blast;
}

CConstRef<blast::CBl2Seq> CSplignSimple::GetBlast(void) const
{
    return m_Blast;
}

/*---------------------------------------------------------------------------*/
// PRE : splign & blast objects initialized
// POST: split results
const CSplign::TResults& CSplignSimple::Run(void)
{
    USING_SCOPE(blast);

    TSeqAlignVector sav (m_Blast->Run());
    CSplign::THitRefs hitrefs;
    ITERATE(TSeqAlignVector, ii, sav) {
        if((*ii)->IsSet()) {
            const CSeq_align_set::Tdata &sas0 = (*ii)->Get();
            ITERATE(CSeq_align_set::Tdata, sa_iter, sas0) {
                    CSplign::THitRef hitref (new CSplign::THit(**sa_iter));
                    if(hitref->GetQueryStrand() == false) {
                        hitref->FlipStrands();
                    }
                    hitrefs.push_back(hitref);
            }
        }
    }

    if(hitrefs.size()) {
        m_Splign->Run(&hitrefs);
    }

    return m_Splign->GetResult();
}

/*---------------------------------------------------------------------------*/
// PRE : splign run
// POST: splign results (if any) as Seq_align_set
CRef<CSeq_align_set> CSplignSimple::GetResultsAsAln(void) const
{
    CRef<CSeq_align_set> sas(CSplignFormatter(*m_Splign).AsSeqAlignSet());

    CRef<CSeq_id> si;

    NON_CONST_ITERATE (CSeq_align_set::Tdata, compartmentI, sas->Set()) {
        NON_CONST_ITERATE (CSeq_align_set::Tdata, saI,
                           (*compartmentI)->SetSegs().SetDisc().Set()) {

            CDense_seg &ds = (*saI)->SetSegs().SetDenseg();
            ds.SetIds().clear();
            si = new CSeq_id;
            si->Assign(*m_TranscriptId);
            ds.SetIds().push_back(si);
            si = new CSeq_id;
            si->Assign(*m_GenomicId);
            ds.SetIds().push_back(si);
        }
    }

    return sas;
}

END_NCBI_SCOPE
