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
    CRef<CSplicedAligner> aligner(new CSplicedAligner16);
    m_Splign->SetAligner() = aligner;
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
            ITERATE(CSeq_align_set::Tdata, sa_iter0, sas0) {
                const CSeq_align_set::Tdata &sas = 
                    (*sa_iter0)->GetSegs().GetDisc().Get();
                ITERATE(CSeq_align_set::Tdata, sa_iter, sas) {
                    CSplign::THitRef hitref (new CSplign::THit(**sa_iter));
                    if(hitref->GetQueryStrand() == false) {
                        hitref->FlipStrands();
                    }
                    hitrefs.push_back(hitref);
                }
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

/*===========================================================================
* $Log$
* Revision 1.20  2006/04/12 16:36:25  kapustin
* Do not throw on failed compartments
*
* Revision 1.19  2005/12/07 15:49:58  kapustin
* Protect scope by default
*
* Revision 1.18  2005/10/19 17:56:35  kapustin
* Switch to using ObjMgr+LDS to load sequence data
*
* Revision 1.17  2005/09/13 18:44:37  kapustin
* Flip hit strands if query is in minus
*
* Revision 1.16  2005/09/12 16:24:00  kapustin
* Move compartmentization to xalgoalignutil.
*
* Revision 1.15  2005/08/29 14:14:49  kapustin
* Retain last subject sequence in memory when in batch mode.
*
* Revision 1.14  2005/08/04 19:24:44  kapustin
* Adjust seq-id handling with changes in the toolkit
*
* Revision 1.13  2005/01/07 19:28:05  dicuccio
* Bug fix: don't try to retrieve sequence from CSeqvector beyond CSeqVector's
* length.
*
* Revision 1.12  2005/01/03 22:47:35  kapustin
* Implement seq-ids with CSeq_id instead of generic strings
*
* Revision 1.11  2004/12/16 23:12:26  kapustin
* algo/align rearrangement
*
* Revision 1.10  2004/11/29 14:37:16  kapustin
* CNWAligner::GetTranscript now returns TTranscript and direction can be 
* specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two 
* additional parameters to specify starting coordinates.
*
* Revision 1.9  2004/11/18 21:27:40  grichenk
* Removed default value for scope argument in seq-loc related functions.
*
* Revision 1.8  2004/07/07 21:40:12  kapustin
* Fix a typo in CSplignObjMgrAccessor::Load
*
* Revision 1.7  2004/06/29 20:51:52  kapustin
* Use CRef to access CObject-derived members
*
* Revision 1.6  2004/05/24 16:13:57  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/05/17 14:50:57  kapustin
* Add/remove/rearrange some includes and object declarations
*
* Revision 1.4  2004/05/12 16:59:13  johnson
* CSplignObjMgrAccessor falls back to id strings if IsSynonym fails
*
* Revision 1.3  2004/05/04 19:39:35  johnson
* return correct seq-ids in seq-align
*
* Revision 1.2  2004/05/04 15:23:45  ucko
* Split splign code out of xalgoalign into new xalgosplign.
*
* Revision 1.1  2004/05/03 15:39:11  johnson
* initial revision
*
* ===========================================================================
*/
