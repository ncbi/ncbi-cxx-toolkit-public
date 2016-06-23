#ifndef CONNECT___NCBI_LBOSP__H
#define CONNECT___NCBI_LBOSP__H
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
 *   This header was made only because of unit testing application. Please, 
 *   include only ncbi_lbos.h.
 *   This file contains only those elements that are absolutely unneeded
 *   if you do not want to dive into internal LBOS client implementation.
*/
#include "ncbi_servicep.h"
#include <connect/ncbi_http_connector.h>
#include "ncbi_lbos.h"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

#ifndef LBOS_METADATA
#define LBOS_METADATA
#endif

/*
 * Additional HTTP codes:
 *  450 - LBOS not found
 *  451 - DNS resolve failed for healthcheck URL
 *  452 - Invalid arguments provided, request was not sent to LBOS
 *  453 - Memory allocation error encountered
 *  454 - LBOS output could not be parsed
 *  550 - LBOS client is OFF in the current process
 */
enum ELBOSStatusCodes {
    eLBOS_Success         = 200, /**< HTTP 200 OK */
    eLBOS_BadRequest      = 400, /**< HTTP 400 Bad Request */
    eLBOS_NotFound        = 404, /**< HTTP 404 Not Found */
    eLBOS_LbosNotFound    = 450, /**< Not a HTTP code. LBOS was not found */
    eLBOS_DNSResolve      = 451, /**< Not a HTTP code. Could not find IP of
                                      localhost */
    eLBOS_InvalidArgs     = 452, /**< Not a HTTP code. Some arguments were
                                      invalid */
    eLBOS_MemAlloc        = 453, /**< Not a HTTP code. Some memory could not be
                                      allocated */
    eLBOS_Protocol        = 454, /**< Not a HTTP code. LBOS response could not
                                      be parsed */
    eLBOS_Server          = 500, /**< HTTP 500 Internal Server Error */
    eLBOS_Disabled        = 550  /**< Not a HTTP code. LBOS Client functionality
                                      is disabled in registry or during
                                      initialization (LBOS Client could not
                                      establish connection with LBOS) */
};

///////////////////////////////////////////////////////////////////////////////
//                             DATA TYPES                                    //
///////////////////////////////////////////////////////////////////////////////
/** @brief Possible values to set primary address where to search for LBOS.
 *
 * LBOS can be located in different places, such as:
 *   - localhost,
 *   - home zone server (i.e. LBOS.dev.be-md.ncbi.nlm.nih.gov, if you
 *     are be-md domain on dev machine),
 *   - somewhere else (set in registry with [CONN]LBOS)
 * You can set priority for the first place to try. If LBOS is not found in
 * this primary location, then it will be searched for in other places
 * following default algorithm of going through places (maybe even through
 * the same place again).                                                    */
typedef enum {
    eLBOSFindMethod_None,           /**< do not search. Used to skip
                                         "custom host" method                */
    eLBOS_FindMethod_CustomHost,     /**< Use custom address provided by
                                         s_SetLBOSaddress()                  */
    eLBOS_FindMethod_Registry,       /**< Use value from registry (default)   */
    eLBOS_FindMethod_Lbosresolve,    /**< Use value from /etc/ncbi/lbosresolve*/
} ELBOSFindMethod;


/** Very simple internal structure which stores information about found 
 * servers.                                                                  */
typedef struct {
    SSERV_Info*         info;        /**< Stores only IP and port for now    */
} SLBOS_Candidate;


/** All the data required to store state of one request to LBOS between 
 * calls to SERV_GetNextInfo.                                                */
typedef struct {
    SConnNetInfo*       net_info;    /**< Connection point                   */
    const char*         lbos_addr;   /**< LBOS host:port or IP:port. Used if
                                          find_method == 
                                          eLBOS_FindMethod_CustomHost        */
    SLBOS_Candidate*    cand;        /**< Array of found server to iterate   */
    size_t              pos_cand;    /**< Current candidate                  */
    size_t              n_cand;      /**< Used space for candidates          */
    size_t              a_cand;      /**< Allocated space for candidates     */
    ELBOSFindMethod     find_method; /**< How we find LBOS. Mainly for
                                          testing                            */
} SLBOS_Data;


/** Data structure to use as user_data in HTTP Connector (header is used 
 *  only for unit testing)                                                   */
typedef struct {
    int http_response_code;
    char* http_status_mesage;
    const char* header;
    size_t content_length; /**< Value of "Content-length" HTTP header tag. 
                                -1 (max value) as no limit */
} SLBOS_UserData;


/** Used for internal storage, so that DeannounceAll() could deannounce 
 *  everything that was announced earlier.
 * @see 
 *  DeannounceAll()
 */
struct SLBOS_AnnounceHandle_Tag
{
    char*            service;       /**< service name of announced server    */
    char*            version;       /**< service version of announced server */
    char*            host;          /**< host of announced server            */
    unsigned short   port;          /**< port of announced server            */
};


/** Possible values of parameter for g_LBOS_CheckIterator(), 
 *  this enum is used only in assertions
 * @see
 *  g_LBOS_CheckIterator()                                                   */
typedef enum {
    ELBOSIteratorCheckType_MustHaveData,   /**< Iterator MUST have 'data'
                                                filled or error will be
                                                returned                     */
    ELBOSIteratorCheckType_DataMustBeNULL, /**< Iterator MUST have 'data' 
                                                NULL or error will be 
                                                returned                     */
    ELBOSIteratorCheckType_NoCheck         /**< No check of 'data'           */
} ELBOSIteratorCheckType;

///////////////////////////////////////////////////////////////////////////////
//                        MOCK FUNCTION TYPEDEFS                             //
///////////////////////////////////////////////////////////////////////////////
/** Send REST API request to LBOS, read answer and return filled array of 
 * SSERV_INFO, containing info about all found servers.
 * @param lbos_address[in]  
 *  IP:port or host:port where to send request.
 * @param serviceName[in]   
 *  Name of service to ask for.
 * @param net_info[in] 
 *  Connection point.
 * @return             
 *  Array of pointers to SSERV_Info structs, containing all found servers. 
 */
typedef
SSERV_Info** FLBOS_ResolveIPPortMethod(const char*     lbos_address,
                                       const char*     serviceName,
                                       SConnNetInfo*   net_info);


/** Read from connection. Handles buffer itself.
 * @param conn[in]     
 *  Connection handle.
 * @param buf[in]      
 *  Memory buffer to read to.
 * @param size[in]     
 *  Max. # of bytes to read.
 * @param n_read[out]  
 *  Non-NULL, # of actually read bytes.
 * @param how[in]      
 *  Peek/read/persist.
 * @return             
 *  Success or some error code.                                              */
typedef
EIO_Status FLBOS_ConnReadMethod(CONN           conn,
                                void*          buf,
                                size_t         size,
                                size_t*        n_read,
                                EIO_ReadMethod how);


/**  Given just empty data structure and name of service, do all necessary 
 *  operations to fill the structure with servers.
 *  @param data[out]    
 *   This structure will be filled.
 *  @param service[in]  
 *   Name of the service of which we search servers.                         */
typedef
void FLBOS_FillCandidatesMethod(SLBOS_Data* data,
                                const char* service);


/**  Destroy data (simulation of destructor as if SLBOS_Data were a class. 
 *  Please note that it will be free()'d and no following access is possible, 
 *  so setting data to NULL after this method is recommended for avoiding 
 *  confusion.
 *  @param data[in]     
 *   Structure to be destroyed.
 *  @note               
 *   It presumes that s_LBOS_Reset was called previously.
 *  @see                
 *   S_LBOS_Reset(), SLBOS_Data                                              */
typedef
void FLBOS_DestroyDataMethod(SLBOS_Data* data);


/** Called under the hood of SERV_GetNextInfo and is responsible for 
 *  LBOS client.
 *  @param iter[in]
 *   Iterator used to iterate through servers.
 *  @param host_info[out]  
 *   Supposed to be set to pointer to info about host on which returned 
 *   server resides, but due to limitations of AWS cloud, it is always NULL.
 *  @return              
 *   Next server.                                                            */
typedef
SSERV_Info* FLBOS_GetNextInfoMethod(SERV_ITER  iter,
                                    HOST_INFO* host_info);


/** For announcement we search for a LBOS which can handle our request. 
 * Search starts with default order of LBOS.
 * @param service[in]    
 *  Name of service as it will appear in ZK. For services this means that 
 *  name should start with '/'.
 * @param version[in]
 *  Any non-NULL valid C-string.
* @param [in] host
*  Optional parameter (NULL to ignore). If provided, tells on which host
*  the server resides. Can be different from healthcheck host. If set to
*  NULL, host is taken from healthcheck.
 * @param port[in]
 *  Port for service. Can differ from healthcheck port.
 * @param healthcheck_url[in]
 *  Full absolute URL starting with "http://" or "https://". Should include
 *  hostname or IP and port, if necessary.
 * @param metadata[in]
 *  URL-ready link with additional meta parameters
 * @param LBOS_answer[out]
 *  This variable will be assigned a pointer to char* with exact answer of
 *  LBOS, or NULL. If it is not NULL, must be free()'d by the caller. If
 *  eLBOS_Success is returned, LBOS answer contains "host:port"
 *  of LBOS that was used for announce. If something else is returned, LBOS
 *  answer contains human-readable error message.
 * @return
 *  Code of success or some error.
 * @see
 *  ELBOS_Result                                                             */
typedef
unsigned short FLBOS_AnnounceMethod(const char*     service,
                                    const char*     version,
                                    const char*     host,
                                    unsigned short  port,
                                    const char*     healthcheck_url,
#ifdef LBOS_METADATA
                                    const char*     metadata,
#endif /* LBOS_METADATA */
                                    char**          LBOS_answer,
                                    char**          http_status_message);


/** Deannounce previously announced service.
 * @param lbos_hostport[in]    
 *  Address of the same LBOS that was used for announcement of the service 
 *  now being de-announced.
 * @param service[in]    
 *  Name of service to be de-announced.
 * @param version[in]    
 *  Version of service to be de-announced.
 * @param port[in]       
 *  Port of service to be de-announced.
 * @param[in]            
 *  IP or hostname of service to be de-announced.
 * @return               
 *  false - any error, no deannounce was made;
 *  true - success, deannounce was made.                                     */
typedef
int/*bool*/ FLBOS_DeannounceMethod(const char*       lbos_hostport,
                                   const char*       service,
                                   const char*       version,
                                   const char*       host,
                                   unsigned short    port,
                                   char**            lbos_answer,
                                   int*              http_status_code,
                                   char**            http_status_message);


/** This function test existence of the application that should always be 
 * found - LBOS itself. If it is not found, we turn client off.              */
typedef 
void FLBOS_InitializeMethod(void);


typedef char* FLBOS_UrlReadAllMethod(SConnNetInfo* net_info, 
                                     const char*   url,
                                     int*          status_code,
                                     char**        status_message);


/** Standard parse header function. The only thing considered is that
   standard "void* data" field is used as int code of HTTP response          */
typedef EHTTP_HeaderParse FLBOS_ParseHeader(const char*      header,
                                             void* /* int* */response_code,
                                             int             server_error);

/** Get (and cache for faster follow-up retrievals) the address of
 * local host.                                                               */
typedef unsigned int FLBOS_SOCKGetLocalHostAddressMethod(ESwitch reget);

///////////////////////////////////////////////////////////////////////////////
//                       VIRTUAL FUNCTIONS TABLE                             //
///////////////////////////////////////////////////////////////////////////////
typedef struct {
    FLBOS_ResolveIPPortMethod*              ResolveIPPort;
    FLBOS_ConnReadMethod*                   Read;
    FLBOS_FillCandidatesMethod*             FillCandidates;
    FLBOS_DestroyDataMethod*                DestroyData;
    FLBOS_GetNextInfoMethod*                GetNextInfo;
    FLBOS_InitializeMethod*                 Initialize;
    FLBOS_UrlReadAllMethod*                 UrlReadAll;
    FLBOS_ParseHeader*                      ParseHeader;
    FLBOS_SOCKGetLocalHostAddressMethod*    LocalHostAddr;
    FLBOS_AnnounceMethod*                   AnnounceEx;
} SLBOS_Functions;


///////////////////////////////////////////////////////////////////////////////
//                             GLOBAL FUNCTIONS                              //
///////////////////////////////////////////////////////////////////////////////
/** Get the best possible LBOS address for this platform.
 * @return
 *  LBOS address that needs to be free()'d by the caller.
 * @see
 *  g_LBOS_GetLBOSAddressEx()                                              */
NCBI_XCONNECT_EXPORT
char* g_LBOS_GetLBOSAddress(void);


/** Get the best possible LBOS addresses for this platform, extended version - 
 * first tries the method of caller's choice.
 * @param[in]   priority_find_method
 *  First method to try.
 * @param[in]   lbos_addr
 *  String with "%hostname%:%port%" or "%IP%:%port%". If priority_find_method
 *  is set to eLBOS_FindMethod_CustomHost, lbos_addr should be non-NULL
 *  (or else the method will be ignored)
 * @return
 *  LBOS address that needs to be free()'d by the caller.
 * @see
 *  g_LBOS_GetLBOSAddress()                                                */
NCBI_XCONNECT_EXPORT
char* g_LBOS_GetLBOSAddressEx(ELBOSFindMethod priority_find_method,
                              const char*     lbos_addr);

/** Creates iterator and fills it with found servers.
 * @param[in,out] iter
 *  Pointer to iterator. It is read and rewritten. If nothing found, it is
 *  free()'d and points to unallocated area.
 * @param[in] net_info
 *  Connection point.
 * @param[out] info
 *  Always assigned NULL, as not used in this client.
 * @return
 *  Table of methods if found servers, NULL if not found.
 * @see
 *  s_Open(), SERV_LOCAL_Open(), SERV_LBSMD_Open(), SERV_DISPD_Open()        */
NCBI_XCONNECT_EXPORT
const SSERV_VTable*  SERV_LBOS_Open(SERV_ITER           iter,
                                    const SConnNetInfo* net_info,
                                    SSERV_Info**        info);


/** Checks C-string if it is NULL or is of zero length.
 * @param[in]   str
 *  String to check.
 * @return
 *  true - string is NULL or empty;
 *  false - string exists and contains elements.                             */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_StringIsNullOrEmpty(const char* str);


/** Compose LBOS address from /etc/ncbi/{role, domain}.
 *  @return
 *   LBOS address. Must be free()'d by the caller.                           */
NCBI_XCONNECT_EXPORT
char* g_LBOS_ComposeLBOSAddress(void);


/** Set primary method how to find LBOS. Default is eLBOS_FindMethod_Registry.
 *  @param[in]  iter
 *   Iterator that represents current request to LBOS.
 *  @param[in]  method
 *   One of methods.
 *  @return             f
 *   false - something went wrong, primary method was not changed;
 *   true  - success.                                                        */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod(SERV_ITER        iter,
                                                 ELBOSFindMethod  method);


/**  Set custom host for LBOS. It will be used when method 
 *  eLBOS_FindMethod_CustomHost is used.
 *  @param[in]  iter
 *   Iterator that represents current request to LBOS.
 *  @param[in]  address
 *   IP:port  or host:port to use.
 *  @return             
 *   false - something went wrong, LBOS address was not changed.
 *   true  - success.                                                        */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSaddress(SERV_ITER  iter,
                                              char*      address);


/**  Set custom files to load role and domain from, respectively.
 *  @param lbosresolverFile[in]
 *   To change lbosresolver file path, pass it here. To use current 
 *   lbosresolver file path, pass NULL.
 *  @return             
 *   false - values not changed;
 *   true  - success.                                                        */
NCBI_XCONNECT_EXPORT int/*bool*/
g_LBOS_UnitTesting_SetLBOSResolverFile(const char* lbosresolverFile);


/**  Checks iterator, fact that iterator belongs to this client, iterator data.
 * Only debug function.
 * @param iter[in]
 *  Iterator to check. Not modified in any way.
 * @param should_have_data[in]
 *  How to check 'data' field of iterator.
 * @see
 *  ELBOSIteratorCheckType
 * @return
 *  true  - iterator is valid;
 *  false - iterator is invalid.                                            
 */
NCBI_XCONNECT_EXPORT
int/*bool*/  g_LBOS_CheckIterator(SERV_ITER               iter,
                                  ELBOSIteratorCheckType  should_have_data);

/** Find server among announced and return its position. If not found, 
 * return -1.                                                               
 */
NCBI_XCONNECT_EXPORT
int  g_LBOS_UnitTesting_FindAnnouncedServer(const char*          service,
                                             const char*          version,
                                             unsigned short       port,
                                             const char*          host);

/** Get LBOS-specific announcement variable from registry                   
 */
NCBI_XCONNECT_EXPORT char* g_LBOS_RegGet(const char* section,
                                          const char* name,
                                          const char* def_value);



/** This service can be used to remove service from configuration. Current
 * version will be empty. Previous version shows deleted version.
 */
NCBI_XCONNECT_EXPORT
unsigned short LBOS_ServiceVersionDelete(const char*   service,
                                               char**  lbos_answer,
                                               char**  http_status_message);


/** This request can be used to set new version for a service. Current and
 * previous versions show currently set and previously used service versions.
 * @param[in] service
 *  Name of service for which the version is going to be changed
 * @param new_version[out]
 *  Version that will be used by default for specefied service
 * @param lbos_answer[out]
 *  Variable to be assigned pointer to C-string with LBOS answer
 * @param http_status_message[out]
 *  Variable to be assigned pointer to C-string with status message from LBOS
 */
NCBI_XCONNECT_EXPORT
unsigned short LBOS_ServiceVersionSet(const char*   service,
                                      const char*   new_version,
                                            char**  lbos_answer,
                                            char**  http_status_message);


/** This request will show currently used version for a requested service.
* Current and previous version will be the same.
* @param service[in]
*  Name of service for which to ask default version
* @param lbos_answer[out]
*  Variable to be assigned pointer to C-string with LBOS answer
* @param http_status_message[out]
*  Variable to be assigned pointer to C-string with status message from LBOS
* @return
*  Status code returned by LBOS
*/
NCBI_XCONNECT_EXPORT
unsigned short LBOS_ServiceVersionGet(const char*  service,
                                            char** lbos_answer,
                                            char** http_status_message);

///////////////////////////////////////////////////////////////////////////////
//                      GLOBAL VARIABLES FOR UNIT TESTS                      //
///////////////////////////////////////////////////////////////////////////////

/** Table of all functions to mock, used solely for unit testing purposes.
 * @see                
 *  SLBOS_Functions                                                          */
NCBI_XCONNECT_EXPORT
SLBOS_Functions* g_LBOS_UnitTesting_GetLBOSFuncs(void);


/** Check whether LBOS client is turned ON or OFF.
 * @return
 *  address of static variable s_LBOS_TurnedOn.
 * @see           
 *  SERV_LBOS_Open()                                                         */
NCBI_XCONNECT_EXPORT
int* g_LBOS_UnitTesting_PowerStatus(void);


/** Check whether LBOS client is turned ON or OFF.
 * @return         
 *  address of static variable s_LBOS_TurnedOn.
 * @see 
 *  SERV_LBOS_Open()                                                         */
NCBI_XCONNECT_EXPORT
int* g_LBOS_UnitTesting_InitStatus(void);


/** List of addresses of LBOS that is maintained in actual state.
 * @return         
 *  address of static variable s_LBOS_InstancesList.
 * @see            
 *  SERV_LBOS_Open(), s_LBOS_FillCandidates()                                */
NCBI_XCONNECT_EXPORT
char** g_LBOS_UnitTesting_Instance(void);


/** List of announced servers that is stored statically
 * @return 
 *  address of static variable s_LBOS_AnnouncedServers                       */
NCBI_XCONNECT_EXPORT
struct SLBOS_AnnounceHandle_Tag** g_LBOS_UnitTesting_GetAnnouncedServers(void);


/** Number of announced servers stored
 * @return 
 *  pointer to s_LBOS_AnnouncedServersNum                                    */
NCBI_XCONNECT_EXPORT
int g_LBOS_UnitTesting_GetAnnouncedServersNum(void);


/**  Pointer to s_LBOS_Lbosresolver
 *  @return
 *   address of static variable s_LBOS_Lbosresolver.
 *  @see                                                                     */
NCBI_XCONNECT_EXPORT
char** g_LBOS_UnitTesting_Lbosresolver(void);


#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/

#endif /* CONNECT___NCBI_LBOSP__H */
