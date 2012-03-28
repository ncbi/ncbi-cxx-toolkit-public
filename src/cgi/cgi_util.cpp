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
 * Authors:  Alexey Grichenko
 *
 * File Description:   CGI related utility classes and functions
 *
 */

#include <ncbi_pch.hpp>
#include <cgi/cgi_util.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Url encode/decode
//

extern SIZE_TYPE URL_DecodeInPlace(string& str, EUrlDecode decode_flag)
{
    NStr::URLDecodeInPlace(str, NStr::EUrlDecode(decode_flag));
    return 0;
}


extern string URL_DecodeString(const string& str,
                               EUrlEncode    encode_flag)
{
    if (encode_flag == eUrlEncode_None) {
        return str;
    }
    return NStr::URLDecode(str, encode_flag == eUrlEncode_PercentOnly ?
        NStr::eUrlDec_Percent : NStr::eUrlDec_All);
}


extern string URL_EncodeString(const string& str,
                               EUrlEncode encode_flag)
{
    return NStr::URLEncode(str, NStr::EUrlEncode(encode_flag));
}


END_NCBI_SCOPE
