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
 * Authors:  Josh Cherry
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include <objects/seq/seqport_util.hpp>
#include <algo/sequence/nuc_prop.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void CNucProp::CountNmers(CSeqVector& seqvec, int n, vector<int>& table)
{
    TSeqPos len = seqvec.size();

    table.resize(NumberOfNmers(n));

    // clear table
    for (int i = 0;  i < NumberOfNmers(n);  i++) {
        table[i] = 0;
    }

    string seq_string;
    seqvec.GetSeqData(0, len, seq_string);
    const char *seq;
    seq = seq_string.data();

    for (TSeqPos i = 0;  i <= len-n;  ++i) {
        int nmerint = Nmer2Int(seq+i, n);
        if (nmerint >= 0) {   // if no ambiguity chars
            table[nmerint]++;
        }
    }
}


// convert nmer of length n, pointed to by seq,
// to an integer representation
// if there's a character other than AGCT, return -1
// n better be small enough that the result fits into an int
int CNucProp::Nmer2Int(const char *seq, int n)
{
    int rv = 0;

    for (int i = 0;  i<n;  i++) {
        rv <<= 2;
        int nucint = Nuc2Nybble(seq[i]);
        if(nucint < 0) {
            return -1;
        }
        rv |= nucint;
    }
    return rv;
}


// convert int from Nmer2Int back to a string
void CNucProp::Int2Nmer(int nmer_int, int nmer_size, string& out)
{
    out.resize(nmer_size);
    for (int i = nmer_size-1;  i >= 0; i--) {
        out[i] = Nybble2Nuc(nmer_int & 3);   // analyze two low-order bits
        nmer_int >>= 2;
    }
}


// there are 4^n n-mers
int CNucProp::NumberOfNmers(int n)
{
    return 1 << (2*n);
}


int CNucProp::Nuc2Nybble(char nuc)
{
    switch (nuc) {
    case 'G':
        return 0;
    case 'A':
        return 1;
    case 'T':
        return 2;
    case 'C':
        return 3;
    default:     // other than GATC
        return -1;
    }
}


char CNucProp::Nybble2Nuc(int n)
{
    switch (n) {
    case 0:
        return 'G';
    case 1:
        return 'A';
    case 2:
        return 'T';
    case 3:
        return 'C';
    default:
        // this should never happen
        return '?';
    }
}


int CNucProp::GetPercentGC(const CSeqVector& seqvec)
{
    TSeqPos gc_count = 0;
    TSeqPos len = seqvec.size();

    for (TSeqPos i = 0;  i < len;  ++i) {
        switch (seqvec[i]) {
        case 'C':
        case 'G':
        case 'S':
            ++gc_count;
            break;
        default:
            break;
        }
    }

    return (int) ((gc_count * 100.0) / len + 0.5);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2003/08/18 17:35:29  jcherry
 * Fixed CountNmers to alter result vectors size, not just its capacity.
 * Made Int2Nmer build its string from scratch, and do so properly.
 *
 * Revision 1.4  2003/07/28 20:41:01  jcherry
 * Changed GetPercentGC() to round properly
 *
 * Revision 1.3  2003/07/28 11:54:34  dicuccio
 * Changed Int2Nmer to use std::string instead of char*
 *
 * Revision 1.2  2003/07/01 19:01:13  ucko
 * Fix scope use
 *
 * Revision 1.1  2003/07/01 15:10:40  jcherry
 * Initial versions of nuc_prop and prot_prop
 *
 * ===========================================================================
 */

