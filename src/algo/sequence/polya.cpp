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

#include <algo/sequence/polya.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <algorithm>
#include <stdexcept>
#include <string.h>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
// PRE : null-terminated string containing sequence; possible cleavage site
// POST: poly-A tail cleavage site, if any (-1 if not)
TSignedSeqPos FindPolyA(const char* seq, TSignedSeqPos possCleavageSite)
{
    TSeqPos seqLen = strlen(seq);

    if ((TSignedSeqPos) seqLen < possCleavageSite) {
        throw range_error("FindPolyA: putative cleavage site off the end "
                          "of the sequence!");
    }

    const char* pos;
    if (possCleavageSite >= 0) {
        pos = seq + max(0, possCleavageSite - 30);
    } else {
        pos = (seqLen > 250) ? seq+seqLen-250 : seq;
    }

    const char* uStrmMotif = pos;
    while (uStrmMotif) {
        pos = uStrmMotif;
        uStrmMotif = strstr(pos, "AATAAA");
        if (!uStrmMotif) {
            uStrmMotif = strstr(pos, "ATTAAA");
        }

        if (uStrmMotif) {
            uStrmMotif += 6; // skip over upstream motif
            pos = uStrmMotif;
            if (possCleavageSite < 0) {
                const char* maxCleavage = min(pos + 30, seq+seqLen);
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
                possCleavageSite = pos - seq;
            } else {
                pos = seq + possCleavageSite;
            }

            //now let's look for poly-adenylated tail..
            unsigned int numA = 0, numOther = 0;
            while (pos < seq+seqLen) {
                if (*pos == 'A') {
                    ++numA;
                } else {
                    ++numOther;
                }
                ++pos;
            }

            if (numOther + numA > 0  &&
                ((double) numA / (numA+numOther)) > 0.95) {
                return possCleavageSite;
            }
        }
    }

    return -1;
}

///////////////////////////////////////////////////////////////////////////////
// PRE : null-terminated string containing sequence; possible 3' cleavage
// site, possible 5' cleavage site (if submitted reversed)
// POST: cleavageSite (if any) and whether we found a poly-A tail, a poly-T
// head, or neither
EPolyTail FindPolyTail(const char* seq, TSignedSeqPos &cleavageSite,
                       TSignedSeqPos possCleavageSite3p,
                       TSignedSeqPos possCleavageSite5p)
{
    cleavageSite = FindPolyA(seq, possCleavageSite3p);
    if (cleavageSite >= 0) {
        return ePolyTail_A3;
    } else {
        TSeqPos seqLen = strlen(seq);
        AutoPtr<char, ArrayDeleter<char> > otherStrand(new char[seqLen]);
        CSeqManip::ReverseComplement(seq, CSeqUtil::e_Iupacna, 0, seqLen,
                                     otherStrand.get());

        cleavageSite = FindPolyA(otherStrand.get(), (possCleavageSite5p >= 0) ?
                                 seqLen - possCleavageSite5p - 1 : -1);

        if (cleavageSite >= 0) {
            cleavageSite = seqLen - cleavageSite - 1;
            return ePolyTail_T5;
        }
    }
    return ePolyTail_None;
}

END_NCBI_SCOPE

/*===========================================================================
* $Log$
* Revision 1.4  2003/12/31 20:41:40  johnson
* FindPolySite takes cleavage prompt for both 3' poly-A and 5' poly-T
*
* Revision 1.3  2003/12/31 00:33:22  johnson
* minor bug fixes
*
* Revision 1.2  2003/11/06 22:33:38  johnson
* typo: include 'string.h', not 'strings.h'
*
* Revision 1.1  2003/11/06 20:36:03  johnson
* first revision
*
* ===========================================================================*/
