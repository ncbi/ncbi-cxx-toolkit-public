#ifndef CONNECT___NCBI_LBOS__H
#define CONNECT___NCBI_LBOS__H
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
*   A service discovery API based on lbos. lbos is an adapter for ZooKeeper
*   cloud-based DB. lbos allows to announce, deannounce and resolve services.
*/


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


/** Result of calling function g_LBOS_Announce() */
typedef enum {
    eLBOS_Success,              /**< Success                           */
    eLBOS_NoLBOS,               /**< lbos was not found                */
    eLBOS_DNSResolveError,      /**< Local address not resolved        */
    eLBOS_InvalidArgs,          /**< Arguments not valid               */
    eLBOS_ServerError,          /**< lbos failed on server side
                                     and returned error code
                                     and error message (possibly)      */
    eLBOS_NotFound,             /**< For deannouncement only. Did not 
                                     find such server to deannounce    */
    eLBOS_Off,                  /**< lbos turned OFF on this machine   */
} ELBOS_Result;


/** Announce server.
*
* @attention
*  IP for the server being announced is taken from healthcheck URL!
* @param service [in]
*  Name of service as it will appear in ZK. For services this means that the
*  name should start with '/'.
* @param version [in]
*  Any non-null non-empty string that will help to identify the version
*  of service. A good idea is to use [semantic versioning]
*  (http://semver.org/) like "4.7.2"
* @param port [in]
*  Port for the service. Can differ from healthcheck port.
* @param healthcheck_url [in]
*  Full absolute URL starting with "http://" or "https://". Should include
*  hostname or IP (and port, if necessary).
* @note
*  If you want to announce a service that is on the same machine that
*  announces it (i.e., if server announces itself), you can write
*  "0.0.0.0" for IP (this is convention with lbos). You still have to
*  provide port, even if you write "0.0.0.0".
* @param lbos_answer [out]
*  This variable will be assigned a pointer to char* with exact
*  answer of lbos (or NULL, if no lbos was reached).
*  The returned string must be free()'d by the caller.
*  If eLBOS_Success is returned, then lbos answer will contain
*  "host:port" of the lbos instance that was used to announce the server.
*  Otherwise, lbos answer will contain human-readable error message or NULL.
* @return
*  Code of success or some error.
*
* @sa LBOS_Deannounce(), LBOS_DeannounceAll()
*/
NCBI_XCONNECT_EXPORT
ELBOS_Result LBOS_Announce(const char*       service,
                           const char*       version,
                           unsigned short    port,
                           const char*       healthcheck_url,
                           char**            lbos_answer);

/** Modification of LBOS_Announce() that gets all needed parameters from 
 * registry.                                                                
 * @param [in] registry_section
 *  Name of section in registry file where to look for announcement parameters.
 *  Parameters are:
 *  SERVICE, VERSION, PORT, HEALTHCHECK
 *  Example:
 *  --------------
 *  [LBOS_ANNOUNCEMENT]
 *  SERVICE=MYSERVICE
 *  VERSION=1.0.0
 *  PORT=8080
 *  HEALTH=http://0.0.0.0:8080/health
 *  --------------                                                           */
NCBI_XCONNECT_EXPORT
ELBOS_Result LBOS_AnnounceFromRegistry(const char*  registry_section,
                                   char**       lbos_answer);

/** Deannounce all servers that were announced during runtime.
*
* @sa LBOS_Announce(), LBOS_Deannounce()
*/
NCBI_XCONNECT_EXPORT
void LBOS_DeannounceAll(void);


/** Deannounce service.
* @note
*  If server was not announced prior to deannounce but
*  arguments were provided correctly, then still return 'true'.
*
* @param lbos_hostport [in]
*  Host:port of the same lbos that was used for announcement of the service
*  now being deannounced (see 'lbos_answer' of g_LBOS_Announce()).
* @param service [in]
*  Name of service to be deannounced.
* @param version [in]
*  Version of service to be deannounced.
* @param port [in]
*  Port of service to be deannounced.
* @param host [in]
*  IP or hostname of service to be deannounced. 0 to use local host address
* @return
*  FALSE on any error, no deannounce was made;  TRUE otherwise
*
* @sa LBOS_Announce(), LBOS_DeannounceAll()
*/
NCBI_XCONNECT_EXPORT
ELBOS_Result LBOS_Deannounce(const char*    service,
                             const char*    version,
                             const char*    host,
                             unsigned short port);


#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


#endif /* CONNECT___NCBI_LBOS__H */
