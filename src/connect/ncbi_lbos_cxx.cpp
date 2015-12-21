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

DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);
static const char* kLBOSAnnounceRegistrySection("LBOS_ANNOUNCEMENT");
static const char* kLBOSServiceVariable("SERVICE");
static const char* kLBOSVersionVariable("VERSION");
static const char* kLBOSServerHostVariable("HOST");
static const char* kLBOSPortVariable("PORT");
static const char* kLBOSHealthcheckUrlVariable("HEALTHCHECK");

/// Functor template for deleting object.
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

bool CLBOSIpCacheKey::operator==(const CLBOSIpCacheKey& rh) const
{
    return x_Service == rh.x_Service &&
        x_Hostname == rh.x_Hostname &&
        x_Version == rh.x_Version &&
        x_Port == rh.x_Port;
}


CLBOSIpCacheKey::CLBOSIpCacheKey(string service, string hostname, string version, unsigned short port) :
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


std::string CLBOSIpCache::HostnameTryFind(string service, string hostname, 
                                      string version, unsigned short port)
{
    if (hostname == "")
        hostname = CSocketAPI::GetLocalHostAddress();
    map<CLBOSIpCacheKey, string>::iterator pos;
    CLBOSIpCacheKey key(service, hostname, version, port);
    pos = x_IpCache->find(key);
    if (pos != x_IpCache->end())
        return pos->second;
    else return hostname;
}

std::string CLBOSIpCache::HostnameResolve(string service, string hostname, 
                                      string version, unsigned short port)
{
    if (hostname == "")
        hostname = CSocketAPI::GetLocalHostAddress();
    map<CLBOSIpCacheKey, string>::iterator pos;
    CLBOSIpCacheKey key(service, hostname, version, port);
    pos = x_IpCache->find(key);
    if (pos != x_IpCache->end())
        return pos->second;
    /* We did not find IP in the cache. Then we resolve the hostname to IP and
       save it to the cache. To make sure that our changes do not interfere
       with changes from another thread, we use 'insert' instead of '[]' (not
       to rewrite) */
    string host =
        CSocketAPI::HostPortToString(CSocketAPI::gethostbyname(hostname), 0);
    std::pair<map<CLBOSIpCacheKey, string>::iterator, bool> res =
        x_IpCache->insert(std::pair<CLBOSIpCacheKey, string>(key, host));
    return res.first->second;
}

void CLBOSIpCache::HostnameDelete(string service, string hostname, 
                              string version, unsigned short port)
{
    if (hostname == "")
        hostname = CSocketAPI::GetLocalHostAddress();
    map<CLBOSIpCacheKey, string>::iterator pos;
    CLBOSIpCacheKey key(service, hostname, version, port);
    pos = x_IpCache->find(key);
    if (pos != x_IpCache->end())
        x_IpCache->erase(pos);
}


CSafeStatic< map< CLBOSIpCacheKey, string > > CLBOSIpCache::x_IpCache;


static void s_ProcessResult(unsigned short result, 
                            const char* lbos_answer,
                            const char* status_message)
{
    if (result == kLBOSSuccess)
        return;

    stringstream message;
    message << result;

    if (status_message == NULL)
        status_message = "";
    else {
        message << " " << status_message;
    }
    if (lbos_answer == NULL)
        lbos_answer = "";
    else {
        message << " " << lbos_answer;
    }
    throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::s_HTTPCodeToEnum(result),
            message.str(), result);
}


void LBOS::Announce(const string& service, const string& version,
                    const string& host, unsigned short port,
                    const string& healthcheck_url)
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    string cur_host = host, ip;
    /* If host is empty, it means that host is the same as in healthcheck 
     * (by convention). We have to parse healthcheck and get host */
    if (cur_host == "")
    {
        SConnNetInfo * healthcheck_info;
        healthcheck_info = ConnNetInfo_Create(NULL);
        healthcheck_info->host[0] = '\0'; /* to be sure that it will be
                                          * overridden                      */
        ConnNetInfo_ParseURL(healthcheck_info, healthcheck_url.c_str());
        /* Create new element of list*/
        cur_host = healthcheck_info->host;
        /* If we could not parse healthcheck URL, throw "Invalid Arguments" */
        if (cur_host == "") {
            ConnNetInfo_Destroy(healthcheck_info);
            throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                 CLBOSException::e_LBOSInvalidArgs,
                                 NStr::IntToString(kLBOSInvalidArgs),
                                 kLBOSInvalidArgs);
        }
        ConnNetInfo_Destroy(healthcheck_info);
    }
    if (cur_host == "0.0.0.0") {
        ip = cur_host;
    } else {
        CFastMutexGuard spawn_guard(s_GlobalLock);
        ip = CLBOSIpCache::HostnameResolve(service, cur_host, version, port);
    }
    /* If DNS could not resolve hostname - throw 400 Bad Request. 
     * Here we emulate LBOS behavior.
     * If we used the same function from C library, it would return 
     * "400 Bad Request" from LBOS, because it does not try to resolve 
     * hostnames before sending them. Here we try to resolve hostname before 
     * sending it to LBOS, so we should return the same answer as LBOS would 
     * do.*/
    if (ip == ":0") {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::e_LBOSBadRequest, "400 Bad Request",
                             kLBOSBadRequest);
    }
    /* If healthcheck is on the same host as server, we try to replace hostname 
     * with IP in healthcheck URL, too */
    string temp_healthcheck = NStr::Replace(healthcheck_url, cur_host, ip);
    unsigned short result = LBOS_Announce(service.c_str(), version.c_str(),
                                          ip.c_str(), port,
                                          temp_healthcheck.c_str(),
                                          &*body_aptr, &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
}


void LBOS::AnnounceFromRegistry(string reg_section)
{
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    /* If "registry_section" is empty, we use default section. */
    if (reg_section == "") {
        reg_section = kLBOSAnnounceRegistrySection;
    }
    string host =     config.Get(reg_section, kLBOSServerHostVariable);
    string service =  config.Get(reg_section, kLBOSServiceVariable);
    string version =  config.Get(reg_section, kLBOSVersionVariable);
    string port_str = config.Get(reg_section, kLBOSPortVariable);
    string health =   config.Get(reg_section, kLBOSHealthcheckUrlVariable);

    /* Check that port is a number between 1 and 65535   */
    int port_int = 0;
    try {
        port_int = NStr::StringToInt(port_str);
    }
    catch (...) {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::e_LBOSInvalidArgs,
                             NStr::IntToString(kLBOSInvalidArgs),
                             kLBOSInvalidArgs);
    }
    if (port_int < 1 || port_int > 65535)
    {
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                             CLBOSException::e_LBOSInvalidArgs, 
                             NStr::IntToString(kLBOSInvalidArgs),
                             kLBOSInvalidArgs);
    }
    unsigned short port = static_cast<unsigned short>(port_int);
    Announce(service, version, host, port, health);
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
    if (host == "" || host == "0.0.0.0") {
        ip = host;
    } else {
        CFastMutexGuard spawn_guard(s_GlobalLock);
        ip = CLBOSIpCache::HostnameTryFind(service, host, version, port);
    }
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_Deannounce(service.c_str(), version.c_str(), 
                                            ip.c_str(), port, &*body_aptr, 
                                            &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
    /* Nothing thrown - deannounce successful! */
    if (host != "" && host != "0.0.0.0") {
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


string LBOS::ServiceVersionGet(const string&  service,
                               bool* exists) 
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = 
        LBOS_ServiceVersionGet(service.c_str(),&*body_aptr,
                                      &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
    SLbosConfigure res = ParseLbosConfigureAnswer(*body_aptr);
    if (exists != NULL) {
        *exists = res.exists;
    }
    return res.current_version;
}


string LBOS::ServiceVersionSet(const string&  service,
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


string LBOS::ServiceVersionDelete(const string&  service,
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


CLBOSException::EErrCode 
    CLBOSException::s_HTTPCodeToEnum(unsigned short http_code) 
{
    switch (http_code) {
    case kLBOSNoLBOS:
        return e_LBOSNoLBOS;
    case kLBOSNotFound:
        return e_LBOSNotFound;
    case kLBOSBadRequest:
        return e_LBOSBadRequest;
    case kLBOSOff:
        return e_LBOSOff;
    case kLBOSInvalidArgs:
        return e_LBOSInvalidArgs;
    case kLBOSDNSResolveError:
        return e_LBOSDNSResolveError;
    case kLBOSMemAllocError:
        return e_LBOSMemAllocError;
    case kLBOSCorruptOutput:
        return e_LBOSCorruptOutput;
    default:
        return e_LBOSUnknown;
    }
}


unsigned short CLBOSException::GetStatusCode(void) const {
    return m_StatusCode;
}


const char* CLBOSException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case e_LBOSNoLBOS:
        return "LBOS was not found";
    case e_LBOSNotFound:
        return "Healthcheck URL is not working or lbos could not resolve"
               "host of healthcheck URL";
    case e_LBOSDNSResolveError:
        return "Could not get IP of current machine";
    case e_LBOSMemAllocError:
        return "Memory allocation error happened while performing request";
    case e_LBOSOff:
        return "lbos functionality is turned OFF. Check config file.";
    case e_LBOSInvalidArgs:
        return "Invalid arguments were provided. No request to lbos was sent";
    default:
        return "Unknown lbos error code";
    }
}


CLBOSException::CLBOSException(void) : m_StatusCode(0)
{

}


CLBOSException::CLBOSException(const CDiagCompileInfo& info, const CException* prev_exception, const CExceptionArgs<EErrCode>& args, const string& message, unsigned short status_code) : CException(info, prev_exception, (message),
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


CLBOSException::CLBOSException(const CDiagCompileInfo& info, const CException* prev_exception, EErrCode err_code, const string& message, unsigned short status_code, EDiagSev severity /*= eDiag_Error*/) : CException(info, prev_exception, (message), severity, 0)
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

END_NCBI_SCOPE
