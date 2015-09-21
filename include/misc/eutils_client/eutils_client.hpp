#ifndef MISC_EUTILS_CLIENT___EUTILS_CLIENT__HPP
#define MISC_EUTILS_CLIENT___EUTILS_CLIENT__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>
#include <sstream>
#include <misc/xmlwrapp/xmlwrapp.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////

class CEUtilsException : public CException
{
public:
    enum EErrCode {
        ePhraseNotFound,
        eFieldNotFound,
        ePhraseIgnored,
        eQuotedPhraseNotFound,
        eOutputMessage
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CEUtilsException, CException);
};

/// Class for querying via E-Utils.
class CEutilsClient
{
public:
    /// Subclass this to override how messages (warnings and errors)
    /// are handled.
    ///
    /// For example, use this to stop on some kinds of errors.
    ///
    /// @see CAgpConverter::CErrorHandler for the inspiration for
    ///      the inspiration behind this class.
    class CMessageHandler : public CObject
    {
    public:
        virtual ~CMessageHandler(void) { }

        /// Pure virtual function, to be implemented by subclass.
        virtual void HandleMessage(EDiagSev severity,
                                   CEUtilsException::EErrCode err_code,
                                   const string & message) const = 0;
    };

    enum EUseHistory {
        eUseHistoryDisabled = 1,
        eUseHistoryEnabled
    };

    enum EContentType {
        eContentType_default = 1, // maps to "xml"
        eContentType_xml,
        eContentType_text,
        eContentType_html,
        eContentType_asn1 
    };

    CEutilsClient();
    CEutilsClient(const string& host);

    /// Default is to log all messages at informational level.
    /// Equivalent to: LOG_POST(Info << ...).
    void SetMessageHandlerDefault();

    /// Equivalent to: ERR_POST(Warning|Error << ...).
    void SetMessageHandlerDiagPost();

    /// Equivalent to: NCBI_THROW, ERR_POST, LOG_POST as appropriate.
    void SetMessageHandlerThrowOnError();

    /// Set custom message handler.
    void SetMessageHandler(CMessageHandler& message_handler);

    void SetUserTag(const string& tag);
    void SetMaxReturn(int ret_max);
    void SetLinkName(const string& link_name);

    Uint8 Count(const string& db,
                const string& term);

    //return the total count
    Uint8 Search(const string& db,
                const string& term,
                vector<int>& uids,
                string xml_path=kEmptyStr);

#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    Uint8 Search(const string& db,
                const string& term,
                vector<TGi>& uids,
                string xml_path=kEmptyStr);
#endif

    void Search(const string& db,
                const string& term,
                CNcbiOstream& ostr,
                EUseHistory use_history=eUseHistoryDisabled);

    void SearchHistory(const string& db,
                       const string& term,
                       string const& web_env,
                       Int8 query_key,
                       int retstart,
                       CNcbiOstream& ostr);


    void Link(const string& db_from,
              const string& db_to,
              const vector<int>& uids_from,
              vector<int>& uids_to,
              string xml_path=kEmptyStr,
              const string command="neighbor");

#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    void Link(const string& db_from,
              const string& db_to,
              const vector<TGi>& uids_from,
              vector<TGi>& uids_to,
              string xml_path=kEmptyStr,
              const string command="neighbor");
#endif

    void Link(const string& db_from,
              const string& db_to,
              const vector<int>& uids_from,
              CNcbiOstream& ostr,
              const string command="neighbor");

#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    void Link(const string& db_from,
              const string& db_to,
              const vector<TGi>& uids_from,
              CNcbiOstream& ostr,
              const string command="neighbor");

#endif

    void LinkHistory(const string& db_from,
                     const string& db_to,
                     const string& web_env,
                     Int8 query_key,
                     CNcbiOstream& ostr);



    void Summary(const string& db,
                const vector<int>& uids,
                xml::document& docsums,
                const string version="");

#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    void Summary(const string& db,
                const vector<TGi>& uids,
                xml::document& docsums,
                const string version="");
#endif

    void SummaryHistory(const string& db,
                        const string& web_env,
                        Int8 query_key,
                        int retstart,           // Position within the result set
                        const string version,   // Version: "" or "2.0" 
                        CNcbiOstream& ostr);

    void Fetch(const string& db,
               const vector<int>& uids,
               CNcbiOstream& ostr,
               const string& retmode="xml");

#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    void Fetch(const string& db,
               const vector<TGi>& uids,
               CNcbiOstream& ostr,
               const string& retmode="xml");
#endif

    void FetchHistory(const string& db,
                      const string& web_env,
                      Int8 query_key,
                      int retstart,
                      EContentType content_type,
                      CNcbiOstream& ostr);


    const list<string> GetUrl();
    const list<CTime> GetTime();

protected:
    Uint8 ParseSearchResults(CNcbiIstream& istr,
                             vector<int>& uids);
    
#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    Uint8 ParseSearchResults(CNcbiIstream& istr,
                             vector<TGi>& uids);
#endif
    
    Uint8 ParseSearchResults(const string& xml_file,
                             vector<int>& uids);

#if defined(NCBI_INT8_GI) || defined(NCBI_STRICT_GI)
    Uint8 ParseSearchResults(const string& xml_file,
                             vector<TGi>& uids);
#endif

private:
    CRef<CMessageHandler> m_MessageHandler;
    string m_HostName;
    string m_UrlTag;
    int m_RetMax;
    string m_LinkName;

    //exact url of last request
    //list holds multiple entries if multiple tries needed
    list<string> m_Url;

    //time of last request
    //list holds multiple entries if multiple tried needed
    list<CTime> m_Time;

    // Execute HTTP GET request.
    // Store received content into the @ostr.
    void x_Get(string const& path, string const& params, CNcbiOstream& ostr);
    string x_GetContentType(EContentType content_type);

    static string x_BuildUrl(const string& host, const string &path, const string &params);

    template<class T> Uint8 x_Search(const string& db,
                                   const string& term,
                                   vector<T>& uids,
                                   string xml_path=kEmptyStr);
    

    template<class T1, class T2> void x_Link(const string& db_from,
                                             const string& db_to,
                                             const vector<T1>& uids_from,
                                             vector<T2>& uids_to,
                                             string xml_path,
                                             const string command);

    template<class T> void x_Link(const string& db_from,
                                  const string& db_to,
                                  const vector<T>& uids_from,
                                  CNcbiOstream& ostr,
                                  const string command);

    template<class T> void x_Summary(const string& db,
                                     const vector<T>& uids,
                                     xml::document& docsums,
                                     const string version="");
    

    template<class T> void x_Fetch(const string& db,
                                   const vector<T>& uids,
                                   CNcbiOstream& ostr,
                                   const string& retmode="xml");

    template<class T> Uint8 x_ParseSearchResults(const string& xml_file,
                                                 vector<T>& uids);

    template<class T> Uint8 x_ParseSearchResults(CNcbiIstream& istr,
                                                 vector<T>& uids);

};




END_NCBI_SCOPE


#endif  // MISC_EUTILS_CLIENT___EUTILS_CLIENT__HPP
