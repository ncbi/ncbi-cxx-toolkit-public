#ifndef EPOST__HPP
#define EPOST__HPP

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
*   EPost request
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/epost/EPostResult.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CEPost_Request
///
///  Post a set if ids to the search history.
///


class NCBI_EUTILS_EXPORT CEPost_Request : public CEUtils_Request
{
public:
    /// Create EPost request for the given database.
    CEPost_Request(const string& db, CRef<CEUtils_ConnContext>& ctx);
    virtual ~CEPost_Request(void);

    /// Get CGI script name (epost.fcgi).
    virtual string GetScriptName(void) const;

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Get serial stream format for reading data.
    virtual ESerialDataFormat GetSerialDataFormat(void) const;

    /// Get search results. If WebEnv and query_key values are
    /// persent in the results, they are stored in the context.
    CRef<epost::CEPostResult> GetEPostResult(void);

    /// Group of ids to be added to the search history.
    const CEUtils_IdGroup& GetId(void) const { return m_Id; }
    CEUtils_IdGroup& GetId(void) { Disconnect(); return m_Id; }

private:
    typedef CEUtils_Request TParent;

    CEUtils_IdGroup m_Id;
};


/* @} */


END_NCBI_SCOPE

#endif  // EPOST__HPP
