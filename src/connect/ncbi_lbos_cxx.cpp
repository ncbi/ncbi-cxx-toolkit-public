/*
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
 * Authors:  Dmitriy Elisov
 * Credits:  Denis Vakatov
 * @file
 * File Description:
 *   C++ Wrapper for the LBOS mapper written in C
 */
/* C++ */
#include <ncbi_pch.hpp>
#include <sstream>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <connect/ncbi_socket.hpp>
#include "ncbi_lbosp.hpp"
/* C */
#include "ncbi_lbosp.h"


BEGIN_NCBI_SCOPE

DEFINE_STATIC_FAST_MUTEX(s_IPCacheLock);
const string kLBOSAnnounceRegistrySection = "LBOS_ANNOUNCEMENT";
const string kLBOSServiceVariable         = "SERVICE";
const string kLBOSVersionVariable         = "VERSION";
const string kLBOSServerHostVariable      = "HOST";
const string kLBOSPortVariable            = "PORT";
const string kLBOSHealthcheckUrlVariable  = "HEALTHCHECK";
const string kLBOSMetaVariable            = "META";

const SConnNetInfo* kEmptyNetInfo = ConnNetInfo_Create(NULL);

/// Functor template for deleting object (for AutoPtr<>)
template<class X>
struct Free
{
    /// Default delete function.
    static void Delete(X* object)
    {
        free(*object);
    }
};


/// A structure to save parse results from LBOS /configuration
struct SLbosConfigure
{
    bool existed; /*< right before request */
    bool exists;  /*< after request  */
    string prev_version;
    string current_version;
};


CSafeStatic< map< CLBOSIpCacheKey, string > > CLBOSIpCache::sm_IpCache;


bool CLBOSIpCacheKey::operator==(const CLBOSIpCacheKey& rh) const
{
    return x_Service == rh.x_Service &&
        x_Hostname == rh.x_Hostname &&
        x_Version == rh.x_Version &&
        x_Port == rh.x_Port;
}


CLBOSIpCacheKey::CLBOSIpCacheKey(const string& service,
                                 const string& hostname,
                                 const string& version,
                                 unsigned short port) :
x_Service(service), x_Hostname(hostname), x_Version(version),
x_Port(port)
{

}

bool CLBOSIpCacheKey::operator<(const CLBOSIpCacheKey& rh) const
{
    if (x_Service != rh.x_Service) {
        return x_Service < rh.x_Service;
    }
    if (x_Hostname != rh.x_Hostname) {
        return x_Hostname < rh.x_Hostname;
    }
    if (x_Version != rh.x_Version) {
        return x_Version < rh.x_Version;
    }
    return x_Port < rh.x_Port;
}

bool CLBOSIpCacheKey::operator>(const CLBOSIpCacheKey& rh) const
{
    return !(*this == rh || *this < rh);
}


std::string CLBOSIpCache::HostnameTryFind(const string& service,
                                          const string& hostname_in,
                                          const string& version, 
                                          unsigned short port)
{
    string hostname = hostname_in;
    if (hostname.empty())
        hostname = CSocketAPI::HostPortToString(
                                          CSocketAPI::GetLocalHostAddress(), 0);
    map<CLBOSIpCacheKey, string>::iterator pos;
    CLBOSIpCacheKey key(service, hostname, version, port);
    {{
        CFastMutexGuard spawn_guard(s_IPCacheLock);
        pos = sm_IpCache->find(key);
        if (pos != sm_IpCache->end())
            return pos->second;
        else return hostname;
    }}
}

std::string CLBOSIpCache::HostnameResolve(const string& service,
                                          const string& hostname,
                                          const string& version,
                                          unsigned short port)
{
    /* Hostname should not be empty in any case */
    if (hostname.empty()) {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::eUnknown, 
            "Internal error in LBOS Client IP Cache. Please contact developer",
            eLBOS_BadRequest);
    }
    map<CLBOSIpCacheKey, string>::iterator pos;
    CLBOSIpCacheKey key(service, hostname, version, port);
    {{
        CFastMutexGuard spawn_guard(s_IPCacheLock);
        pos = sm_IpCache->find(key);
        /* If hostname is already in the cache, return previously resolved IP */
        if (pos != sm_IpCache->end())
            return pos->second;

    }}
    /* We did not find IP in the cache. Then we resolve the hostname to IP and
        save it to the cache. To make sure that our changes do not interfere
        with changes from another thread, we use 'insert' instead of '[]' (not
           to rewrite) */
    string host =
        CSocketAPI::HostPortToString(CSocketAPI::gethostbyname(hostname), 0);
    /* If gethostbyname() could not resolve the hostname, it will return ":0".
        In this case we go back to the original hostname, so that LBOS can try 
        to resolve it itself. */
    if (host == ":0") {
        host = hostname; 
    }
    {{
        CFastMutexGuard spawn_guard(s_IPCacheLock);
        /* Save pair */
        std::pair<map<CLBOSIpCacheKey, string>::iterator, bool> res =
            sm_IpCache->insert(std::pair<CLBOSIpCacheKey, string>(key, host));
        return res.first->second;
    }}
}

void CLBOSIpCache::HostnameDelete(const string& service,
                                  const string& hostname_in,
                                  const string& version,
                                  unsigned short port)
{
    string hostname = hostname_in;
    if (hostname.empty())
        hostname = CSocketAPI::HostPortToString(
                                          CSocketAPI::GetLocalHostAddress(), 0);
    map<CLBOSIpCacheKey, string>::iterator pos;
    CLBOSIpCacheKey key(service, hostname, version, port);
    {{
        CFastMutexGuard spawn_guard(s_IPCacheLock);
        pos = sm_IpCache->find(key);
        if (pos != sm_IpCache->end())
            sm_IpCache->erase(pos);
    }}
}


static void s_ProcessResult(unsigned short result, 
                            const char* lbos_answer,
                            const char* status_message)
{
    if (result == eLBOS_Success)
        return;

    stringstream message;
    message << result;

    if (status_message != NULL) {
        message << " " << status_message;
    }
    if (lbos_answer != NULL) {
        message << " " << lbos_answer;
    }
    throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::s_HTTPCodeToEnum(result),
            message.str(), result);
}


void LBOS::Announce(const string&  service, 
                    const string&  version,
                    const string&  host, 
                    unsigned short port,
                    const string&  healthcheck_url,
                    const string&  metadata)
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    string cur_host = host, ip;
    /* If host is empty, it means that host is the same as in healthcheck 
     * (by convention). We have to parse healthcheck and get host */
    if (cur_host.empty())
    {
        SConnNetInfo * healthcheck_info;
        healthcheck_info = ConnNetInfo_Clone(kEmptyNetInfo);
        healthcheck_info->host[0] = '\0'; /* to be sure that it will be
                                          * overridden                      */
        /* Save info about host */
        ConnNetInfo_ParseURL(healthcheck_info, healthcheck_url.c_str());
        cur_host = healthcheck_info->host;
        /* If we could not parse healthcheck URL, throw "Invalid Arguments" */
        if (cur_host.empty()) {
            ConnNetInfo_Destroy(healthcheck_info);
            throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                 CLBOSException::eInvalidArgs,
                                 NStr::IntToString(eLBOS_InvalidArgs),
                                 eLBOS_InvalidArgs);
        }
        ConnNetInfo_Destroy(healthcheck_info);
    }
    if (cur_host == "0.0.0.0") {
        ip = cur_host;
    } else {
        ip = CLBOSIpCache::HostnameResolve(service, cur_host, version, port);
    }
    /* If healthcheck is on the same host as server, we try to replace hostname 
     * with IP in healthcheck URL, too */
    string temp_healthcheck = NStr::Replace(healthcheck_url, cur_host, ip);
    unsigned short result = LBOS_Announce(service.c_str(), 
                                          version.c_str(),
                                          ip.c_str(),
                                          port,
                                          temp_healthcheck.c_str(),
                                          metadata.c_str(),
                                          &*body_aptr, 
                                          &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
}


void LBOS::Announce(const string& service, 
                    const string& version,
                    const string& host, 
                    unsigned short port,
                    const string& healthcheck_url,
                    const CMetaData& metadata)
{
    Announce(service, version, host, port, healthcheck_url, 
             metadata.GetMetaString());
}


void LBOS::AnnounceFromRegistry(const string& reg_sec)
{
    /* If "reg_section" is empty, we use default section. */
    const string& reg_section = reg_sec.empty() ? kLBOSAnnounceRegistrySection
                                                : reg_sec;
    LOG_POST(Error << "Registry section is " << reg_section);
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    string host =     config.Get(reg_section, kLBOSServerHostVariable);
    string service =  config.Get(reg_section, kLBOSServiceVariable);
    string version =  config.Get(reg_section, kLBOSVersionVariable);
    string port_str = config.Get(reg_section, kLBOSPortVariable);
    string health =   config.Get(reg_section, kLBOSHealthcheckUrlVariable);
    string meta   =   config.Get(reg_section, kLBOSMetaVariable);

    /* Check that port is a number between 1 and 65535   */
    int port_int = 0;
    try {
        port_int = NStr::StringToInt(port_str);
    }
    catch (...) {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs,
                             "Could not parse port \"" + port_str + 
                             "\" in section \"" + reg_section + "\"",
                             eLBOS_InvalidArgs);
    }
    if (port_int < 1 || port_int > 65535)
    {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs, 
                             "Invalid server port \"" + port_str + 
                             "\" in section \"" + reg_section + "\"",
                             eLBOS_InvalidArgs);
    }
    unsigned short port = static_cast<unsigned short>(port_int);
    Announce(service, version, host, port, health, meta);
}


void LBOS::DeannounceAll(void)
{
    LBOS_DeannounceAll();
}


void LBOS::Deannounce(const string&         service,
                      const string&         version,
                      const string&         host,
                      unsigned short        port)
{
    char* body_str = NULL, *status_message_str = NULL;
    string ip;
    if (host.empty() || host == "0.0.0.0") {
        ip = host;
    } else {
        ip = CLBOSIpCache::HostnameTryFind(service, host, version, port);
    }
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_Deannounce(service.c_str(), version.c_str(), 
                                            ip.c_str(), port, &*body_aptr, 
                                            &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
    /* Nothing thrown - deannounce successful! */
    /* Then remove resolution result from cache */
    if (host != "" /* invalid input */ &&
        host != "0.0.0.0" /* not handled by cache */) {
        CLBOSIpCache::HostnameDelete(service, host, version, port);
    }
}

/** Check LBOS response for /configuration request 
 * @param[in] lbos_answer
 *  Exact body of LBOS response/
 * @param[in] method_get
 *  If the method was GET, than we throw NOT FOUND if no current version was 
 *  returned 
 * @return 
 *  Filled structure with previous version and current version (or one of 
 *  them or none of them)*/
SLbosConfigure ParseLbosConfigureAnswer(const char* lbos_answer)
{
    SLbosConfigure res;
    if (lbos_answer == NULL) {
        lbos_answer = strdup("");
    }
    string body = lbos_answer;
    
    /* How the string we parse looks like:
     * <lbos>
     * <configuration name="/lbos" currentVersion="0.1.4-76-ami-c61703ae" 
     *                             previousVersion="0.1.4-76-ami-c61703ae"/>
     * </lbos>
     */
    /* First, just check if we can parse at least service name*/
    size_t name_start = body.find("name=\"") +
                        strlen("name=\"");
    if (name_start == 0) {
        ERR_POST(Error << "Could not parse ZK configuration answer");
        return res;
    } 
    size_t name_end = body.find("\"", name_start);
    string name = body.substr(name_start, name_end - name_start);
    /* Now let's get current version */
    size_t cur_ver_start = body.find("currentVersion=\"");
    res.exists = cur_ver_start != string::npos;
    if (res.exists) {
        cur_ver_start += strlen("currentVersion=\"");
        size_t cur_ver_end = body.find("\"", cur_ver_start);
        res.current_version = body.substr(cur_ver_start, 
                                          cur_ver_end - cur_ver_start);
    }
    /* Now let's get previous version, if possible  */
    size_t prev_ver_start = body.find("previousVersion=\"");
    res.existed = prev_ver_start != string::npos;
    if (res.existed) {
        prev_ver_start += strlen("previousVersion=\"");
        size_t prev_ver_end = body.find("\"", prev_ver_start);
        res.prev_version = body.substr(prev_ver_start,
                                       prev_ver_end - prev_ver_start);
    }
    return res;
}


string LBOSPrivate::GetServiceVersion(const string&  service,
                                      bool*          exists) 
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_ServiceVersionGet(service.c_str(), 
                                                   &*body_aptr,
                                                   &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
    SLbosConfigure res = ParseLbosConfigureAnswer(*body_aptr);
    if (exists != NULL) {
        *exists = res.exists;
    }
    return res.current_version;
}


string LBOSPrivate::SetServiceVersion(const string&  service,
                                      const string&  new_version,
                                      bool* existed)
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_ServiceVersionSet(service.c_str(),
                                                   new_version.c_str(),
                                                   &*body_aptr,
                                                   &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
    SLbosConfigure res = ParseLbosConfigureAnswer(*body_aptr);
    if (existed != NULL) {
        *existed = res.existed;
    }
    return res.prev_version;
}


string LBOSPrivate::DeleteServiceVersion(const string&  service,
                                         bool* existed)
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_ServiceVersionDelete(service.c_str(),
                                                      &*body_aptr,
                                                      &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
    SLbosConfigure res = ParseLbosConfigureAnswer(*body_aptr);
    if (existed != NULL) {
        *existed = res.existed;
    }
    return res.prev_version;
}


CLBOSException::EErrCode CLBOSException::s_HTTPCodeToEnum(unsigned short code) 
{
    switch (code) {
    case eLBOS_LbosNotFound:
        return eLbosNotFound;
    case eLBOS_NotFound:
        return eNotFound;
    case eLBOS_BadRequest:
        return eBadRequest;
    case eLBOS_Disabled:
        return eDisabled;
    case eLBOS_InvalidArgs:
        return eInvalidArgs;
    case eLBOS_DNSResolve:
        return eDNSResolve;
    case eLBOS_MemAlloc:
        return eMemAlloc;
    case eLBOS_Protocol:
        return eProtocol;
    case eLBOS_Server:
        return eServer;
    default:
        return eUnknown;
    }
}


unsigned short CLBOSException::GetStatusCode(void) const {
    return m_StatusCode;
}


const char* CLBOSException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    /* 400 */
    case eBadRequest:
        return "";
    /* 404 */
    case eNotFound:
        return "";
    /* 500 */
    case eServer:
        return "";
    /* 450 */
    case eLbosNotFound:
        return "LBOS was not found";
    /* 451 */
    case eDNSResolve:
        return "DNS error. Possibly, cannot get IP of current machine or "
               "resolve provided hostname for the server";
    /* 452 */
    case eInvalidArgs:
        return "Invalid arguments were provided. No request to LBOS was sent";
    /* 453 */
    case eMemAlloc:
        return "Memory allocation error happened while performing request";
    /* 454 */
    case eProtocol:
        return "Failed to parse LBOS output.";
    /* 550 */
    case eDisabled:
        return "LBOS functionality is turned OFF. Check config file or "
               "connection to LBOS.";
    default:
        return "Unknown LBOS error code";
    }
}


CLBOSException::CLBOSException(void) : m_StatusCode(0)
{
}


CLBOSException::CLBOSException(const CDiagCompileInfo& info, 
                               const CException* prev_exception, 
                               const CExceptionArgs<EErrCode>& args, 
                               const string& message, 
                               unsigned short status_code) 
    : CException(info, prev_exception, (message),
      args.GetSeverity(), 0)
{

    x_Init(info, message, prev_exception, args.GetSeverity());
    x_InitArgs(args);
    x_InitErrCode((CException::EErrCode) args.GetErrCode());
    m_StatusCode = status_code;
    stringstream message_builder;
    message_builder << "Error: "
        << status_code << " " << GetErrCodeString() << endl;
    m_Message = message_builder.str();
}


CLBOSException::CLBOSException(const CDiagCompileInfo& info, 
                               const CException* prev_exception, 
                               EErrCode err_code, 
                               const string& message, 
                               unsigned short status_code, 
                               EDiagSev severity /*= eDiag_Error*/) 
    : CException(info, prev_exception, (message), severity, 0)
{
    x_Init(info, message, prev_exception, severity);
    x_InitErrCode((CException::EErrCode) err_code);
    m_StatusCode = status_code;
    stringstream message_builder;
    message_builder << "Error: "
        << message << endl;
    m_Message = message_builder.str();
}

const char* CLBOSException::what() const throw()
{

    return m_Message.c_str();
}

CLBOSException::~CLBOSException(void) throw()
{

}

const char* CLBOSException::GetType(void) const
{
    return "CLBOSException";
}

CLBOSException::TErrCode CLBOSException::GetErrCode(void) const
{
    return typeid(*this) == typeid(CLBOSException) ?
        (TErrCode) this->x_GetErrCode() :
        (TErrCode)CException::eInvalid;
}

const CException* CLBOSException::x_Clone(void) const
{
    return new CLBOSException(*this);
}

#ifdef LBOS_METADATA
LBOS::CMetaData::CMetaData()
{
}


void LBOS::CMetaData::Set(const CTempString name_in, const CTempString val_in)
{
    string name = name_in;
    /* First, transform name to lower register to search it */
    NStr::ToLower(name);
    /* Forbidden names for meta parameters */
    if (name == "version" || name == "ip" || name == "port" || name == "check" 
        || name == "format" || name == "name") {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs, 
                             "This name cannot be used for metadata", 
                             eLBOS_InvalidArgs);
    }
    /* If val is empty, we delete the value from m_Meta */
    if (val_in.empty()) {
        auto iter = m_Meta.find(name);
        if (iter != m_Meta.end()) {
            m_Meta.erase(iter);
        }
    }
    else {
        /* If val is not empty, save it to m_Meta or rewrite previous value */
        m_Meta[name] = val_in;
    }
}


string LBOS::CMetaData::Get(const string& name) const
{
    auto iter = m_Meta.find(name);
    if (iter != m_Meta.end()) {
        return iter->second;
    }
    return "";
}


void LBOS::CMetaData::GetNames(list<string>& cont)
{
    auto iter = m_Meta.begin();
    for (; iter != m_Meta.end(); iter++) {
        cont.push_back(iter->first);
    }
}


void LBOS::CMetaData::GetNames(vector<string>& cont)
{
    auto iter = m_Meta.begin();
    for (; iter != m_Meta.end(); iter++) {
        cont.push_back(iter->first);
    }
}


void LBOS::CMetaData::SetRate(const string& rate)
{
    if (rate.empty()) {
        Set("rate", "");
    } else {
        try {
            SetRate(NStr::StringToInt(rate));
        } 
        catch (const CStringException&) {
            throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                 CLBOSException::eInvalidArgs,
                                 "Could not parse string value for SetRate",
                                 eLBOS_InvalidArgs);
        }
    }
}


void LBOS::CMetaData::SetRate(double rate)
{
    if (rate == 0)
        Set("rate", "");
    else
        Set("rate", NStr::DoubleToString(rate));
}


double LBOS::CMetaData::GetRate() const
{
    string rate = Get("rate");
    if (rate.empty())
        return 0;
    try {
        return NStr::StringToDouble(rate);
    }
    catch (const CStringException&) {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs,
                             "Value in \"rate\" meta parameter cannot "
                             "be represented as an integer",
                             eLBOS_InvalidArgs);
    }
}


void LBOS::CMetaData::SetType(const string& host_type)
{
    if (host_type.find_first_of(" \t\n\v\f\r") != string::npos) {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::eInvalidArgs,
            "This convenience function throws on whitespace characters "
            "in \"type\" meta parameter. If you know what you are doing, "
            "you can use CMetaData::Set(\"type\", ...)",
            eLBOS_InvalidArgs);
    }
    string type = host_type;
    type = NStr::ToUpper(type);
    Set("type", type);
}


void LBOS::CMetaData::SetType(EHostType host_type)
{
    switch (host_type) {
    case eHTTP:
        SetType("HTTP");
        break;
    case eHTTP_POST:
        SetType("HTTP_POST");
        break;
    case eHTTP_GET:
        SetType("HTTP_GET");
        break;
    case eStandalone:
        SetType("STANDALONE");
        break;
    case eNCBID:
        SetType("NCBID");
        break;
    case eDNS:
        SetType("DNS");
        break;
    case eFirewall:
        SetType("FIREWALL");
        break;
    case eNone:
        SetType("");
        break;
    default:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs, "Unknown EHostType "
                             "value. If you are sure that a correct value is "
                             "used, please tell the developer about this issue", 
                             eLBOS_InvalidArgs);
    }
}


void LBOS::CMetaData::SetType(ESERV_Type host_type)
{
    switch (host_type) {
    case fSERV_Http:
        SetType("HTTP");
        break;
    case fSERV_HttpPost:
        SetType("HTTP_POST");
        break;
    case fSERV_HttpGet:
        SetType("HTTP_GET");
        break;
    case fSERV_Standalone:
        SetType("STANDALONE");
        break;
    case fSERV_Ncbid:
        SetType("NCBID");
        break;
    case fSERV_Dns:
        SetType("DNS");
        break;
    case fSERV_Firewall:
        SetType("FIREWALL");
        break;
    default:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs, "Unknown ESERV_Type "
                             "value. If you are sure that a correct value is "
                             "used, please tell the developer about this issue", 
                             eLBOS_InvalidArgs);
    }
}


void LBOS::CMetaData::SetType(int host_type)
{
    switch (host_type) {
    case (int)eHTTP:
        SetType("HTTP");
        break;
    case (int)eHTTP_POST:
        SetType("HTTP_POST");
        break;
    case (int)eStandalone:
        SetType("STANDALONE");
        break;
    case (int)eNCBID:
        SetType("NCBID");
        break;
    case (int)eDNS:
        SetType("DNS");
        break;
    case (int)eNone:
        SetType("");
        break;
    default:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::eInvalidArgs, "Unknown EHostType "
                             "value. If you are sure that a correct value is "
                             "used, please tell the developer about this issue",
                             eLBOS_InvalidArgs);
    }
}


string LBOS::CMetaData::GetType(bool) const
{
    string type = Get("type");
    return NStr::ToUpper(type);
}


LBOS::CMetaData::EHostType LBOS::CMetaData::GetType() const
{
    string type = GetType(true);
    if (type == "HTTP") {
        return eHTTP;
    }
    else if (type == "HTTP_POST") {
        return eHTTP_POST;
    }
    else if (type == "STANDALONE") {
        return eStandalone;
    }
    else if (type == "NCBID") {
        return eNCBID;
    }
    else if (type == "DNS") {
        return eDNS;
    }
    else if (type.empty()) {
        return eNone;
    }
    else {
        return eUnknown;
    }
}


void LBOS::CMetaData::SetExtra(const string& extra)
{
    if (extra.find_first_of(" \t\n\v\f\r") != string::npos) {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::eInvalidArgs,
            "This convenience function throws on whitespace characters "
            "in \"extra\" meta parameter. If you know what you are doing, "
            "you can use CMetaData::Set(\"extra\", ...)",
            eLBOS_InvalidArgs);
    }
    Set("extra", extra);
}


std::string LBOS::CMetaData::GetExtra() const
{
    return Get("extra");
}


string LBOS::CMetaData::GetMetaString() const
{
    stringstream meta_stringstream;
    auto iter = m_Meta.begin();
    for (  ;  iter != m_Meta.end()  ;  ) {
        meta_stringstream << NStr::URLEncode(iter->first) << "=" 
                          << NStr::URLEncode(iter->second);
        if (++iter != m_Meta.end()) {
            meta_stringstream << "&";
        }
    }
    return meta_stringstream.str();
}


ostream& operator<<(ostream& os, const LBOS::CMetaData& metadata)
{
    os << metadata.GetMetaString();
    return os;
}


#endif /* LBOS_METADATA */

END_NCBI_SCOPE
