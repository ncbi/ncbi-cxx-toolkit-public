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
 * Author: Aleksey Grichenko
 *
 * File Description:
 *	 CRetryContext implementation.
 *
 * ===========================================================================
 */
 
#include <ncbi_pch.hpp>
#include <corelib/ncbi_cookies.hpp>
#include <util/retry_ctx.hpp>

BEGIN_NCBI_SCOPE


const char* CHttpRetryContext::kHeader_Stop = "X-NCBI-Retry-Stop";
const char* CHttpRetryContext::kHeader_Delay = "X-NCBI-Retry-Delay";
const char* CHttpRetryContext::kHeader_Args = "X-NCBI-Retry-Args";
const char* CHttpRetryContext::kHeader_Url = "X-NCBI-Retry-URL";
const char* CHttpRetryContext::kHeader_Content = "X-NCBI-Retry-Content";
const char* CHttpRetryContext::kContent_None = "no_content";
const char* CHttpRetryContext::kContent_FromResponse = "from_response";
const char* CHttpRetryContext::kContent_Value = "content:";


CHttpRetryContext::CHttpRetryContext(const CRetryContext& ctx)
{
    *this = ctx;
}


CHttpRetryContext& CHttpRetryContext::operator=(const CRetryContext& ctx)
{
    if (&ctx != this) {
        // Copy parent's data, ignore any extras.
        CRetryContext::operator=(ctx);
    }
    return *this;
}


void CHttpRetryContext::ParseHeader(const char* http_header)
{
    list<string> lines;
    NStr::Split(http_header, HTTP_EOL, lines,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    SetContentOverride(eNot_set);
    ResetContent();
    ResetNeedRetry();

    string name, value;
    ITERATE(list<string>, line, lines) {
        size_t delim = line->find(':');
        if (delim == NPOS  ||  delim < 1) {
            // skip lines without delimiter - can be HTTP status or
            // something else.
            continue;
        }
        name = line->substr(0, delim);
        value = line->substr(delim + 1);
        NStr::TruncateSpacesInPlace(value, NStr::eTrunc_Both);

        if ( NStr::EqualNocase(name, kHeader_Stop) ) {
            SetStop(value);
        }
        if ( NStr::EqualNocase(name, kHeader_Delay) ) {
            double val = NStr::StringToDouble(value);
            if (errno == 0) {
                SetDelay(val);
            }
            SetNeedRetry(); // Even if the value was wrong.
        }
        if ( NStr::EqualNocase(name, kHeader_Args) ) {
            SetArgs(value);
            SetNeedReconnect();
            SetNeedRetry();
        }
        if ( NStr::EqualNocase(name, kHeader_Url) ) {
            SetUrl(value);
            SetNeedReconnect();
            SetNeedRetry();
        }
        if ( NStr::EqualNocase(name, kHeader_Content) ) {
            string content = value;
            if ( NStr::EqualNocase(content, kContent_None) ) {
                SetContentOverride(eNoContent);
            }
            else if ( NStr::EqualNocase(content, kContent_FromResponse) ) {
                SetContentOverride(eFromResponse);
            }
            else if ( NStr::StartsWith(content, kContent_Value, NStr::eNocase) ) {
                SetContentOverride(eData);
                SetContent(NStr::URLDecode(content.substr(strlen(kContent_Value))));
            }
            SetNeedRetry();
        }
    }
}


void CHttpRetryContext::GetValues(TValues& values) const
{
    values.clear();
    if ( IsSetStop() ) {
        values[kHeader_Stop] = GetStopReason();
    }
    if ( IsSetDelay() ) {
        values[kHeader_Delay] = NStr::NumericToString(GetDelay().GetAsDouble());
    }
    if ( IsSetArgs() ) {
        values[kHeader_Args] = GetArgs();
    }
    if ( IsSetUrl() ) {
        values[kHeader_Url] = GetUrl();
    }
    if ( IsSetContentOverride() ) {
        switch ( GetContentOverride() ) {
        case eNoContent:
            values[kHeader_Content] = kContent_None;
            break;
        case eFromResponse:
            values[kHeader_Content] = kContent_FromResponse;
            break;
        case eData:
        {
            string value = kContent_Value;
            if ( IsSetContent() ) {
                value += NStr::URLEncode(GetContent());
            }
            values[kHeader_Content] = value;
            break;
        }
        default:
            break;
        }
    }
}


END_NCBI_SCOPE
