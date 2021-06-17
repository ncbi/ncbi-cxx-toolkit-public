#ifndef RETRY_CTX__HPP
#define RETRY_CTX__HPP

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
 * Author:  Aleksey Grichenko
 *
 */

/// @file retry_ctx.hpp
/// Retry context class.

#include <map>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_url.hpp>


/** @addtogroup Retry Context
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CRetryContext -- Retry context.
///
/// The class used to store, send and receive retry related data.
///
class NCBI_XUTIL_EXPORT CRetryContext : public CObject
{
public:
    CRetryContext(void);
    virtual ~CRetryContext(void) {}

    /// Content override flags
    enum EContentOverride {
        eNot_set,       ///< No content provided, send the original request data
        eNoContent,     ///< Do not send any content on retry
        eFromResponse,  ///< On retry send content from the response body
        eData           ///< On retry send content from the response header
    };

    /// Check if another retry attempt has been requested (any new headers
    /// except STOP received in the last reply).
    bool GetNeedRetry(void) const;
    void SetNeedRetry(void);
    void ResetNeedRetry(void);

    /// Clear all options.
    void Reset(void);

    /// Check if STOP flag is set.
    bool IsSetStop(void) const;
    /// Get STOP reason (or empty string).
    const string& GetStopReason(void) const;
    /// Set STOP flag, clear all other options.
    void SetStop(const string& reason);
    /// Reset STOP flag and reason.
    void ResetStop(void);

    /// Check if retry delay is set.
    bool IsSetDelay(void) const;
    /// Get retry delay.
    const CTimeSpan& GetDelay(void) const;
    /// Set retry delay.
    void SetDelay(CTimeSpan delay);
    /// Set retry delay in seconds.
    void SetDelay(double sec);
    /// Reset delay.
    void ResetDelay(void);

    /// Check if retry args are set.
    bool IsSetArgs(void) const;
    /// Get retry args, URL-decoded.
    const string& GetArgs(void) const;
    /// Set retry args. The arguments must be pre-URL-encoded as required.
    /// The arguments set here will override any values set in the URL.
    /// @sa SetUrl()
    void SetArgs(const string& args);
    /// Set retry args.
    /// The arguments set here will override any values set in the URL.
    /// @sa SetUrl()
    void SetArgs(const CUrlArgs& args);
    /// Reset args.
    void ResetArgs(void);

    /// Check if retry URL is set.
    bool IsSetUrl(void) const;
    /// Get retry URL.
    const string& GetUrl(void) const;
    /// Set retry URL. If the URL contains any arguments, they may be
    /// overwritten by the values set through SetArgs().
    void SetUrl(const string& url);
    /// Set retry URL. If the URL contains any arguments, they may be
    /// overwritten by the values set through SetArgs().
    void SetUrl(const CUrl& url);
    /// Reset URL.
    void ResetUrl(void);

    /// Check if content source is set.
    bool IsSetContentOverride(void) const;
    /// Get content source.
    EContentOverride GetContentOverride(void) const;
    /// Set content source.
    void SetContentOverride(EContentOverride content);
    /// Reset content source.
    void ResetContentOverride(void);

    /// Check if retry content is set and content-override flag assumes any
    /// content (eFromResponse or eData).
    bool IsSetContent(void) const;
    /// Get retry content.
    const string& GetContent(void) const;
    /// Set retry content. If the content is non-empty and content-override
    /// is eFromResponse or eData, the value should be sent by the server
    /// automatically.
    void SetContent(const string& content);
    /// Reset content.
    void ResetContent(void);

    /// Check if the client should reconnect (e.g. if URL or args have changed).
    bool NeedReconnect(void) const { return m_NeedReconnect; }
    /// Set need-reconnect flag.
    void SetNeedReconnect(void) { m_NeedReconnect = true; }
    /// Reset need-reconnect flag (e.g. after reconnecting to the new
    /// URL, but before reading new context).
    void ResetNeedReconnect(void) { m_NeedReconnect = false; }

    /// Map of name-value pairs.
    typedef map<string, string> TValues;
    /// Fill name-value pairs (e.g. to be used by a server application
    /// to serve retry information). The default implementaion does nothing.
    virtual void GetValues(TValues& /*values*/) const {}

private:
    enum FFlags {
        fStop =             1 << 0,
        fDelay =            1 << 1,
        fArgs =             1 << 2,
        fUrl =              1 << 3,
        fContentOverride =  1 << 4,
        fContent =          1 << 5
    };
    typedef int TFlags;

    bool x_IsSetFlag(FFlags f) const;
    void x_SetFlag(FFlags f, bool value = true);

    TFlags m_Flags;
    string m_StopReason;
    CTimeSpan m_Delay;
    string m_Args;
    string m_Url;
    EContentOverride m_ContentOverride;
    string m_Content;
    bool m_NeedRetry;
    bool m_NeedReconnect;
};


/// HTTP-specific retry context implementation.
class NCBI_XUTIL_EXPORT CHttpRetryContext : public CRetryContext
{
public:
    CHttpRetryContext(void) {}
    virtual ~CHttpRetryContext(void) {}

    // Copy functions from CRetryContext - required by CRPCClient_Base
    CHttpRetryContext(const CRetryContext& ctx);
    CHttpRetryContext& operator=(const CRetryContext& ctx);

    void ParseHeader(const char* http_header);

    /// Fills HTTP header names and values.
    virtual void GetValues(TValues& values) const;

    static const char* kHeader_Stop; // X-NCBI-Retry-Stop
    static const char* kHeader_Delay; // X-NCBI-Retry-Delay
    static const char* kHeader_Args; // X-NCBI-Retry-Args
    static const char* kHeader_Url; // X-NCBI-Retry-URL
    static const char* kHeader_Content; // X-NCBI-Retry-Content
    static const char* kContent_None; // no_content
    static const char* kContent_FromResponse; // from_response
    static const char* kContent_Value; // content:<url-encoded content>
};


/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//


inline
CRetryContext::CRetryContext(void)
    : m_Flags(0),
      m_ContentOverride(eNot_set),
      m_NeedRetry(false),
      m_NeedReconnect(false)
{
}


inline
bool CRetryContext::GetNeedRetry(void) const
{
    return m_NeedRetry;
}


inline
void CRetryContext::SetNeedRetry(void)
{
    m_NeedRetry = true;
}


inline
void CRetryContext::ResetNeedRetry(void)
{
    m_NeedRetry = false;
}


inline
void CRetryContext::Reset(void)
{
    ResetStop();
    ResetDelay();
    ResetArgs();
    ResetUrl();
    ResetContentOverride();
    ResetContent();
    ResetNeedRetry();
    ResetNeedReconnect();
}


inline
bool CRetryContext::IsSetStop(void) const
{
    return x_IsSetFlag(fStop);
}


inline
const string& CRetryContext::GetStopReason(void) const
{
    return m_StopReason;
}


inline
void CRetryContext::SetStop(const string& reason)
{
    x_SetFlag(fStop);
    m_StopReason = reason;
}


inline
void CRetryContext::ResetStop(void)
{
    x_SetFlag(fStop, false);
    m_StopReason.clear();
}


inline
bool CRetryContext::IsSetDelay(void) const
{
    return x_IsSetFlag(fDelay);
}


inline
const CTimeSpan& CRetryContext::GetDelay(void) const
{
    return m_Delay;
}


inline
void CRetryContext::SetDelay(CTimeSpan delay)
{
    x_SetFlag(fDelay);
    m_Delay = delay;
}


inline
void CRetryContext::SetDelay(double sec)
{
    x_SetFlag(fDelay);
    m_Delay.Set(sec);
}


inline
void CRetryContext::ResetDelay(void)
{
    x_SetFlag(fDelay, false);
    m_Delay.Clear();
}


inline
bool CRetryContext::IsSetArgs(void) const
{
    return x_IsSetFlag(fArgs);
}


inline
const string& CRetryContext::GetArgs(void) const
{
    return m_Args;
}


inline
void CRetryContext::SetArgs(const string& args)
{
    x_SetFlag(fArgs);
    m_Args = args;
}


inline
void CRetryContext::SetArgs(const CUrlArgs& args)
{
    x_SetFlag(fArgs);
    m_Args = args.GetQueryString(CUrlArgs::eAmp_Char);
}


inline
void CRetryContext::ResetArgs(void)
{
    x_SetFlag(fArgs, false);
    m_Args.clear();
}


inline
bool CRetryContext::IsSetUrl(void) const
{
    return x_IsSetFlag(fUrl);
}


inline
const string& CRetryContext::GetUrl(void) const
{
    return m_Url;
}


inline
void CRetryContext::SetUrl(const string& url)
{
    x_SetFlag(fUrl);
    m_Url = url;
}


inline
void CRetryContext::SetUrl(const CUrl& url)
{
    x_SetFlag(fUrl);
    m_Url = url.ComposeUrl(CUrlArgs::eAmp_Char);
}


inline
void CRetryContext::ResetUrl(void)
{
    x_SetFlag(fUrl, false);
    m_Url.clear();
}


inline
bool CRetryContext::IsSetContentOverride(void) const
{
    return x_IsSetFlag(fContentOverride)  &&
        m_ContentOverride != eNot_set;
}


inline
CRetryContext::EContentOverride
CRetryContext::GetContentOverride(void) const
{
    return m_ContentOverride;
}


inline
void CRetryContext::SetContentOverride(EContentOverride content)
{
    x_SetFlag(fContentOverride);
    m_ContentOverride = content;
}


inline
void CRetryContext::ResetContentOverride(void)
{
    x_SetFlag(fContentOverride, false);
    m_ContentOverride = eNot_set;
}


inline
bool CRetryContext::IsSetContent(void) const
{
    return x_IsSetFlag(fContent)  &&
        (m_ContentOverride == eData  || m_ContentOverride == eFromResponse)
        &&  !m_Content.empty();
}


inline
const string& CRetryContext::GetContent(void) const
{
    return m_Content;
}


inline
void CRetryContext::SetContent(const string& content)
{
    x_SetFlag(fContent);
    m_Content = content;
}


inline
void CRetryContext::ResetContent(void)
{
    x_SetFlag(fContent, false);
    m_Content.clear();
}


inline
bool CRetryContext::x_IsSetFlag(FFlags f) const
{
    return (m_Flags & f) == f;
}


inline
void CRetryContext::x_SetFlag(FFlags f, bool value)
{
    if ( value ) {
        m_Flags |= f;
    }
    else {
        m_Flags &= ~f;
    }
}


END_NCBI_SCOPE

#endif  /* RETRY_CTX__HPP */
