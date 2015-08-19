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
 * @file
 * File Description:
 *   This header was made only because of unit testing application. Please, 
 *   include only ncbi_lbos.h.
 *   This file contains only those elements that are absolutely unneeded
 *   if you do not want to dive into internal lbos mapper implementation.
*/
#include "ncbi_servicep.h"
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_lbos.h>


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


///////////////////////////////////////////////////////////////////////////////
//                             DATA TYPES                                    //
///////////////////////////////////////////////////////////////////////////////
/** @brief Possible values to set primary address where to search for lbos.
 *
 * lbos can be located in different places, such as:
 *   - localhost,
 *   - home zone server (i.e. lbos.dev.be-md.ncbi.nlm.nih.gov, if you
 *     are be-md domain on dev machine),
 *   - somewhere else (set in registry with [CONN]lbos)
 * You can set priority for the first place to try. If lbos is not found in
 * this primary location, then it will be searched for in other places
 * following default algorithm of going through places (maybe even through
 * the same place again).                                                    */
typedef enum {
    eLBOSFindMethod_None,           /**< do not search. Used to skip
                                         "custom host" method                */
    eLBOSFindMethod_CustomHost,     /**< Use custom address provided by
                                         s_SetLBOSaddress()                  */
    eLBOSFindMethod_Registry,       /**< Use value from registry (default)   */
    eLBOSFindMethod_EtcNcbiDomain,  /**< Use value from /etc/ncbi/domain     */
    eLBOSFindMethod_127001          /**< Use 127.0.0.1 from /etc/ncbi/domain */
} ELBOSFindMethod;


/** Very simple internal structure which stores information about found 
 * servers.                                                                  */
typedef struct {
    SSERV_Info*         info;        /**< Stores only IP and port for now    */
} SLBOS_Candidate;


/** All the data required to store state of one request to lbos between 
 * calls to SERV_GetNextInfo.                                                */
typedef struct {
    SConnNetInfo*       net_info;    /**< Connection point                   */
    const char*         lbos_addr;   /**< lbos host:port or IP:port. Used if
                                          find_method == 
                                           eLBOSFindMethod_CustomHost        */
    ELBOSFindMethod     find_method; /**< How we find lbos. Mainly for
                                          testing                            */
    SLBOS_Candidate*    cand;        /**< Array of found server to iterate   */
    size_t              pos_cand;    /**< Current candidate                  */
    size_t              n_cand;      /**< Used space for candidates          */
    size_t              a_cand;      /**< Allocated space for candidates     */
} SLBOS_Data;

struct SLBOS_AnnounceHandle_Tag
{
    char*            service;       /**< service name of announced server    */
    char*            version;       /**< service version of announced server */
    unsigned short   port;          /**< port of announced server            */
    char*            host;          /**< host of announced server            */
    char*            lbos_hostport; /**< lbos that processed announcement    */
};

/** Possible values of parameter for g_LBOS_CheckIterator().
*  @see
*   g_LBOS_CheckIterator()                                                   */
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
/** Send REST API request to lbos, read answer and return filled array of 
 * SSERV_INFO, containing info about all found servers.
 * @param lbos_address[in]  
 *  IP:port or host:port where to send request.
 * @param serviceName[in]   
 *  Name of service to ask for.
 * @param net_info[in] 
 *  Connection point.
 * @return             
 *  Array of pointers to SSERV_Info structs, containing all found servers. 
 * @                     */
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


/**  Compose lbos address from /etc/ncbi/{role, domain}.
 *  @return             
 *   Constructed host:port or IP:port. Must be free()'d by the caller.       */
typedef
char* FLBOS_ComposeLBOSAddressMethod(void);


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
 *  lbos mapper.
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


/** For announcement we search for a lbos which can handle our request. 
 * Search starts with default order of lbos.
 * @param service[in]    
 *  Name of service as it will appear in ZK. For services this means that 
 *  name should start with '/'.
 * @param version[in]
 *  Any non-NULL valid C-string.
 * @param port[in]
 *  Port for service. Can differ from healthcheck port.
 * @param healthcheck_url[in]
 *  Full absolute URL starting with "http://" or "https://". Should include
 *  hostname or IP and port, if necessary.
 * @param LBOS_answer[out]
 *  This variable will be assigned a pointer to char* with exact answer of
 *  lbos, or NULL. If it is not NULL, must be free()'d by the caller. If
 *  eLBOS_Success is returned, lbos answer contains "host:port"
 *  of lbos that was used for announce. If something else is returned, lbos
 *  answer contains human-readable error message.
 * @return
 *  Code of success or some error.
 * @see
 *  ELBOS_Result                                                      */
typedef
ELBOS_Result FLBOS_AnnounceMethod(const char*      service,
                                           const char*      version,
                                           unsigned short   port,
                                           const char*      healthcheck_url,
                                           char**           LBOS_answer);


/** Deannounce previously announced service.
 * @param lbos_hostport[in]    
 *  Address of the same lbos that was used for announcement of the service 
 *  now being deannounced.
 * @param service[in]    
 *  Name of service to be deannounced.
 * @param version[in]    
 *  Version of service to be deannounced.
 * @param port[in]       
 *  Port of service to be deannounced.
 * @param[in]            
 *  IP or hostname of service to be deannounced.
 * @return               
 *  false - any error, no deannounce was made;
 *  true - success, deannounce was made.                                     */
typedef
int/*bool*/ FLBOS_DeannounceMethod(const char*       lbos_hostport,
                                   const char*       service,
                                   const char*       version,
                                   const char*       host,
                                   unsigned short    port);


/** This function test existence of the application that should always be 
 * found - lbos itself. If it is not found, we turn mapper off.              */
typedef 
void FLBOS_InitializeMethod(void);


typedef char* FLBOS_UrlReadAllMethod(SConnNetInfo* net_info, 
                                     const char*   url);


/** Standard parse header function. The only thing considered is that
   standard "void* data" field is used as int code of HTTP response          */
typedef EHTTP_HeaderParse FLBOS_ParseHeader(const char*      header,
                                             void* /* int* */response_code,
                                             int             server_error);


typedef const char* FLBOS_SOCKGetHostByAddrExMethod(unsigned int host,
                                                    char*        buf,
                                                    size_t       bufsize,
                                                    ESwitch      log);

///////////////////////////////////////////////////////////////////////////////
//                       VIRTUAL FUNCTIONS TABLE                             //
///////////////////////////////////////////////////////////////////////////////
typedef struct {
    FLBOS_ResolveIPPortMethod*          ResolveIPPort;
    FLBOS_ConnReadMethod*               Read;
    FLBOS_ComposeLBOSAddressMethod*     ComposeLBOSAddress;
    FLBOS_FillCandidatesMethod*         FillCandidates;
    FLBOS_DestroyDataMethod*            DestroyData;
    FLBOS_GetNextInfoMethod*            GetNextInfo;
    FLBOS_InitializeMethod*             Initialize;
    FLBOS_UrlReadAllMethod*             UrlReadAll;
    FLBOS_ParseHeader*                  ParseHeader;
    FLBOS_SOCKGetHostByAddrExMethod*    GetHostByAddr;
    FLBOS_AnnounceMethod*               AnnounceEx;
} SLBOS_Functions;


///////////////////////////////////////////////////////////////////////////////
//                             GLOBAL FUNCTIONS                              //
///////////////////////////////////////////////////////////////////////////////
/** Get all possible lbos addresses for this platform.
 * @return
 *  Array of lbos addresses that needs to be free()'d by the caller.
 * @see
 *  g_LBOS_GetLBOSAddressesEx()                                              */
NCBI_XCONNECT_EXPORT
char** g_LBOS_GetLBOSAddresses(void);


/** Get all possible lbos addresses for this platform, extended version.
 * @param priority_find_method[in]
 *  First method to try.
 * @param lbos[in]
 *  If primary method is set to eLBOSFindMethod_CustomHost, lbos IP:port or
 *  host:port should be provided.
 * @return
 *  Array of lbos addresses that needs to be free()'d by the caller.
 * @see
 *  g_LBOS_GetLBOSAddresses()                                                */
NCBI_XCONNECT_EXPORT
char** g_LBOS_GetLBOSAddressesEx(ELBOSFindMethod priority_find_method,
                                 const char*     lbos);

/** Creates iterator and fills it with found servers.
 * @param[in,out] iter
 *  Pointer to iterator. It is read and rewritten. If nothing found, it is
 *  free()'d and points to unallocated area.
 * @param[in] net_info
 *  Connection point.
 * @param[out] info
 *  Always assigned NULL, as not used in this mapper.
 * @return
 *  Table of methods if found servers, NULL if not found.
 * @see
 *  s_Open(), SERV_LOCAL_Open(), SERV_LBSMD_Open(), SERV_DISPD_Open()        */
NCBI_XCONNECT_EXPORT
const SSERV_VTable*  SERV_LBOS_Open(SERV_ITER           iter,
                                    const SConnNetInfo* net_info,
                                    SSERV_Info**        info);




/** Checks C-string if it is NULL or is of zero length.
 * @param str[in]
 *  String to check.
 * @return
 *  true - string is NULL or empty;
 *  false - string exists and contains elements.                             */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_StringIsNullOrEmpty(const char* str);


/** Compose lbos address from /etc/ncbi/{role, domain}.
 *  @return
 *   lbos address. Must be free()'d by the caller.                           */
NCBI_XCONNECT_EXPORT
char* g_LBOS_ComposeLBOSAddress(void);


/** Set primary method how to find lbos. Default is eLBOSFindMethod_Registry.
 *  @param iter[in]     
 *   Iterator that represents current request to lbos.
 *  @param method[in]   
 *   One of methods.
 *  @return             f
 *   false - something went wrong, primary method was not changed;
 *   true  - success.                                                        */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod(SERV_ITER        iter,
                                                 ELBOSFindMethod  method);


/**  Set custom host for lbos. It will be used when method 
 *  eLBOSFindMethod_CustomHost is used.
 *  @param iter[in]     
 *   Iterator that represents current request to lbos.
 *  @param address[in]  
 *   IP:port  or host:port to use.
 *  @return             
 *   false - something went wrong, lbos address was not changed.
 *   true  - success.                                                        */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSaddress(SERV_ITER  iter,
                                              char*      address);


/**  Set custom files to load role and domain from, respectively.
 *  @param roleFile[in] 
 *   To change role file, pass it here. To use current role file, pass NULL.
 *  @param domainFile[in]  
 *   To change domain file, pass it here. To use current domain file, 
 *   pass NULL.
 *  @return             
 *   false - something went wrong, values not changed;
 *   true  - success.                                                        */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(const char*  roleFile,
                                                       const char* domainFile);


/**  Checks iterator, fact that iterator belongs to this mapper, iterator data.
 * Only debug function.
 * @param iter[in]
 *  Iterator to check. Not modified in any way.
 * @param should_have_data[in]
 *  How to check 'data' field of iterator.
 * @see
 *  ELBOSIteratorCheckType
 * @return
 *  true  - iterator is valid;
 *  false - iterator is invalid.                                             */
 NCBI_XCONNECT_EXPORT
 int/*bool*/  g_LBOS_CheckIterator(SERV_ITER               iter,
                                   ELBOSIteratorCheckType  should_have_data);

 /** Find server among announced and return its position. If not found, 
  * return -1.                                                               */
 NCBI_XCONNECT_EXPORT
 int  g_LBOS_UnitTesting_FindAnnouncedServer(const char*          service,
                                             const char*          version,
                                             unsigned short       port,
                                             const char*          host);


///////////////////////////////////////////////////////////////////////////////
//                      GLOBAL VARIABLES FOR UNIT TESTS                      //
///////////////////////////////////////////////////////////////////////////////

/** Table of all functions to mock, used solely for unit testing purposes.
 * @see                
 *  SLBOS_Functions                                                          */
NCBI_XCONNECT_EXPORT
SLBOS_Functions* g_LBOS_UnitTesting_GetLBOSFuncs(void);


/** Check whether lbos mapper is turned ON or OFF.
 * @return
 *  address of static variable s_LBOS_TurnedOn.
 * @see           
 *  SERV_LBOS_Open()                                                         */
NCBI_XCONNECT_EXPORT
int* g_LBOS_UnitTesting_PowerStatus(void);


/** Check whether lbos mapper is turned ON or OFF.
 * @return         
 *  address of static variable s_LBOS_TurnedOn.
 * @see 
 *  SERV_LBOS_Open()                                                         */
NCBI_XCONNECT_EXPORT
int* g_LBOS_UnitTesting_InitStatus(void);


/** List of addresses of lbos that is maintained in actual state.
 * @return         
 *  address of static variable s_LBOS_InstancesList.
 * @see            
 *  SERV_LBOS_Open(), s_LBOS_FillCandidates()                                */
NCBI_XCONNECT_EXPORT
char*** g_LBOS_UnitTesting_InstancesList(void);


/** List of announced servers that is stored statically
 * @return 
 *  address of static variable s_LBOS_AnnouncedServers                       */
struct SLBOS_AnnounceHandle_Tag** g_LBOS_UnitTesting_GetAnnouncedServers(void);


/** Number of announced servers stored
 * @return 
 *  pointer to s_LBOS_AnnouncedServersNum                                    */
int g_LBOS_UnitTesting_GetAnnouncedServersNum(void);


/**  Pointer to s_LBOS_CurrentDomain
 *  @return
 *   address of static variable s_LBOS_CurrentDomain.
 *  @see                                                                     */
char** g_LBOS_UnitTesting_CurrentDomain(void);


/**  Pointer to s_LBOS_CurrentRole
 *  @return
 *   address of static variable s_LBOS_CurrentRole.
 *  @see                                                                     */
char** g_LBOS_UnitTesting_CurrentRole(void);


#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/

#endif /* CONNECT___NCBI_LBOSP__H */
