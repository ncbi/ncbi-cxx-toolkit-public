#ifndef ESPELL__HPP
#define ESPELL__HPP

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
*   ESpell request
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/espell/ESpellResult.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CESpell_Request
///
///  Retrieves spelling suggestions, if available.
///


class NCBI_EUTILS_EXPORT CESpell_Request : public CEUtils_Request
{
public:
    /// Create ESpell request for the given database.
    CESpell_Request(const string& db, CRef<CEUtils_ConnContext>& ctx);
    virtual ~CESpell_Request(void);

    /// Get CGI script name (espell.fcgi)
    virtual string GetScriptName(void) const;

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Get serial stream format for reading data.
    virtual ESerialDataFormat GetSerialDataFormat(void) const;

    /// Get search results.
    CRef<espell::CESpellResult> GetESpellResult(void);

    /// Search term.
    const string& GetTerm(void) const { return m_Term; }
    void SetTerm(const string& term) { Disconnect(); m_Term = term; }

private:
    typedef CEUtils_Request TParent;

    string m_Term;
};


/* @} */


END_NCBI_SCOPE

#endif  // ESPELL__HPP
