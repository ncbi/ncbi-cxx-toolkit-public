#ifndef MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP
#define MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP

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
* Author:  Maxim Didenko
*
*/

#include <stddef.h>

#include <cgi/cgi_session.hpp>

#include <map>
#include <string>


BEGIN_NCBI_SCOPE

class IRegistry;
class INetScheduleStorage;

class NCBI_XGRIDCGI_EXPORT CCgiSession_Netcache : public ICgiSession
{
public:
    CCgiSession_Netcache(const IRegistry&);

    virtual ~CCgiSession_Netcache();

    virtual void CreateNewSession();
    virtual EStatus LoadSession(const string& sessionid);
    virtual string GetSessionId() const;
    virtual EStatus GetStatus() const ;

    virtual void GetAttributeNames(TNames& names) const;

    virtual CNcbiIstream& GetAttrIStream(const string& name,
                                         size_t* size = 0);
    virtual CNcbiOstream& GetAttrOStream(const string& name);

    virtual void SetAttribute(const string& name, const string& value);
    virtual void GetAttribute(const string& name, string& value) const;  

    virtual void RemoveAttribute(const string& name);
    virtual void DeleteSession();

private:
    typedef map<string,string> TBlobs;

    void x_Reset();

    string m_SessionId;    
    auto_ptr<INetScheduleStorage> m_Storage;
    
    TBlobs m_Blobs;    
    bool m_Dirty;
    EStatus m_Status;
    
};

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/12/15 18:21:15  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */

#endif  /* MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP */
