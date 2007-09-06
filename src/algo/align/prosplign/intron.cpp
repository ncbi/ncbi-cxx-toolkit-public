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

#include <algo/align/prosplign/prosplign_exception.hpp>

#include"nucprot.hpp"
#include"intron.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

int CAnyIntron::ini_nuc_margin = 1;//minimum - 0 
                                   // for versions with no end gap cost at least one required.

void CAnyIntron::SimpleNucStep() {
    j++;
    swa.first -= ie;
    swt.first -= ie;
    swg.first -= ie;
    swc.first -= ie;
    swn.first -= ie;

    sea.first -= ie;
    set.first -= ie;
    seg.first -= ie;
    sec.first -= ie;
    sen.first -= ie;

    ++sw111;
    ++sfv111;
    ++sw000;
    ++sh000;
    ++sv000;
    ++sfh000;
    ++sfv000;
    ++sw012;
    ++sh012;
    ++sw021;
    ++sh021;
}

void CAnyIntron::AddW1() {
    int jj = j-lmin-2;
    int sco = esc[jj-1]; 
            switch (nseq[jj-1]) {
                case nA:
                    if(swa.first<sco) {
                            swa.first = sco;
                            swa.second = jj;
                    }
                    break;
                case nC:
                    if(swc.first<sco) {
                            swc.first = sco;
                            swc.second = jj;
                    }
                    break;
                case nG:
                    if(swg.first<sco) {
                            swg.first = sco;
                            swg.second = jj;
                    }
                    break;
                case nT:
                    if(swt.first<sco) {
                            swt.first = sco;
                            swt.second = jj;
                    }
                    break;
                default:
                    if(swn.first<sco) {
                            swn.first = sco;
                            swn.second = jj;
                    }
                    break;
            }
}

void CAnyIntron::AddW2(const CProSplignScaledScoring scoring) {
            int sco = esc[j-lmin-3] + matrix.MultScore(nseq[j-lmin-3], nseq[j-lmin-2], nA, amin, scoring);
            if(sco > sea.first) {
                sea.first = sco;
                sea.second = j-lmin-1;
            }
            sco = esc[j-lmin-3] + matrix.MultScore(nseq[j-lmin-3], nseq[j-lmin-2], nT, amin, scoring);
            if(sco > set.first) {
                set.first = sco;
                set.second = j-lmin-1;
            }
            sco = esc[j-lmin-3] + matrix.MultScore(nseq[j-lmin-3], nseq[j-lmin-2], nG, amin, scoring);
            if(sco > seg.first) {
                seg.first = sco;
                seg.second = j-lmin-1;
            }
            sco = esc[j-lmin-3] + matrix.MultScore(nseq[j-lmin-3], nseq[j-lmin-2], nC, amin, scoring);
            if(sco > sec.first) {
                sec.first = sco;
                sec.second = j-lmin-1;
            }
            sco = esc[j-lmin-3] + matrix.MultScore(nseq[j-lmin-3], nseq[j-lmin-2], nN, amin, scoring);
            if(sco > sen.first) {
                sen.first = sco;
                sen.second = j-lmin-1;
            }
}

    
void CAnyIntron::NucStep(const CProSplignScaledScoring scoring)
{
    SimpleNucStep();
    if(j - lmin - 3 >= ini_nuc_margin) {
        AddW1();
        AddW2(scoring);
    }//end j > lmin +3 
    sw111.AddDon();
    sfv111.AddDon();
    sw000.AddDon();
    sh000.AddDon();
    sv000.AddDon();
    sfh000.AddDon();
    sfv000.AddDon();
    sw012.AddDon();
    sh012.AddDon();
    sw021.AddDon();
    sh021.AddDon();

}           

//CFIntron implementation

CFIntron::CFIntron(const CNSeq& nseq) : m_nseq(nseq)
{
    if(lmin<4) NCBI_THROW(CProSplignException, eParam, "minimum intron length should exceed 3");
    CFIntornData cd; 
    int jend = nseq.size() + 1;
    m_data.resize(jend+1);//m_data[0] is a dummy, m_data[1;nseq.size()+1] used
    for(int j=0; j < jend; ++j) {
        //setup for w1, w2
        if(j - lmin - 3 < 0) {
            cd.m_dt1 = cd.m_dt2 = eANY;
            cd.m_at1 = cd.m_at2 = eANYa;
            cd.m_1nuc = cd.m_2nuc = nN;
        } else {
            int ind = j - lmin - 4;
            cd.m_1nuc = (Nucleotides)nseq[++ind];
            char n1 = nseq[++ind];
            char n2 = nseq[++ind];
            char n3 = nseq[++ind];
            cd.m_dt1 = GetDonType(n1, n2);
            cd.m_dt2 = GetDonType(n2, n3);
            ind = j - 5;
            n1 = nseq[++ind];
            n2 = nseq[++ind];
            n3 = nseq[++ind];
            cd.m_2nuc = (Nucleotides)nseq[++ind];
            cd.m_at1 = GetAccType(n1, n2);
            cd.m_at2 = GetAccType(n2, n3);
        }
        //setup for w
        if(j - lmin < 0) {
            cd.m_dt = eANY;
            cd.m_at = eANYa;
        } else {
            cd.m_dt = GetDonType(nseq[j-lmin], nseq[j-lmin+1]);
            cd.m_at = GetAccType(nseq[j-2], nseq[j-1]);
        }
        m_data[j+1] = cd;
    }
}

void CFIntronDon::Reset(void)
{
    m_v.first = infinity;
    m_w.first = infinity;
    m_h1.first = infinity;
    m_h2.first = infinity;
    m_h3.first = infinity;
    for(int i=0; i<5; ++i) {
        m_w1[i] = m_w2[i] = infinity;
    }
}

void CFIntron::InitRowScores(CAlignRow *row, vector<int>& prevw, int j)
{
    m_gt.Reset();
    m_gc.Reset();
    m_at.Reset();
    m_any.Reset();
    for(int i=0; i<5; ++i) {
        m_w1s[i] = m_w2s[i] = 0;
    }
    m_cd = &m_data[j];
    m_w = &row->m_w[3+j];
    m_w12 = &prevw[j];
    m_v = &row->m_v[j];
    m_h1 = &row->m_h1[j];
    m_h2 = &row->m_h2[j];
    m_h3 = &row->m_h3[j];
}


END_SCOPE(prosplign)
END_NCBI_SCOPE
