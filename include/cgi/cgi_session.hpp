#ifndef CGI___SESSION__HPP
#define CGI___SESSION__HPP

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

#include <corelib/ncbistre.hpp>

#include <list>
#include <string>

/** @addtogroup CGI
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XCGI_EXPORT ICgiSession 
{
public:
    typedef list<string> TNames;

    /// Session status
    enum EStatus {
        eLoaded,     ///< The session is loaded
        eNotLoaded,  ///< The session has not been loaded yet
        eDeleted     ///< The session is deleted
    };

    virtual ~ICgiSession();

    /// Create a new empty session
    ///
    virtual void CreateNewSession() = 0;

    /// Load a session this the given session id from
    /// the underlying storage. If the session with given id
    /// is not found it returns eNotLoaded status.
    ///
    virtual EStatus LoadSession(const string& sessionid) = 0;

    /// Get a session id string. If the session is not load yet
    /// it returns an empty string.
    ///
    virtual string GetSessionId() const = 0;

    /// Get the session status
    ///
    virtual EStatus GetStatus() const = 0;

    /// Retrieve a list of attributes names attached to this session
    ///
    virtual void GetAttributeNames(TNames& names) const = 0;

    /// Get an input stream for an attribute with the given name
    /// If the attribute does not exist the empty stream is returned
    ///
    virtual CNcbiIstream& GetAttrIStream(const string& name, 
                                         size_t* size = 0) = 0;

    /// Get an output stream for an attribute with the given name
    /// If the attribute does not exist it will be create and added 
    /// to the session. If the attribute exists its content will be
    /// overwritten.
    ///
    virtual CNcbiOstream& GetAttrOStream(const string& name) = 0;

    /// Set a string attribute
    ///
    virtual void SetAttribute(const string& name, const string& value) = 0;

    /// Get a string attribute. If the attribute does not exist an empty
    /// string is returned
    ///
    virtual void GetAttribute(const string& name, string& value) const = 0;

    /// Remove an attribute from the session
    ///
    virtual void RemoveAttribute(const string& name) = 0;

    /// Delete the current session
    virtual void DeleteSession() = 0;
    
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

#endif  /* CGI___SESSION__HPP */
