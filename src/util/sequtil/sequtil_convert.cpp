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
 *      Sequence conversion utility.
 */   
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <vector>

#include <util/sequtil/sequtil_convert.hpp>
#include "sequtil_convert_imp.hpp"


BEGIN_NCBI_SCOPE


//  -- Conversion methods

// string to string
SIZE_TYPE CSeqConvert::Convert
(const string& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst, 
 TCoding dst_coding)
{
    return CSeqConvert_imp::Convert(src, src_coding,
                                    pos, length,
                                    dst, dst_coding);
}


// string to vector
SIZE_TYPE CSeqConvert::Convert
(const string& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 vector< char >& dst,
 TCoding dst_coding)
{
    return CSeqConvert_imp::Convert(src, src_coding,
                                    pos, length,
                                    dst, dst_coding);
}


// vector to string
SIZE_TYPE CSeqConvert::Convert
(const vector< char >& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst,
 TCoding dst_coding)
{
    return CSeqConvert_imp::Convert(src, src_coding,
                                    pos, length,
                                    dst, dst_coding);
}


// vector to vector
SIZE_TYPE CSeqConvert::Convert
(const vector< char >& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 vector< char >& dst,
 TCoding dst_coding)
{
    return CSeqConvert_imp::Convert(src, src_coding,
                                    pos, length,
                                    dst, dst_coding);
}

// char* to char*
SIZE_TYPE CSeqConvert::Convert
(const char src[],
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 char dst[],
 TCoding dst_coding)
{
    return CSeqConvert_imp::Convert(src, src_coding,
                                    pos, length,
                                    dst, dst_coding);
}


//  -- Get part of a sequence (same coding conversion)

SIZE_TYPE CSeqConvert::Subseq
(const string& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst)
{
    return CSeqConvert_imp::Subseq(src, coding, pos, length, dst);
}


SIZE_TYPE CSeqConvert::Subseq
(const string& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 vector<char>& dst)
{
    return CSeqConvert_imp::Subseq(src, coding, pos, length, dst);
}


SIZE_TYPE CSeqConvert::Subseq
(const vector<char>& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst)
{
    return CSeqConvert_imp::Subseq(src, coding, pos, length, dst);
}


SIZE_TYPE CSeqConvert::Subseq
(const vector<char>& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 vector<char>& dst)
{
    return CSeqConvert_imp::Subseq(src, coding, pos, length, dst);
}


SIZE_TYPE CSeqConvert::Subseq
(const char* src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return CSeqConvert_imp::Subseq(src, coding, pos, length, dst);
}


// -- Packing

SIZE_TYPE CSeqConvert::Pack
(const string& src,
 TCoding src_coding,
 vector<char>& dst,
 TCoding& dst_coding,
 TSeqPos length)
{
    return CSeqConvert_imp::Pack(src, src_coding, dst, dst_coding, length);
}


SIZE_TYPE CSeqConvert::Pack
(const vector<char>& src,
 TCoding src_coding,
 vector<char>& dst,
 TCoding& dst_coding,
 TSeqPos length)
{
    return CSeqConvert_imp::Pack(src, src_coding, dst, dst_coding, length);
}

SIZE_TYPE CSeqConvert::Pack
(const char* src,
 TSeqPos length,
 TCoding src_coding,
 char* dst,
 TCoding& dst_coding)
{
    return CSeqConvert_imp::Pack(src, length, src_coding, dst, dst_coding);
}


END_NCBI_SCOPE

 /*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/05/17 21:08:53  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2003/10/08 13:34:37  shomrat
* Initial version
*
*
* ===========================================================================
*/
