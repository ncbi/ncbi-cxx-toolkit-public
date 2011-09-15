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


#ifndef INTRON_HPP
#define INTRON_HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidbg.hpp>

#include <utility>
#include <vector>
#include <fstream>

#include <objtools/alnmgr/nucprot.hpp>

#include "AlignInfo.hpp"
#include "nucprot.hpp"
#include "NSeq.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

extern const int infinity;

class CHIntronScore : public pair<int, int> { //score, length / donor position
public:
    CHIntronScore(int score = infinity) {
        first = score;
    }
};

class CAnyIntron {
protected:
    class CSIntronScore : public CHIntronScore {
        int jsc;//where count score
        const int *sc;
    public:
        void Init(const vector<int>& sco, int j, const CProSplignScaledScoring& scoring, int shl = 0, int shr = 0) {
            sc = &sco.front();
            jsc = j - shl - shr - scoring.lmin;
        }

        inline CSIntronScore& increment(const CProSplignScaledScoring& scoring) {
            jsc++;
            first -= scoring.ie;
            return *this;
        }

        inline void AddDon(const CProSplignScaledScoring& scoring) {
            if(jsc >= scoring.ini_nuc_margin) {//require at least ini_nuc_margin nuc. before splice in this version
//            if(jsc >=0) {
                int score = sc[jsc];
                if(score > first) {
                    first = score;
                    second = jsc;
                }
            }
        }
    };

    CHIntronScore swa, swt, swg, swc, swn;
    CHIntronScore sea, set, seg, sec, sen;
    const int *esc;//where to get score for swa-sen
    int /*i, */j;//current position
    char amin;//should be pseq[i-1];
    const CNSeq& nseq;

public:
    //same coord for donor and acceptor in one group
    //group 2
    CSIntronScore sw012, sh012;
    //group 1
    CSIntronScore sfv111, sw111, sw021, sh021;
    //group 0
     CSIntronScore sw000, sh000, sv000, sfv000, sfh000;

#define GET0(fn,sn) \
     inline CHIntronScore fn() { \
        CHIntronScore res(sn); \
        res.second = j - res.second; \
        return res; \
     }

GET0(Getw000,sw000)
GET0(Getv000,sv000)
GET0(Geth000,sh000)
GET0(Getfh000,sfh000)
GET0(Getfv000,sfv000)

#define GETANY(fn,sn, st) \
     inline CHIntronScore fn() { \
        CHIntronScore res(sn); \
        res.second = j - res.second - st; \
        return res; \
     }

GETANY(Getw111,sw111,2)
GETANY(Getfv111,sfv111,2)
GETANY(Getw012,sw012,3)
GETANY(Geth012,sh012,3)
GETANY(Getw021,sw021,3)
GETANY(Geth021,sh021,3)




    CAnyIntron(/*int i1, */int j1, char amin1, const CAlignInfo& prev,  const CAlignInfo& cur, const CNSeq& nseq_ori, const CProSplignScaledScoring& scoring)
  :  nseq(nseq_ori)
    {
        //i=i1;
        j=j1;
        amin=amin1;

        esc = &prev.w[0];

        sw111.Init(prev.w,j,scoring,1,1);
        sfv111.Init(prev.fv,j,scoring,1,1);
        sw000.Init(cur.w,j,scoring);
        sh000.Init(cur.h,j,scoring);
        sv000.Init(cur.v,j,scoring);
        sfh000.Init(cur.fh,j,scoring);
        sfv000.Init(cur.fv,j,scoring);
        sw012.Init(cur.w,j,scoring,1,2);
        sh012.Init(cur.h,j,scoring,1,2);
        sw021.Init(cur.w,j,scoring,2,1);
        sh021.Init(cur.h,j,scoring,2,1);
    }

    void SimpleNucStep(const CProSplignScaledScoring scoring);
    void NucStep(const CProSplignScaledScoring scoring, const CSubstMatrix& matrix);
    void AddW1(const CProSplignScaledScoring scoring);
    void AddW2(const CProSplignScaledScoring scoring, const CSubstMatrix& matrix);
            
    inline CHIntronScore GetW1(const CSubstMatrix& matrix) {
        CHIntronScore res(swa);
        res.first += matrix.MultScore(nA, nseq[j-2], nseq[j-1], amin);
        CHIntronScore tmp(swc);
        tmp.first += matrix.MultScore(nC, nseq[j-2], nseq[j-1], amin);
        if(tmp.first > res.first) res = tmp;
        tmp = swg;
        tmp.first += matrix.MultScore(nG, nseq[j-2], nseq[j-1], amin);
        if(tmp.first > res.first) res = tmp;
        tmp = swt;
        tmp.first += matrix.MultScore(nT, nseq[j-2], nseq[j-1], amin);
        if(tmp.first > res.first) res = tmp;
        tmp = swn;
        tmp.first += matrix.MultScore(nN, nseq[j-2], nseq[j-1], amin);
        if(tmp.first > res.first) res = tmp;

        res.second = j - 2 - res.second;
        return res;
    }
 
    inline CHIntronScore GetW2() {
            switch (nseq[j-1]) {
                case nA: 
                    {
                    CHIntronScore res(sea);
                    res.second = j - 1 - res.second;
                    return res;
                    }
                    break;
                case nC:
                    {
                    CHIntronScore res(sec);
                    res.second = j - 1 - res.second;
                    return res;
                    }
                    break;
                case nG:
                    {
                    CHIntronScore res(seg);
                    res.second = j - 1 - res.second;
                    return res;
                    }
                    break;
                case nT:
                    {
                    CHIntronScore res(set);
                    res.second = j - 1 - res.second;
                    return res;
                    }
                    break;
                default:
                    break;
            }
               // default:
            CHIntronScore res(sen);
            res.second = j - 1 - res.second;
            return res;
    }

};


class CIntron : public CAnyIntron {
public:
    //group 2
        int donj2;//where to check if donor
        int acsj2;//where to check if acseptor
    //group 1
        int donj1;//where to check if donor
        int acsj1;//where to check if acseptor

    //group 0
        int donj0;//where to check if donor
        int acsj0;//where to check if acseptor

char don1, don2;//donor letters
char acs1, acs2;//acsessor letters

inline bool IsDon(int jn/*nucleotide pos*/) {
        if(jn<0) return false;
        if(nseq[jn] == don1 && nseq[jn+1] == don2) return true;
        return false;
}

inline bool NotAcs(int jn/*first nucl after intron*/) {
        if(jn<2) return true;
        if(nseq[jn-2] == acs1 && nseq[jn-1] == acs2) return false;
        return true;
}

inline void SimpleNucStep(const CProSplignScaledScoring scoring) {
    CAnyIntron::SimpleNucStep(scoring);
    donj0++;
    acsj0++;
    donj1++;
    acsj1++;
    donj2++;
    acsj2++;
}
inline void NucStep(const CProSplignScaledScoring scoring, const CSubstMatrix& matrix)
{
    SimpleNucStep(scoring);
    if(j - scoring.lmin - 3 >= scoring.ini_nuc_margin) {
        if(IsDon(j-scoring.lmin-2)) {//new donor
            AddW1(scoring);
        }
        if(IsDon(j-scoring.lmin-1)) {//new donor
            AddW2(scoring, matrix);
        }
    }//end j > lmin +3
    if(IsDon(donj0)) {
        sw000.AddDon(scoring);
        sh000.AddDon(scoring);
        sv000.AddDon(scoring);
        sfh000.AddDon(scoring);
        sfv000.AddDon(scoring);
    }
    if(IsDon(donj1)) {
        sw111.AddDon(scoring);
        sfv111.AddDon(scoring);
        sw021.AddDon(scoring);
        sh021.AddDon(scoring);
    }
    if(IsDon(donj2)) {
        sw012.AddDon(scoring);
        sh012.AddDon(scoring);
    }

}           

#define MGET(fn,acs) \
    inline CHIntronScore fn() { \
            if(NotAcs(acs)) return CHIntronScore(); \
            return CAnyIntron::fn(); \
    }

MGET(Getw000,acsj0)
MGET(Geth000,acsj0)
MGET(Getv000,acsj0)
MGET(Getfv000,acsj0)
MGET(Getfh000,acsj0)
MGET(Getw111,acsj1)
MGET(Getfv111,acsj1)
MGET(Getw021,acsj1)
MGET(Geth021,acsj1)
MGET(Getw012,acsj2)
MGET(Geth012,acsj2)
//MGET(GetW1,j-2)
inline CHIntronScore GetW1(const CSubstMatrix& matrix) {
    if(NotAcs(j-2)) return CHIntronScore();
    return CAnyIntron::GetW1(matrix);
}

MGET(GetW2,j-1)

    CIntron(/*int i,*/ int j, char amin, char don11, char don21, char acs11, char acs21, const CAlignInfo& prev,  const CAlignInfo& cur, const CNSeq& nseq, const CProSplignScaledScoring& scoring)
            : CAnyIntron(/*i, */j, amin, prev, cur, nseq, scoring) {
            don1 = CTranslationTable::CharToNuc(don11);
            don2 = CTranslationTable::CharToNuc(don21);
            acs1 = CTranslationTable::CharToNuc(acs11);
            acs2 = CTranslationTable::CharToNuc(acs21);
            donj0 = j-scoring.lmin;
            donj1 = donj0 - 1;
            donj2 = donj1 - 1;
            acsj0 = j;
            acsj1 = acsj0 - 1;
            acsj2 = acsj1 - 1;
        }
};

class CBestIntron {
public:
    CIntron gt, gc, at;
    CAnyIntron an;
    int gtcost, gccost, atcost, anycost;//cost of intron of minimum length
 
    CBestIntron(/*int i, */int j, char amin, const CAlignInfo& prev,  const CAlignInfo& cur, const CNSeq& nseq, const CProSplignScaledScoring& scoring) : 
        gt(/*i, */j, amin, 'G', 'T', 'A', 'G', prev, cur, nseq, scoring), 
      gc(/*i, */j, amin, 'G', 'C', 'A', 'G', prev, cur, nseq, scoring), 
      at(/*i, */j, amin, 'A', 'T', 'A', 'C', prev, cur, nseq, scoring),
      an(/*i, */j, amin, prev, cur, nseq, scoring) {
          gtcost = scoring.sm_ICGT + scoring.lmin*scoring.ie;
        gccost = scoring.sm_ICGC + scoring.lmin*scoring.ie;
        atcost = scoring.sm_ICAT + scoring.lmin*scoring.ie;
        anycost = scoring.sm_ICANY + scoring.lmin*scoring.ie;
      }

      inline void NucStep(const CProSplignScaledScoring scoring, const CSubstMatrix& matrix) {
          gt.NucStep(scoring, matrix);
          gc.NucStep(scoring, matrix);
          at.NucStep(scoring, matrix);
          an.NucStep(scoring, matrix);
      }

    inline CHIntronScore GetBest(CHIntronScore sgt, CHIntronScore sgc, CHIntronScore sat, CHIntronScore sany) {
        sgt.first -= gtcost;
        sgc.first -= gccost;
        sat.first -= atcost;
        sany.first -= anycost;
        if(sgt.first > sgc.first) {
            if(sat.first > sany.first) {
                if(sgt.first > sat.first) return sgt;
                else return sat;
            } 
            if(sgt.first > sany.first) return sgt;
            return sany;
        }
        if(sat.first > sany.first) {
            if(sgc.first > sat.first) return sgc;
            return sat;
        } 
        if(sgc.first > sany.first) return sgc;
        return sany;
    }

#define FUNC(name) \
    inline CHIntronScore name() { return GetBest(gt.name(), gc.name(), at.name(), an.name()); }

//FUNC(GetW1)
    inline CHIntronScore GetW1(const CSubstMatrix& matrix) { return GetBest(gt.GetW1(matrix), gc.GetW1(matrix), at.GetW1(matrix), an.GetW1(matrix)); }

FUNC(GetW2)
FUNC(Getw111)
FUNC(Getfv111)
FUNC(Getw012)
FUNC(Geth012)
FUNC(Getw021)
FUNC(Geth021)
FUNC(Geth000)
FUNC(Getv000)
FUNC(Getw000)
FUNC(Getfh000)
FUNC(Getfv000)
};


// ***  FAST INTRON IMPLEMENTATION

/*
struct CBestI 
{
  int m_v, m_h1, m_h2, m_h3, m_w1, m_w2, m_w;
};
*/

struct CBestI 
{
  int v, h1, h2, h3, w1, w2, w;
};

enum EDonType
{
    eGT,
    eGC,
    eAT,
    eANY
};

enum EAccType
{
    eAG,
    eAC,
    eANYa
};

class CFIntornData
{
public:
    EDonType m_dt, m_dt1, m_dt2;
    EAccType m_at, m_at1, m_at2;
    Nucleotides m_1nuc, m_2nuc;
    //int m_w1s[5];//match scores for w1-splice
    //int m_w2s[5];//match scores for w2-splice
};

class CFIntronDon
{
public:
    CHIntronScore m_v, m_h1, m_h2, m_h3, m_w;
    CHIntronScore m_w1[5];//in Nucleotides order, i.e m_w1[nA] - score for A
    CHIntronScore m_w2[5];//same order

    void Reset(void);
};

class CFIntron
{
private:
    CBestI m_bei;
    CFIntronDon m_gt, m_gc, m_at, m_any;
    int m_w1s[5], m_w2s[5];//match scores
    vector<CFIntornData> m_data;
    CFIntornData *m_cd;//current pointer inside m_data
    int *m_v, *m_h1, *m_h2, *m_h3, *m_w, *m_w12;//where to get score
    const CNSeq& m_nseq;

    CFIntron& operator=(const CFIntron&);
    CFIntron(const CFIntron&);

    //initialization methods
    inline static EDonType GetDonType(char nuc1, char nuc2) {
        if(nuc1 == nG) {
            if(nuc2 == nT) return eGT;
            if(nuc2 == nC) return eGC;
        } else if(nuc1 == nA && nuc2 == nT) return eAT;
        return eANY;
    }
    inline static EAccType GetAccType(char nuc1, char nuc2) {
        if(nuc1 == nA) {
            if(nuc2 == nG) return eAG;
            if(nuc2 == nC) return eAC;
        }
        return eANYa;
    }
    inline void InitW12s(int j, const CProSplignScaledScoring& scoring, const CFastIScore& fiscore) {
        j -= scoring.lmin + 3;
        if(j<0) return;
        char nuc1 = m_nseq[j];
        char nuc2 = m_nseq[++j];
        m_w2s[nA] = fiscore.GetScore(nuc1, nuc2, nA);
        m_w2s[nT] = fiscore.GetScore(nuc1, nuc2, nT);
        m_w2s[nG] = fiscore.GetScore(nuc1, nuc2, nG);
        m_w2s[nC] = fiscore.GetScore(nuc1, nuc2, nC);
        m_w2s[nN] = fiscore.GetScore(nuc1, nuc2, nN);
        nuc1 = m_nseq[j+=scoring.lmin];
        nuc2 = m_nseq[++j];
        m_w1s[nA] = fiscore.GetScore(nA, nuc1, nuc2);
        m_w1s[nT] = fiscore.GetScore(nT, nuc1, nuc2);
        m_w1s[nG] = fiscore.GetScore(nG, nuc1, nuc2);
        m_w1s[nC] = fiscore.GetScore(nC, nuc1, nuc2);
        m_w1s[nN] = fiscore.GetScore(nN, nuc1, nuc2);
    }
    // Acceptor methods
    inline void BestScAny(const CProSplignScaledScoring& scoring) {
        m_bei.w = m_any.m_w.first - scoring.sm_ICANY;
        m_bei.v = m_any.m_v.first - scoring.sm_ICANY;
        m_bei.h1 = m_any.m_h1.first - scoring.sm_ICANY;
        m_bei.h2 = m_any.m_h2.first - scoring.sm_ICANY;
        m_bei.h3 = m_any.m_h3.first - scoring.sm_ICANY;
    }
    inline void BestScCon(const CFIntronDon& don, int cost, int j, const CProSplignScaledScoring& scoring) {
        int tmp = don.m_w.first - cost - scoring.ie*(j - don.m_w.second);
        if(tmp > m_bei.w) m_bei.w = tmp;
        tmp = don.m_v.first - cost - scoring.ie*(j - don.m_v.second);
        if(tmp > m_bei.v) m_bei.v = tmp;
        tmp = don.m_h1.first - cost - scoring.ie*(j - don.m_h1.second);
        if(tmp > m_bei.h1) m_bei.h1 = tmp;
        tmp = don.m_h2.first - cost - scoring.ie*(j - don.m_h2.second);
        if(tmp > m_bei.h2) m_bei.h2 = tmp;
        tmp = don.m_h3.first - cost - scoring.ie*(j - don.m_h3.second);
        if(tmp > m_bei.h3) m_bei.h3 = tmp;
    }
    inline int BestSc1(const CFIntronDon& don, int j, const CProSplignScaledScoring& scoring) const {
        const CHIntronScore *sp = don.m_w1;
        const int *ap = m_w1s;
        //int res = don.m_w1[nA].first + m_w1s[nA] - ie*(j - don.m_w1[nA].second);
        int res = sp->first + *(ap++) - scoring.ie*(j - sp->second);
        ++sp;
        int tmp = sp->first + *(ap++) - scoring.ie*(j - sp->second);
        if(tmp > res) res = tmp;
        ++sp;
        tmp = sp->first + *(ap++) - scoring.ie*(j - sp->second);
        if(tmp > res) res = tmp;
        ++sp;
        tmp = sp->first + *(ap++) - scoring.ie*(j - sp->second);
        if(tmp > res) res = tmp;
        ++sp;
        tmp = sp->first + *(ap++) - scoring.ie*(j - sp->second);
        if(tmp > res) res = tmp;
        return res;
    }
    inline int GetLen1(int sc, const CFIntronDon& don, int j, const CProSplignScaledScoring& scoring) const {
        const CHIntronScore *sp = don.m_w1;
        const int *ap = m_w1s;
        if(sc == sp->first + *(ap++) - scoring.ie*(j - sp->second + scoring.lmin)) return j - sp->second + scoring.lmin;
        ++sp;
        if(sc == sp->first + *(ap++) - scoring.ie*(j - sp->second + scoring.lmin)) return j - sp->second + scoring.lmin;
        ++sp;
        if(sc == sp->first + *(ap++) - scoring.ie*(j - sp->second + scoring.lmin)) return j - sp->second + scoring.lmin;
        ++sp;
        if(sc == sp->first + *(ap++) - scoring.ie*(j - sp->second + scoring.lmin)) return j - sp->second + scoring.lmin;
        ++sp;
        if(sc == sp->first + *(ap++) - scoring.ie*(j - sp->second + scoring.lmin)) return j - sp->second + scoring.lmin;
        return 0;
    }
    //Donor methods
    inline static void AddDonAny(int sc, CHIntronScore& bsc, int j, const CProSplignScaledScoring& scoring) {
        if(sc > (bsc.first -= scoring.ie)) {
            bsc.first = sc;
            bsc.second = j;
        }
    }
    inline static void AddDonCon(int sc, CHIntronScore& bsc, int j, const CProSplignScaledScoring& scoring) {
        if(sc > (bsc.first - scoring.ie*(j-bsc.second))) {
            bsc.first = sc;
            bsc.second = j;
        }
    }
    inline void AddDon1(CFIntronDon& don, Nucleotides& nuc, int j, const CProSplignScaledScoring& scoring) {
        AddDonCon(*++m_w12, m_any.m_w1[nuc], j, scoring);
        AddDonCon(*m_w12, don.m_w1[nuc], j, scoring);
    }
    inline void AddDon1(Nucleotides& nuc, int j, const CProSplignScaledScoring& scoring) {
        AddDonCon(*++m_w12, m_any.m_w1[nuc], j, scoring);
    }
    inline void AddDon2(int j, const CProSplignScaledScoring& scoring) {
        int sc = *m_w12;
        AddDonAny(sc + m_w2s[nA], m_any.m_w2[nA], j, scoring);
        AddDonAny(sc + m_w2s[nC], m_any.m_w2[nC], j, scoring);
        AddDonAny(sc + m_w2s[nG], m_any.m_w2[nG], j, scoring);
        AddDonAny(sc + m_w2s[nT], m_any.m_w2[nT], j, scoring);
        AddDonAny(sc + m_w2s[nN], m_any.m_w2[nN], j, scoring);
    }
    inline void AddDon2(CFIntronDon& don, int j, const CProSplignScaledScoring& scoring) {
        int sc = *m_w12;
        AddDonAny(sc + m_w2s[nA], m_any.m_w2[nA], j, scoring);
        AddDonAny(sc + m_w2s[nC], m_any.m_w2[nC], j, scoring);
        AddDonAny(sc + m_w2s[nG], m_any.m_w2[nG], j, scoring);
        AddDonAny(sc + m_w2s[nT], m_any.m_w2[nT], j, scoring);
        AddDonAny(sc + m_w2s[nN], m_any.m_w2[nN], j, scoring);
        AddDonCon(sc + m_w2s[nA], don.m_w2[nA], j, scoring);
        AddDonCon(sc + m_w2s[nC], don.m_w2[nC], j, scoring);
        AddDonCon(sc + m_w2s[nG], don.m_w2[nG], j, scoring);
        AddDonCon(sc + m_w2s[nT], don.m_w2[nT], j, scoring);
        AddDonCon(sc + m_w2s[nN], don.m_w2[nN], j, scoring);
    }
    inline void AddDon(int j, const CProSplignScaledScoring& scoring) {
        AddDonAny(*++m_w, m_any.m_w, j, scoring);
        AddDonAny(*++m_v, m_any.m_v, j, scoring);
        AddDonAny(*++m_h1, m_any.m_h1, j, scoring);
        AddDonAny(*++m_h2, m_any.m_h2, j, scoring);
        AddDonAny(*++m_h3, m_any.m_h3, j, scoring);
    }
    inline void AddDon(CFIntronDon& don, int j, const CProSplignScaledScoring& scoring) {
        AddDonAny(*++m_w, m_any.m_w, j, scoring);
        AddDonAny(*++m_v, m_any.m_v, j, scoring);
        AddDonAny(*++m_h1, m_any.m_h1, j, scoring);
        AddDonAny(*++m_h2, m_any.m_h2, j, scoring);
        AddDonAny(*++m_h3, m_any.m_h3, j, scoring);
        AddDonCon(*m_w, don.m_w, j, scoring);
        AddDonCon(*m_v, don.m_v, j, scoring);
        AddDonCon(*m_h1, don.m_h1, j, scoring);
        AddDonCon(*m_h2, don.m_h2, j, scoring);
        AddDonCon(*m_h3, don.m_h3, j, scoring);
    }

public:     
//     static int lmin;//minimum intron length
//     static int ie;//intron extention cost (scaled)

    void InitRowScores(CAlignRow *row, vector<int>& prevw, int j);
    CFIntron(const CNSeq& nseq, const CProSplignScaledScoring scoring);

    inline const CBestI& Step(int j, const CProSplignScaledScoring& scoring, const CFastIScore& fiscore)
    {
        InitW12s(j, scoring, fiscore);
        switch((++m_cd)->m_dt2) {
        case eGT :
            AddDon(j, scoring);
            AddDon1(m_cd->m_1nuc, j, scoring);
            AddDon2(m_gt, j, scoring);
            break;
        case eGC :
            AddDon(j, scoring);
            AddDon1(m_cd->m_1nuc, j, scoring);
            AddDon2(m_gc, j, scoring);
            break;
        case eAT :
            AddDon(j, scoring);
            AddDon1(m_cd->m_1nuc, j, scoring);
            AddDon2(m_at, j, scoring);
            break;
        default:
            switch(m_cd->m_dt) {
            case eGT :
                AddDon(m_gt, j, scoring);
                break;
            case eGC :
                AddDon(m_gc, j, scoring);
                break;
            case eAT :
                AddDon(m_at, j, scoring);
                break;
            default:
                AddDon(j, scoring);
                break;
            }
            switch(m_cd->m_dt1) {
            case eGT :
                AddDon1(m_gt, m_cd->m_1nuc, j, scoring);
                break;
            case eGC :
                AddDon1(m_gc, m_cd->m_1nuc, j, scoring);
                break;
            case eAT :
                AddDon1(m_at, m_cd->m_1nuc, j, scoring);
                break;
            default:
                AddDon1(m_cd->m_1nuc, j, scoring);
                break;
            }
            AddDon2(j, scoring);
            break;
        }
        //acceptor
        BestScAny(scoring);
        m_bei.w1 = BestSc1(m_any, j, scoring) - scoring.sm_ICANY;
        m_bei.w2 = m_any.m_w2[m_cd->m_2nuc].first - scoring.sm_ICANY;
        int tmp;
        switch(m_cd->m_at2) {
        case eAC:
            tmp = m_at.m_w2[m_cd->m_2nuc].first - scoring.ie*(j - m_at.m_w2[m_cd->m_2nuc].second) - scoring.sm_ICAT;
            if(tmp > m_bei.w2) m_bei.w2 = tmp;
            break;
        case eAG:
            tmp = m_gt.m_w2[m_cd->m_2nuc].first - scoring.ie*(j - m_gt.m_w2[m_cd->m_2nuc].second) - scoring.sm_ICGT;
            if(tmp > m_bei.w2) m_bei.w2 = tmp;
            tmp = m_gc.m_w2[m_cd->m_2nuc].first - scoring.ie*(j - m_gc.m_w2[m_cd->m_2nuc].second) - scoring.sm_ICGC;
            if(tmp > m_bei.w2) m_bei.w2 = tmp;
            break;
        default:
                switch(m_cd->m_at) {
                case eAC:
                    BestScCon(m_at, scoring.sm_ICAT, j, scoring);
                    break;
                case eAG:
                    BestScCon(m_gt, scoring.sm_ICGT, j, scoring);
                    BestScCon(m_gc, scoring.sm_ICGC, j, scoring);
                    break;
                default:
                    break;
                }
                switch(m_cd->m_at1) {
                case eAC :
                  tmp = BestSc1(m_at, j, scoring) - scoring.sm_ICAT;
                  if(tmp > m_bei.w1) m_bei.w1 = tmp;
                  break;
                case eAG :
                  tmp = BestSc1(m_gt, j, scoring) - scoring.sm_ICGT;
                  if(tmp > m_bei.w1) m_bei.w1 = tmp;
                  tmp = BestSc1(m_gc, j, scoring) - scoring.sm_ICGC;
                  if(tmp > m_bei.w1) m_bei.w1 = tmp;
                  break;
                default:
                    break;
                }
                break;
        }
        m_bei.h1 -= scoring.ie_x_lmin;
        m_bei.h2 -= scoring.ie_x_lmin;
        m_bei.h3 -= scoring.ie_x_lmin;
        m_bei.w1 -= scoring.ie_x_lmin;
        m_bei.w2 -= scoring.ie_x_lmin;
        m_bei.w -= scoring.ie_x_lmin;
        m_bei.v -= scoring.ie_x_lmin;
        return m_bei;
    }
    inline static int Getlen(const EAccType& at, int sc, const CHIntronScore& sc_any, const CHIntronScore& sc_at,
                      const CHIntronScore& sc_gt, const CHIntronScore& sc_gc, int j, const CProSplignScaledScoring& scoring) { 
        if(sc == sc_any.first - scoring.sm_ICANY - scoring.ie_x_lmin) return j - sc_any.second + scoring.lmin;
        _ASSERT(at == eAC || at == eAG);
        if(at == eAC) {
            _ASSERT(sc == sc_at.first - scoring.sm_ICAT - scoring.ie*(j - sc_at.second + scoring.lmin));
            return j - sc_at.second + scoring.lmin;
        }
        _ASSERT(at == eAG);
        if(sc == sc_gt.first - scoring.sm_ICGT - scoring.ie*(j - sc_gt.second + scoring.lmin)) return j - sc_gt.second + scoring.lmin;
        _ASSERT(sc == sc_gc.first - scoring.sm_ICGC - scoring.ie*(j - sc_gc.second + scoring.lmin));
        return j - sc_gc.second + scoring.lmin;
    }
    inline int GetWlen(int j, const CProSplignScaledScoring& scoring) const { return Getlen(m_cd->m_at, m_bei.w, m_any.m_w, m_at.m_w, m_gt.m_w, m_gc.m_w, j, scoring); }
    inline int GetVlen(int j, const CProSplignScaledScoring& scoring) const { return Getlen(m_cd->m_at, m_bei.v, m_any.m_v, m_at.m_v, m_gt.m_v, m_gc.m_v, j, scoring); }
    inline int GetH1len(int j, const CProSplignScaledScoring& scoring) const { return Getlen(m_cd->m_at, m_bei.h1, m_any.m_h1, m_at.m_h1, m_gt.m_h1, m_gc.m_h1, j, scoring); }
    inline int GetH2len(int j, const CProSplignScaledScoring& scoring) const { return Getlen(m_cd->m_at, m_bei.h2, m_any.m_h2, m_at.m_h2, m_gt.m_h2, m_gc.m_h2, j, scoring); }
    inline int GetH3len(int j, const CProSplignScaledScoring& scoring) const { return Getlen(m_cd->m_at, m_bei.h3, m_any.m_h3, m_at.m_h3, m_gt.m_h3, m_gc.m_h3, j, scoring); }
    inline int GetW2len(int j, const CProSplignScaledScoring& scoring) const {
        int m_2nuc = m_cd->m_2nuc;
        if(m_bei.w2 == m_any.m_w2[m_2nuc].first - scoring.sm_ICANY - scoring.ie_x_lmin)
            return j - m_any.m_w2[m_2nuc].second + scoring.lmin;
        _ASSERT(m_cd->m_at2 == eAC || m_cd->m_at2 == eAG);

        int w2len;

        if(m_cd->m_at2 == eAC) {
            w2len = j - m_at.m_w2[m_2nuc].second + scoring.lmin;
            _ASSERT(m_bei.w2 == m_at.m_w2[m_2nuc].first - scoring.sm_ICAT - scoring.ie*w2len);
            return w2len;
        }
        _ASSERT(m_cd->m_at2 == eAG);
        w2len = j - m_gt.m_w2[m_2nuc].second + scoring.lmin;
        if(m_bei.w2 == m_gt.m_w2[m_2nuc].first - scoring.sm_ICGT - scoring.ie*w2len)
            return w2len;
        w2len = j - m_gc.m_w2[m_2nuc].second + scoring.lmin;
        _ASSERT(m_bei.w2 == m_gc.m_w2[m_2nuc].first - scoring.sm_ICGC - scoring.ie*w2len);
        return w2len;
    }
    inline int GetW1len(int j, const CProSplignScaledScoring& scoring) const {
        int len = GetLen1(m_bei.w1 + scoring.sm_ICANY, m_any, j, scoring);
        if(len) return len;
        _ASSERT(m_cd->m_at1 == eAC || m_cd->m_at1 == eAG);
        if(m_cd->m_at1 == eAC) {
            len = GetLen1(m_bei.w1 + scoring.sm_ICAT, m_at, j, scoring);
            _ASSERT(len);
            return len;
        }
        _ASSERT(m_cd->m_at1 == eAG);
        len = GetLen1(m_bei.w1 + scoring.sm_ICGT, m_gt, j, scoring);
        if(len) return len;
        len = GetLen1(m_bei.w1 + scoring.sm_ICGC, m_gc, j, scoring);
        _ASSERT(len);
        return len;
    }
};

// ***  END OF FAST INTRON IMPLEMENTATION


END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //INTRON_HPP
