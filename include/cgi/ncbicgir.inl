#if defined(NCBICGIR__HPP)  &&  !defined(NCBICGIR__INL)
#define NCBICGIR__INL

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
* Author: Eugene Vasilchenko
*
* File Description:
*  Inline methods of CGI response generator class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/12/11 18:00:53  vasilche
* Added cookies and output stream
*
* Revision 1.2  1998/12/10 19:58:19  vasilche
* Header option made more generic
*
* Revision 1.1  1998/12/09 20:18:11  vasilche
* Initial implementation of CGI response generator
*
* ===========================================================================
*/

inline void CCgiResponse::SetContentType(const string& type)
{
    SetHeaderValue(sm_ContentTypeName, type);
}

inline string CCgiResponse::GetContentType(void) const
{
    return GetHeaderValue(sm_ContentTypeName);
}

inline const CCgiCookies& CCgiResponse::Cookies(void) const
{
    return m_Cookies;
}

inline CCgiCookies& CCgiResponse::Cookies(void)
{
    return m_Cookies;
}

inline void CCgiResponse::AddCookie(const string& name, const string& value)
{
    m_Cookies.Add(name, value);
}

inline void CCgiResponse::AddCookie(const CCgiCookie& cookie)
{
    m_Cookies.Add(cookie);
}

inline void CCgiResponse::AddCookies(const CCgiCookies& cookies)
{
    m_Cookies.Add(cookies);
}

inline void CCgiResponse::RemoveCookie(const string& name)
{
    m_Cookies.Remove(name);
}

inline void CCgiResponse::RemoveAllCookies(void)
{
    m_Cookies.Clear();
}

inline bool CCgiResponse::HaveCookies(void) const
{
    return !m_Cookies.Empty();
}

inline bool CCgiResponse::HaveCookie(const string& name) const
{
    return m_Cookies.Find(name) != 0;
}

inline CCgiCookie* CCgiResponse::FindCookie(const string& name) const
{
    return m_Cookies.Find(name);
}

inline CNcbiOstream* CCgiResponse::SetOutput(CNcbiOstream* out)
{
    CNcbiOstream* old = m_Output;
    m_Output = out;
    return old;
}

inline CNcbiOstream* CCgiResponse::GetOutput(void) const
{
    return m_Output;
}

inline void CCgiResponse::Flush(void) const
{
    out() << NcbiFlush;
}

inline CNcbiOstream& CCgiResponse::WriteHeader(void) const
{
    return WriteHeader(out());
}

#endif /* def NCBICGIR__HPP  &&  ndef NCBICGIR__INL */
