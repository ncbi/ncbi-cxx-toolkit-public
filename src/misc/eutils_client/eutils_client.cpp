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
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbiapp.hpp>
#include <misc/eutils_client/eutils_client.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <misc/xmlwrapp/event_parser.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
#include <misc/error_codes.hpp>

#include <cmath>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <type_traits>

#define NCBI_USE_ERRCODE_X Misc_EutilsClient

BEGIN_NCBI_SCOPE

using namespace objects;


//////////////////////////////////////////////////////////////////////////////

namespace edirect {

    // Authors: Jonathan Kans, Aaron Ucko

    string Execute (
        const string& cmmd,
        const vector<string>& args,
        const string& data
    )

    {
        static const STimeout five_seconds = { 5, 0 };
        CConn_PipeStream ps(cmmd.c_str(), args, CPipe::fStdErr_Share, 0, &five_seconds);

        if ( ! data.empty() ) {
            ps << data;
            if (! NStr::EndsWith(data, "\n")) {
                ps << "\n";
            }
        }

        ps.flush();
        ps.GetPipe().CloseHandle(CPipe::fStdIn);

        string str;
        NcbiStreamToString(&str, ps);

        return str;
    }
}


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

template<class T> static void s_FormatIds(ostream& osm, const vector<T>& uids) {
    osm << "&id=";
    if (!uids.empty()) {
        osm << uids.front();
        for (auto it = uids.begin()+1; it != uids.end(); ++it) {
            osm << ',' << *it;
        }
    }
}

template<> void s_FormatIds<CSeq_id_Handle>(ostream& osm, const vector<CSeq_id_Handle>& uids) {
    osm << "&id=";
    CSeq_id::E_Choice type = CSeq_id::e_not_set;
    for (auto it = uids.begin(); it != uids.end(); ++it) {
        if (it != uids.begin()) {
            osm << ',';
        }
        auto& seh = *it;
        if (seh.Which() == CSeq_id::e_Gi) {
            if (type != CSeq_id::e_Gi && type != CSeq_id::e_not_set) {
                NCBI_THROW(CException, eUnknown,
                           "Argument list contains seq-ids of mixed types");
            }
            type = CSeq_id::e_Gi;
            osm << seh.GetGi();
        } else {
            if (type != CSeq_id::e_not_set && type != seh.Which()) {
                NCBI_THROW(CException, eUnknown,
                           "Argument list contains seq-ids of mixed types");
            }
            type = seh.Which();
            osm << seh.GetSeqId()->GetSeqIdString(true);
        }
    }
    osm << "&idtype=" << (type == CSeq_id::e_Gi ? "gi" : "acc");
}

template<> void s_FormatIds<string>(ostream& osm, const vector<string>& uids)
{
    osm << "&id=";
    if (!uids.empty()) {
        osm << uids.front();
        for (auto it = uids.begin()+1; it != uids.end(); ++it) {
            osm << ',' << *it;
        }
    }
    osm << "&idtype=acc";
}

template<class T> static inline T s_ParseId(const string& str)
{
    return NStr::StringToNumeric<T>(str);
}

template<> inline CSeq_id_Handle s_ParseId<CSeq_id_Handle>(const string& str)
{
    return CSeq_id_Handle::GetHandle(str);
}

template<> inline string s_ParseId<string>(const string& str)
{
    return str;
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
            if (err_code != CEUtilsException::ePhraseNotFound ||
                !IsTaxidQuery(message))
            {
                // NCBI_THROW, but wth err_code as a variable.
                throw CEUtilsException(DIAG_COMPILE_INFO, 0, err_code, message);
            }

            // This is a taxid query that happens to be be found in database;
            // treat as warning

            NCBI_FALLTHROUGH;

        default:
            // ERR_POST, but with severity as a variable.
            CNcbiDiag(DIAG_COMPILE_INFO, severity).GetRef()
                    << s_GetErrCodeString(err_code)
                    << ": "
                    << message << Endm;
        }
    }

private:

    bool IsTaxidQuery(const string &message) const {
        /// Taxid queries are of form "txid<number>[orgn]" or "txid<number>[progn"}
        /// The best way to check this would be using class CRegexp, but that
        /// could create a circular library dependency, so using more primitive
        /// string checking methods
        if (!NStr::StartsWith(message, "txid") ||
            (!NStr::EndsWith(message, "[orgn]") &&
             !NStr::EndsWith(message, "[porgn]")))
        {
            return false;
        }
        try {
            NStr::StringToUInt(message.substr(4, message.find('[')-4));
        } catch (CStringException &) {
            /// "taxid" string is not a number
            return false;
        }
        return true;
    }
};


class CEUtilsParser : public xml::event_parser
{
public:
    CEUtilsParser()
        : m_HasError(false)
    {
    }

    bool HasError(void) const            { return m_HasError; }
    void GetErrors(list<string>& errors) { errors = m_Errors; }

protected:
    bool error(const string& message)
    {
        ERR_POST(Error << "parse error: " << message);
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

    virtual bool OnEndElement(void) { return true; }
    
    string GetText(void) const { return NStr::Join(m_Text_chunks, ""); }

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


template<class T>
class CESearchParser : public CEUtilsParser
{
public:
    CESearchParser(vector<T>& uids,
                   CEutilsClient::CMessageHandler& message_handler)
        : m_MessageHandler(message_handler)
        , m_Count(0)
        , m_Uids(uids)
    { }

    ~CESearchParser()
    {
        ProcessMessages();
    }

    Uint8 GetCount(void) const { return m_Count; }

    /// Processes the warning and error messages from
    /// E-Utils, delivering them to the message handler
    /// and clearing them from the queue.
    ///
    /// The XML parser does not like exceptions thrown
    /// during the parse, so the messages are queued.
    void ProcessMessages(void)
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


    bool OnEndElement(void)
    {
        string contents = GetText();

        if (m_Path == "eSearchResult/Count") {
            m_Count = NStr::StringToUInt8(contents);
        }
        else if (x_IsSuffix(m_Path, "/IdList/Id")) {
            m_Uids.push_back(s_ParseId<T>(contents));
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
            TMessage message(CEUtilsException::eQuotedPhraseNotFound,contents);
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
    vector<T>& m_Uids;

    /// List of error messages from the E-Utils request.
    /// These are distinct from errors in parsing the
    /// E-Utils reply.
    list<TMessage> m_ResultErrors;

    /// List of warning messages from the E-Utils request.
    /// These are distinct from errors in parsing the
    /// E-Utils reply.
    list<TMessage> m_ResultWarnings;
};


template<class T>
class CELinkParser : public CEUtilsParser
{
public:
    CELinkParser(const string& dbfrom, const string& dbto,
                 vector<T>& uids)
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

    bool OnEndElement(void)
    {
        if ( !m_InLinkSet && NStr::EndsWith(m_Path, "/LinkName") ) {
            if ( GetText() == m_LinkName) {
                m_InLinkSet = true;
            }
        }
        else if (m_InLinkSet && NStr::EndsWith(m_Path, "/Link/Id") ) {
            m_Uids.push_back(s_ParseId<T>( GetText() ));
        }
        return true;
    }

private:
    string m_LinkName;
    vector<T>& m_Uids;
    bool m_InLinkSet;
};



//////////////////////////////////////////////////////////////////////////////


#define EUTILS_CLIENT_SECTION "eutils_client"
// Temporary limit to be virtually 'unlimited' but prevent server failure - see CXX-14065 and EU-3437
#define DEFAULT_RETMAX 100000000
// The default values approximate the old "sqrt(retry)" wait times.
#define DEFAULT_MAX_RETRIES 9
#define DEFAULT_WAIT_TIME 0
#define DEFAULT_WAIT_TIME_MULTIPLIER 1
#define DEFAULT_WAIT_TIME_INCREMENT 0.5
#define DEFAULT_WAIT_TIME_MAX 3


static CIncreasingTime::SAllParams s_WaitTimeParams = {
    {
        "wait_time",
        0,
        DEFAULT_WAIT_TIME
    },
    {
        "wait_time_max",
        0,
        DEFAULT_WAIT_TIME_MAX
    },
    {
        "wait_time_multiplier",
        0,
        DEFAULT_WAIT_TIME_MULTIPLIER
    },
    {
        "wait_time_increment",
        0,
        DEFAULT_WAIT_TIME_INCREMENT
    }
};


CEutilsClient::CEutilsClient()
    : m_CachedHostNameCount(0), m_RetMax(DEFAULT_RETMAX), m_MaxRetries(DEFAULT_MAX_RETRIES), m_WaitTime(s_WaitTimeParams)
{
    class CInPlaceConnIniter : protected CConnIniter
    {
    } conn_initer;  /*NCBI_FAKE_WARNING*/
    SetMessageHandlerDefault();
    x_InitParams();
}

CEutilsClient::CEutilsClient(const string& host)
    : m_CachedHostNameCount(0),
      m_HostName(host),
      m_RetMax(DEFAULT_RETMAX),
      m_MaxRetries(DEFAULT_MAX_RETRIES),
      m_WaitTime(s_WaitTimeParams)
{
    class CInPlaceConnIniter : protected CConnIniter
    {
    } conn_initer;  /*NCBI_FAKE_WARNING*/
    SetMessageHandlerDefault();
    x_InitParams();
}

void CEutilsClient::SetMessageHandlerDefault(void)
{
    m_MessageHandler.Reset(new CMessageHandlerDefault);
}

void CEutilsClient::SetMessageHandlerDiagPost(void)
{
    m_MessageHandler.Reset(new CMessageHandlerDiagPost);
}

void CEutilsClient::SetMessageHandlerThrowOnError(void)
{
    m_MessageHandler.Reset(new CMessageHandlerThrowOnError);
}

void CEutilsClient::SetMessageHandler(CMessageHandler& message_handler)
{
    m_MessageHandler.Reset(&message_handler);
}

void CEutilsClient::SetUserTag(const string& tag)
{
    m_UrlTag = tag;
}

void CEutilsClient::ClearAddedParameters()
{
    m_AdditionalParams.clear();
}

void CEutilsClient::AddParameter(const string &name, const string &value)
{
    m_AdditionalParams[name] = NStr::URLEncode(value);
}

void CEutilsClient::SetLinkName(const string& link_name)
{
    m_LinkName = link_name;
}

void CEutilsClient::SetMaxReturn(int ret_max)
{
    m_RetMax = ret_max;
}


//////////////////////////////////////////////////////////////////////////////

void CEutilsClient::x_InitParams(void)
{
    m_MaxRetries = DEFAULT_MAX_RETRIES;
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (app) {
        const CNcbiRegistry& reg = app->GetConfig();
        string max_retries = reg.GetString(EUTILS_CLIENT_SECTION, "max_retries", "");
        if (!max_retries.empty()) {
            try {
                m_MaxRetries = NStr::StringToNumeric<unsigned int>(max_retries);
            }
            catch (...) {
                ERR_POST("Invalid max_retries value: " << max_retries);
                m_MaxRetries = DEFAULT_MAX_RETRIES;
            }
        }
        string timeout = reg.GetString(EUTILS_CLIENT_SECTION, "conn_timeout", "");
        if (!timeout.empty()) {
            try {
                m_Timeout.Set(NStr::StringToDouble(timeout, NStr::fDecimalPosixOrLocal));
            }
            catch (...) {
                ERR_POST("Invalid conn_timeout value: " << timeout);
                m_Timeout.Set(CTimeout::eDefault);
            }
        }
        m_WaitTime.Init(reg, EUTILS_CLIENT_SECTION, s_WaitTimeParams);
    }
}


template<class Call>
typename std::invoke_result<Call>::type CEutilsClient::CallWithRetry(Call&& call, const char* name)
{
    for ( m_Attempt = 1; m_Attempt < m_MaxRetries; ++m_Attempt ) {
        try {
            return call();
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<"CEutilsClient::" << name << "() try " << m_Attempt << " exception: " << exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<"CEutilsClient::" << name << "() try " << m_Attempt << " exception: " << exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<"CEutilsClient::" << name << "() try " << m_Attempt << " exception");
        }
        double wait_sec = m_WaitTime.GetTime(m_Attempt - 1);
        LOG_POST(Warning<<"CEutilsClient: waiting " << wait_sec << "s before retry");
        SleepMilliSec(Uint4(wait_sec*1000));
    }
    return call();
}


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
    x_AddAdditionalParameters(params);

    LOG_POST(Trace << "Executing: db=" << db << " query=" << term);
    m_Url.clear();
    m_Time.clear();
    try {
        return CallWithRetry(bind(&CEutilsClient::x_CountOnce, this, cref(params)), "Count");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute query: " + term);
    }

    return 0;
}


Uint8 CEutilsClient::x_CountOnce(const string& params)
{
    string path = "/entrez/eutils/esearch.fcgi";
    string hostname = x_GetHostName();
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(x_BuildUrl(hostname, path, params));
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));
    vector<Int8> uids;
    CESearchParser<Int8> parser(uids, *m_MessageHandler);

    xml::error_messages msgs;
    parser.parse_stream(istr, &msgs);

    if (msgs.has_errors()  ||  msgs.has_fatal_errors()) {
        NCBI_THROW(CException, eUnknown,
                    "error parsing xml: " + msgs.print());
    }

    parser.ProcessMessages();
    return parser.GetCount();
}



#ifdef NCBI_INT8_GI
Uint8 CEutilsClient::ParseSearchResults(CNcbiIstream& istr,
                                        vector<int>& uids)
{
    return x_ParseSearchResults(istr, uids);
}
#endif

Uint8 CEutilsClient::ParseSearchResults(CNcbiIstream& istr,
                                        vector<CSeq_id_Handle>& uids)
{
    return x_ParseSearchResults(istr, uids);
}

Uint8 CEutilsClient::ParseSearchResults(CNcbiIstream& istr,
                                        vector<string>& uids)
{
    return x_ParseSearchResults(istr, uids);
}

Uint8 CEutilsClient::ParseSearchResults(CNcbiIstream& istr,
                                        vector<TEntrezId>& uids)
{
    return x_ParseSearchResults(istr, uids);
}

template<class T>
Uint8 CEutilsClient::x_ParseSearchResults(CNcbiIstream& istr,
                                          vector<T>& uids)
{
    CESearchParser<T> parser(uids, *m_MessageHandler);
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

#ifdef NCBI_INT8_GI
Uint8 CEutilsClient::ParseSearchResults(const string& xml_file,
                                        vector<int>& uids)
{
    return x_ParseSearchResults(xml_file, uids);
}
#endif

Uint8 CEutilsClient::ParseSearchResults(const string& xml_file,
                                        vector<CSeq_id_Handle>& uids)
{
    return x_ParseSearchResults(xml_file, uids);
}

Uint8 CEutilsClient::ParseSearchResults(const string& xml_file,
                                        vector<string>& uids)
{
    return x_ParseSearchResults(xml_file, uids);
}

Uint8 CEutilsClient::ParseSearchResults(const string& xml_file,
                                        vector<TEntrezId>& uids)
{
    return x_ParseSearchResults(xml_file, uids);
}

template<class T>
Uint8 CEutilsClient::x_ParseSearchResults(const string& xml_file,
                                          vector<T>& uids)
{
    CNcbiIfstream istr(xml_file.c_str());
    if ( !istr ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to open file: " + xml_file);
    }
    return ParseSearchResults(istr, uids);
}

#ifdef NCBI_INT8_GI
Uint8 CEutilsClient::Search(const string& db,
                           const string& term,
                           vector<int>& uids,
                           const string& xml_path)
{
    return x_Search(db, term, uids, xml_path);
}
#endif

Uint8 CEutilsClient::Search(const string& db,
                            const string& term,
                            vector<CSeq_id_Handle>& uids,
                            const string& xml_path)
{
    return x_Search(db, term, uids, xml_path);
}

Uint8 CEutilsClient::Search(const string& db,
                            const string& term,
                            vector<string>& uids,
                            const string& xml_path)
{
    return x_Search(db, term, uids, xml_path);
}


Uint8 CEutilsClient::Search(const string& db,
                            const string& term,
                            vector<TEntrezId>& uids,
                            const string& xml_path)
{
    return x_Search(db, term, uids, xml_path);
}

template<class T>
Uint8 CEutilsClient::x_Search(const string& db,
                              const string& term,
                              vector<T>& uids,
                              const string& xml_path)
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
    if (is_same<TEntrezId, T>::type::value || is_same<int, T>::type::value) {
        params += "&idtype=gi";
    } else {
        params += "&idtype=acc";
    }
    x_AddAdditionalParameters(params);
    
    LOG_POST(Trace << "Executing: db=" << db << " query=" << term);
    m_Url.clear();
    m_Time.clear();

    try {
        return CallWithRetry(bind(&CEutilsClient::x_SearchOnce<T>, this, cref(params), ref(uids), cref(xml_path)), "Search");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute query: " + term);
    }

    return 0;
}


template<class T>
Uint8 CEutilsClient::x_SearchOnce(const string& params,
                                  vector<T>& uids,
                                  const string& xml_path)
{
    Uint8 count = 0;

    string path = "/entrez/eutils/esearch.fcgi";
    string hostname = x_GetHostName();
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(x_BuildUrl(hostname, path, params));
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));

    if(!xml_path.empty()) {
        string xml_file = xml_path + '.' + NStr::NumericToString(m_Attempt);
        CNcbiOfstream ostr(xml_file.c_str());
        if (ostr.good()) {
            NcbiStreamCopy(ostr, istr);
            ostr.close();
                    
            if(!ostr  ||  200 != istr.GetStatusCode()) {
                NCBI_THROW(CException, eUnknown,
                           "Failure while writing entrez xml response"
                           " to file: " + xml_file);
            }

            count = ParseSearchResults(xml_file, uids);
        }
        else {
            ERR_POST(Error << "Unable to open file for writing: "
                        + xml_file);
            count = ParseSearchResults(istr, uids);
        }
    }
    else {
        count = ParseSearchResults(istr, uids);
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

static void s_SearchHistoryQuery(ostringstream& oss,
                                const string& db,
                                const string& term,
                                const string& web_env,
                                int retstart,
                                int retmax)
{
    oss << "db=" << NStr::URLEncode(db)
        << "&term=" << NStr::URLEncode(term)
        << "&retmode=xml";
    if ( retstart ) {
        oss << "&retstart="
            << retstart;
    }
    if (retmax) {
        oss << "&retmax="
            << retmax;
    }

    oss << "&usehistory=y"
        << "&WebEnv="
        << web_env;
}

void CEutilsClient::SearchHistory(const string& db,
                                  const string& term,
                                  const string& web_env,
                                  Int8 query_key,
                                  int retstart,
                                  CNcbiOstream& ostr)
{
    ostringstream oss;
    s_SearchHistoryQuery(oss, db, term, web_env, retstart, m_RetMax);
    if ( query_key > 0 ) {
        oss << "&query_key="
            << query_key;
    } 

    x_Get("/entrez/eutils/esearch.fcgi", oss.str(), ostr);
}

void CEutilsClient::SearchHistory(const string& db,
                                  const string& term,
                                  const string& web_env,
                                  CSeq_id_Handle query_key,
                                  int retstart,
                                  CNcbiOstream& ostr)
{
    ostringstream oss;
    s_SearchHistoryQuery(oss, db, term, web_env, retstart, m_RetMax);
    oss << "&query_key=" << query_key << "&idtype=acc";

    x_Get("/entrez/eutils/esearch.fcgi", oss.str(), ostr);
}

void CEutilsClient::SearchHistory(const string& db,
                                  const string& term,
                                  const string& web_env,
                                  const string& query_key,
                                  int retstart,
                                  CNcbiOstream& ostr)
{
    ostringstream oss;
    s_SearchHistoryQuery(oss, db, term, web_env, retstart, m_RetMax);
    oss << "&query_key=" << query_key << "&idtype=acc";

    x_Get("/entrez/eutils/esearch.fcgi", oss.str(), ostr);
}


#define HOST_NAME_REFRESH_FREQ 100

const string& CEutilsClient::x_GetHostName(void) const
{
    if (!m_HostName.empty()) {
        return m_HostName;
    }

    if (++m_CachedHostNameCount > HOST_NAME_REFRESH_FREQ) {
        m_CachedHostName.clear();
        m_CachedHostNameCount = 0;
    }

    if (!m_CachedHostName.empty()) {
        return m_CachedHostName;
    }

    // See also: objtools/eutils/api/eutils.cpp
    static const char kEutils[]   = "eutils.ncbi.nlm.nih.gov";
    static const char kEutilsLB[] = "eutils_lb";

    string host;
    SConnNetInfo* net_info = ConnNetInfo_Create(kEutilsLB);
    SSERV_Info*       info = SERV_GetInfo(kEutilsLB, fSERV_Dns,
                                          SERV_ANYHOST, net_info);
    ConnNetInfo_Destroy(net_info);
    if (info) {
        if (info->host) {
            host = CSocketAPI::ntoa(info->host);
        }
        free(info);
    }

    string scheme("http");
    if (host.empty()) {
        char buf[80];
        const char* web = ConnNetInfo_GetValue(kEutilsLB, REG_CONN_HOST,
                                               buf, sizeof(buf), kEutils);
        host = string(web  &&  *web ? web : kEutils);
        scheme += 's';
    }
    _ASSERT(!host.empty());
    m_CachedHostName = scheme + "://" + host;
    return m_CachedHostName;
}


void CEutilsClient::x_Get(string const& path, 
                          string const& params, 
                          CNcbiOstream& ostr)
{
    m_Url.clear();
    m_Time.clear();
    string extra_params{ params };
    x_AddAdditionalParameters(extra_params);
    try {
        CallWithRetry(bind(&CEutilsClient::x_GetOnce, this, cref(path), cref(extra_params), ref(ostr)), "Get");
    }
    catch (...) {
        ostringstream msg;
        msg << "Failed to execute request: "
            << path
            << "?"
            << params;
        NCBI_THROW(CException, eUnknown, msg.str());
    }
}


void CEutilsClient::x_GetOnce(string const& path, string const& extra_params, CNcbiOstream& ostr)
{
    string hostname = x_GetHostName();
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(x_BuildUrl(hostname, path, extra_params));
    istr << extra_params;
    m_Time.push_back(CTime(CTime::eCurrent));
    ostringstream reply;
    if (!NcbiStreamCopy(reply, istr)  ||  200 != istr.GetStatusCode()) {
        NCBI_THROW(CException, eUnknown, "Failure while reading response");
    }
    string reply_str = reply.str();
    istringstream reply_istr(reply.str());
    vector<TEntrezId> uids;
    /// We don't actually need the uids, but parse the reply anyway
    /// in order to catch errors
    ParseSearchResults(reply_istr, uids);

    /// No errors found in reply; copy to output stream
    ostr << reply_str;
}


//////////////////////////////////////////////////////////////////////////////

#ifdef NCBI_INT8_GI
void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<int>& uids_from,
                         vector<int>& uids_to,
                         const string& xml_path,
                         const string& command)

{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}
#endif

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<CSeq_id_Handle>& uids_from,
                         vector<CSeq_id_Handle>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<string>& uids_from,
                         vector<string>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<TEntrezId>& uids_from,
                         vector<TEntrezId>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}

#ifdef NCBI_INT8_GI
void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<int>& uids_from,
                         vector<TEntrezId>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<TEntrezId>& uids_from,
                         vector<int>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}
#endif

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<TEntrezId>& uids_from,
                         vector<CSeq_id_Handle>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<CSeq_id_Handle>& uids_from,
                         vector<TEntrezId>& uids_to,
                         const string& xml_path,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, uids_to, xml_path, command);
}

template<class T1, class T2>
void CEutilsClient::x_Link(const string& db_from,
                           const string& db_to,
                           const vector<T1>& uids_from,
                           vector<T2>& uids_to,
                           const string& xml_path,
                           const string& command)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&cmd=" << NStr::URLEncode(command);
    s_FormatIds(oss, uids_from);
    string params = oss.str();
    x_AddAdditionalParameters(params);

    m_Url.clear();
    m_Time.clear();

    try {
        CallWithRetry(bind(&CEutilsClient::x_LinkOnceT<T2>, this,
            cref(db_from), cref(db_to), ref(uids_to), cref(xml_path), cref(params)), "Link");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute elink request: " + params);
    }
}


template<class T>
void CEutilsClient::x_LinkOnceT(const string& db_from,
                                const string& db_to,
                                vector<T>& uids_to,
                                const string& xml_path,
                                const string& params)
{
    string path = "/entrez/eutils/elink.fcgi";
    string hostname = x_GetHostName();
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(x_BuildUrl(hostname, path, params));
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));
    CELinkParser<T> parser(db_from, db_to, uids_to);
    if ( !m_LinkName.empty() ) {
        parser.SetLinkName(m_LinkName);
    }
    xml::error_messages msgs;
    if(!xml_path.empty()) {
        string xml_file
            = xml_path + '.' + NStr::NumericToString(m_Attempt);

        CNcbiOfstream ostr(xml_file.c_str());
        if(ostr.good()) {
            NcbiStreamCopy(ostr, istr);
            ostr.close();
            parser.parse_file(xml_file.c_str(), &msgs);
            if(!ostr || 200 != istr.GetStatusCode()) {
                NCBI_THROW(CException, eUnknown,
                            "Failure while writing entrez xml response"
                            " to file: " + xml_file);
            }
        }
        else {
            ERR_POST(Error << "Unable to open file for writing: "
                        + xml_file);
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
}


#ifdef NCBI_INT8_GI
void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<int>& uids_from,
                         CNcbiOstream& ostr,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, ostr, command);
}
#endif

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<CSeq_id_Handle>& uids_from,
                         CNcbiOstream& ostr,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, ostr, command);
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<string>& uids_from,
                         CNcbiOstream& ostr,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, ostr, command);
}

void CEutilsClient::Link(const string& db_from,
                         const string& db_to,
                         const vector<TEntrezId>& uids_from,
                         CNcbiOstream& ostr,
                         const string& command)
{
    x_Link(db_from, db_to, uids_from, ostr, command);
}

template<class T>
void CEutilsClient::x_Link(const string& db_from,
                           const string& db_to,
                           const vector<T>& uids_from,
                           CNcbiOstream& ostr,
                           const string& command)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&cmd=" + NStr::URLEncode(command);
    s_FormatIds(oss, uids_from);
    string params = oss.str();
    x_AddAdditionalParameters(params);

    m_Url.clear();
    m_Time.clear();
    try {
        CallWithRetry(bind(&CEutilsClient::x_LinkOnce, this, ref(ostr), cref(params)), "Link");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute elink request: " + params);
    }
}


void CEutilsClient::x_LinkOnce(CNcbiOstream& ostr, const string& params)
{
    string path = "/entrez/eutils/elink.fcgi";
    string hostname = x_GetHostName();
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(x_BuildUrl(hostname, path, params));
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));
    if (!NcbiStreamCopy(ostr, istr)  ||  200 != istr.GetStatusCode()) {
        NCBI_THROW(CException, eUnknown, "Failure while reading response");
    }
}


void CEutilsClient::LinkHistory(const string& db_from,
                                const string& db_to,
                                const string& web_env,
                                Int8 query_key,
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

void CEutilsClient::LinkHistory(const string& db_from,
                                const string& db_to,
                                const string& web_env,
                                CSeq_id_Handle query_key,
                                CNcbiOstream& ostr)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&WebEnv=" << web_env 
        << "&query_key=" << query_key
        << "&idtype=acc";

    x_Get("/entrez/eutils/elink.fcgi", oss.str(), ostr);
}

void CEutilsClient::LinkHistory(const string& db_from,
                                const string& db_to,
                                const string& web_env,
                                const string& query_key,
                                CNcbiOstream& ostr)
{
    std::ostringstream oss;
    
    oss << "db=" << NStr::URLEncode(db_to)
        << "&dbfrom=" << NStr::URLEncode(db_from)
        << "&retmode=xml"
        << "&WebEnv=" << web_env 
        << "&query_key=" << query_key
        << "&idtype=acc";

    x_Get("/entrez/eutils/elink.fcgi", oss.str(), ostr);
}

#ifdef NCBI_INT8_GI
void CEutilsClient::LinkOut(const string& db,
                            const vector<int>& uids,
                            xml::document& doc,
                            const string& cmd)
{
    x_LinkOut(db, uids, doc, cmd);
}

#endif

void CEutilsClient::LinkOut(const string& db,
                            const vector<objects::CSeq_id_Handle>& uids,
                            xml::document& doc,
                            const string& cmd)
{
    x_LinkOut(db, uids, doc, cmd);
}

void CEutilsClient::LinkOut(const string& db,
                            const vector<string>& uids,
                            xml::document& doc,
                            const string& cmd)
{
    x_LinkOut(db, uids, doc, cmd);
}

void CEutilsClient::LinkOut(const string& db,
                            const vector<TEntrezId>& uids,
                            xml::document& doc,
                            const string& cmd)
{
    x_LinkOut(db, uids, doc, cmd);
}

template<class T> void CEutilsClient::x_LinkOut(const string& db,
                                              const vector<T>& uids,
                                              xml::document& doc,
                                              const string& cmd)
{
    ostringstream oss;
    oss << "dbfrom=" << NStr::URLEncode(db)
        << "&cmd=" << NStr::URLEncode(cmd)
        << "&retmode=xml";
    s_FormatIds(oss, uids);
    string params = oss.str();
    x_AddAdditionalParameters(params);

    m_Url.clear();
    m_Time.clear();
    try {
        CallWithRetry(bind(&CEutilsClient::x_LinkOutOnce, this, ref(doc), cref(params)), "LinkOut");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute esummary request: " + params);
    }
}


void CEutilsClient::x_LinkOutOnce(xml::document& doc, const string& params)
{
    string path = "/entrez/eutils/elink.fcgi?";
    string hostname = x_GetHostName();
    string url = x_BuildUrl(hostname, path, params);
    LOG_POST(Trace << "query: " << url);
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(url);
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));
    // slurp up all the output.
    stringbuf sb;
    istr >> &sb;
    if (200 != istr.GetStatusCode()) {
        NCBI_THROW(CException, eUnknown, "Failure while reading response");
    }
    string docstr(sb.str());
    // turn it into an xml::document
    xml::error_messages msgs;
    xml::document xmldoc(docstr.data(), docstr.size(), &msgs );
    doc.swap(xmldoc);
}


#ifdef NCBI_INT8_GI
void CEutilsClient::Summary(const string& db,
                            const vector<int>& uids,
                            xml::document& docsums,
                            const string& version)
{
    x_Summary(db, uids, docsums, version);
}
#endif

void CEutilsClient::Summary(const string& db,
                            const vector<CSeq_id_Handle>& uids,
                            xml::document& docsums,
                            const string& version)
{
    x_Summary(db, uids, docsums, version);
}

void CEutilsClient::Summary(const string& db,
                            const vector<string>& uids,
                            xml::document& docsums,
                            const string& version)
{
    x_Summary(db, uids, docsums, version);
}

void CEutilsClient::Summary(const string& db,
                            const vector<TEntrezId>& uids,
                            xml::document& docsums,
                            const string& version)
{
    x_Summary(db, uids, docsums, version);
}

template<class T>
void CEutilsClient::x_Summary(const string& db,
                              const vector<T>& uids,
                              xml::document& docsums,
                              const string& version)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=xml";
    if ( !version.empty() ) {
        oss << "&version=" 
            << version;
    } 
    s_FormatIds(oss, uids);
    string params = oss.str();
    x_AddAdditionalParameters(params);

    m_Url.clear();
    m_Time.clear();
    try {
        CallWithRetry(bind(&CEutilsClient::x_SummaryOnce, this, ref(docsums), cref(params)), "Summary");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute esummary request: " + params);
    }
}


void CEutilsClient::x_SummaryOnce(xml::document& docsums, const string& params)
{
    string path = "/entrez/eutils/esummary.fcgi?";
    string hostname = x_GetHostName();
    string url = x_BuildUrl(hostname, path, params);
    LOG_POST(Trace << "query: " << url);
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(url);
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));
    // slurp up all the output.
    stringbuf sb;
    istr >> &sb;
    if (200 != istr.GetStatusCode()) {
        NCBI_THROW(CException, eUnknown, "Failure while reading response");
    }
    string docstr(sb.str());
    // turn it into an xml::document
    xml::error_messages msgs;
    xml::document xmldoc(docstr.data(), docstr.size(), &msgs );
    docsums.swap(xmldoc);
}


static inline void s_SummaryHistoryQuery(ostream& oss,
                                         const string& db,
                                         const string& web_env,
                                         int retstart,
                                         const string version,
                                         int retmax)
{
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=xml"
        << "&WebEnv=" << web_env;

    if ( retstart > 0 ) {
        oss << "&retstart=" << retstart;
    }
   
    if ( retmax ) {
        oss << "&retmax=" << retmax;
    } 
        
    if ( !version.empty() ) {
        oss << "&version=" 
            << version;
    } 
}

void CEutilsClient::SummaryHistory(const string& db,
                                   const string& web_env,
                                   Int8 query_key,
                                   int retstart,
                                   const string& version,
                                   CNcbiOstream& ostr)
{
    ostringstream oss;
    s_SummaryHistoryQuery(oss, db, web_env, retstart, version, m_RetMax);
    oss << "&query_key=" << query_key;
    x_Get("/entrez/eutils/esummary.fcgi?", oss.str(), ostr);
}

void CEutilsClient::SummaryHistory(const string& db,
                                   const string& web_env,
                                   CSeq_id_Handle query_key,
                                   int retstart,
                                   const string& version,
                                   CNcbiOstream& ostr)
{
    ostringstream oss;
    s_SummaryHistoryQuery(oss, db, web_env, retstart, version, m_RetMax);
    oss << "&query_key=" << query_key
        << "&idtype=acc";
     x_Get("/entrez/eutils/esummary.fcgi?", oss.str(), ostr);
}

void CEutilsClient::SummaryHistory(const string& db,
                                   const string& web_env,
                                   const string& query_key,
                                   int retstart,
                                   const string& version,
                                   CNcbiOstream& ostr)
{
    ostringstream oss;
    s_SummaryHistoryQuery(oss, db, web_env, retstart, version, m_RetMax);
    oss << "&query_key=" << query_key
        << "&idtype=acc";
    x_Get("/entrez/eutils/esummary.fcgi?", oss.str(), ostr);
}

#ifdef NCBI_INT8_GI
void CEutilsClient::Fetch(const string& db,
                          const vector<int>& uids,
                          CNcbiOstream& ostr,
                          const string& retmode)
{
    x_Fetch(db, uids, ostr, retmode);
}
#endif

void CEutilsClient::Fetch(const string& db,
                          const vector<CSeq_id_Handle>& uids,
                          CNcbiOstream& ostr,
                          const string& retmode)
{
    x_Fetch(db, uids, ostr, retmode);
}

void CEutilsClient::Fetch(const string& db,
                          const vector<string>& uids,
                          CNcbiOstream& ostr,
                          const string& retmode)
{
    x_Fetch(db, uids, ostr, retmode);
}

void CEutilsClient::Fetch(const string& db,
                          const vector<TEntrezId>& uids,
                          CNcbiOstream& ostr,
                          const string& retmode)
{
    x_Fetch(db, uids, ostr, retmode);
}

template<class T>
void CEutilsClient::x_Fetch(const string& db,
                            const vector<T>& uids,
                            CNcbiOstream& ostr,
                            const string& retmode)
{
    ostringstream oss;
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=" << NStr::URLEncode(retmode);

    s_FormatIds(oss, uids);
    string params = oss.str();
    x_AddAdditionalParameters(params);

    m_Url.clear();
    m_Time.clear();
    try {
        CallWithRetry(bind(&CEutilsClient::x_FetchOnce, this, ref(ostr), cref(params)), "Fetch");
    }
    catch (...) {
        NCBI_THROW(CException, eUnknown,
                   "failed to execute efetch request: " + params);
    }
}


void CEutilsClient::x_FetchOnce(CNcbiOstream& ostr, const string& params)
{
    string path = "/entrez/eutils/efetch.fcgi";
    string hostname = x_GetHostName();
    STimeout timeout_value;
    const STimeout* timeout = g_CTimeoutToSTimeout(m_Timeout, timeout_value);
    CConn_HttpStream istr(x_BuildUrl(hostname, path, kEmptyStr), fHTTP_AutoReconnect, timeout);
    m_Url.push_back(x_BuildUrl(hostname, path, params));
    istr << params;
    m_Time.push_back(CTime(CTime::eCurrent));
    if (!NcbiStreamCopy(ostr, istr)  ||  200 != istr.GetStatusCode()) {
        NCBI_THROW(CException, eUnknown, "Failure while reading response");
    }
}


static inline string s_GetContentType(CEutilsClient::EContentType content_type)
{
    switch (content_type) {
    case CEutilsClient::eContentType_xml:
        return "xml";
    case CEutilsClient::eContentType_text:
        return "text";
    case CEutilsClient::eContentType_html:
        return "html";
    case CEutilsClient::eContentType_asn1:
        return "asn.1";
    default:
        break;
    }
    // Default content type
    return "xml";
}

static inline void s_FetchHistoryQuery(ostream& oss,
                                       const string& db,
                                       const string& web_env,
                                       int retstart,
                                       int retmax,
                                       CEutilsClient::EContentType content_type)
{
    oss << "db=" << NStr::URLEncode(db)
        << "&retmode=" << s_GetContentType(content_type)
        << "&WebEnv=" << web_env;
    if ( retstart > 0 ) {
        oss << "&retstart=" << retstart;
    }

    if ( retmax ) {
        oss << "&retmax=" << retmax;
    }
}

void CEutilsClient::FetchHistory(const string& db,
                                 const string& web_env,
                                 Int8 query_key,
                                 int retstart,
                                 EContentType content_type,
                                 CNcbiOstream& ostr)
{
    ostringstream oss;
    s_FetchHistoryQuery(oss, db, web_env, retstart, m_RetMax, content_type);
    oss << "&query_key=" << query_key;

    x_Get("/entrez/eutils/efetch.fcgi", oss.str(), ostr);
}

void CEutilsClient::FetchHistory(const string& db,
                                 const string& web_env,
                                 CSeq_id_Handle query_key,
                                 int retstart,
                                 EContentType content_type,
                                 CNcbiOstream& ostr)
{
    ostringstream oss;
    s_FetchHistoryQuery(oss, db, web_env, retstart, m_RetMax, content_type);
    oss << "&query_key=" << query_key
        << "&idtype=acc";

    x_Get("/entrez/eutils/efetch.fcgi", oss.str(), ostr);
}

void CEutilsClient::FetchHistory(const string& db,
                                 const string& web_env,
                                 const string& query_key,
                                 int retstart,
                                 EContentType content_type,
                                 CNcbiOstream& ostr)
{
    ostringstream oss;
    s_FetchHistoryQuery(oss, db, web_env, retstart, m_RetMax, content_type);
    oss << "&query_key=" << query_key
        << "&idtype=acc";

    x_Get("/entrez/eutils/efetch.fcgi", oss.str(), ostr);
}

const list<string> CEutilsClient::GetUrl() const
{
    return m_Url;
}

const list<CTime> CEutilsClient::GetTime() const
{
    return m_Time;
}

string CEutilsClient::x_BuildUrl(const string& host, const string &path,
                                 const string &params)
{
    string url = host + path;
    if(!params.empty()) {
        url += '?' + params;
    }
    return url;
}

inline void CEutilsClient::x_AddAdditionalParameters(string &params)
{
    if (m_AdditionalParams.empty())
        return;
    ostringstream oss;
    for (const TParamList::value_type &param : m_AdditionalParams) {
        oss << '&' << param.first << '=' << param.second;
    }
    params += oss.str();
}

END_NCBI_SCOPE
