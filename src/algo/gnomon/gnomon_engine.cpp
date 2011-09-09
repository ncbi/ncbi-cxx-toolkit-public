
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

void CGnomonEngine::AddSRAIntrons(const set<TSignedSeqRange>* sraintrons, double sraintronpenalty)
{
    m_data->m_sraintrons = sraintrons;
    m_data->m_sraintronpenalty = sraintronpenalty;
}

const set<TSignedSeqRange>* CGnomonEngine::GetSRAIntrons() const 
{
    return m_data->m_sraintrons;
}

double CGnomonEngine::GetSRAIntronPenalty() const 
{
    return m_data->m_sraintronpenalty;
}

void CGnomonEngine::AddSRAIslands(const set<TSignedSeqRange>* sraislands, double sraislandpenalty)
{
    m_data->m_sraislands = sraislands;
    m_data->m_sraislandpenalty = sraislandpenalty;
}

const set<TSignedSeqRange>* CGnomonEngine::GetSRAIslands() const 
{
    return m_data->m_sraislands;
}

double CGnomonEngine::GetSRAIslandPenalty() const 
{
    return m_data->m_sraislandpenalty;
}

CGnomonEngine::SGnomonEngineImplData::SGnomonEngineImplData
(CConstRef<CHMMParameters> hmm_params, const CResidueVec& sequence, TSignedSeqRange range) :
    m_seq(sequence), m_range(range), m_gccontent(0), m_hmm_params(hmm_params), m_sraintrons(0), m_sraislands(0)
{}

CGnomonEngine::SGnomonEngineImplData::~SGnomonEngineImplData() {}

CGnomonEngine::CGnomonEngine(CConstRef<CHMMParameters> hmm_params, const CResidueVec& sequence, TSignedSeqRange range)
    : m_data(new SGnomonEngineImplData(hmm_params,sequence,range))
{
    CheckRange();
    Convert(m_data->m_seq,m_data->m_ds);
    
    ResetRange(m_data->m_range);
}

CGnomonEngine::~CGnomonEngine() {}

int CGnomonEngine::GetMinIntronLen() const
{
    return m_data->m_intron_params->m_intronlen.MinLen();
}

int CGnomonEngine::GetMinIntergenicLen() const
{
    return m_data->m_intergenic_params->m_intergeniclen.MinLen();
}

int CGnomonEngine::GetMaxIntronLen() const
{
    return m_data->m_intron_params->m_intronlen.MaxLen();
}

double CGnomonEngine::GetChanceOfIntronLongerThan(int l) const
{
    double p = exp(m_data->m_intron_params->m_intronlen.ClosingScore(l));
    return p;
}

void CGnomonEngine::CheckRange()
{
    m_data->m_range.IntersectWith(TSignedSeqRange(0,m_data->m_seq.size()-1));
    if (m_data->m_range.Empty() )
        NCBI_THROW(CGnomonException, eGenericError, "range out of sequence");
}

void CGnomonEngine::ResetRange(TSignedSeqRange range)
{
    m_data->m_range = range;
    CheckRange();
 
    // compute the GC content of the sequence
    m_data->m_gccontent = 0;

    TSignedSeqPos middle = (m_data->m_range.GetFrom()+m_data->m_range.GetTo())/2;
    const int GC_RANGE_SIZE = 200000;
    TSignedSeqRange gc_range(middle-GC_RANGE_SIZE/2, middle+GC_RANGE_SIZE/2);
    gc_range &= TSignedSeqRange(0,m_data->m_seq.size()-1);
    gc_range += m_data->m_range;

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


    m_data->GetHMMParameter(m_data->m_donor);
    m_data->GetHMMParameter(m_data->m_acceptor);
    m_data->GetHMMParameter(m_data->m_start);
    m_data->GetHMMParameter(m_data->m_stop);
    m_data->GetHMMParameter(m_data->m_cdr);
    m_data->GetHMMParameter(m_data->m_ncdr);
    m_data->GetHMMParameter(m_data->m_intrg);

    m_data->GetHMMParameter(m_data->m_intron_params);
    m_data->m_intron_params->SetSeqLen( m_data->m_range.GetLength() );
    m_data->GetHMMParameter(m_data->m_intergenic_params);
    m_data->m_intergenic_params->SetSeqLen( m_data->m_range.GetLength() );
    m_data->GetHMMParameter(m_data->m_exon_params);
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
    TGeneModelList cls;

    return Run( cls, repeats,
                leftwall, rightwall, false, false,
                mpp
              );
}

double CGnomonEngine::Run(const TGeneModelList& cls,
                          bool repeats, bool leftwall, bool rightwall, bool leftanchor, bool rightanchor, double mpp, double consensuspenalty)
{
    m_data->m_parse.reset();
    m_data->m_ss.reset();

    TInDels initial_fshifts;

    m_data->m_ss.reset( new CSeqScores(*m_data->m_acceptor, *m_data->m_donor, *m_data->m_start, *m_data->m_stop,
                                       *m_data->m_cdr, *m_data->m_ncdr, *m_data->m_intrg,
                                       *m_data->m_intron_params,
                                       m_data->m_range.GetFrom(),  m_data->m_range.GetTo(),
                                       cls, initial_fshifts, mpp, *this)
                );
    m_data->m_ss->Init(m_data->m_seq, repeats, leftwall, rightwall, consensuspenalty, *m_data->m_intergenic_params);
    m_data->m_parse.reset( new CParse(*m_data->m_ss,
                                      *m_data->m_intron_params,
                                      *m_data->m_intergenic_params,
                                      *m_data->m_exon_params,
                                      leftanchor, rightanchor) );
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
    const CAlignMap& seq_map = m_data->m_ss->FrameShiftedSeqMap();
    
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
