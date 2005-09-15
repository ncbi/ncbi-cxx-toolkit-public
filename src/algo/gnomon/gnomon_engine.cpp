
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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)


CGnomonEngine::CGnomonEngine(const string& modeldatafilename, const string& seqname, const CResidueVec& sequence, TSignedSeqRange range) : m_data(NULL)
{
    try {
        m_data = new SGnomonEngineImplData(modeldatafilename,seqname,sequence,range);
        CheckRange();
        Convert(m_data->m_seq,m_data->m_ds);


        // compute the GC content of the sequence
        m_data->m_gccontent = 0;

        ITERATE (CEResidueVec, i, m_data->m_ds[ePlus]) {
            if (*i == enC  ||  *i == enG) {
                ++m_data->m_gccontent;
            }
        }
        m_data->m_gccontent = static_cast<int>
            (m_data->m_gccontent*100.0 / (m_data->m_range.GetLength()) + 0.5);
        m_data->m_gccontent = max(1,m_data->m_gccontent);
        m_data->m_gccontent = min(99,m_data->m_gccontent);

        if(m_data->m_modeldatafilename.find("Human.inp") == m_data->m_modeldatafilename.length() - 9)
            m_data->m_donor.reset( new CMDD_Donor(modeldatafilename, m_data->m_gccontent) );
        else
            m_data->m_donor.reset( new CWAM_Donor<2>(modeldatafilename, m_data->m_gccontent) );
        
        m_data->m_acceptor.reset( new CWAM_Acceptor<2>       (modeldatafilename, m_data->m_gccontent));
        m_data->m_start.reset   ( new CWMM_Start             (modeldatafilename, m_data->m_gccontent));
        m_data->m_stop.reset    ( new CWAM_Stop              (modeldatafilename, m_data->m_gccontent));
        m_data->m_cdr.reset     ( new CMC3_CodingRegion<5>   (modeldatafilename, m_data->m_gccontent));
        m_data->m_ncdr.reset    ( new CMC_NonCodingRegion<5> (modeldatafilename, m_data->m_gccontent));
        m_data->m_intrg.reset   ( new CMC_NonCodingRegion<5> (modeldatafilename, m_data->m_gccontent));

    }
    catch(...) {
        cleanup();
        throw;
    }
}

CGnomonEngine::~CGnomonEngine()
{
    cleanup();
}

void CGnomonEngine::cleanup()
{
    delete(m_data);
}

int CGnomonEngine::GetMinIntronLen()
{
    return CIntron::MinIntron();
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
}

int CGnomonEngine::GetGCcontent() const
{
    return m_data->m_gccontent;
}

double CGnomonEngine::Run(bool repeats, bool leftwall, bool rightwall, double mpp)
{
    static TAlignList cls;

    return Run( cls, repeats,
                leftwall, rightwall,
                mpp
              );
}

double CGnomonEngine::Run(const TAlignList& cls, //  const TFrameShifts& initial_fshifts,
                          bool repeats, bool leftwall, bool rightwall, double mpp)
{
    m_Annot.Reset();
    m_data->m_parse.reset();
    m_data->m_ss.reset();

    static TSignedSeqPos prev_range_len;
    if (prev_range_len != m_data->m_range.GetLength()) {
        CIntron::Init    (m_data->m_modeldatafilename, m_data->m_gccontent, m_data->m_range.GetLength());
        CIntergenic::Init(m_data->m_modeldatafilename, m_data->m_gccontent, m_data->m_range.GetLength());
        CExon::Init      (m_data->m_modeldatafilename, m_data->m_gccontent);
        prev_range_len = m_data->m_range.GetLength();
    }

    TFrameShifts initial_fshifts;

    m_data->m_ss.reset( new CSeqScores(*m_data->m_acceptor, *m_data->m_donor, *m_data->m_start, *m_data->m_stop,
                               *m_data->m_cdr, *m_data->m_ncdr, *m_data->m_intrg,
                               m_data->m_seq, m_data->m_range.GetFrom(),  m_data->m_range.GetTo(),
                               cls, initial_fshifts, repeats,
                               leftwall, rightwall, m_data->m_seqname,
                               mpp
                               )
                );
    //seqscr->CodingScore(RegionStart(),RegionStop(),Strand(),frame);
    CHMM_State::SetSeqScores(*m_data->m_ss);
    m_data->m_parse.reset( new CParse(*m_data->m_ss) );
    return m_data->m_parse->Path()->Score();
}

list<CGene> CGnomonEngine::GetGenes() const
{
    if (m_data->m_parse.get()==NULL)
        NCBI_THROW(CGnomonException, eGenericError, "gnomon not run");
    return m_data->m_parse->GetGenes();
}

int CGnomonEngine::PrintGenes(CUniqNumber& unumber, CNcbiOstream& to, CNcbiOstream& toprot, bool complete) const
{
    if (m_data->m_parse.get()==NULL)
        NCBI_THROW(CGnomonException, eGenericError, "gnomon not run");
    return m_data->m_parse->PrintGenes(unumber,to,toprot,complete);
}

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
 * Revision 1.1  2005/09/15 21:28:07  chetvern
 * Sync with Sasha's working tree
 *
 *
 * ==========================================================================
 */

