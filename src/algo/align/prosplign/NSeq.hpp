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

#ifndef NSEQ_H
#define NSEQ_H

#include <corelib/ncbistl.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_loc;
END_SCOPE(objects)
BEGIN_SCOPE(prosplign)
USING_SCOPE(ncbi::objects);

enum Nucleotides { nA, nC, nG, nT, nN };

typedef vector<char> NSEQ;

class CNSeq
{
public:
    CNSeq();
    CNSeq(CScope& scope, CSeq_loc& genomic)
    {
        Init(scope,genomic);
    }
    ~CNSeq(void);

//both init methods try to preserve three extra letters at the end of seq for stop presence check

    void Init(CScope& scope, CSeq_loc& genomic);
    //just cut ranges given in igi from fullseq
    void Init(const CNSeq& fullseq, const vector<pair<int, int> >& igi);

    // letter by position
    char Upper(int pos) const;
    inline int GetNuc(NSEQ::size_type j) const { return seq[j]; }
    inline int operator[](NSEQ::size_type j)  const { return seq[j]; }
    inline int size(void) const{ return m_size; }
    //can a letter be taken at position 'pos'?
    inline bool ValidPos(NSEQ::size_type pos) const{ return pos >= 0 && pos < seq.size(); }
private:
    int m_size;//represents part of sequence involved into alignment; m_size could be less than seq.size()
    NSEQ seq;  //real sequence may have up to three extra letters  for stop check that are not involved
               // into alignment.
    CNSeq(const CNSeq&);
    CNSeq operator=(const CNSeq&);
};

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif//NSEQ_H
