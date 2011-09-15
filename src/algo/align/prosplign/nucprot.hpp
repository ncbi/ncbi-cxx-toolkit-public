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

#ifndef NUCPROT_H
#define NUCPROT_H

#include <corelib/ncbi_limits.hpp>

#include <algorithm>
#include <sstream>

#include "NSeq.hpp"
#include "PSeq.hpp"
#include "BackAlignInfo.hpp"
#include "scoring.hpp"

BEGIN_NCBI_SCOPE

class CSubstMatrix;

BEGIN_SCOPE(prosplign)


enum EWMode {
  eD, 
  eV, //v - gap
  eH,  //H - gap
  eH1, // H gap (frameshift) of 3*N + 1 length
  eH2, // H gap (frameshift) of 3*N + 2 length
  eH3, // H gap of 3*N length
  eFV, //v - gap with frameshift
  eFH,  //h - gap with frameshift
  eS //splice
};

/*
struct BMode {//info for back tracking
  int wmode; //one of EWMode
  int dmode, hmode, vmode, fhmode, fvmode;
  int dslen, hslen, vslen, fhslen, fvslen;//splice length if ?mode shows splice
  int wslen;
};
*/

struct CBMode {//back tracking for one stage fast version (AlignFNog, BackAlignFNog)
    int wmode; // to keep w, h1-3 and v modes
    int wlen, vlen, h1len, h2len, h3len; //splice lengths
};

/* * fast score access * */
class CFastIScore
{
public:
    void SetAmin(char amin, const CSubstMatrix& matrix);//call before GetScore() and/or GetScore(int n1, int n2, int n3)
    void Init(const CNSeq& seq, const CSubstMatrix& matrix);//call before GetScore()
    inline int GetScore() { return *++m_pos; }
    inline int GetScore(int n1, int n2, int n3) const { return m_gpos[n1*25+n2*5+n3]; }
    CFastIScore() :  m_size(0), m_init(false) { m_scores.resize(1); }
private:
    vector<int> m_scores;
    int *m_pos;
    int m_size;

    int *m_gpos;
    vector<int> m_gscores;
    void Init(const CSubstMatrix& matrix);
    bool m_init;

    CFastIScore(const CFastIScore&);
    CFastIScore& operator=(const CFastIScore&);
};


void ReadFasta(vector<char>& pseq, istream& ifs, bool is_nuc, string& id);

//reads one time only
void ReadNucFa(CNSeq& seq, const string& fname, string& id);
void ReadProtFa(PSEQ& seq, const string& fname, string& id);

class CAli;
class CAlignInfo;
class CProSplignScaledScoring;

// *****   versions without gap/frameshift penalty at the beginning/end
//fast variant of FrAlignNog1
int   FrAlignFNog1(CBackAlignInfo& bi, const PSEQ& pseq, const CNSeq& nseq, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix, bool left_gap = false, bool right_gap = false);
//**** good  for FrAlignNog, FrAlignNog1, FrAlign and FrAlignFNog1
void FrBackAlign(CBackAlignInfo& bi, CAli& ali);

// *****   versions without gap/frameshift penalty at the beginning/end ONE STAGE, FAST
int AlignFNog(CTBackAlignInfo<CBMode>& bi, const PSEQ& pseq, const CNSeq& nseq, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix);
void BackAlignNog(CTBackAlignInfo<CBMode>& bi, CAli& ali);

// *****   versions without gap/frameshift penalty at the beginning/end FAST
int FindFGapIntronNog(vector<pair<int, int> >& igi/*to return end gap/intron set*/, 
                      const PSEQ& pseq, const CNSeq& nseq, bool& left_gap, bool& right_gap, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix);

//*** mixed gap penalty (OLD, aka version 2)
int FindIGapIntrons(vector<pair<int, int> >& igi/*to return end gap/intron set*/, const PSEQ& pseq, const CNSeq& nseq, int g/*gap opening*/, int e/*one nuc extension cost*/,
            int f/*frameshift opening cost*/, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix);
int   FrAlign(CBackAlignInfo& bi, const PSEQ& pseq, const CNSeq& nseq, int g/*gap opening*/, int e/*one nuc extension cost*/,
            int f/*frameshift opening cost*/, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix);



// DEPRECATED:
// *****   versions without gap/frameshift penalty at the beginning/end
//pag f2
//int   FrAlignNog(CBackAlignInfo& bi, const PSEQ& pseq, const CNSeq& nseq, int g/*gap opening*/, int e/*one nuc extension cost*/,
//            int f/*frameshift opening cost*/);
//aka version 3, see page 14
//int   FrAlignNog1(CBackAlignInfo& bi, const PSEQ& pseq, const CNSeq& nseq, int g/*gap opening*/, int e/*one nuc extension cost*/,
//            int f/*frameshift opening cost*/);


END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //NUCPROT_H
