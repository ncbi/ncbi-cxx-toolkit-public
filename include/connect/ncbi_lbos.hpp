#ifndef CONNECT___NCBI_LBOS__HPP
#define CONNECT___NCBI_LBOS__HPP
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
*   A C++ wrapper for service discovery API based on lbos. 
*   lbos is an adapter for ZooKeeper cloud-based DB. 
*   lbos allows to announce, deannounce and resolve services.
*/

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

struct SLbosConfigure
{
    string prev_version;
    string current_version;
};


class NCBI_XNCBI_EXPORT LBOS
{
public:
    /** Announce server.
    *
    * @param [in] service
    *  Name of service as it will appear in ZK. For services this means that the
    *  name should start with '/'.
    * @param [in] version
    *  Any non-null non-empty string that will help to identify the version
    *  of service. A good idea is to use [semantic versioning]
    *  (http://semver.org/) like "4.7.2"
    * @param [in] host
    *  Optional parameter. Set to "" (empty string) to use host from 
    *  healthcheck_url to make sure that server and healthcheck actually reside 
    *  on the same machine (it can be important when you specify hostname that 
    *  can be resolved to multiple IPs, and LBOS ends up registering server 
    *  on one host and healthcheck on another)
    * @param [in] port
    *  Port for the service. Can differ from healthcheck port.
    * @param [in] healthcheck_url
    *  Full absolute URL starting with "http://" or "https://". Should include
    *  hostname or IP (and port, if necessary).
    * @note
    *  If you want to announce a service that is on the same machine that
    *  announces it (i.e., if server announces itself), you can write
    *  "0.0.0.0" for IP (this is convention with lbos). You still have to
    *  provide port, even if you write "0.0.0.0".
    * @return
    *  Returns nothing if announcement was successful. Otherwise, throws an
    *  exception.
    * @exception CLBOSException
    * @sa AnnounceFromRegistry(), Deannounce(), DeannounceAll(), CLBOSException
    */
    static void 
        Announce(const string&   service,
                 const string&   version,
                 const string&   host,
                 unsigned short  port,
                 const string&   healthcheck_url);
                                  

   /** Modification of Announce() that gets all needed parameters from 
    * registry.
    *
    * @attention
    *  IP for the server being announced is taken from healthcheck URL!
    * @param [in] registry_section
    *  Name of section in registry file where to look for 
    *  announcement parameters. Please check documentation for Announce() to
    *  to see requirements for the arguments.
    *  Parameters are:
    *  SERVICE, VERSION, PORT, HEALTHCHECK
    *  Example:
    *  --------------
    *  [LBOS_ANNOUNCEMENT]
    *  SERVICE=MYSERVICE
    *  VERSION=1.0.0
    *  PORT=8080
    *  HEALTH=http://0.0.0.0:8080/health
    *
    * @return
    *  Returns nothing if announcement was successful. Otherwise, throws an
    *  exception.
    * @exception CLBOSException
    * @sa Announce(), Deannounce(), DeannounceAll(), CLBOSException
    */
    static void AnnounceFromRegistry(string  registry_section);


    /** Deannounce service.
    * @param [in] service
    *  Name of service to be deannounced.
    * @param [in] version
    *  Version of service to be deannounced.
    * @param [in] host
    *  IP or hostname of service to be deannounced. Provide empty string
    *  (not "0.0.0.0") to use local host address.
    * @param [in] port
    *  Port of service to be deannounced.
    * @return
    *  Returns nothing if deannouncement was successful. Otherwise, throws an
    *  exception.
    * @exception CLBOSException
    * @sa Announce(), DeannounceAll(), CLBOSException
    */
    static void Deannounce(const string&  service,
                           const string&  version,
                           const string&  host,
                           unsigned short port);


    /** Deannounce all servers that were announced during runtime.
    * @note
    *  There is no guarantee that all servers were deannounced successfully
    *  after this function returns. There is a guarantee that deannouncement
    *  request was sent to LBOS for each server that was announced during
    *  runtime.
    * @return
    *  Returns nothing, never throws.
    * @sa Announce(), Deannounce()
    */
    static void DeannounceAll(void);


    /** This request will show currently used version for a requested service.
    * Current and previous version will be the same.
    * @param service[in]
    *  Name of service for which to ask default version
    * @return
    *  Structure with version before and after request (they will be the 
    *  same, since nothing will be changed)
    */
    static
    SLbosConfigure ServiceVersionGetCurrent(const string&  service);


    /** This request can be used to set new version for a service. Current and
    * previous versions show currently set and previously used service versions.
    * @param[in] service
    *  Name of service for which the version is going to be changed
    * @param new_version[out]
    *  Version that will be used by default for specefied service
    * @return
    *  Structure with version before and after request
    */
    static
    SLbosConfigure ServiceVersionUpdate(const string&  service,
                                              const string&  new_version);


    /** This service can be used to remove service from configuration. Current
    *   version will be empty. Previous version shows deleted version.
    * @param[in] service
    *  Name of service for which the version is going to be deleted
    * @return
    *  Structure with version before and after request (version after 
    *  request will be an empty string)
    */
    static
    SLbosConfigure ServiceVersionDelete(const string&  service);
};


/**  CLBOSException is thrown if annoucement/deannouncement fails for any
 * reason. CLBOSException has overloaded "what()" method that will return
 * message from LBOS, which should contain status code and status message.
 * If announcement failed not because of LBOS, but because of bad arguments,
 * memory error, etc., you will non-HTTP status code. To understand it, you
 * can call CLBOSException::GetErrCodeString(void) to see its human language
 * description.
*/
class NCBI_XNCBI_EXPORT CLBOSException : public CException
{
public:
    typedef int TErrCode;
    enum EErrCode {
        e_NoLBOS,               /**< lbos was not found                       */
        e_DNSResolveError,      /**< Local address not resolved               */
        e_InvalidArgs,          /**< Arguments not valid                      */
        e_DeannounceFail,       /**< Error while deannounce/                  */
        e_NotFound,             /**< For deannouncement only. Did not
                                     find such server to deannounce           */
        e_Off,                  /**< LBOS mapper is off for any of two reasons:
                                     either it is not enalbed in registry, or
                                     no LBOS was found at initialization      */
        e_MemAllocError,        /**< A memory allocation error encountered    */
        e_LBOSCorruptOutput,    /**< LBOS returned unexpected output          */
        e_BadRequest,           /**< LBOS returned "400 Bad Request"          */
        e_Unknown               /**< No information about this error 
                                     code meaning                             */
    };

    CLBOSException(const CDiagCompileInfo& info,
                   const CException* prev_exception, EErrCode err_code,
                   const string& message, unsigned short status_code,
                   EDiagSev severity = eDiag_Error);

    CLBOSException(const CDiagCompileInfo& info, 
                   const CException* prev_exception, 
                   const CExceptionArgs<EErrCode>& args, 
                   const string& message,
                   unsigned short status_code);
    virtual ~CLBOSException(void) throw();
    
    /** Get original status code and status message from LBOS in a string     */
    virtual const char* what() const throw();

    /** Translate from the error code value to its string representation.     */
    virtual const char* GetErrCodeString(void) const;

    /** Translate from numerical HTTP status code to lbos-specific
     * error code */
    static EErrCode s_HTTPCodeToEnum(unsigned short http_code);

    unsigned short GetStatusCode(void) const;
    virtual const char* GetType(void) const;
    TErrCode GetErrCode(void) const;
    NCBI_EXCEPTION_DEFAULT_THROW(CLBOSException)
protected:
    CLBOSException(void);
    virtual const CException* x_Clone(void) const;
private:
    unsigned short m_StatusCode;
    string m_Message;
};

END_NCBI_SCOPE

#endif /* CONNECT___NCBI_LBOS__HPP */
