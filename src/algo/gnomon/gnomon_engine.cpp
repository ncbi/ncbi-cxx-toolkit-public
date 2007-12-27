
/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software / database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software / database is freely available
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include "gnomon_seq.hpp"
#include "parse.hpp"
#include "hmm.hpp"
#include "hmm_inlines.hpp"
#include "gnomon_engine.hpp"
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

void CGnomonEngine::ReadOrgHMMParameters(CNcbiIstream& from)
{
    COrgParameters::Instance().Read(from);
}

CGnomonEngine::SGnomonEngineImplData::SGnomonEngineImplData(const CResidueVec& sequence, TSignedSeqRange range) :
    m_seq(sequence), m_range(range), m_gccontent(0)
{}
CGnomonEngine::SGnomonEngineImplData::~SGnomonEngineImplData() {}

CGnomonEngine::CGnomonEngine(const CResidueVec& sequence, TSignedSeqRange range) : m_data(new SGnomonEngineImplData(sequence,range))
{
    CheckRange();
    Convert(m_data->m_seq,m_data->m_ds);

    ResetRange(m_data->m_range);
}

CGnomonEngine::~CGnomonEngine() {}

int CGnomonEngine::GetMinIntronLen() const
{
    return CIntron::MinIntron();
}

double CGnomonEngine::GetChanceOfIntronLongerThan(int l) const
{
    return CIntron::ChanceOfIntronLongerThan(l);
}

void CGnomonEngine::CheckRange()
{
    m_data->m_range.IntersectWith(TSignedSeqRange(0,m_data->m_seq.size()-1));
    if (m_data->m_range.Empty() )
        NCBI_THROW(CGnomonException, eGenericError, "range out of sequence");
}

void CGnomonEngine::ResetRange(TSignedSeqRange range)
{
    m_data->m_range = range; CheckRange();
 
    // compute the GC content of the sequence
    m_data->m_gccontent = 0;

    TSignedSeqPos middle = (range.GetFrom()+range.GetTo())/2;
    const int GC_RANGE_SIZE = 200000;
    TSignedSeqRange gc_range(middle-GC_RANGE_SIZE/2, middle+GC_RANGE_SIZE/2);
    gc_range &= TSignedSeqRange(0,m_data->m_seq.size()-1);
    gc_range += range;

    for (TSignedSeqPos i = gc_range.GetFrom();i<=gc_range.GetTo(); ++i) {
        EResidue c = m_data->m_ds[ePlus][i];
        if (c == enC  ||  c == enG) {
            ++m_data->m_gccontent;
        }
    }
    m_data->m_gccontent = static_cast<int>
        (m_data->m_gccontent*100.0 / (gc_range.GetLength()) + 0.5);
    m_data->m_gccontent = max(1,m_data->m_gccontent);
    m_data->m_gccontent = min(99,m_data->m_gccontent);

    m_data->m_donor = GET_ORG_PARAMETER(CWAM_Donor<2>, m_data->m_gccontent);
        
    m_data->m_acceptor = GET_ORG_PARAMETER(CWAM_Acceptor<2>, m_data->m_gccontent);
    m_data->m_start = GET_ORG_PARAMETER(CWMM_Start, m_data->m_gccontent);
    m_data->m_stop = GET_ORG_PARAMETER(CWAM_Stop, m_data->m_gccontent);
    m_data->m_cdr = GET_ORG_PARAMETER(CMC3_CodingRegion<5>, m_data->m_gccontent);
    m_data->m_ncdr = GET_ORG_PARAMETER(CMC_NonCodingRegion<5>, m_data->m_gccontent);
    m_data->m_intrg = GET_ORG_PARAMETER(CMC_NonCodingRegion<5>, m_data->m_gccontent);

    CIntron::SetParams(*GET_ORG_PARAMETER(CIntronParameters, m_data->m_gccontent)).SetSeqLen( m_data->m_range.GetLength() );
    CIntergenic::SetParams(*GET_ORG_PARAMETER(CIntergenicParameters, m_data->m_gccontent)).SetSeqLen( m_data->m_range.GetLength() );
    CExon::SetParams(*GET_ORG_PARAMETER(CExonParameters, m_data->m_gccontent));
}

const CResidueVec& CGnomonEngine::GetSeq() const
{
    return m_data->m_seq;
}

int CGnomonEngine::GetGCcontent() const
{
    return m_data->m_gccontent;
}

double CGnomonEngine::Run(bool repeats, bool leftwall, bool rightwall, double mpp)
{
    static TAlignList cls;

    return Run( cls, repeats,
                leftwall, rightwall, false, false,
                mpp
              );
}

double CGnomonEngine::Run(const TAlignList& cls,
                          bool repeats, bool leftwall, bool rightwall, bool leftanchor, bool rightanchor, double mpp, double consensuspenalty)
{
    m_data->m_parse.reset();
    m_data->m_ss.reset();

    TFrameShifts initial_fshifts;

    m_data->m_ss.reset( new CSeqScores(*m_data->m_acceptor, *m_data->m_donor, *m_data->m_start, *m_data->m_stop,
                               *m_data->m_cdr, *m_data->m_ncdr, *m_data->m_intrg,
                               m_data->m_range.GetFrom(),  m_data->m_range.GetTo(),
                                       cls, initial_fshifts, mpp, *this)
                );
    m_data->m_ss->Init(m_data->m_seq, repeats, leftwall, rightwall, consensuspenalty);
    CHMM_State::SetSeqScores(*m_data->m_ss);
    m_data->m_parse.reset( new CParse(*m_data->m_ss, leftanchor, rightanchor) );
    return m_data->m_parse->Path()->Score();
}

list<CGeneModel> CGnomonEngine::GetGenes() const
{
    if (m_data->m_parse.get()==NULL)
        NCBI_THROW(CGnomonException, eGenericError, "gnomon not run");
    return m_data->m_parse->GetGenes();
}

TSignedSeqPos CGnomonEngine::PartialModelStepBack(list<CGeneModel>& genes) const
{
    const CFrameShiftedSeqMap& seq_map = m_data->m_ss->FrameShiftedSeqMap();
    
    TSignedSeqPos right = seq_map.MapEditedToOrig(m_data->m_ss->SeqLen()-1);
    if (!genes.empty() && !genes.back().RightComplete()) {
        TSignedSeqPos partial_start = genes.back().Limits().GetFrom();
        genes.pop_back();
            
        if(!genes.empty()) { // end of the last complete gene
            right = genes.back().Limits().GetTo();
        } else {
            if(int(partial_start) > seq_map.MapEditedToOrig(0)+1000) {
                right = partial_start-100;
            } else {
                return -1;   // calling program MUST be aware of this!!!!!!!
            }
        }
    }
    
    return right;
}

// void PrintGenes(const list<CGene>& genes, CUniqNumber& unumber, CNcbiOstream& to, CNcbiOstream& toprot)
// {
//     ITERATE (list<CGene>, it, genes) {
//         ++unumber;
//         it->Print(unumber, unumber, to, toprot);
//     }
// }

void CGnomonEngine::PrintInfo() const
{
    if (m_data->m_parse.get()==NULL)
        NCBI_THROW(CGnomonException, eGenericError, "gnomon not run");
    m_data->m_parse->PrintInfo();
}

END_SCOPE(gnomon)
END_NCBI_SCOPE

/*
 * ==========================================================================
 * $Log$
 * Revision 1.6.2.3  2006/11/30 20:10:35  souvorov
 * Implementation of proper mapping for prediction
 *
 * Revision 1.6.2.2  2006/10/12 19:08:22  chetvern
 * Changed hmm parameters reading
 *
 * Revision 1.6.2.1  2006/10/06 14:19:36  chetvern
 * Major overhaul. Single format for intermediate files.
 *
 * Revision 1.8  2006/10/05 15:32:05  souvorov
 * Implementation of anchors for intergenics
 *
 * Revision 1.7  2006/03/06 15:52:53  souvorov
 * Changes needed for ChanceOfIntronLongerThan(int l)
 *
 * Revision 1.6  2005/11/22 18:52:29  chetvern
 * More robust check for 'Human.inp' parameter file
 *
 * Revision 1.5  2005/11/21 21:33:45  chetvern
 * Splitted CParse::PrintGenes into CGnomonEngine::PartialModelStepBack and PrintGenes function
 *
 * Revision 1.4  2005/10/20 19:29:31  souvorov
 * Penalty for nonconsensus starts/stops/splices
 *
 * Revision 1.3  2005/10/06 14:34:25  souvorov
 * CGnomonEngine::GetSeqName() introduced
 *
 * Revision 1.2  2005/09/27 17:13:59  chetvern
 * fix gc-content calculation
 *
 * Revision 1.1  2005/09/15 21:28:07  chetvern
 * Sync with Sasha's working tree
 *
 *
 * ==========================================================================
 */

