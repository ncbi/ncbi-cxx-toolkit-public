/*  $Id$
* ===========================================================================
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
* Author: Philip Johnson
*
* File Description: finds mRNA 3' modification (poly-A tails)
*
* ---------------------------------------------------------------------------
*/
#ifndef ALGO_SEQUENCE___POLYA__HPP
#define ALGO_SEQUENCE___POLYA__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

enum EPolyTail {
    ePolyTail_None,
    ePolyTail_A3, //> 3' poly-A tail
    ePolyTail_T5  //> 5' poly-T head (submitted to db reversed?)
};


///////////////////////////////////////////////////////////////////////////////
/// PRE : two random access iterators pointing to sequence data [begin,
/// end)
/// POST: poly-A tail cleavage site, if any (-1 if not)
template <typename Iterator>
TSignedSeqPos
FindPolyA(Iterator begin, Iterator end);

///////////////////////////////////////////////////////////////////////////////
/// PRE : two random access iterators pointing to sequence data [begin,
/// end)
/// POST: cleavageSite (if any) and whether we found a poly-A tail, a poly-T
/// head, or neither
template <typename Iterator>
EPolyTail
FindPolyTail(Iterator begin, Iterator end, TSignedSeqPos &cleavageSite);


///////////////////////////////////////////////////////////////////////////////
/// Implementation [in header because of templates]

template<typename Iterator>
class CRevComp_It {
public:
    CRevComp_It(void) {}
    CRevComp_It(const Iterator &it) {
        m_Base = it;
    }

    char operator*(void) const {
        Iterator tmp = m_Base;
        switch (*--tmp) {
        case 'A': return 'T';
        case 'T': return 'A';
        case 'C': return 'G';
        case 'G': return 'C';
        default: return *tmp;
        }
    }
    CRevComp_It& operator++(void) {
        --m_Base;
        return *this;
    }
    CRevComp_It operator++(int) {
        CRevComp_It it = m_Base;
        --m_Base;
        return it;
    }
    CRevComp_It& operator+=(int i) {
        m_Base -= i;
        return *this;
    }
    CRevComp_It& operator-=(int i) {
        m_Base += i;
        return *this;
    }
    CRevComp_It operator+ (int i) const {
        CRevComp_It it(m_Base);
        it += i;
        return it;
    }
    CRevComp_It operator- (int i) const {
        CRevComp_It it(m_Base);
        it -= i;
        return it;
    }
    int operator- (const CRevComp_It &it) const {
        return it.m_Base - m_Base;
    }
    
    //booleans
    bool operator>= (const CRevComp_It &it) const {
        return m_Base <= it.m_Base;
    }
    bool operator>  (const CRevComp_It &it) const {
        return m_Base < it.m_Base;
    }
    bool operator<= (const CRevComp_It &it) const {
        return m_Base >= it.m_Base;
    }
    bool operator<  (const CRevComp_It &it) const {
        return m_Base > it.m_Base;
    }
    bool operator== (const CRevComp_It &it) const {
        return m_Base == it.m_Base;
    }
    bool operator!= (const CRevComp_It &it) const {
        return m_Base != it.m_Base;
    }
private:
    Iterator m_Base;
};


///////////////////////////////////////////////////////////////////////////////
// PRE : same conditions as STL 'search', but iterators must have ptrdiff_t
// difference type
// POST: same as STL 'search'
template <typename ForwardIterator1, typename ForwardIterator2>
ForwardIterator1 ItrSearch(ForwardIterator1 first1, ForwardIterator1 last1,
                           ForwardIterator2 first2, ForwardIterator2 last2)
{
    ptrdiff_t d1 = last1 - first1;
    ptrdiff_t d2 = last2 - first2;
    if (d1 < d2) {
        return last1;
    }

    ForwardIterator1 current1 = first1;
    ForwardIterator2 current2 = first2;

    while (current2 != last2) {
        if (!(*current1 == *current2)) {
            if (d1-- == d2) {
                return last1;
            } else {
                current1 = ++first1;
                current2 = first2;
            }
        } else {
            ++current1;
            ++current2;
        }
    }
    return (current2 == last2) ? first1 : last1;
}

///////////////////////////////////////////////////////////////////////////////
// PRE : two random access iterators pointing to sequence data [begin,
// end)
// POST: poly-A tail cleavage site, if any (-1 if not)
template <typename Iterator>
TSignedSeqPos FindPolyA(Iterator begin, Iterator end)
{
    string motif1("AATAAA");
    string motif2("ATTAAA");

    Iterator pos = max(begin, end-250);

    Iterator uStrmMotif = pos;
    while (uStrmMotif != end) {
        pos = uStrmMotif;
        uStrmMotif = ItrSearch(pos, end, motif1.begin(), motif1.end());
        if (uStrmMotif == end) {
            uStrmMotif = ItrSearch(pos, end, motif2.begin(), motif2.end());
        }

        if (uStrmMotif != end) {
            uStrmMotif += 6; // skip over upstream motif
            pos = uStrmMotif;

            Iterator maxCleavage = (end - pos < 30) ? end : pos + 30;

            unsigned int aRun = 0;
            for (pos += 10;  pos < maxCleavage  &&  aRun < 3;  ++pos) {
                if (*pos == 'A') {
                    ++aRun;
                } else {
                    aRun = 0;
                }
            }
            
            if (aRun) {
                pos -= aRun;
            }
            TSignedSeqPos cleavageSite = pos - begin;

            //now let's look for poly-adenylated tail..
            unsigned int numA = 0, numOther = 0;
            while (pos != end) {
                if (*pos == 'A') {
                    ++numA;
                } else {
                    ++numOther;
                }
                ++pos;
            }

            if (numOther + numA > 0  &&
                ((double) numA / (numA+numOther)) > 0.95) {
                return cleavageSite;
            }
        }
    }

    return -1;
}

///////////////////////////////////////////////////////////////////////////////
// PRE : two random access iterators pointing to sequence data [begin,
// end)
// POST: cleavageSite (if any) and whether we found a poly-A tail, a poly-T
// head, or neither
template<typename Iterator>
EPolyTail FindPolyTail(Iterator begin, Iterator end,
                       TSignedSeqPos &cleavageSite)
{
    cleavageSite = FindPolyA(begin, end);
    if (cleavageSite >= 0) {
        return ePolyTail_A3;
    } else {
        int seqLen = end - begin;

        cleavageSite = FindPolyA(CRevComp_It<Iterator>(end),
                                 CRevComp_It<Iterator>(begin));

        if (cleavageSite >= 0) {
            cleavageSite = seqLen - cleavageSite - 1;
            return ePolyTail_T5;
        }
    }

    return ePolyTail_None;
}

END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.5  2004/05/11 18:36:56  johnson
* made reverse iterator more standard (== base-1); avoid add a value to the
* iterator that could potentially take it past the end
*
* Revision 1.4  2004/04/28 15:19:46  johnson
* Uses templated iterators; no longer accepts user suggestion for cleavage
* site
*
* Revision 1.3  2003/12/31 20:41:39  johnson
* FindPolySite takes cleavage prompt for both 3' poly-A and 5' poly-T
*
* Revision 1.2  2003/12/30 21:28:31  johnson
* added msvc export specifiers
*
* ===========================================================================
*/

#endif /*ALGO_SEQUENCE___POLYA__HPP*/
