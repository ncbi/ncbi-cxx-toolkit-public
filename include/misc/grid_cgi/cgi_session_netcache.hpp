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
#include <corelib/ncbiexpt.hpp>

#include <cgi/cgi_session.hpp>

#include <map>
#include <string>


BEGIN_NCBI_SCOPE

class IRegistry;
class IBlobStorage;

class NCBI_XGRIDCGI_EXPORT CCgiSession_NetCache : public ICgiSessionStorage
{
public:
    CCgiSession_NetCache(const IRegistry&);

    virtual ~CCgiSession_NetCache();

    virtual string CreateNewSession();
    virtual bool LoadSession(const string& sessionid);

    virtual TNames GetAttributeNames(void) const;

    virtual CNcbiIstream& GetAttrIStream(const string& name,
                                         size_t* size = 0);
    virtual CNcbiOstream& GetAttrOStream(const string& name);

    virtual void SetAttribute(const string& name, const string& value);
    virtual string GetAttribute(const string& name) const;  

    virtual void RemoveAttribute(const string& name);
    virtual void DeleteSession();

    virtual void Reset();
private:
    typedef map<string,string> TBlobs;

    string m_SessionId;    
    auto_ptr<IBlobStorage> m_Storage;
    
    TBlobs m_Blobs;    
    bool m_Dirty;

    bool m_Loaded;
    
    void x_CheckStatus() const;

    CCgiSession_NetCache(const CCgiSession_NetCache&);
    CCgiSession_NetCache& operator=(const CCgiSession_NetCache&);
};

class CCgiSessionNCException : public CException
{
public:
    enum EErrCode {
        eNotLoaded,
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eNotLoaded: return "eNotLoaded";
        default:      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCgiSessionNCException, CException);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/12/23 14:25:26  didenko
 * Renamed CCgiSession_Netcache to CCgiSession_NetCache
 *
 * Revision 1.4  2005/12/20 20:36:02  didenko
 * Comments cosmetics
 * Small interace changes
 *
 * Revision 1.3  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.2  2005/12/19 16:55:04  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.1  2005/12/15 18:21:15  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */

#endif  /* MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP */
