#ifndef CGI___CGI_UTIL__HPP
#define CGI___CGI_UTIL__HPP

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
 * Authors: Alexey Grichenko
 *
 */

/// @file cont_util.hpp
///
/// CGI related utility classes and functions.
///

#include <util/ncbi_url.hpp>

/** @addtogroup CGI
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//   DEPRECATED
//
/////////////////////////////////////////////////////////////////////////////


/// @deprecated Use NStr::EUrlEncode
enum EUrlEncode {
    eUrlEncode_None             = NStr::eUrlEnc_None,
    eUrlEncode_SkipMarkChars    = NStr::eUrlEnc_SkipMarkChars,
    eUrlEncode_ProcessMarkChars = NStr::eUrlEnc_ProcessMarkChars,
    eUrlEncode_PercentOnly      = NStr::eUrlEnc_PercentOnly,
    eUrlEncode_Path             = NStr::eUrlEnc_Path
};

/// @deprecated Use NStr::EUrlDecode
enum EUrlDecode {
    eUrlDecode_All              = NStr::eUrlDec_All,
    eUrlDecode_Percent          = NStr::eUrlDec_Percent
};


/// @deprecated Use NStr::URLDecode()
NCBI_DEPRECATED
NCBI_XCGI_EXPORT
extern string
URL_DecodeString(const string& str,
                 EUrlEncode    encode_flag = eUrlEncode_SkipMarkChars);

/// @deprecated Use NStr::URLDecodeInPlace()
NCBI_DEPRECATED
NCBI_XCGI_EXPORT
extern SIZE_TYPE
URL_DecodeInPlace(string& str, EUrlDecode decode_flag = eUrlDecode_All);

/// @deprecated Use NStr::URLEncode()
NCBI_DEPRECATED
NCBI_XCGI_EXPORT
extern string
URL_EncodeString(const      string& str,
                 EUrlEncode encode_flag = eUrlEncode_SkipMarkChars);


/// @deprecated Use CUrlArgs_Parser
NCBI_DEPRECATED_CLASS NCBI_XCGI_EXPORT CCgiArgs_Parser : public CUrlArgs_Parser
{
public:
    CCgiArgs_Parser(void) {}
    void SetQueryString(const string& query, EUrlEncode encode)
    { CUrlArgs_Parser::SetQueryString(query, NStr::EUrlEncode(encode)); }
    void SetQueryString(const string& query,
                        const IUrlEncoder* encoder = 0)
    { CUrlArgs_Parser::SetQueryString(query, encoder); }
};


/// @deprecated Use CUrlArgs
NCBI_DEPRECATED_CLASS NCBI_XCGI_EXPORT CCgiArgs : public CUrlArgs
{
public:
    CCgiArgs(void) {}
    CCgiArgs(const string& query, EUrlEncode decode)
        : CUrlArgs(query, NStr::EUrlEncode(decode)) {}
    string GetQueryString(EAmpEncoding amp_enc,
                          EUrlEncode encode) const
    { return CUrlArgs::GetQueryString(amp_enc, NStr::EUrlEncode(encode)); }
};


END_NCBI_SCOPE

#endif  /* CGI___CGI_UTIL__HPP */
