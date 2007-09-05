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

CAlignInfo::CAlignInfo(int length)
{
    w.resize(length);
    h.resize(length);
    v.resize(length);
    fh.resize(length);
    fv.resize(length);
    wis.resize(length, this);
    his.resize(length, this);
    vis.resize(length, this);
    fhis.resize(length, this);
    fvis.resize(length, this);
}

void CAlignInfo::ClearIIC(void)
{
    vector<CIgapIntronChain >::iterator it;
    for(it=wis.begin(); it != wis.end(); ++it) it->Clear();
    for(it=his.begin(); it != his.end(); ++it) it->Clear();
    for(it=vis.begin(); it != vis.end(); ++it) it->Clear();
    for(it=fhis.begin(); it != fhis.end(); ++it) it->Clear();
    for(it=fvis.begin(); it != fvis.end(); ++it) it->Clear();
}

CFindGapIntronRow::CFindGapIntronRow(int length): CAlignRow(length)
{
        wis.resize(length, this);
        vis.resize(length, this);
        h1is.resize(length, this);
        h2is.resize(length, this);
        h3is.resize(length, this);
}

void CFindGapIntronRow::ClearIIC(void)
{
    vector<CIgapIntronChain >::iterator it;
    for(it=wis.begin(); it != wis.end(); ++it) it->Clear();
    for(it=vis.begin(); it != vis.end(); ++it) it->Clear();
    for(it=h1is.begin(); it != h1is.end(); ++it) it->Clear();
    for(it=h2is.begin(); it != h2is.end(); ++it) it->Clear();
    for(it=h3is.begin(); it != h3is.end(); ++it) it->Clear();
}

CAlignRow::CAlignRow(int length) {
        m_w.resize(length + CFIntron::lmin + 4, infinity);
        w = &m_w[0] + CFIntron::lmin + 4;
        m_v.resize(length + CFIntron::lmin + 1, infinity);
        v = &m_v[0] + CFIntron::lmin + 1;
        m_h1.resize(length + CFIntron::lmin + 1, infinity);
        h1 = &m_h1[0] + CFIntron::lmin + 1;
        m_h2.resize(length + CFIntron::lmin + 1, infinity);
        h2 = &m_h2[0] + CFIntron::lmin + 1;
        m_h3.resize(length + CFIntron::lmin + 1, infinity);
        h3 = &m_h3[0] + CFIntron::lmin + 1;
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
