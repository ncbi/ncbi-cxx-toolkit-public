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

#include "nucprot.hpp"
#include "intron.hpp"
#include "Info.hpp"
#include "BackAlignInfo.hpp"
#include "Ali.hpp"
#include "AlignInfo.hpp"
#include "scoring.hpp"

#include <util/tables/raw_scoremat.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

const int infinity = numeric_limits<int>::min()/3;

CSubstMatrix::CSubstMatrix(const string& matrix_name, int scaling)
{
    const SNCBIPackedScoreMatrix* packed_mtx = &NCBISM_Blosum62;
//        NCBISM_GetStandardMatrix(matrix_name.c_str());
    if (packed_mtx == NULL)
        NCBI_THROW(CProSplignException, eParam, "unknown scoring matrix: "+matrix_name);

    m_alphabet = packed_mtx->symbols;
    m_alphabet = NStr::ToUpper(m_alphabet);

    if (string::npos == m_alphabet.find('X'))
        NCBI_THROW(CProSplignException, eParam, "unsupported scoring matrix: "+matrix_name);

    SNCBIFullScoreMatrix mtx;
    NCBISM_Unpack(packed_mtx, &mtx);

    for(int i = 0; i < 256;  ++i) {
        for(int j = 0; j < 256;  ++j) {
            scaled_subst_matrix[i][j] = packed_mtx->defscore*scaling;
        }
    }

    int score;
    for(const char* p1 = packed_mtx->symbols; *p1; ++p1) {
        int c = toupper(*p1);
        int lc = tolower(c);
        for(const char* p2 = packed_mtx->symbols; *p2; ++p2) {
            int d = toupper(*p2);
            int ld = tolower(d);
            score = mtx.s[(int)(*p1)][(int)(*p2)]*scaling;
            scaled_subst_matrix[c][d] = score;
            scaled_subst_matrix[lc][ld] = score;
            scaled_subst_matrix[c][ld] = score;
            scaled_subst_matrix[lc][d] = score;
    }}
}

void CSubstMatrix::SetTranslationTable(const CTranslationTable* trans_table)
{
    m_trans_table.Reset(trans_table);
}

CTranslationTable::CTranslationTable(int gcode, bool allow_alt_starts) : 
        m_trans_table(CGen_code_table::GetTransTable(gcode)), m_allow_alt_starts(allow_alt_starts)
{
    for(int i=0; i<5; ++i)
    for(int j=0; j<5; ++j)
    for(int k=0; k<5; ++k)
        aa_table[i*(8*8)+j*8+k] =
            TranslateTriplet(NucToChar(i),NucToChar(j),NucToChar(k));
}

int CTranslationTable::CharToNuc(char c) {
  if(c == 'A' || c == 'a') return nA;
  if(c == 'C' || c == 'c') return nC;
  if(c == 'G' || c == 'g') return nG;
  if(c == 'T' || c == 't') return nT;
  return nN;
}

char CTranslationTable::NucToChar(int n) {
    if(n == nA) return 'A';
    if(n == nT) return 'T';
    if(n == nG) return 'G';
    if(n == nC) return 'C';
    return 'N';
}

//fast score access implementation
void CFastIScore::Init(const CNSeq& seq, const CSubstMatrix& matrix) {
    Init(matrix);
    m_size = seq.size() - 2;
    m_scores.resize( m_size *  matrix.m_alphabet.size() + 1);
    int j;
    string::size_type i;
    int *pos = &m_scores[0];
    for(i=0; i<matrix.m_alphabet.size(); ++i) {
        m_gpos = &m_gscores[i * 125];
        for(j=2; j<seq.size(); ++j) {
            *++pos = GetScore(seq[j-2], seq[j-1], seq[j]);
        }
    }
}

void CFastIScore::SetAmin(char amin, const CSubstMatrix& matrix) {
    Init(matrix);
    string::size_type num = matrix.m_alphabet.find(toupper(amin));
    if(num == string::npos) num = matrix.m_alphabet.find('X');
    m_gpos = &m_gscores[num * 125];
    m_pos = &m_scores[num * m_size];
}

// bool CFastIScore::m_init = false;
// int *CFastIScore::m_gpos;
// vector<int> CFastIScore::m_gscores;

void CFastIScore::Init(const CSubstMatrix& matrix) {
    if(m_init) return;
    m_init = true;
    m_gscores.resize(125*matrix.m_alphabet.size());
    int *pos = &m_gscores[0];
    int arr[] = { nA, nC, nG, nT, nN };
    int i1, i2, i3;
    string::size_type i;
    for(i=0; i<matrix.m_alphabet.size(); ++i) {
        char amin = matrix.m_alphabet[i];
        for(i1 = 0; i1<5; ++i1) {
            for(i2 = 0; i2<5; ++i2) {
                for(i3 = 0; i3<5; ++i3) *pos++ = matrix.MultScore(arr[i1], arr[i2], arr[i3], amin);
            }
        }
    }
}
//end of fast score access implementation



int   FrAlign(CBackAlignInfo& bi, const PSEQ& pseq, const CNSeq& nseq, int g/*gap opening*/, int e/*one nuc extension cost*/,
              int f/*frameshift opening cost*/, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix)
{
  int ilen = (int)pseq.size() + 1;
  int jlen = nseq.size() + 1;
  CFrAlignRow row1(jlen), row2(jlen);
  CFrAlignRow *crow = &row1, *prow = &row2;
  int v1, v2, h1, h2, w1, w2, w3, w4, w5, w6, w7, w8, w9, v0/* = w10*/, h0/* = w11*/, w0;
  int hpre1, hpre2, hpre3;
  int i, j;
  //first row, i.e. i=0
    for(j=0;j<jlen;j++) {
      crow->w[j] = 0;
      crow->v[j] = infinity; 
    }
  // *******  MAIN LOOP ******************
  for(i=1;i<ilen;i++) {
    swap(crow, prow);
    crow->w[0] = - g - 3*i*e;
    //leads to artefacts!
    crow->w[1] = - f - (i*3 - 1)*e;
    bi.b[i-1][0] = 3;
    crow->w[2] = - f - (i*3 - 2)*e;
    bi.b[i-1][1] = 5;
    /**/
    crow->v[1] = crow->v[2]  = infinity;
    hpre1 = hpre2 = h0 = infinity;
	for(j=3;j<jlen;j++) {
        char& b = bi.b[i-1][j-1];
        b = (char)0;
        hpre3 = hpre2;
        hpre2 = hpre1;
        hpre1 = h0;
        //best v-gap
        v1 = prow->w[j] - g - 3*e;
        v2 = prow->v[j] - 3*e;
        v0 = max(v1, v2);
        crow->v[j] = v0; 
        if(v0 == v2) b += (char)32;
        //best h-gap
        h1 = crow->w[j-3] - g - 3*e;
        h2 = hpre3 - 3*e;
        h0 = max(h1, h2);
        if(h0 == h2) b += (char)16;
        //rest
        w1 = prow->w[j-3] + matrix.MultScore(nseq[j-3], nseq[j-2], nseq[j-1], pseq[i-1]);
        w2 = prow->w[j-1] - f - 2*e;
        w3 = prow->v[j-1] - (f - g) - 2*e;
        w4 = prow->w[j-2] - f - e;
        w5 = prow->v[j-2] - (f - g) - e;
        w6 = crow->w[j-1] - f - e;
        w7 = hpre1 - (f - g) - e;
        w8 = crow->w[j-2] - f - 2*e;
        w9 = hpre2 - (f - g) - 2*e;
        w0 = max(w1, max(w2, max(w3, max(w4, max(w5, max(w6, max(w7, max(w8, max(w9, max(v0, h0))))))))));
        if(w0 == w1) b += 1;
        else if(w0 == w2) b += 2;
        else if(w0 == w3) b += 3;
        else if(w0 == w4) b += 4;
        else if(w0 == w5) b += 5;
        else if(w0 == w6) b += 6;
        else if(w0 == w7) b += 7;
        else if(w0 == w8) b += 8;
        else if(w0 == w9) b += 9;
        else if(w0 == v0) b += 10;
        else if(w0 == h0) b += 11;
        crow->w[j] = w0;
    }
  }
  //best from the last row
  bi.maxi = bi.ilen-1;// == ilen-2
  int wmax = crow->w[0];
  int jmax = 0;
  for(j=1;j<jlen;++j) {
      if(wmax <= crow->w[j]) {
          wmax = crow->w[j];
          jmax = j;
      }
  }
  bi.maxj = jmax - 1;//shift to back align coord
/*
  bi.maxi = bi.ilen-1;// == ilen-2
  bi.maxj = bi.jlen-1;// == jlen-2
  return crow->w[jlen-1]; 
*/
  return wmax;
}

int FindFGapIntronNog(vector<pair<int, int> >& igi/*to return end gap/intron set*/, const PSEQ& pseq, const CNSeq& nseq, bool& left_gap, bool& right_gap, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix)
{
	CIgapIntronPool pool;

  left_gap = false;
  right_gap = false;
  if(nseq.size() < 1) return 0;
   // in matrices letters starts at [1]
  int ilen = (int)pseq.size() + 1;
  int jlen = nseq.size() + 1;
  CFindGapIntronRow row1(jlen, scoring, pool), row2(jlen, scoring, pool);
  CFindGapIntronRow *crow = &row1, *prow = &row2;

  int i, j;
  int wmax = 0;//max score in the last column
  int imax = 0;//row number for wmax
  int jmax = jlen - 1;//column number for wmax
  CIgapIntronChain lsb; //last column best, corresponds to wmax
  lsb.SetPool(pool);
  lsb.Creat(0, jmax);

  CFastIScore fiscore;
  fiscore.Init(nseq, matrix);
  CFIntron fin(nseq, scoring);
    // ** prepare for main loop
    //penalties
  //    CScoring::Init();
    int e = scoring.sm_Ine;
    int g = scoring.sm_Ig;
    int f = scoring.sm_If;
    int pev1 =  - g - 3*e;
    int pev2 = - 3*e;
    int pe2 =  - f - 2*e;
    int pe3 =  - (f - g) - 2*e;
    int pe4 =  - f - e;
    int pe5 =  - (f - g) - e;
    int pe6 = f - g - e;
    int *cv, *cw, *pv, *pv1, *pw1, *pw3, *ch1, *ch2, *ch3;

    //first row, i.e. i=0
    crow->w[0] = 0;
    crow->v[0] = infinity; 
    for(j=1;j<jlen;j++) {
      crow->w[j] = 0;
      crow->v[j] = infinity; 
      crow->wis[j].Creat(0, 0);//to differ initial gap from intron starting at 0
      crow->wis[j].Expand(crow->wis[j], 0, j);
    }

    // *******  MAIN LOOP ******************
    int jlen_1 = jlen - 1;
    for(i=1;i<ilen;++i) {
        swap(crow, prow);
        crow->w[0] = 0;
        crow->w[1] = 0;
        crow->w[2] = 0;
        crow->v[0] = crow->v[1] = crow->v[2]  = infinity;
        crow->h1[0] = crow->h1[1] = crow->h1[2]  = infinity;
        crow->h2[0] = crow->h2[1] = crow->h2[2]  = infinity;
        crow->h3[0] = crow->h3[1] = crow->h3[2]  = infinity;
        int h1 = infinity;
        int h2 = infinity;
        int h3 = infinity;
       //extra init for intron scoring
        fin.InitRowScores(crow, prow->m_w, 3);
        fiscore.SetAmin(pseq[i-1], matrix);
       // pointers
        cv = &crow->v[2];
        ch1 = &crow->h1[2];
        ch2 = &crow->h2[2];
        ch3 = &crow->h3[2];
        cw = &crow->w[2];
        pw3 =  &prow->w[0];
        pw1 =  &prow->w[2];
        pv1 =  &prow->v[2];
        pv =  &prow->v[0];
        // *******  INTERNAL LOOP ******************
    	for(j=3;j<jlen_1;++j) {
            const CBestI& bei = fin.Step(j, scoring, fiscore);
            //rest
            int w1 = *pw3 + fiscore.GetScore();
            int w2 = *pw1 + pe2;
            int w3 = *pv1 + pe3;
            int w4 = *++pw3 + pe4;
            int w5 = *++pv + pe5;
            //best v-gap
            int v0 = *++pw1 + pev1;
            int v2 = *++pv1 + pev2;
            if(bei.v > v0 && bei.v > v2) {
                v0 = bei.v;
                int len = fin.GetVlen(j, scoring);
                crow->vis[j].Expand(crow->vis[j - len], j - len, len);
            } else if(v2 > v0) {
                v0 = v2;
                crow->vis[j].Copy(prow->vis[j]);
            } else {
                crow->vis[j].Copy(prow->wis[j]);
            }
            *++cv = v0; 
            //best h-gap
            int h12 = h3 + pe5;
            h3 = h2 + pe6;
            if(bei.h3 > h3) {
                h3 = bei.h3;
                int len = fin.GetH3len(j, scoring);
                crow->h3is[j].Expand(crow->h3is[j - len], j - len, len);
            } else {
                crow->h3is[j].Copy(crow->h2is[j - 1]);
            }
            h2 = h1 - e;
            if(bei.h2 > h2) {
                h2 = bei.h2;
                int len = fin.GetH2len(j, scoring);
                crow->h2is[j].Expand(crow->h2is[j - len], j - len, len);
            } else {
                crow->h2is[j].Copy(crow->h1is[j - 1]);
            }
            h1 = *cw + pe4;
            if(bei.h1 > h1 && bei.h1 > h12) {
                h1 = bei.h1;
                int len = fin.GetH1len(j, scoring);
                crow->h1is[j].Expand(crow->h1is[j - len], j - len, len);
            } else if(h12 > h1) {
                h1 = h12;
                crow->h1is[j].Copy(crow->h3is[j - 1]);
            } else {
                crow->h1is[j].Copy(crow->wis[j - 1]);
            }
            *++ch1 = h1;
            *++ch2 = h2;
            *++ch3 = h3;
            //max
            int w0 = max(w1, max(w2, max(w3, max(w4, max(w5, max(h1, max(h2, max(h3, max(v0, max(bei.w2, max(bei.w1, bei.w)))))))))));
            if(w0 == w1) crow->wis[j].Copy(prow->wis[j - 3]);
            else if(w0 == v0) crow->wis[j].Copy(crow->vis[j]);
            else if(w0 == h3) crow->wis[j].Copy(crow->h3is[j]);
            else if(w0 == h1) crow->wis[j].Copy(crow->h1is[j]);
            else if(w0 == h2) crow->wis[j].Copy(crow->h2is[j]);
            else if(w0 == w2) crow->wis[j].Copy(prow->wis[j - 1]);
            else if(w0 == w3) crow->wis[j].Copy(prow->vis[j - 1]);
            else if(w0 == w4) crow->wis[j].Copy(prow->wis[j - 2]);
            else if(w0 == w5) crow->wis[j].Copy(prow->vis[j - 2]);
            else if(w0 == bei.w1) {
                int len = fin.GetW1len(j, scoring);
                crow->wis[j].Expand(prow->wis[j - len - 3], j - len - 2, len);
            } else if(w0 == bei.w2) {
                int len = fin.GetW2len(j, scoring);
                crow->wis[j].Expand(prow->wis[j - len - 3], j - len - 1, len);
            } else { //w == bei.w
                int len = fin.GetWlen(j, scoring);
                crow->wis[j].Expand(crow->wis[j - len], j - len, len);
            }
            *++cw = w0;
        }
        //the last column !
        if(2 < jlen_1) { //j == jlen_1 
            const CBestI bei = fin.Step(j, scoring, fiscore);
            //rest
            int w1 = *pw3 + fiscore.GetScore();
            int w12 = prow->w[j-1];
            int w13 = prow->w[j-2];
            //best h-gap
            int h12 = h3 + pe5;
            h3 = h2 + pe6;
            if(bei.h3 > h3) {
                h3 = bei.h3;
                int len = fin.GetH3len(j, scoring);
                crow->h3is[j].Expand(crow->h3is[j - len], j - len, len);
            } else {
                crow->h3is[j].Copy(crow->h2is[j - 1]);
            }
            h2 = h1 - e;
            if(bei.h2 > h2) {
                h2 = bei.h2;
                int len = fin.GetH2len(j, scoring);
                crow->h2is[j].Expand(crow->h2is[j - len], j - len, len);
            } else {
                crow->h2is[j].Copy(crow->h1is[j - 1]);
            }
            h1 = *cw + pe4;
            if(bei.h1 > h1 && bei.h1 > h12) {
                h1 = bei.h1;
                int len = fin.GetH1len(j, scoring);
                crow->h1is[j].Expand(crow->h1is[j - len], j - len, len);
            } else if(h12 > h1) {
                h1 = h12;
                crow->h1is[j].Copy(crow->h3is[j - 1]);
            } else {
                crow->h1is[j].Copy(crow->wis[j - 1]);
            }
            //max
            int w0 = max(w1, max(w12, max(w13, max(h1, max(h2, max(h3, max(bei.w2, max(bei.w1, bei.w))))))));
            if(w0 == w1) crow->wis[j].Copy(prow->wis[j - 3]);
            else if(w0 == h3) crow->wis[j].Copy(crow->h3is[j]);
            else if(w0 == h1) crow->wis[j].Copy(crow->h1is[j]);
            else if(w0 == h2) crow->wis[j].Copy(crow->h2is[j]);
            else if(w0 == w12) crow->wis[j].Copy(prow->wis[j - 1]);
            else if(w0 == w13) crow->wis[j].Copy(prow->wis[j - 2]);
            else if(w0 == bei.w1) {
                int len = fin.GetW1len(j, scoring);
                crow->wis[j].Expand(prow->wis[j - len - 3], j - len - 2, len);
            } else if(w0 == bei.w2) {
                int len = fin.GetW2len(j, scoring);
                crow->wis[j].Expand(prow->wis[j - len - 3], j - len - 1, len);
            } else { //w == bei.w
                int len = fin.GetWlen(j, scoring);
                crow->wis[j].Expand(crow->wis[j - len], j - len, len);
            }
            crow->w[j] = w0;
        }
        prow->ClearIIC();
        //remember the best W in the last column
        if(wmax <= crow->w[jlen - 1]) {
            wmax = crow->w[jlen - 1];
            imax = i;
            lsb.Clear();
            lsb.Copy(crow->wis[jlen - 1]);
        }
      }
    //at this point wmax - best from the last column
    //find best from the last row
    for(j=1;j<jlen;++j) {
        if(wmax <= crow->w[j]) {
            wmax = crow->w[j];
            imax = ilen - 1;
            jmax = j;
        }
    }
    igi.clear();
    CIgapIntron *tmp;
    if(jmax<jlen-1) {//there is an end gap
        right_gap = true;
        igi.push_back(make_pair(jmax, jlen - jmax - 1));
        tmp = crow->wis[jmax].m_Top;
    } else tmp = lsb.m_Top;
     while(tmp) {
        if(tmp->m_Len > 0) igi.push_back(make_pair(tmp->m_Beg, tmp->m_Len));
        else left_gap = true;
        tmp = tmp->m_Prev;
    }
    reverse(igi.begin(), igi.end());
    if(left_gap) _ASSERT(!igi.empty() && igi.front().first == 0);
    crow->ClearIIC();
    lsb.Clear();
    return wmax;
}

int FindIGapIntrons(vector<pair<int, int> >& igi/*to return end gap/intron set*/, const PSEQ& pseq, const CNSeq& nseq, int g/*gap opening*/, int e/*one nuc extension cost*/,
                    int f/*frameshift opening cost*/, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix)
{
    // in matrices letters starts at [1]

	CIgapIntronPool pool;
 
  int ilen = (int)pseq.size() + 1;
  int jlen = nseq.size() + 1;
  CAlignInfo row1(jlen, pool), row2(jlen, pool);
  CAlignInfo *crow = &row1, *prow = &row2;

  int i, j;
  //first row, i.e. i=0
    for(j=0;j<jlen;j++) {
      crow->w[j] = 0;
      crow->wis[j].Creat(0, j);
      crow->v[j] = infinity; 
      crow->fv[j] = infinity;
    }

  // *******  MAIN LOOP ******************
  for(i=1;i<ilen;i++) {
    swap(crow, prow);
    //do not need to init intron-gap chain where no end H-gap
    crow->w[0] = - g - i*e*3;
    //second column
    crow->w[1] = crow->v[1] = - g - i*e*3;
    crow->wis[1].Creat(0, 1);
    crow->vis[1].Copy(crow->wis[1]);
    crow->fv[1] = - f - (i*3 - 1)*e;
    //third column
    crow->w[2] = crow->v[2] = - g - i*e*3;
    crow->wis[2].Creat(0, 2);
    crow->vis[2].Copy(crow->wis[2]);
    crow->fv[2] = - f - (i*3 - 2)*e;

    crow->h[0] = crow->h[1] = crow->h[2] = infinity;
    crow->fh[0] = crow->fh[1] = crow->fh[2] = infinity;
    CBestIntron chin(/*i, */2, pseq[i-1], *prow, *crow, nseq, scoring);
    CHIntronScore spl101, spl102, spl103, spl104, spl105;
    CHIntronScore dspl101, dspl102, dspl103, dspl104;
	for(j=3;j<jlen;j++) {
            chin.NucStep(scoring, matrix);
	  int d1, d2, d3, d4, d5, d6;
      int h1, h2;
      int v1, v2;
      int fv1, fv2, fv3;
      int fh1, fh2, fh3;
      int d101, d102, d103, d104, h101, h102, h103, h104, h105, fv101, fv102, fh101, v101, s0;
	  int d0, v0, h0, fv0, fh0, w0; //maximum values

      dspl101 = chin.GetW1(matrix);
      d101 = dspl101.first;
      dspl102 = chin.GetW2();
      d102 = dspl102.first;
      dspl103 = chin.Getw111();
      d103 = dspl103.first -f - e;
      dspl104 = chin.Getfv111();
      d104 = dspl104.first - e;
	  d1 = prow->w[j-3] + matrix.MultScore(nseq[j-3], nseq[j-2], nseq[j-1], pseq[i-1]);
	  d2 = prow->w[j-1] - f - 2*e;
	  d3 = prow->fv[j-1] - 2*e;
      d4 = prow->v[j-1] - (f - g) - 2*e;
	  d5 = prow->w[j-2] - f - e;
	  d6 = prow->fv[j-2] - e;
	  d0 = max(d1, max(d2, max(d3, max(d4, max(d5, max(d6, max(d101, max(d102, max(d103, d104)))))))));

      spl101 = chin.Geth000();
      h101 = spl101.first;
      spl102 = chin.Getw012();
      h102 = spl102.first - g - 3*e;
      spl103 = chin.Geth012();
      h103 = spl103.first - 3*e;
      spl104 = chin.Getw021();
      h104 = spl104.first - g - 3*e;
      spl105 = chin.Geth021();
      h105 = spl105.first - 3*e;
	  h1 = crow->w[j-3] - g - 3*e;
	  h2 = crow->h[j-3] - 3*e;
	  h0 = max(h1, max(h2, max(h101, max(h102, max(h103, max(h104, h105))))));
      if(h0 == h1) crow->his[j].Copy(crow->wis[j-3]);
	  else if(h0 == h2) crow->his[j].Copy(crow->his[j-3]);
      else if(h0 == h101) crow->his[j].Expand(crow->his[j-spl101.second], j-spl101.second, spl101.second);
      else if(h0 == h102) crow->his[j].Expand(crow->wis[j-3-spl102.second], j-2-spl102.second, spl102.second);
      else if(h0 == h103) crow->his[j].Expand(crow->his[j-3-spl103.second], j-2-spl103.second, spl103.second);
      else if(h0 == h104) crow->his[j].Expand(crow->wis[j-3-spl104.second], j-1-spl104.second, spl104.second);
      else if(h0 == h105) crow->his[j].Expand(crow->his[j-3-spl105.second], j-1-spl105.second, spl105.second);
	  crow->h[j] = h0;

      spl101 = chin.Getfh000();
      fh101 = spl101.first;
	  fh1 = crow->w[j-1] - f - e;
	  fh2 = crow->fh[j-1] - e;
      fh3 = crow->h[j-1] - (f-g) - e;
	  fh0 = max(fh1, max(fh2, max(fh3, fh101)));
	  if(fh0 == fh1) crow->fhis[j].Copy(crow->wis[j-1]);
      else if(fh0 == fh2) crow->fhis[j].Copy(crow->fhis[j-1]);
	  else if(fh0 == fh3) crow->fhis[j].Copy(crow->his[j-1]);
      else crow->fhis[j].Expand(crow->fhis[j-spl101.second], j-spl101.second, spl101.second);
	  crow->fh[j] = fh0;

      spl101 = chin.Getfv000();
      fv101 = spl101.first;
      spl102 = chin.Getw111();
      fv102 = spl102.first - f - e;
      fv1 = prow->w[j-2] - f - e;
	  fv2 = prow->w[j-1] - f -2*e;
	  fv3 = prow->fv[j] - 3*e;
	  fv0 = max(fv1, max(fv2, max(fv3, max(fv101, fv102))));
      if(fv0 == fv1) crow->fvis[j].Copy(prow->wis[j-2]);
	  else if(fv0 == fv2) crow->fvis[j].Copy(prow->wis[j-1]);
	  else if(fv0 == fv3) crow->fvis[j].Copy(prow->fvis[j]);
      else if(fv0 == fv101) crow->fvis[j].Expand(crow->fvis[j-spl101.second], j-spl101.second, spl101.second);
      else if(fv0 == fv102) crow->fvis[j].Expand(prow->wis[j-2-spl102.second], j-1-spl102.second, spl102.second);
	  crow->fv[j] = fv0;

      spl101 = chin.Getv000();
      v101 = spl101.first;
	  v1 = prow->w[j] - g - 3*e;
	  v2 = prow->v[j] - 3*e;
      v0 = max(v1, max(v2, v101));
	  if(v0 == v1) crow->vis[j].Copy(prow->wis[j]);
	  else if(v0 == v2) crow->vis[j].Copy(prow->vis[j]);
      else if(v0 == v101) crow->vis[j].Expand(crow->vis[j-spl101.second], j-spl101.second, spl101.second);
	  crow->v[j] = v0;
	  
      spl101 = chin.Getw000();
      s0 = spl101.first;
      
      w0 = max(d0, max(v0, max(h0, max(fv0, max(fh0, s0)))));
      if(w0 == d0) {
          if(d0 == d1) crow->wis[j].Copy(prow->wis[j-3]);
	      else if(d0 == d2) crow->wis[j].Copy(prow->wis[j-1]);
	      else if(d0 == d3) crow->wis[j].Copy(prow->fvis[j-1]);
	      else if(d0 == d4) crow->wis[j].Copy(prow->vis[j-1]);
	      else if(d0 == d5) crow->wis[j].Copy(prow->wis[j-2]);
	      else if(d0 == d6) crow->wis[j].Copy(prow->fvis[j-2]);
          else if(d0 == d101) crow->wis[j].Expand(prow->wis[j-3-dspl101.second], j - 2 - dspl101.second, dspl101.second);
          else if(d0 == d102) crow->wis[j].Expand(prow->wis[j-3-dspl102.second], j - 1 - dspl102.second, dspl102.second);
          else if(d0 == d103) crow->wis[j].Expand(prow->wis[j-2-dspl103.second], j - 1 - dspl103.second, dspl103.second);
          else if(d0 == d104) crow->wis[j].Expand(prow->fvis[j-2-dspl104.second], j - 1 - dspl104.second, dspl104.second);
      }
      else if(w0 == h0) crow->wis[j].Copy(crow->his[j]);
	  else if(w0 == v0) crow->wis[j].Copy(crow->vis[j]);
	  else if(w0 == fv0) crow->wis[j].Copy(crow->fvis[j]);
	  else if(w0 == fh0) crow->wis[j].Copy(crow->fhis[j]);
      else crow->wis[j].Expand(crow->wis[j-spl101.second], j-spl101.second, spl101.second);
	  crow->w[j] = w0;

	}
    prow->ClearIIC();

  }
  //best from the last row
  int wmax = crow->w[0];
  int jmax = 0;
  for(j=1;j<jlen;++j) {
      if(wmax < crow->w[j]) {
          wmax = crow->w[j];
          jmax = j;
      }
  }
  igi.clear();
  if(jmax<jlen-1) igi.push_back(make_pair(jmax, jlen - jmax - 1));
  CIgapIntron *tmp = crow->wis[jmax].m_Top;
  while(tmp) {
      if(tmp->m_Len > 0) igi.push_back(make_pair(tmp->m_Beg, tmp->m_Len));
      tmp = tmp->m_Prev;
  }
  reverse(igi.begin(), igi.end());
  crow->ClearIIC();//not really needed, it will die by itself
  return wmax;
}

void FrBackAlign(CBackAlignInfo& bi, CAli& ali) {
  CAliCreator alic(ali);
  int i, j;
  for(i = bi.ilen-1; i>bi.maxi; --i) {
        alic.Add(eVP, 3);
  }
  for(j = bi.jlen-1; j>bi.maxj; --j) {
        alic.Add(eHP, 1);
  }
  int curGAPmode = eD;
  while(i>=0 && j>=0) {
	char b = bi.b[i][j];
    char h1mode = b&64;
    char vmode = b&32;
    char hmode = b&16;
    b &= 0xf;
    if(b == 12) {//handles new case (FrAlignNog() and FrAlignNog1()), does not affect old one (FrAlign())
        alic.Add(eVP, 2);
        alic.Add(eMP, 1);
        i -= 1;
        j -= 1;
        curGAPmode = eD;
    } else if(b == 13) {//handles new case (FrAlignNog() and FrAlignNog1()), does not affect old one (FrAlign())
        alic.Add(eVP, 1);
        alic.Add(eMP, 2);
        i -= 1;
        j -= 2;
        curGAPmode = eD;
    } else if( curGAPmode == eV || ( curGAPmode == eD && b == 10 ) ) {
        alic.Add(eVP, 3);
        i -= 1;
        if(vmode) curGAPmode = eV;
        else curGAPmode = eD;
    } else if(curGAPmode == eH || ( curGAPmode == eD && b == 11 ) ) {
        alic.Add(eHP, 3);
        j -= 3;
        if(hmode) curGAPmode = eH;
        else curGAPmode = eD;
    } else if(curGAPmode == eH3 || ( curGAPmode == eD && b == 15) ) {//FrAlignNog1() only
        alic.Add(eHP, 2);
        j -= 2;
        curGAPmode = eH1;
    } else if(curGAPmode == eH2 || ( curGAPmode == eD && b == 14) ) {//FrAlignNog1() only
        alic.Add(eHP, 1);
        j -= 1;
        curGAPmode = eH1;
    } else if(curGAPmode == eH1 || ( curGAPmode == eD && b == 0) ) {//FrAlignNog1() only
        alic.Add(eHP, 1);
        j -= 1;
        if(h1mode) curGAPmode = eH3;
        else curGAPmode = eD;
   } else {//eD mode
        if(b == 1) {
            alic.Add(eMP, 3);
            i -= 1;
            j -= 3;
        } else if( b == 2 || b == 3 ) {
            alic.Add(eMP, 1);
            alic.Add(eVP, 2);
            i -= 1;
            j -= 1;
        } else if( b == 4 || b == 5 ) {
            alic.Add(eMP, 2);
            alic.Add(eVP, 1);
            i -= 1;
            j -= 2;
        } else if( b == 6 || b == 7 ) {
            alic.Add(eHP, 1);
            j -= 1;
        } else if( b == 8 || b == 9 ) {
            alic.Add(eHP, 2);
            j -= 2;
        }
        else NCBI_THROW(CProSplignException, eBackAli, "wrong value for FR back alignment");
        if( b == 3 || b == 5 ) curGAPmode = eV;
        else if( b == 7 || b == 9 ) curGAPmode = eH;
    }
  }
  	for(int j1 = j; j1 >= 0; j1--) {//rest of nuc
      alic.Add(eHP, 1);
   }
	for(int i1 = i; i1>= 0 ; i1--) { //rest of amins 
      alic.Add(eVP, 3);
    }

  alic.Fini();
}



int   FrAlignFNog1(CBackAlignInfo& bi, const PSEQ& pseq, const CNSeq& nseq,
                   // int g/*gap opening*/, int e/*one nuc extension cost*/, int f/*frameshift opening cost*/,
                   const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix,
                   bool left_gap, bool right_gap)
{
    int g/*gap opening*/ = scoring.GetGapOpeningCost();
    int e/*one nuc extension cost*/ = scoring.GetGapExtensionCost();
    int f/*frameshift opening cost*/ = scoring.GetFrameshiftOpeningCost();
 
  if(nseq.size() < 1) return 0;
  int ilen = (int)pseq.size() + 1;
  int jlen = nseq.size() + 1;
  CFrAlignRow row1(jlen), row2(jlen);
  CFrAlignRow *crow = &row1, *prow = &row2;
  int i, j;
  CFastIScore fiscore;
  fiscore.Init(nseq, matrix);
  //first row, i.e. i=0
    for(j=0;j<jlen;j++) {
      crow->w[j] = 0;
      crow->v[j] = infinity; 
    }
    int wmax = 0;
    int imax = 0;
    int jmax = jlen - 1;
    // ** prepare for main loop
    //penalties
    int pev1 =  - g - 3*e;
    int pev2 = - 3*e;
    int pe2 =  - f - 2*e;
    int pe3 =  - (f - g) - 2*e;
    int pe4 =  - f - e;
    int pe5 =  - (f - g) - e;
    int pe6 = f - g - e;
    char *pb, bb;
    int *cv, *cw, *pv, *pv1, *pw1, *pw3;
  // *******  MAIN LOOP ******************
  int jlen_1;
  if(right_gap) {//penalty for end gap because there was a H-gap on the first stage
    jlen_1 = jlen;
  } else {
    jlen_1 = jlen - 1;
  }
  for(i=1;i<ilen;++i) {
    swap(crow, prow);
    if(left_gap) {//penalty for end gap because there was a H-gap on the first stage
        crow->w[0] = -g -3*e*i;
        crow->w[1] =  - f - (i*3 - 1)*e;
        crow->w[2] =   - f - (i*3 - 2)*e;
    } else {
        crow->w[0] = 0;
        crow->w[1] = 0;
        crow->w[2] = 0;
    }
    bi.b[i-1][0] = 3;
    bi.b[i-1][1] = 5;
    /**/
    crow->v[1] = crow->v[2]  = infinity;
    int h1 = infinity;
    int h2 = infinity;
    int h3 = infinity;
    pb = &bi.b[i-1][1];
    cv = &crow->v[2];
    cw = &crow->w[2];
    pw3 =  &prow->w[0];
    pw1 =  &prow->w[2];
    pv1 =  &prow->v[2];
    pv =  &prow->v[0];
    fiscore.SetAmin(pseq[i-1], matrix);
	for(j=3;j<jlen_1;++j) {
        bb = 0;
        //rest
        int w1 = *pw3 + fiscore.GetScore();
        int w2 = *pw1 + pe2;
        int w3 = *pv1 + pe3;
        int w4 = *++pw3 + pe4;
        int w5 = *++pv + pe5;
        //best v-gap
        int v0 = *++pw1 + pev1;
        int v2 = *++pv1 + pev2;
        if(v2 > v0) {
            v0 = v2;
            bb |= 32;
        } 
        *++cv = v0; 
        //best h-gap
        int h12 = h3 + pe5;
        h3 = h2 + pe6;
        h2 = h1 - e;
        h1 = *cw + pe4;
        if(h12 > h1) {
            h1 = h12;
            bb |= 64;
        } 
        int w0 = max(w1, max(w2, max(w3, max(w4, max(w5, max(h1, max(h2, max(h3, v0))))))));
        if(w0 == w1) bb += 1;
        else if(w0 == v0) bb += 10;
        else if(w0 == h3) bb += 15;
        else if(w0 == h2) bb += 14;
        else if(w0 == w2) bb += 2;
        else if(w0 == w3) bb += 3;
        else if(w0 == w4) bb += 4;
        else if(w0 == w5) bb += 5;
        //default w=h1, b += 0;
        *++cw = w0;
        *++pb = bb;
    }
    //the last column !
    if(!right_gap) {//last column is different if we don't charge for end gap
        if(2 < jlen_1) { //j == jlen_1 
            _ASSERT(j == jlen_1);
            char& b = bi.b[i-1][j-1];
            b = 0;
            //v-gap is not needed
            //best h-gap
            int h12 = h3 + pe5;
            h3 = h2 + pe6;
            h2 = h1 - e;
            h1 = crow->w[j-1] + pe4;
            if(h12 > h1) {
                h1 = h12;
                b |= 64;
            }
            //rest
            int w1 = prow->w[j-3] + matrix.MultScore(nseq[j-3], nseq[j-2], nseq[j-1], pseq[i-1]);
            int w12 = prow->w[j-1];
            int w13 = prow->w[j-2];
            int w0 = max(w1, max(w12, max(w13, max(h1, max(h2, h3)))));
            if(w0 == w1) b += 1;
            else if(w0 == w12) b += 12;
            else if(w0 == w13) b += 13;
            else if(w0 == h2) b += 14;
            else if(w0 == h3) b += 15;
            //default w=h1, b += 0;
            crow->w[j] = w0;
        }
        //remember the best W in the last column
        if(wmax <= crow->w[jlen - 1]) {
            wmax = crow->w[jlen - 1];
            imax = i;
        }
    } else {//regular score for the right v-gap
            wmax = crow->w[jlen - 1];
            imax = i;
            jmax = jlen - 1;
    }
  }
  //at this point wmax - best from the last column
  //find best from the last row
  for(j=1;j<jlen;++j) {
      if(wmax <= crow->w[j]) {
          wmax = crow->w[j];
          imax = ilen - 1;
          jmax = j;
      }
  }
  bi.maxi = imax - 1;//shift to back align coord
  bi.maxj = jmax - 1;//shift to back align coord
  return wmax;
}


int AlignFNog(CTBackAlignInfo<CBMode>& bi, const PSEQ& pseq, const CNSeq& nseq, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix)
{
  if(nseq.size() < 1) return 0;
  int ilen = (int)pseq.size() + 1;
  int jlen = nseq.size() + 1;
  CAlignRow row1(jlen, scoring), row2(jlen, scoring);
  CAlignRow *crow = &row1, *prow = &row2;
  int i, j;
  CFastIScore fiscore;
  fiscore.Init(nseq, matrix);
  CFIntron fin(nseq, scoring);
  //first row, i.e. i=0
    for(j=0;j<jlen;j++) {
      crow->w[j] = 0;
      crow->v[j] = infinity; 
    }
    int wmax = 0;
    int imax = 0;
    int jmax = jlen - 1;
  // ** prepare for main loop
  //penalties
    //    CScoring::Init();
    int e = scoring.sm_Ine;
    int g = scoring.sm_Ig;
    int f = scoring.sm_If;
  int pev1 =  - g - 3*e;
  int pev2 = - 3*e;
  int pe2 =  - f - 2*e;
  int pe3 =  - (f - g) - 2*e;
  int pe4 =  - f - e;
  int pe5 =  - (f - g) - e;
  int pe6 =  f - g - e;
  int *cv, *cw, *pv, *pv1, *pw1, *pw3, *ch1, *ch2, *ch3;
  // *******  MAIN LOOP ******************
  for(i=1;i<ilen;++i) {
    swap(crow, prow);
    //set first few columns
    crow->w[0] = 0;
    crow->w[1] = 0;
    bi.b[i-1][0].wmode = 4;
    crow->w[2] = 0;
    bi.b[i-1][1].wmode = 6;
    crow->v[1] = crow->v[2]  = infinity;
    int h1 = infinity;
    int h2 = infinity;
    int h3 = infinity;
   //extra init for intron scoring
    fin.InitRowScores(crow, prow->m_w, 3);
   // pointers
    CBMode *pb = &bi.b[i-1][2];
    cv = &crow->v[2];
    ch1 = &crow->h1[2];
    ch2 = &crow->h2[2];
    ch3 = &crow->h3[2];
    cw = &crow->w[2];
    pw3 =  &prow->w[0];
    pw1 =  &prow->w[2];
    pv1 =  &prow->v[2];
    pv =  &prow->v[0];
    fiscore.SetAmin(pseq[i-1], matrix);
    int jlen_1 = jlen - 1;
  // *******  INTERNAL LOOP ******************
	for(j=3;j<jlen_1;++j) {
        int bb = 0;
        const CBestI& bei = fin.Step(j, scoring, fiscore);
        //rest
        int w1 = *pw3 + fiscore.GetScore();
        int w2 = *pw1 + pe2;
        int w3 = *pv1 + pe3;
        int w4 = *++pw3 + pe4;
        int w5 = *++pv + pe5;
        //best v-gap
        int v0 = *++pw1 + pev1;
        int v2 = *++pv1 + pev2;
        if(bei.v > v0 && bei.v > v2) {
            v0 = bei.v;
            pb->vlen = fin.GetVlen(j, scoring);
            bb |= 32;
        } else if(v2 > v0) {
            v0 = v2;
            bb |= 512;
        } 
        *++cv = v0; 
        //best h-gap
        int h12 = h3 + pe5;
        h3 = h2 + pe6;
        if(bei.h3 > h3) {
            h3 = bei.h3;
            pb->h3len = fin.GetH3len(j, scoring);
            bb |= 256;
        }
        h2 = h1 - e;
        if(bei.h2 > h2) {
            h2 = bei.h2;
            pb->h2len = fin.GetH2len(j, scoring);
            bb |= 128;
        }
        h1 = *cw + pe4;
        if(bei.h1 > h1 && bei.h1 > h12) {
            h1 = bei.h1;
            pb->h1len = fin.GetH1len(j, scoring);
            bb |= 64;
        } else if(h12 > h1) {
            h1 = h12;
            bb |= 1024;
        }
        *++ch1 = h1;
        *++ch2 = h2;
        *++ch3 = h3;
        //max
        int w0 = max(w1, max(w2, max(w3, max(w4, max(w5, max(h1, max(h2, max(h3, max(v0, max(bei.w2, max(bei.w1, bei.w)))))))))));
        if(w0 == w1) bb += 3;
        else if(w0 == v0) bb += 1;
        else if(w0 == h3) bb += 11;
        else if(w0 == h1) bb += 8;
        else if(w0 == h2) bb += 10;
        else if(w0 == w2) bb += 4;
        else if(w0 == w3) bb += 5;
        else if(w0 == w4) bb += 6;
        else if(w0 == w5) bb += 7;
        else if(w0 == bei.w1) {
            bb += 21;
            pb->wlen = fin.GetW1len(j, scoring);
        } else if(w0 == bei.w2) {
            bb += 22;
            pb->wlen = fin.GetW2len(j, scoring);
        } else { //w == bei.w
            bb += 20;
            pb->wlen = fin.GetWlen(j, scoring);
        }
        *++cw = w0;
        (pb++)->wmode = bb;
    }
    //the last column !
    if(2 < jlen_1) { //j == jlen_1 
        int& bb = bi.b[i-1][j-1].wmode;
        bb = 0;
        const CBestI bei = fin.Step(j, scoring, fiscore);
        //rest
        int w1 = *pw3 + fiscore.GetScore();
        int w12 = prow->w[j-1];
        int w13 = prow->w[j-2];
        //best h-gap
        int h12 = h3 + pe5;
        h3 = h2 + pe6;
        if(bei.h3 > h3) {
            h3 = bei.h3;
            pb->h3len = fin.GetH3len(j, scoring);
            bb |= 256;
        }
        h2 = h1 - e;
        if(bei.h2 > h2) {
            h2 = bei.h2;
            pb->h2len = fin.GetH2len(j, scoring);
            bb |= 128;
        }
        h1 = *cw + pe4;
        if(bei.h1 > h1 && bei.h1 > h12) {
            h1 = bei.h1;
            pb->h1len = fin.GetH1len(j, scoring);
            bb |= 64;
        } else if(h12 > h1) {
            h1 = h12;
            bb |= 1024;
        }
        //max
        int w0 = max(w1, max(w12, max(w13, max(h1, max(h2, max(h3, max(bei.w2, max(bei.w1, bei.w))))))));
        if(w0 == w1) bb += 3;
        else if(w0 == h3) bb += 11;
        else if(w0 == h1) bb += 8;
        else if(w0 == h2) bb += 10;
        else if(w0 == w12) bb += 12;
        else if(w0 == w13) bb += 13;
        else if(w0 == bei.w1) {
            bb += 21;
            pb->wlen = fin.GetW1len(j, scoring);
        } else if(w0 == bei.w2) {
            bb += 22;
            pb->wlen = fin.GetW2len(j, scoring);
        } else { //w == bei.w
            bb += 20;
            pb->wlen = fin.GetWlen(j, scoring);
        }
        crow->w[j] = w0;
    }
    //remember the best W in the last column
    if(wmax <= crow->w[jlen - 1]) {
        wmax = crow->w[jlen - 1];
        imax = i;
    }
  }
  //at this point wmax - best from the last column
  //find best from the last row
  for(j=1;j<jlen;++j) {
      if(wmax <= crow->w[j]) {
          wmax = crow->w[j];
          imax = ilen - 1;
          jmax = j;
      }
  }
  bi.maxi = imax - 1;//shift to back align coord
  bi.maxj = jmax - 1;//shift to back align coord
  return wmax;
}

void BackAlignNog(CTBackAlignInfo<CBMode>& bi, CAli& ali)
{
  CAliCreator alic(ali);
  int i, j;
  for(i = bi.ilen-1; i>bi.maxi; --i) {
        alic.Add(eVP, 3);
  }
  for(j = bi.jlen-1; j>bi.maxj; --j) {
        alic.Add(eHP, 1);
  }
  int curGAPmode = eD;
  int vs, h1s, h2s, h3s, vmode, h1mode, wm;
  while(i>=0 && j>=0) {
    CBMode bm = bi.b[i][j];
    wm = bm.wmode;
    vs = wm&32;
    h1s = wm&64;
    h2s = wm&128;
    h3s = wm&256;
    vmode = wm&512;
    h1mode = wm&1024;
    wm &= 31;
    if(curGAPmode == eV || (curGAPmode == eD && wm == 1)) {
        if(vs) {//v-splice
            alic.Add(eSP, bm.vlen);
            j -= bm.vlen;
            curGAPmode = eV;
        } else {
            alic.Add(eVP, 3);
            i -= 1;
            if(vmode) curGAPmode = eV;
           else curGAPmode = eD;
        }
    } else if(curGAPmode == eH1 || (curGAPmode == eD && wm == 8)) {
        if(h1s) {//h1-splice
            alic.Add(eSP, bm.h1len);
            j -= bm.h1len;
            curGAPmode = eH1;
        } else {
            alic.Add(eHP, 1);
            j -= 1;
            if(h1mode) curGAPmode = eH3;
            else curGAPmode = eD;
        }
    } else if(curGAPmode == eH2 || (curGAPmode == eD && wm == 10)) {
        if(h2s) {//h2-splice
            alic.Add(eSP, bm.h2len);
            j -= bm.h2len;
            curGAPmode = eH2;
        } else {
            alic.Add(eHP, 1);
            j -= 1;
            curGAPmode = eH1;
        }
    } else if(curGAPmode == eH3 || (curGAPmode == eD && wm == 11)) {
        if(h3s) {//h3-splice
            alic.Add(eSP, bm.h3len);
            j -= bm.h3len;
            curGAPmode = eH3;
        } else {
            alic.Add(eHP, 1);
            j -= 1;
            curGAPmode = eH2;
        }
    } else if(curGAPmode == eD && ((wm > 2 && wm < 8) 
              || wm == 20 || wm == 21 || wm == 22 || wm == 12 || wm == 13)) {
        if(wm == 20) {
            alic.Add(eSP, bm.wlen);
            j -= bm.wlen;
        } else if(wm == 21) {
            alic.Add(eMP, 2);
            alic.Add(eSP, bm.wlen);
            alic.Add(eMP, 1);
            j -= 3;
            j -= bm.wlen;
            i -= 1;
        } else if(wm == 22) {
            alic.Add(eMP, 1);
            alic.Add(eSP, bm.wlen);
            alic.Add(eMP, 2);
            j -= 3;
            j -= bm.wlen;
            i -= 1;
        } else if(wm == 3) {
            alic.Add(eMP, 3);
            j -= 3;
            i -= 1;
        } else if(wm == 4 || wm == 5) {
            alic.Add(eMP, 1);
            alic.Add(eVP, 2);
            i -= 1;
            j -= 1;
        } else if(wm == 6 || wm == 7) {
            alic.Add(eMP, 2);
            alic.Add(eVP, 1);
            i -= 1;
            j -= 2;
        } else if(wm == 12) {//right end
            alic.Add(eVP, 2);
            alic.Add(eMP, 1);
            i -= 1;
            j -= 1;
        } else if(wm == 13) {//right end
            alic.Add(eVP, 1);
            alic.Add(eMP, 2);
            i -= 1;
            j -= 2;
        }
        if(wm == 5 || wm == 7) curGAPmode = eV;
    } else NCBI_THROW(CProSplignException, eBackAli, "wrong value for 1 stage back alignment");
  }
  if(( j!= -1 && i != -1) || j < -1 || i < -1) NCBI_THROW(CProSplignException, eBackAli, "wrong data for 1 stage back alignment");
  for(int j1 = j; j1 >= 0; j1--) {//rest of nuc
      alic.Add(eHP, 1);
  }
  for(int i1 = i; i1>= 0 ; i1--) { //rest of amins 
      alic.Add(eVP, 3);
  }

  alic.Fini();
}


END_SCOPE(prosplign)
END_NCBI_SCOPE
