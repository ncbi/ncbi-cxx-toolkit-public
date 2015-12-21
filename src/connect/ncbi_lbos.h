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

    
/** Announce server.
*
* @attention
*  IP for the server being announced is taken from healthcheck URL!
* @param [in] service 
*  Name of service as it will appear in ZK. For services this means that the
*  name should start with '/'.
* @param [in] version
*  Any non-null non-empty string that will help to identify the version
*  of service. A good idea is to use [semantic versioning]
*  (http://semver.org/) like "4.7.2"
* @param [in] host
*  Optional parameter (NULL to ignore). If provided, tells on which host
*  the server resides. Can be different from healthcheck host. If set to
*  NULL, host is taken from healthcheck.
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
* @param [out] lbos_answer
*  This variable will be assigned a pointer to C-string with exact body of
*  lbos' response (or NULL, if no lbos was reached).
*  The returned string must be free()'d by the caller.
*  If eLBOS_Success is returned, then lbos_answer will contain
*  "host:port" of the lbos instance that was used to announce the server.
*  Otherwise, lbos answer will contain human-readable error message or NULL.
* @param [out] http_status_message
*  This variable will be assigned a pointer to C-string with exact 
*  status message of lbos' response (or NULL, if no lbos was reached).
*  The returned string must be free()'d by the caller.
* @return
*  HTTP status code returned by lbos (or one of additional codes, 
*  see ncbi_lbosp.h)
*
* @sa LBOS_Deannounce(), LBOS_DeannounceAll()
*/
NCBI_XCONNECT_EXPORT
unsigned short LBOS_Announce(const char*             service,
                             const char*             version,
                             const char*             host,
                             unsigned short          port,
                             const char*             healthcheck_url,
                             char**                  lbos_answer,
                             char**                  http_status_message);


/** Modification of LBOS_Announce() that gets all needed parameters from 
 * registry.
 *
 * @attention
 *  IP for the server being announced is taken from healthcheck URL!
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
 * @param [out] lbos_answer
 *  This variable will be assigned a pointer to C-string with exact body of
 *  lbos' response (or NULL, if no lbos was reached).
 *  The returned string must be free()'d by the caller.
 * @param [out] http_status_message 
 *  This variable will be assigned a pointer to C-string with exact
 *  status message of lbos' response (or NULL, if no lbos was reached).
 *  The returned string must be free()'d by the caller.
 * @return
 *  HTTP status code returned by lbos (or one of additional codes,
 *  see ncbi_lbosp.h)
 *  
 *  --------------                                                           */
NCBI_XCONNECT_EXPORT
unsigned short LBOS_AnnounceFromRegistry(const char*  registry_section,
                                         char**       lbos_answer,
                                         char**       http_status_message);


/** Deannounce service.*
* @param [in] service
*  Name of service to be de-announced.
* @param [in] version
*  Version of service to be de-announced.
* @param [in] host
*  IP or hostname of service to be de-announced. NULL to use local host address
* @param [in] port
*  Port of service to be de-announced.
* @param [out] lbos_answer
*  This variable will be assigned a pointer to C-string with exact body of
*  lbos' response (or NULL, if no lbos was reached).
*  The returned string must be free()'d by the caller.
* @param [out] http_status_message
*  This variable will be assigned a pointer to C-string with exact
*  status message of lbos' response (or NULL, if no lbos was reached).
*  The returned string must be free()'d by the caller.
* @return
*  HTTP status code returned by lbos (or one of additional codes,
*  see ncbi_lbosp.h)
*
* @sa LBOS_Announce(), LBOS_DeannounceAll()
*/
NCBI_XCONNECT_EXPORT
unsigned short LBOS_Deannounce(const char*        service,
                               const char*        version,
                               const char*        host,
                               unsigned short     port,
                               char**             lbos_answer,
                               char**             http_status_message);


/** Deannounce all servers that were announced during runtime.
*
* @sa LBOS_Announce(), LBOS_Deannounce()
*/
NCBI_XCONNECT_EXPORT
void LBOS_DeannounceAll(void);


#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


#endif /* CONNECT___NCBI_LBOS__H */
