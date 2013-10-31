#ifndef ESUMMARY__HPP
#define ESUMMARY__HPP

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
*   ESummary request
*
*/

#include <corelib/ncbistd.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/esummary/ESummaryResult.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CESummary_Request
///
///  Retreives document Summaries.
///


class NCBI_EUTILS_EXPORT CESummary_Request : public CEUtils_Request
{
public:
    /// Create ESummary request for the given database.
    CESummary_Request(const string& db, CRef<CEUtils_ConnContext>& ctx);
    virtual ~CESummary_Request(void);

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Get serial stream format for reading data.
    virtual ESerialDataFormat GetSerialDataFormat(void) const;

    /// Group of ids required if history is not used.
    const CEUtils_IdGroup& GetId(void) const { return m_Id; }
    CEUtils_IdGroup& GetId(void) { Disconnect(); return m_Id; }

    /// Sequential number of the first id retrieved. Default is 0 which
    /// will retrieve the first record.
    int GetRetStart(void) const { return m_RetStart; }
    void SetRetStart(int retstart) { Disconnect(); m_RetStart = retstart; }

    /// Number of items retrieved.
    int GetRetMax(void) const { return m_RetMax; }
    void SetRetMax(int retmax) { Disconnect(); m_RetMax = retmax; }

    /// Get search result. Depending on the requested database this call
    /// may fail (some databases use different DTDs for serialization).
    /// To get data from these databases one have to parse the data e.g. using
    /// XmlWrapp library.
    CRef<esummary::CESummaryResult> GetESummaryResult(void);

private:
    typedef CEUtils_Request TParent;

    CEUtils_IdGroup m_Id;
    int             m_RetStart;
    int             m_RetMax;
};


/* @} */


END_NCBI_SCOPE

#endif  // ESUMMARY__HPP
