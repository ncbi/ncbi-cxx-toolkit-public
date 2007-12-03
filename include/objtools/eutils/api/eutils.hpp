#ifndef EUTILS__HPP
#define EUTILS__HPP

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
*   EUtils base classes
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <connect/ncbi_types.h>
#include <corelib/ncbiobj.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/serialdef.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CEUtils_ConnContext
///
///  Connection context for EUtils. A context can be shared between
///  multiple requests.
///


class NCBI_EUTILS_EXPORT CEUtils_ConnContext : public CObject
{
public:
    CEUtils_ConnContext(void) {}
    virtual ~CEUtils_ConnContext(void) {}

    /// Get timeout
    const CTimeout& GetTimeout(void) const { return m_Timeout; }
    /// Set timeout
    void SetTimeout(const CTimeout& tmo) { m_Timeout = tmo; }

    /// Get WebEnv
    const string& GetWebEnv(void) const { return m_WebEnv; }
    /// Set WebEnv
    void SetWebEnv(const string& webenv) { m_WebEnv = webenv; }

    /// Get query_key
    const string& GetQueryKey(void) const { return m_QueryKey; }
    /// Set query_key
    void SetQueryKey(const string& query_key) { m_QueryKey = query_key; }

    /// A string with no internal spaces that identifies the resource
    /// which is using Entrez links (optional).
    const string& GetTool(void) const { return m_Tool; }
    void SetTool(const string& tool) { m_Tool = tool; }

    /// Optional contact e-mail.
    const string& GetEmail(void) const { return m_Email; }
    void SetEmail(const string& email) { m_Email = email; }

private:
    CTimeout m_Timeout;
    string   m_WebEnv;
    string   m_QueryKey;
    string   m_Tool;
    string   m_Email;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CEUtils_Request
///
///  Base class for all EUtils requests
///


class NCBI_EUTILS_EXPORT CEUtils_Request
{
public:
    /// Create request. If the context is NULL, a new empty context will be
    /// created for the request.
    CEUtils_Request(CRef<CEUtils_ConnContext>& ctx) : m_Context(ctx) {}
    virtual ~CEUtils_Request(void) {}

    /// Get CGI script name (e.g. efetch.fcgi).
    virtual string GetScriptName(void) const { return kEmptyStr; }

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Get serial stream format for reading data.
    virtual ESerialDataFormat GetSerialDataFormat(void) const
        { return eSerial_None; }

    /// Get current request context. This call does not disconnect the
    /// request, all changes made to the context will not be picked up
    /// until the new connection.
    CRef<CEUtils_ConnContext>& GetConnContext(void) const;

    /// Set new request context
    void SetConnContext(const CRef<CEUtils_ConnContext>& ctx);

    /// Open connection, create the stream.
    void Connect(void);

    /// Close connection, destroy the stream.
    void Disconnect(void) { m_Stream.reset(); }

    /// Get input stream for reading plain data. Auto connect if the stream
    /// does not yet exist.
    CNcbiIostream& GetStream(void);

    /// Get serial stream for reading xml or asn data.
    CObjectIStream* GetObjectIStream(void);

    /// Read the whole stream into the string. Discard data if
    /// the pointer is NULL.
    void Read(string* content);

    /// Database (usually set by each specific request class).
    const string& GetDatabase(void) const { return m_Database; }
    /// Setting new database disconnects the request.
    void SetDatabase(const string& database);

    /// Read query_key value from the request or from the connection context.
    const string& GetQueryKey(void) const;
    /// Override query_key stored in the connection context. Can be used to
    /// refer different queries from the history without changing current
    /// context. Disconnects the request.
    void SetQueryKey(const string& key);
    /// Reset requests's query_key, use the one from the connectiono context.
    void ResetQueryKey(void);

private:
    mutable CRef<CEUtils_ConnContext> m_Context;
    auto_ptr<CConn_HttpStream>        m_Stream;

    string           m_QueryKey; // empty = use value from ConnContext
    string           m_Database;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CEUtils_IdGroup
///
///  Group of IDs used in some requests. Represented in query string as
///  'id=123,456,78'.
///


class NCBI_EUTILS_EXPORT CEUtils_IdGroup
{
public:
    CEUtils_IdGroup(void) {}
    ~CEUtils_IdGroup(void) {}

    typedef vector<string> TIdList;

    /// Add a single id to the list.
    void AddId(const string& id) { m_Ids.push_back(id); }
    /// Get read-only list of ids.
    const TIdList& GetIds(void) const { return m_Ids; }
    /// Get non-const list of ids.
    TIdList& GetIds(void) { return m_Ids; }

    /// Parse all ids from a string (e.g. 'id=123,456,78').
    void SetIds(const string& ids);

    /// Get a formatted list of ids (e.g. 'id=123,456,78').
    string AsQueryString(void) const;

private:
    TIdList m_Ids;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CEUtils_IdGroupSet
///
///  A set of ID groups used in some requests. Represented in query string as
///  'id=12,34&id=56,78'.
///


class NCBI_EUTILS_EXPORT CEUtils_IdGroupSet
{
public:
    CEUtils_IdGroupSet(void) {}
    ~CEUtils_IdGroupSet(void) {}

    typedef vector<CEUtils_IdGroup> TIdGroupSet;

    /// Add a group of ids.
    void AddGroup(const CEUtils_IdGroup& group) { m_Groups.push_back(group); }
    /// Get read-only list of groups.
    const TIdGroupSet& GetGroups(void) const { return m_Groups; }
    /// Get non-const list of groups.
    TIdGroupSet& GetGroups(void) { return m_Groups; }

    /// Parse a set of id groups from a single string ('id=12,34&id=56,78').
    void SetGroups(const string& groups);

    /// Return a formatted list of groups ('id=12,34&id=56,78').
    string AsQueryString(void) const;

private:
    TIdGroupSet m_Groups;
};


/* @} */


END_NCBI_SCOPE

#endif  // EUTILS__HPP
