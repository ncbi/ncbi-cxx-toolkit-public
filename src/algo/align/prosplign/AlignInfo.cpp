/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "AlignInfo.hpp"
#include "intron.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

CAlignInfo::CAlignInfo(int length) : m_length(length)
{
    w.resize(length);
    h.resize(length);
    v.resize(length);
    fh.resize(length);
    fv.resize(length);
    wis = new CIgapIntronChain[m_length];
    his = new CIgapIntronChain[m_length];
    vis = new CIgapIntronChain[m_length];
    fhis = new CIgapIntronChain[m_length];
    fvis = new CIgapIntronChain[m_length];
}

CAlignInfo::~CAlignInfo()
{
    delete[] wis;
    delete[] his;
    delete[] vis;
    delete[] fhis;
    delete[] fvis;
}


void CAlignInfo::ClearIIC(void)
{
    CIgapIntronChain* wis_it = wis;
    CIgapIntronChain* his_it = his;
    CIgapIntronChain* vis_it = vis;
    CIgapIntronChain* fhis_it = fhis;
    CIgapIntronChain* fvis_it = fvis;

    for(size_t i=0; i < m_length; ++i) {
        wis_it++->Clear();
        his_it++->Clear();
        vis_it++->Clear();
        fhis_it++->Clear();
        fvis_it++->Clear();
    }
}

CFindGapIntronRow::CFindGapIntronRow(int length, const CProSplignScaledScoring& scoring): CAlignRow(length, scoring), m_length(length)
{
        wis = new CIgapIntronChain[m_length];
        vis = new CIgapIntronChain[m_length];
        h1is = new CIgapIntronChain[m_length];
        h2is = new CIgapIntronChain[m_length];
        h3is = new CIgapIntronChain[m_length];
}

CFindGapIntronRow::~CFindGapIntronRow()
{
    delete[] wis;
    delete[] vis;
    delete[] h1is;
    delete[] h2is;
    delete[] h3is;
}
void CFindGapIntronRow::ClearIIC(void)
{
    CIgapIntronChain* wis_it = wis;
    CIgapIntronChain* vis_it = vis;
    CIgapIntronChain* h1is_it = h1is;
    CIgapIntronChain* h2is_it = h2is;
    CIgapIntronChain* h3is_it = h3is;

    for(size_t i=0; i < m_length; ++i) {
        wis_it++->Clear();
        vis_it++->Clear();
        h1is_it++->Clear();
        h2is_it++->Clear();
        h3is_it++->Clear();
    }
}

CAlignRow::CAlignRow(int length, const CProSplignScaledScoring& scoring) {
        m_w.resize(length + scoring.lmin + 4, infinity);
        w = &m_w[0] + scoring.lmin + 4;
        m_v.resize(length + scoring.lmin + 1, infinity);
        v = &m_v[0] + scoring.lmin + 1;
        m_h1.resize(length + scoring.lmin + 1, infinity);
        h1 = &m_h1[0] + scoring.lmin + 1;
        m_h2.resize(length + scoring.lmin + 1, infinity);
        h2 = &m_h2[0] + scoring.lmin + 1;
        m_h3.resize(length + scoring.lmin + 1, infinity);
        h3 = &m_h3[0] + scoring.lmin + 1;
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
