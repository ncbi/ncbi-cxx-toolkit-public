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
 * Authors:  Denis Vakatov, Anton Lavrentiev, Dmitriy Elisov
 * @file
 * File Description:
 *   A service discovery API based on LBOS. LBOS is an adapter for ZooKeeper
 *   cloud-based DB. LBOS allows to announce, deannounce and resolve services.
 */


#include "ncbi_servicep.h"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


///////////////////////////////////////////////////////////////////////////////
//                             DATA TYPES                                    //
///////////////////////////////////////////////////////////////////////////////
/** Result of calling function g_LBOS_AnnounceEx() or g_LBOS_AnnounceSelf()  */
typedef enum {
    eLBOSAnnounceResult_Success,             /**< Success                    */
    eLBOSAnnounceResult_NoLBOS,              /**< LBOS was not found, error  */
    eLBOSAnnounceResult_DNSResolveError,     /**< Local address not resolved,
                                                  error                      */
    eLBOSAnnounceResult_InvalidArgs,         /**< Arguments not valid, error */
    eLBOSAnnounceResult_LBOSError,           /**< LBOS failed on server side
                                                  and returned error code
                                                  and error message
                                                  (possibly)                 */
} ELBOSAnnounceResult;


/** @brief Possible values to set primary address where to search for LBOS.
 *
 * LBOS can be located in different places, such as: 
 *   - localhost, 
 *   - home zone server (i.e. lbos.dev.be-md.ncbi.nlm.nih.gov, if you 
 *     are be-md domain on dev machine), 
 *   - somewhere else (set in registry with [CONN]lbos)
 * You can set priority for the first place to try. If LBOS is not found in 
 * this primary location, then it will be searched for in other places 
 * following default algorithm of going through places (maybe even through
 * the same place again)                                                      */
typedef enum {
    eLBOSFindMethod_None,           /**< do not search. Used to skip
                                         "custom host" method                */
    eLBOSFindMethod_CustomHost,     /**< Use custom address provided by
                                         s_SetLBOSaddress()                  */
    eLBOSFindMethod_Registry,       /**< Use value from registry (default)   */
    eLBOSFindMethod_EtcNcbiDomain,  /**< Use value from /etc/ncbi/domain     */
    eLBOSFindMethod_127001          /**< Use 127.0.0.1 from /etc/ncbi/domain */
} ELBOSFindMethod;

///////////////////////////////////////////////////////////////////////////////
//                             GLOBAL FUNCTIONS                              //
///////////////////////////////////////////////////////////////////////////////
/** Get all possible LBOS addresses for this platform.
 * @return    
 *  Array of LBOS addresses which needs to be free()'d by the caller
 * @see       
 *  g_LBOS_GetLBOSAddressesEx()                                              */
NCBI_XCONNECT_EXPORT
char** g_LBOS_GetLBOSAddresses(void);


/** Get all possible LBOS addresses for this platform, extended version.
 * @param priority_find_method[in]   
 *  First method to try
 * @param lbos[in]     
 *  If primary method is set to eLBOSFindMethod_CustomHost, lbos IP:port or 
 *  host:port should be provided
 * @return             
 *  Array of LBOS addresses which needs to be free()'d by the caller
 * @see                
 *  g_LBOS_GetLBOSAddresses()                                                */
NCBI_XCONNECT_EXPORT
char** g_LBOS_GetLBOSAddressesEx(ELBOSFindMethod priority_find_method,
                                 const char*     lbos);

/** Creates iterator and fills it with found servers. 
 * @param[in,out] iter  
 *  Pointer to iterator. It is read and rewritten. If nothing found, it is 
 *  freed and points to unallocated area.
 * @param[in] net_info  
 *  Connection point
 * @param[out] info     
 *  Always assigned NULL, as not used in this mapper
 * @return              
 *  Table of methods if found servers, NULL if not found
 * @see                 
 *  s_Open(), SERV_LOCAL_Open(), SERV_LBSMD_Open(), SERV_DISPD_Open()        */
NCBI_XCONNECT_EXPORT
const SSERV_VTable*  SERV_LBOS_Open(SERV_ITER           iter,
                                    const SConnNetInfo* net_info,
                                    SSERV_Info**        info);




/** Checks C-string if it is NULL or is of zero length
 * @param str[in]      
 *  String to check
 * @return             
 *  true - string is NULL or empty
 *  false - string exists and contains elements                              */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_StringIsNullOrEmpty(const char* str);


/** Compose LBOS address from /etc/ncbi/{role, domain}
 *  @return             
 *   LBOS address. Must be free()'d by the caller                            */
NCBI_XCONNECT_EXPORT
char* g_LBOS_ComposeLBOSAddress(void);


/** For announcement we search for a LBOS which can handle our request. Search 
 * starts with default order of LBOS.
 * @param service[in]    
 *  Name of service as it will appear in ZK. For services this means that name 
 *  should start with '/'
 * @param version[in]    
 *  Any non-NULL valid C-string
 * @param port[in]       
 *  Port for service. Can differ from healthcheck port
 * @param healthcheck_url[in]   
 *  Full absolute URL starting with "http://" or "https://". Should include 
 *  hostname or IP and port, if necessary.
 * @param LBOS_answer[out]   
 *  This variable will be assigned a pointer to char* with exact answer of 
 *  LBOS, or NULL. If it is not NULL, do not forget to free() it! If 
 *  eLBOSAnnounceResult_Success is returned, LBOS answer contains "host:port" 
 *  of LBOS that was used for announce. If something else is returned, LBOS 
 *  answer contains human-readable error message.
 * @return               
 *  Code of success or some error
 * @see                  
 *  ELBOSAnnounceResult                                                      */
/*NCBI_XCONNECT_EXPORT
ELBOSAnnounceResult  g_LBOS_AnnounceEx(const char*      service,
                                       const char*      version,
                                       unsigned short   port,
                                       const char*      healthcheck_url,
                                       char**           LBOS_answer) */


/** Deannounce previously announced service.
 * @param lbos_hostport[in]    
 *  Address of the same LBOS that was used for announcement of the service 
 *  now being deannounced
 * @param service[in]    
 *  Name of service to be deannounced
 * @param version[in]    
 *  Version of service to be deannounced
 * @param port[in]       
 *  Port of service to be deannounced
 * @param[in]           
 *  IP or hostname of service to be deannounced
 * @return             
 *  0 - any error, no deannounce was made 
 *  1 - success, deannounce was made                                         */
/*int*//*bool*/ /*g_LBOS_Deannounce (const char*    lbos_hostport,
                                     const char*    service,
                                     const char*    version,
                                     unsigned short port,
                                     const char*    ip)                      */


/*  To deannounce we need to go the very same LBOS which we used for 
 * announcement. If it is dead, that means that we are anyway not visible 
 * for ZK, so we can just exit deannouncement
 * @return               
 *  0 - any error, no deannounce was made
 *  1 - success, deannounce was made                */
/*NCBI_XCONNECT_EXPORT*/
/*int*/ /* bool*/ /*g_LBOS_DeannounceSelf(void);                             */

#ifdef __cplusplus
} /* extern "C" */


#endif /*__cplusplus*/

#endif /* CONNECT___NCBI_LBOS__H */
