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

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <misc/eutils_client/eutils_client.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <misc/xmlwrapp/event_parser.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <misc/error_codes.hpp>

#include <cmath>
#include <sstream>
#include <iterator>
#include <algorithm>

#define NCBI_USE_ERRCODE_X Misc_EutilsClient

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////


static const char*
s_GetErrCodeString(CEUtilsException::TErrCode err_code)
{
    switch ( err_code ) {
    case CEUtilsException::ePhraseNotFound:
        return "Phrase not found";
    case CEUtilsException::eFieldNotFound:
        return "Field not found";
    case CEUtilsException::ePhraseIgnored:
        return "Phrase ignored";
    case CEUtilsException::eQuotedPhraseNotFound:
        return "Quoted phrase not found";
    case CEUtilsException::eOutputMessage:
        return "Output message";
    default:
        return "Unknown error";
    }
}

const char* CEUtilsException::GetErrCodeString(void) const
{
    return s_GetErrCodeString(GetErrCode());
}

class CMessageHandlerDefault : public CEutilsClient::CMessageHandler
{
public:
    virtual void HandleMessage(EDiagSev severity,
                               CEUtilsException::EErrCode err_code,
                               const string & message) const
    {
        LOG_POST(Info << CNcbiDiag::SeverityName(severity)
                      << " - " << s_GetErrCodeString(err_code)
                      << ": " << message);
    }
};


class CMessageHandlerDiagPost : public CEutilsClient::CMessageHandler
{
public:
    virtual void HandleMessage(EDiagSev severity,
                               CEUtilsException::EErrCode err_code,
                               const string& message) const
    {
        // ERR_POST, but with severity as a variable.
        CNcbiDiag(DIAG_COMPILE_INFO, severity).GetRef()
                << s_GetErrCodeString(err_code)
                << ": "
                << message << Endm;
    }
};

class CMessageHandlerThrowOnError : public CEutilsClient::CMessageHandler
{
public:
    virtual void HandleMessage(EDiagSev severity,
                               CEUtilsException::EErrCode err_code,
                               const string& message) const
    {
        switch(severity) {
        case eDiag_Error:
        case eDiag_Critical:
        case eDiag_Fatal:
            // NCBI_THROW, but wth err_code as a variable.
            throw CEUtilsException(DIAG_COMPILE_INFO, 0, err_code, message);
        default:
            // ERR_POST, but with severity as a variable.
            CNcbiDiag(DIAG_COMPILE_INFO, severity).GetRef()
                    << s_GetErrCodeString(err_code)
                    << ": "
                    << message << Endm;
        }
    }
};

class CEUtilsParser : public xml::event_parser
{
public:
    CEUtilsParser()
        : m_HasError(false)
    {
    }

    bool HasError() const { return m_HasError; }
    void GetErrors(list<string>& errors)
    {
        errors = m_Errors;
    }

protected:
    bool error(const string& message)
    {
        LOG_POST(Error << "parse error: " << message);
        return false;
    }

    bool start_element(const string& name,
                       const attrs_type& attrs)
    {
        m_Text_chunks.clear();

        if ( !m_Path.empty() ) {
            m_Path += "/";
        }
        m_Path += name;
        return true;
    }

    bool end_element(const string& name)
    {
        bool result = OnEndElement();

        string::size_type pos = m_Path.find_last_of("/");
        if (pos != string::npos) {
            m_Path.erase(pos);
        }
        return result;
    }

    bool text(const string& contents) 
    {
        m_Text_chunks.push_back(contents);
        return true;
    }

    virtual bool OnEndElement()
    {
        return true;
    }
    
    string GetText() const { return NStr::Join(m_Text_chunks, ""); }

protected:
    string m_Path;
    /// List of parsing errors.
    /// @note Parsing errors deal with the syntax of a
    ///     reply from E-Utils. Parsing errors should be
    ///     distinguished from errors encountered in
    ///     processing a data request.
    list<string> m_Errors;
    bool m_HasError;
    // Accumulator of text chunks.
    list<string> m_Text_chunks;
};


class CESearchParser : public CEUtilsParser
{
public:
    CESearchParser(vector<int>& uids,
                   CEutilsClient::CMessageHandler& message_handler)
        : m_MessageHandler(message_handler)
        , m_Count(0)
        , m_Uids(uids)
    {
    }

    ~CESearchParser()
    {
        ProcessMessages();
    }

    Uint8 GetCount() const
    {
        return m_Count;
    }

    /// Processes the warning and error messages from
    /// E-Utils, delivering them to the message handler
    /// and clearing them from the queue.
    ///
    /// The XML parser does not like exceptions thrown
    /// during the parse, so the messages are queued.
    void ProcessMessages()
    {
        // Process warnings first; it's better to emit
        // as many messages as possible, and errors are
        // more likely to lead to premature exit.
        ITERATE(list<TMessage>, i, m_ResultWarnings) {
            m_MessageHandler.HandleMessage(eDiag_Warning,
                                           i->first, i->second);
        }
        m_ResultWarnings.clear();
        ITERATE(list<TMessage>, i, m_ResultErrors) {
            m_MessageHandler.HandleMessage(eDiag_Error,
                                           i->first, i->second);
        }
        m_ResultErrors.clear();
    }

protected:
    typedef pair<CEUtilsException::EErrCode, string> TMessage;

    bool x_IsSuffix(const string& s,
                    const char* suffix)
    {
        string::size_type pos = s.rfind(suffix);
        return (pos != string::npos  &&  pos == s.size() - strlen(suffix));
    }


    bool OnEndElement()
    {
        string contents = GetText();

        if (m_Path == "eSearchResult/Count") {
            m_Count = NStr::StringToUInt8(contents);
        }
        else if (x_IsSuffix(m_Path, "/IdList/Id")) {
            m_Uids.push_back(NStr::StringToInt(contents));
        }
        else if (x_IsSuffix(m_Path, "/ErrorList/PhraseNotFound")) {
            TMessage message(CEUtilsException::ePhraseNotFound, contents);
            m_ResultErrors.push_back(message);
        }
        else if (x_IsSuffix(m_Path, "/ErrorList/FieldNotFound")) {
            TMessage message(CEUtilsException::eFieldNotFound, contents);
            m_ResultErrors.push_back(message);
        }
        else if (x_IsSuffix(m_Path, "/WarningList/PhraseIgnored")) {
            TMessage message(CEUtilsException::ePhraseIgnored, contents);
            m_ResultWarnings.push_back(message);
        }
        else if (x_IsSuffix(m_Path, "/WarningList/QuotedPhraseNotFound")) {
            TMessage message(CEUtilsException::eQuotedPhraseNotFound, contents);
            m_ResultWarnings.push_back(message);
        }
        else if (x_IsSuffix(m_Path, "/WarningList/OutputMessage")) {
            TMessage message(CEUtilsException::eOutputMessage, contents);
            m_ResultWarnings.push_back(message);
        }
        else if (m_Path == "ERROR"  ||  m_Path == "eSearchResult/ERROR") {
            m_HasError = true;
            m_Errors.push_back(contents);
            return false;
        }
        return true;
    }

private:
    CEutilsClient::CMessageHandler& m_MessageHandler;
    Uint8 m_Count;
    vector<int>& m_Uids;

    /// List of error messages from the E-Utils request.
    /// These are distinct from errors in parsing the
    /// E-Utils reply.
    list<TMessage> m_ResultErrors;

    /// List of warning messages from the E-Utils request.
    /// These are distinct from errors in parsing the
    /// E-Utils reply.
    list<TMessage> m_ResultWarnings;
};



class CELinkParser : public CEUtilsParser
{
public:
    CELinkParser(const string& dbfrom, const string& dbto,
                 vector<int>& uids)
        : m_LinkName(dbfrom + "_" + dbto)
        , m_Uids(uids)
        , m_InLinkSet(false)
    {
        NStr::ToLower(m_LinkName);
    }

    void SetLinkName(const string& link_name)
    {
        m_LinkName = link_name;
        NStr::ToLower(m_LinkName);
    }

protected:

    bool end_element(const string& name)
    {
        CEUtilsParser::end_element(name);
        if (name == "LinkSetDb") {
            m_InLinkSet = false;
        }
        return true;
    }

    bool OnEndElement()
    {
        if ( !m_InLinkSet && NStr::EndsWith(m_Path, "/LinkName") ) {
            if ( GetText() == m_LinkName) {
                m_InLinkSet = true;
            }
        }
        else if (m_InLinkSet && NStr::EndsWith(m_Path, "/Link/Id") ) {
            m_Uids.push_back(NStr::StringToInt( GetText() ));
        }
        return true;
    }

private:
    string m_LinkName;
    vector<int>& m_Uids;
    bool m_InLinkSet;
};

/*
class CESummaryParser : public CEUtilsParser
{
public:
    CESummaryParser(vector<AutoPtr<xml::node> >& docsums)
        : m_Docsums(docsums)
    {
    }

private:
    vector<AutoPtr<xml::node> > & m_Docsums;
}
*/




/////////////////////////////////////////////////////////////////////////////
///

CEutilsClient::
CEutilsClient()
    : m_HostName("eutils.ncbi.nlm.nih.gov")
    , m_UrlTag("gpipe")
    , m_RetMax(kMax_Int)
{
    SetMessageHandlerDefault();
}

CEutilsClient::
CEutilsClient(const string& host)
    : m_HostName(host)
    , m_UrlTag("gpipe")
    , m_RetMax(kMax_Int)
{
    SetMessageHandlerDefault();
}

void CEutilsClient::
SetMessageHandlerDefault()
{
    m_MessageHandler.Reset(new CMessageHandlerDefault);
}

void CEutilsClient::
SetMessageHandlerDiagPost()
{
    m_MessageHandler.Reset(new CMessageHandlerDiagPost);
}
void CEutilsClient::
SetMessageHandlerThrowOnError()
{
    m_MessageHandler.Reset(new CMessageHandlerThrowOnError);
}

void CEutilsClient::
SetMessageHandler(CMessageHandler& message_handler)
{
    m_MessageHandler.Reset(&message_handler);
}

void CEutilsClient::
SetUserTag(const string& tag)
{
    m_UrlTag = tag;
}

void CEutilsClient::
SetLinkName(const string& link_name)
{
    m_LinkName = link_name;
}

void CEutilsClient::
SetMaxReturn(int ret_max)
{
    m_RetMax = ret_max;
}


//////////////////////////////////////////////////////////////////////////////


Uint8 CEutilsClient::Count(const string& db,
                           const string& term)
{
    string params;
    params += "db=" + NStr::URLEncode(db);
    params += "&term=" + NStr::URLEncode(term);
    params += "&retmode=xml&retmax=1";
    if ( !m_UrlTag.empty() ) {
        params += "&user=" + NStr::URLEncode(m_UrlTag);
    }

    Uint8 count = 0;
    LOG_POST(Info << " executing: db=" << db << "  query=" << term);
    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            string path = "/entrez/eutils/esearch.fcgi";
            CConn_HttpStream istr(m_HostName,
                                  path);
            m_Url.push_back(x_BuildUrl(m_HostName, path, params));

            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));
            vector<int> uids;
            CESearchParser parser(uids, *m_MessageHandler);

            xml::error_messages msgs;
            parser.parse_stream(istr, &msgs);

            if (msgs.has_errors()  ||  msgs.has_fatal_errors()) {
                NCBI_THROW(CException, eUnknown,
                           "error parsing xml: " + msgs.print());
            }

            parser.ProcessMessages();
            count = parser.GetCount();

            success = true;
            break;
        }
        catch (CException& e) {
            ERR_POST_X(1, Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = (int)::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute query: " + term);
    }

    return count;
}


Uint8 CEutilsClient::ParseSearchResults(CNcbiIstream& istr,
                                        vector<int>& uids)
{
    CESearchParser parser(uids, *m_MessageHandler);
    xml::error_messages msgs;
    parser.parse_stream(istr, &msgs);

    if (msgs.has_errors()  ||  msgs.has_fatal_errors()) {
        NCBI_THROW(CException, eUnknown,
                   "error parsing xml: " + msgs.print());
    }

    if (parser.HasError()) {
        list<string> errs;
        parser.GetErrors(errs);

        string msg = NStr::Join(errs, "; ");
        NCBI_THROW(CException, eUnknown,
                   "error returned from query: " + msg);
    }

    parser.ProcessMessages();
    return parser.GetCount();
}


Uint8 CEutilsClient::ParseSearchResults(const string& xml_file,
                                        vector<int>& uids)
{
    CNcbiIfstream istr(xml_file.c_str());
    if ( !istr ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to open file: " + xml_file);
    }
    return ParseSearchResults(istr, uids);
}


Uint8 CEutilsClient::Search(const string& db,
                           const string& term,
                           vector<int>& uids,
                           string xml_path)
{
    string params;
    params += "db=" + NStr::URLEncode(db);
    params += "&term=" + NStr::URLEncode(term);
    params += "&retmode=xml";
    if (m_RetMax) {
        params += "&retmax=" + NStr::NumericToString(m_RetMax);
    }
    if ( !m_UrlTag.empty() ) {
        params += "&user=" + NStr::URLEncode(m_UrlTag);
    }

    Uint8 count = 0;

    LOG_POST(Info << " executing: db=" << db << "  query=" << term);
    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            string path = "/entrez/eutils/esearch.fcgi";
            CConn_HttpStream istr(m_HostName,
                                  path);

            m_Url.push_back(x_BuildUrl(m_HostName, path, params));
            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));

            if(!xml_path.empty()) {
                string xml_file = xml_path + '.'+NStr::NumericToString(retries+1);
                
                CNcbiOfstream ostr(xml_file.c_str());
                if (ostr.good()) {
                    NcbiStreamCopy(ostr, istr);
                    ostr.close();

                    if(!ostr) {
                        NCBI_THROW(CException, eUnknown, "Failure while writing entrez xml response to file: " + xml_file);
                    }

                    count = ParseSearchResults(xml_file, uids);
                }
                else {
                    LOG_POST(Error << "Unable to open file for writing: " << xml_file);
                    count = ParseSearchResults(istr, uids);
                }
            }
            else {
                count = ParseSearchResults(istr, uids);
            }

            success = true;
            break;
        }
        catch (CException& e) {
            ERR_POST_X(2, Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = (int)::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute query: " + term);
    }
    return count;
}

void CEutilsClient::Search(const string& db,
                           const string& term,
                           CNcbiOstream& ostr,
                           EUseHistory use_history)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&term=" << NStr::URLEncode(term)
        << "&retmode=xml";
    if (m_RetMax) {
        oss << "&retmax="
            << m_RetMax;
    }

    if ( use_history == eUseHistoryEnabled ) {
        oss << "&usehistory=y";
    }

    x_Get("/entrez/eutils/esearch.fcgi", oss.str(), ostr);
}

void CEutilsClient::SearchHistory(const string& db,
                                  const string& term,
                                  const string& web_env,
                                  int query_key,
                                  int retstart,
                                  CNcbiOstream& ostr)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&term=" << NStr::URLEncode(term)
        << "&retmode=xml";
    if ( retstart ) {
        oss << "&retstart="
            << retstart;
    }
    if (m_RetMax) {
        oss << "&retmax="
            << m_RetMax;
    }

    oss << "&usehistory=y"
        << "&WebEnv="
        << web_env;
    if ( query_key > 0 ) {
        oss << "&query_key="
            << query_key;
    } 

    x_Get("/entrez/eutils/esearch.fcgi", oss.str(), ostr);
}

void CEutilsClient::x_Get(string const& path, 
                          string const& params, 
                          CNcbiOstream& ostr)
{
    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            CConn_HttpStream istr(m_HostName,
                                  path);
            m_Url.push_back(x_BuildUrl(m_HostName, path, params));
            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));
            if (NcbiStreamCopy(ostr, istr)) {
                success = true;
                break;
            }
        }
        catch (CException& e) {
            ERR_POST_X(3, Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = (int)::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        ostringstream msg;
        msg << "Failed to execute request: "
            << path
            << "?"
            << params;
        NCBI_THROW(CException, eUnknown, msg.str());
    }
}


//////////////////////////////////////////////////////////////////////////////


void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<int>& uids_from,
                         vector<int>& uids_to,
                         string xml_path,
                         const string command)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&cmd=" + NStr::URLEncode(command)
        << "&id=";
    std::copy(uids_from.begin(), uids_from.end(), std::ostream_iterator<int>(oss, ",") );
    string params = oss.str();
    // remove trailing comma
    params.resize(params.size() - 1);

    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            string path = "/entrez/eutils/elink.fcgi";
            CConn_HttpStream istr(m_HostName,
                                  path);

            m_Url.push_back(x_BuildUrl(m_HostName, path, params));
            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));
            CELinkParser parser(db_from, db_to, uids_to);
            if ( !m_LinkName.empty() ) {
                parser.SetLinkName(m_LinkName);
            }
            xml::error_messages msgs;
            if(!xml_path.empty()) {
                string xml_file = xml_path + '.'+NStr::NumericToString(retries+1);
               
                CNcbiOfstream ostr(xml_file.c_str());
                if(ostr.good()) {
                    NcbiStreamCopy(ostr, istr);
                    ostr.close();
                    parser.parse_file(xml_file.c_str(), &msgs);
                    if(!ostr) {
                        NCBI_THROW(CException, eUnknown, "Failure while writing entrez xml response to file: " + xml_file);
                    }
                }
                else {
                    LOG_POST(Error << "Unable to open file for writing: " << xml_file);
                    parser.parse_stream(istr, &msgs);
                }
            }
            else {
                parser.parse_stream(istr, &msgs);
            }

            if (msgs.has_errors()  ||  msgs.has_fatal_errors()) {
                NCBI_THROW(CException, eUnknown,
                           "error parsing xml: " + msgs.print());
            }
            success = true;
            break;
        }
        catch (CException& e) {
            ERR_POST_X(4, Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = (int)::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute elink request: " + params);
    }
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<int>& uids_from,
                         CNcbiOstream& ostr,
                         const string command)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&cmd=" + NStr::URLEncode(command)
        << "&id=";
    std::copy(uids_from.begin(), uids_from.end(), std::ostream_iterator<int>(oss, ",") );
    string params = oss.str();
    // remove trailing comma
    params.resize(params.size() - 1);

    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            string path = "/entrez/eutils/elink.fcgi";
            CConn_HttpStream istr(m_HostName,
                                  path);
            m_Url.push_back(x_BuildUrl(m_HostName, path, params));
            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));
            if (NcbiStreamCopy(ostr, istr)) {
                success = true;
                break;
            }
        }
        catch (CException& e) {
            ERR_POST_X(5, Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = (int)::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute elink request: " + params);
    }
}

void CEutilsClient::LinkHistory(const string& db_from,
                                const string& db_to,
                                const string& web_env,
                                int query_key,
                                CNcbiOstream& ostr)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&WebEnv=" << web_env 
        << "&query_key=" << query_key;

    x_Get("/entrez/eutils/elink.fcgi", oss.str(), ostr);
}

void CEutilsClient::Summary(const string& db,
                            const vector<int>& uids,
                            xml::document& docsums,
                            const string version)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=xml";
    if ( !version.empty() ) {
        oss << "&version=" 
            << version;
    } 
    oss << "&id=";
    std::copy(uids.begin(), uids.end(), std::ostream_iterator<int>(oss, ",") );
    string params = oss.str();
    // remove trailing comma
    params.resize(params.size() - 1);

    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            string path = "/entrez/eutils/esummary.fcgi?";
            LOG_POST(Info << "query: " << m_HostName + path + params );
            CConn_HttpStream istr(m_HostName,
                                  path);
            m_Url.push_back(x_BuildUrl(m_HostName, path, params));
            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));
            // slurp up all the output.
            stringbuf sb;
            istr >> &sb;
            string docstr(sb.str());

            // LOG_POST(Info << "Raw results: " << docstr );

            // turn it into an xml::document
            xml::error_messages msgs;
            xml::document xmldoc(docstr.data(), docstr.size(), &msgs );

            docsums.swap(xmldoc);
            success = true;
            break;
        }
        catch (const xml::parser_exception& e) {
            ERR_POST_X(6, Warning << "failed on attempt " << retries + 1
                     << ": error parsing xml: " << e.what());
        }
        catch (CException& e) {
            ERR_POST_X(6, Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = (int)::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute esummary request: " + params);
    }
}

void CEutilsClient::SummaryHistory(const string& db,
                                   const string& web_env,
                                   int query_key,
                                   int retstart,
                                   const string version,
                                   CNcbiOstream& ostr)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=xml"
        << "&WebEnv=" << web_env
        << "&query_key=" << query_key;

    if ( retstart > 0 ) {
        oss << "&retstart=" << retstart;
    }
   
    if ( m_RetMax ) {
        oss << "&retmax=" << m_RetMax;
    } 

        
    if ( !version.empty() ) {
        oss << "&version=" 
            << version;
    } 
 
    x_Get("/entrez/eutils/esummary.fcgi?", oss.str(), ostr);
}

void CEutilsClient::Fetch(const string& db,
                          const vector<int>& uids,
                          CNcbiOstream& ostr,
                          const string& retmode)
{
    string params;
    params += "db=" + NStr::URLEncode(db);
    params += "&retmode=" + NStr::URLEncode(retmode);

    string s;
    ITERATE (vector<int>, it, uids) {
        if ( !s.empty() ) {
            s += ",";
        }
        s += NStr::NumericToString(*it);
    }
    params += "&id=" + s;

    bool success = false;
    m_Url.clear();
    m_Time.clear();
    for (int retries = 0;  retries < 10;  ++retries) {
        try {
            string path = "/entrez/eutils/efetch.fcgi";
            CConn_HttpStream istr(m_HostName,
                                  path);
            m_Url.push_back(x_BuildUrl(m_HostName, path, params));
            istr << params;
            m_Time.push_back(CTime(CTime::eCurrent));
            if (NcbiStreamCopy(ostr, istr)) {
                success = true;
                break;
            }
        }
        catch (CException& e) {
            ERR_POST(Warning << "failed on attempt " << retries + 1
                     << ": " << e);
        }

        int sleep_secs = ::sqrt((double)retries);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }

    if ( !success ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute efetch request: " + params);
    }
}

void CEutilsClient::FetchHistory(const string& db,
                                 const string& web_env,
                                 int query_key,
                                 int retstart,
                                 EContentType content_type,
                                 CNcbiOstream& ostr)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=" << x_GetContentType(content_type)
        << "&WebEnv=" << web_env
        << "&query_key=" << query_key;

    if ( retstart > 0 ) {
        oss << "&retstart=" << retstart;
    }

    if ( m_RetMax ) {
        oss << "&retmax=" << m_RetMax;
    }

    x_Get("/entrez/eutils/efetch.fcgi", oss.str(), ostr);
}


const list<string> CEutilsClient::GetUrl()
{
    return m_Url;
}

const list<CTime> CEutilsClient::GetTime()
{
    return m_Time;
}


string CEutilsClient::x_BuildUrl(const string& host, const string &path, const string &params)
{
    string url = "http://" + host + path;
    if(!params.empty()) {
        url += '?' + params;
    }
    return url;
}

string CEutilsClient::x_GetContentType(EContentType content_type)
{
    if ( eContentType_xml == content_type ) {
        return "xml";
    }
    else if ( eContentType_text == content_type ) {
        return "text";
    }
    else if ( eContentType_html == content_type ) {
        return "html";
    }
    else if ( eContentType_asn1 == content_type ) {
        return "asn.1";
    }
    else {
        // Default content type
        return "xml";
    }
}

END_NCBI_SCOPE

