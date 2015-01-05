#ifndef ESEARCH__HPP
#define ESEARCH__HPP

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
*   ESearch request
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/esearch/ESearchResult.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CESearch_Request
///
///  Searches and retrieves primary ids and term translations.
///


class NCBI_EUTILS_EXPORT CESearch_Request : public CEUtils_Request
{
public:
    /// Create ESearch request for the given database.
    CESearch_Request(const string& db, CRef<CEUtils_ConnContext>& ctx);
    virtual ~CESearch_Request(void);

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Get serial stream format for reading data.
    virtual ESerialDataFormat GetSerialDataFormat(void) const;

    /// Get search results. If WebEnv and query_key values are
    /// persent in the results, they are stored in the context.
    CRef<esearch::CESearchResult> GetESearchResult(void);

    /// History update flag, on by default. If set, WebEnv and query_key
    /// are updated to include the results in the history.
    bool GetUseHistory(void) const { return m_UseHistory; }
    void SetUseHistory(bool value) { Disconnect(); m_UseHistory = value; }

    /// Search term.
    const string& GetTerm(void) const { return m_Term; }
    void SetTerm(const string& term) { Disconnect(); m_Term = term; }

    /// Search field.
    const string& GetField(void) const { return m_Field; }
    void SetField(const string& field) { Disconnect(); m_Field = field; }

    /// Relative date to start search with, in days.
    /// 0 = not set
    int GetRelDate(void) const { return m_RelDate; }
    void SetRelDate(int days) { Disconnect(); m_RelDate = days; }

    /// Min date. Both min and max date must be set.
    const CTime& GetMinDate(void) const { return m_MinDate; }
    void SetMinDate(const CTime& date) { Disconnect(); m_MinDate = date; }

    /// Max date. Both min and max date must be set.
    const CTime& GetMaxDate(void) const { return m_MaxDate; }
    void SetMaxDate(const CTime& date) { Disconnect(); m_MaxDate = date; }

    /// Limit dates to a specific date field (e.g. edat, mdat).
    const string& GetDateType(void) const { return m_DateType; }
    void SetDateType(const string& type) { Disconnect(); m_DateType = type; }

    /// Sequential number of the first id retrieved. Default is 0 which
    /// will retrieve the first record.
    int GetRetStart(void) const { return m_RetStart; }
    void SetRetStart(int retstart) { Disconnect(); m_RetStart = retstart; }

    /// Number of items retrieved.
    int GetRetMax(void) const { return m_RetMax; }
    void SetRetMax(int retmax) { Disconnect(); m_RetMax = retmax; }

    /// Output data types.
    enum ERetType {
        eRetType_none,
        eRetType_count,
        eRetType_uilist
    };
    /// Output data type.
    ERetType GetRetType(void) const { return m_RetType; }
    void SetRetType(ERetType rettype) { Disconnect(); m_RetType = rettype; }

    /// Sort orders.
    /// @deprecated Use string sort orders instead.
    enum ESort {
        eSort_none,         // no sorting or sort order set by string
        eSort_author,       // author
        eSort_last_author,  // last+author
        eSort_journal,      // journal
        eSort_pub_date,     // pub+date
    };
    /// Get sort order. @deprecated Use GetSortOrderName().
    NCBI_DEPRECATED ESort GetSort(void) const { return m_Sort; }
    /// Set sort order. @deprecated Use SetSortOrderName().
    NCBI_DEPRECATED void SetSort(ESort order);

    /// Get sort order. Empty string indicates no sorting (or the default one).
    const string& GetSortOrderName(void) const { return m_SortName; }
    /// Set sort order. Empty string indicates no sorting (or the default one).
    void SetSortOrderName(CTempString name);

private:
    typedef CEUtils_Request TParent;

    const char* x_GetRetTypeName(void) const;

    bool       m_UseHistory;
    string     m_Term;
    string     m_Field;
    int        m_RelDate;
    CTime      m_MinDate;
    CTime      m_MaxDate;
    string     m_DateType; // ???
    int        m_RetStart;
    int        m_RetMax;
    ERetType   m_RetType;
    ESort      m_Sort;
    string     m_SortName;
};


/* @} */


END_NCBI_SCOPE

#endif  // ESEARCH__HPP
