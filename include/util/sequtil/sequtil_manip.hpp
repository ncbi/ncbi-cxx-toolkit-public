#ifndef UTIL_SEQUTIL___SEQUTIL_MANIP__HPP
#define UTIL_SEQUTIL___SEQUTIL_MANIP__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Various sequnce manipulations
 */   
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <vector>

#include <util/sequtil/sequtil.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// Sequence Manipulations
//


class NCBI_XUTIL_EXPORT CSeqManip
{
public:

    // types
    typedef CSeqUtil::TCoding   TCoding;
 
    // Reverse
    static SIZE_TYPE Reverse(const string& src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             string& dst);
    static SIZE_TYPE Reverse(const vector<char>& src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             vector<char>& dst);
    static SIZE_TYPE Reverse(const char* src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             char* dst);

    // Complement
    static SIZE_TYPE Complement(const string& src, TCoding src_coding,
                                TSeqPos pos, TSeqPos length,
                                string& dst);
    static SIZE_TYPE Complement(const vector<char>& src, TCoding src_coding,
                                TSeqPos pos, TSeqPos length,
                                vector<char>& dst);
    static SIZE_TYPE Complement(const char* src, TCoding src_coding,
                                TSeqPos pos, TSeqPos length,
                                char* dst);

    // Reverse + Complement

    // place result in an auxiliary container
    static SIZE_TYPE ReverseComplement(const string& src, TCoding src_coding,
                                       TSeqPos pos, TSeqPos length,
                                       string& dst);
    static SIZE_TYPE ReverseComplement(const vector<char>& src, TCoding src_coding,
                                       TSeqPos pos, TSeqPos length,
                                       vector<char>& dst);
    static SIZE_TYPE ReverseComplement(const char* src, TCoding src_coding,
                                       TSeqPos pos, TSeqPos length,
                                       char* dst);

    // in place operation
    static SIZE_TYPE ReverseComplement(string& src, TCoding src_coding,
                                TSeqPos pos, TSeqPos length);
    static SIZE_TYPE ReverseComplement(vector<char>& src, TCoding src_coding,
                                TSeqPos pos, TSeqPos length);
    static SIZE_TYPE ReverseComplement(char* src, TCoding src_coding,
                                TSeqPos pos, TSeqPos length);
};


END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL_MANIP__HPP */
