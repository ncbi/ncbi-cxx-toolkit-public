#ifndef ELINK__HPP
#define ELINK__HPP

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
*   ELink request
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/elink/ELinkResult.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CELink_Request
///
///  Search for Related Articles link


class NCBI_EUTILS_EXPORT CELink_Request : public CEUtils_Request
{
public:
    /// Create ELink request for the given destination database.
    /// Use "all" to retrieve links for all Entrez databases.
    CELink_Request(const string& db, CRef<CEUtils_ConnContext>& ctx);
    virtual ~CELink_Request(void);

    /// Get CGI script name (elink.fcgi).
    virtual string GetScriptName(void) const;

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Get serial stream format for reading data.
    virtual ESerialDataFormat GetSerialDataFormat(void) const;

    /// Get search result.
    CRef<elink::CELinkResult> GetELinkResult(void);

    /// Origination database.
    const string& GetDbFrom(void) const { return m_DbFrom; }
    void SetDbFrom(const string& dbfrom) { Disconnect(); m_DbFrom = dbfrom; }

    /// Multiple ID groups.
    const CEUtils_IdGroupSet& GetIdGroups(void) const { return m_IdGroups; }
    CEUtils_IdGroupSet& GetIdGroups(void) { Disconnect(); return m_IdGroups; }

    /// Search term.
    const string& GetTerm(void) const { return m_Term; }
    void SetTerm(const string& term) { Disconnect(); m_Term = term; }

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

    /// Limit dates to a specific date field based on database
    /// (e.g. edat, mdat).
    const string& GetDateType(void) const { return m_DateType; }
    void SetDateType(const string& type) { Disconnect(); m_DateType = type; }

    /// Output data types.
    enum ERetMode {
        eRetMode_none,
        eRetMode_xml,  /// Default mode is XML
        eRetMode_ref   ///< used only with cmd=prlinks
    };
    /// Output data type.
    ERetMode GetRetMode(void) const { return m_RetMode; }
    void SetRetMode(ERetMode retmode) { Disconnect(); m_RetMode = retmode; }

    /// ELink commands.
    enum ECommand {
        eCmd_none,
        eCmd_prlinks,          ///< Links to the primary provider
        eCmd_llinks,           ///< LinkOut URLs, except PubMed libraries
        eCmd_llinkslib,        ///< LinkOut URLs and Attributes
        eCmd_lcheck,           ///< Check for the existence of external links
        eCmd_ncheck,           ///< Check for the existence of neighbor links
        eCmd_neighbor,         ///< Display neighbors within a database (default)
        eCmd_neighbor_history, ///< Create history for use in other EUtils
        eCmd_acheck            ///< Entrez databases links
    };
    ECommand GetCommand(void) const { return m_Cmd; }
    void SetCommand(ECommand cmd) { Disconnect(); m_Cmd = cmd; }

    /// Link to a specific neighbor subset.
    /// A full list of allowed link names is available at
    /// http://eutils.ncbi.nlm.nih.gov/entrez/query/static/entrezlinks.html
    const string& GetLinkName(void) const { return m_LinkName; }
    void SetLinkName(const string& linkname)
        { Disconnect(); m_LinkName = linkname; }

    /// List LinkOut URLs for the specified holding provider.
    /// Used only in conjunction with cmd=llinks or cmd=llinkslib.
    const string& GetHolding(void) const { return m_Holding; }
    void SetHolding(const string& holding)
        { Disconnect(); m_Holding = holding; }

    /// Latest DTD version.
    const string& GetVersion(void) const { return m_Version; }
    void SetVersion(const string& version)
        { Disconnect(); m_Version = version; }

private:
    typedef CEUtils_Request TParent;

    const char* x_GetRetModeName(void) const;
    const char* x_GetCommandName(void) const;

    string             m_DbFrom;
    CEUtils_IdGroupSet m_IdGroups;
    string             m_Term;
    int                m_RelDate;
    CTime              m_MinDate;
    CTime              m_MaxDate;
    string             m_DateType; // ???
    ERetMode           m_RetMode;
    ECommand           m_Cmd;
    string             m_LinkName;
    string             m_Holding;
    string             m_Version;
};


/* @} */


END_NCBI_SCOPE

#endif  // ELINK__HPP
