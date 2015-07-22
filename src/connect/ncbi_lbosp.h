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
 *   if you do not want to dive into internal LBOS mapper implementation.
*/

#include "ncbi_lbos.h"


/*#define ANNOUNCE_DEANNOUNCE_TEST*/ /* Define for announcement/deannouncement.
                                      * Not meant to be in trunk yet */


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


///////////////////////////////////////////////////////////////////////////////
//                             DATA TYPES                                    //
///////////////////////////////////////////////////////////////////////////////
/** Very simple internal structure which stores information about found 
   servers                                                                   */
typedef struct {
    SSERV_Info*         info;        /**< stores only IP and port for now    */
} SLBOS_Candidate;


/** All the data required to store state of one request to LBOS between 
 * calls to SERV_GetNextInfo                                                 */
typedef struct {
    SConnNetInfo*       net_info;
    SLBOS_Candidate*    cand;
    const char*         lbos;        /**< LBOS host:port or IP:port. Used if
                                          find_method == 
                                           eLBOSFindMethod_CustomHost        */
    ELBOSFindMethod     find_method; /**< how we find LBOS. Mainly for
                                          testing                            */
    size_t              pos_cand;    /**< current candidate                  */
    size_t              n_cand;      /**< full number of candidates          */
    size_t              a_cand;      /**< space for candidates               */
} SLBOS_Data;



/** Possible values of parameter for g_LBOS_CheckIterator()
 * @see
 *  g_LBOS_CheckIterator()                                                   */
typedef enum {
    ELBOSIteratorCheckType_MustHaveData,   /**< iterator MUST have 'data'
                                                filled or error will be
                                                returned                     */
    ELBOSIteratorCheckType_DataMustBeNULL, /**< iterator MUST have 'data' NULL
                                                or error will be returned    */
    ELBOSIteratorCheckType_NoCheck         /**< no check of 'data'           */
} ELBOSIteratorCheckType;

///////////////////////////////////////////////////////////////////////////////
//                        MOCK FUNCTION TYPEDEFS                             //
///////////////////////////////////////////////////////////////////////////////
/** Send REST API request to LBOS, read answer and return filled array of 
   SSERV_INFO, containing info about all found servers
 * @param lbos_address[in]  
    IP:port or host:port where to send request
 * @param serviceName[in]   
    Name of service to ask for
 * @param 
    net_info[in] Connection point
 *  @return             
     Array of pointers to SSERV_Info structs, containing all found servers. 
     It can (and must) be safely free()'d. Do not forget to first free() 
     all single SSERV_Info, then free() the array itself                     */
typedef
SSERV_Info**        FLBOS_ResolveIPPortMethod    (const char*     lbos_address,
                                                  const char*      serviceName,
                                                  SConnNetInfo*      net_info);


/** @brief              Read from connection. Handles buffer itself, just
 *                      returns final char* string
 *  @param conn[in]     connection handle
 *  @param buf[in]      memory buffer to read to
 *  @param size[in]     max. # of bytes to read
 *  @param n_read[out]  non-NULL, # of actually read bytes
 *  @param how[in]      peek/read/persist
 *  @return             Success or some error code                           */
typedef
EIO_Status FLBOS_ConnReadMethod(CONN           conn,
                                void*          buf,
                                size_t         size,
                                size_t*        n_read,
                                EIO_ReadMethod how);


/**  Checks iterator, fact that iterator belongs to this mapper, iterator data. 
 * Only debug function.
 * @param iter[in]     
 *  Iterator to check. Not modified in any way
 * @param 
 *  should_have_data[in]  
 * @see               
 *  ELBOSIteratorCheckType
 * @return
 *  true  - iterator is valid
 *  false - iterator is invalid                                              */
NCBI_XCONNECT_EXPORT
int/*bool*/  g_LBOS_CheckIterator(SERV_ITER              iter,
                                  ELBOSIteratorCheckType should_have_data);


/** @brief              Compose LBOS address from /etc/ncbi/{role, domain}
 *  @return             char* with constructed host:port or IP:port, which
 *                      can (and must be) safely free()'d                    */
typedef
char* FLBOS_ComposeLBOSAddressMethod(void);


/** @brief              Given just empty data structure and name of service,
 *                      do all necessary operations to fill the structure
 *                      with servers
 *  @param data[out]    This structure will be filled
 *  @param service[in]  Name of the service of which we search servers
 *  @return             None                                                 */
typedef
void FLBOS_FillCandidatesMethod(SLBOS_Data* data,
                                const char* service);


/** @brief              Destroy data (simulation of destructor as if
 *                      SLBOS_Data were a class. Please note that it will
 *                      be free()'d and no following access is possible, so
 *                      setting data to NULL after this method is recommended
 *                      for avoiding confusion
 *  @param data[in]     Structure to be destroyed
 *  @return             None
 *  @note               It presumes that s_LBOS_Reset was called previously
 *  @see                S_LBOS_Reset(), SLBOS_Data                           */
typedef
void FLBOS_DestroyDataMethod(SLBOS_Data* data);


/* Delete on 8/1/2015  if not needed
* @brief              Construct data (simulation of constructor as if
 *                      SLBOS_Data were a class
 *  @param candidatesCapacity[in]  Initial space allocated for candidates
 *  @return             pointer to new empty initialized SLBOS_Data         
 typedef
SLBOS_Data*      FLBOS_ConstructDataMethod    (int      candidatesCapacity);*/


/** @brief              Submethod of SERV_GetNextInfo that is responsible
 *                      for LBOS mapper
 *  @param iter[in]
 *  @param host_info[out]  Supposed to be set to pointer to info about host
 *                       on which returned server resides, but due to
 *                       limitations of AWS cloud, it is always NULL
 *  @return              Next server                                         */
typedef
SSERV_Info* FLBOS_GetNextInfoMethod(SERV_ITER  iter,
                                    HOST_INFO* host_info);


/** @brief                For announcement we search for a LBOS which
 *                       can handle our request. Search starts with
 *                       default order of LBOS.
 * @param service[in]    Name of service as it will appear in ZK. For
 *                       services this means that name should start
 *                       with '/'
 * @param version[in]    Any non-NULL valid C-string
 * @param port[in]       Port for service. Can differ from healthcheck
 *                       port
 * @param healthcheck_url[in]   Full absolute URL starting with "http://" or
 *                       "https://".
 *                       Should include hostname or IP and port, if
 *                       necessary.
 * @param LBOS_answer[out]   This variable will be assigned a pointer to
 *                       char* with exact answer of LBOS, or NULL.
 *                       If it is not NULL, do not forget to
 *                       free() it!
 *                       If eLBOSAnnounceResult_Success is
 *                       returned, LBOS answer contains "host:port"
 *                       of LBOS that was used for announce.
 *                       If something else is returned, LBOS answer
 *                       contains human-readable error message.
 * @return               Code of success or some error
 * @see                  ELBOSAnnounceResult                                 */
typedef
ELBOSAnnounceResult FLBOS_AnnounceExMethod(const char*      service,
                                           const char*      version,
                                           unsigned short   port,
                                           const char*      healthcheck_url,
                                           char**           LBOS_answer);


/** @brief               Deannounce previously announced service.
 * @param lbos_hostport[in]    Address of the same LBOS that was used for
 *                       announcement of the service now being
 *                       deannounced
 * @param service[in]    Name of service to be deannounced
 * @param version[in]    Version of service to be deannounced
 * @param port[in]       Port of service to be deannounced
 * @param[in]            IP or hostname of service to be deannounced
 * @return               0 means any error, no deannounce was made
 *                       1 means success, deannounce was made                */
typedef
int/*bool*/ FLBOS_DeannounceMethod(const char*      lbos_hostport,
                                   const char*      service,
                                   const char*      version,
                                   unsigned short   port,
                                   const char*      ip);


/** @brief This function test existence of the application that should always
           be found - LBOS itself. If it is not found, we turn mapper off.   */
typedef 
void FLBOS_Initialize(void);


///////////////////////////////////////////////////////////////////////////////
//                       VIRTUAL FUNCTIONS TABLE                             //
///////////////////////////////////////////////////////////////////////////////
typedef struct {
    FLBOS_ResolveIPPortMethod*      ResolveIPPort;
    FLBOS_ConnReadMethod*           Read; /**<  This function pointer
                                                is not used only by external
                                                functions, but also by
                                                s_LBOS_UrlReadAll */
    FLBOS_ComposeLBOSAddressMethod* ComposeLBOSAddress;
    FLBOS_FillCandidatesMethod*     FillCandidates;
    /*FLBOS_ConstructDataMethod*      ConstructData;*/
    FLBOS_DestroyDataMethod*        DestroyData;
    FLBOS_GetNextInfoMethod*        GetNextInfo;
    FLBOS_Initialize*               Initialize;
#ifdef ANNOUNCE_DEANNOUNCE_TEST
    /*FLBOS_AnnounceExMethod             AnnounceEx;
    FLBOS_DeannounceMethod               Deannounce;*/
#endif
} SLBOS_Functions;


///////////////////////////////////////////////////////////////////////////////
//                             GLOBAL FUNCTIONS                              //
///////////////////////////////////////////////////////////////////////////////
/** @brief              Set primary method how to find LBOS. Default is
 *                      eLBOSFindMethod_Registry
 *  @param iter[in]     Iterator that represents current request to LBOS
 *  @param method[in]   One of methods
 *  @return             0 - something went wrong, primary method was not
 *                      changed
 *                      1 - success                                          */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod(SERV_ITER       iter,
                                                 ELBOSFindMethod method);


/** @brief              Set custom host for LBOS. It will be used when
 *                      method eLBOSFindMethod_CustomHost is used
 *  @param iter[in]     Iterator that represents current request to LBOS
 *  @param address[in]  IP:port  or host:port to use
 *  @return             0 - something went wrong, LBOS address was not changed
 *                      1 - success                                          */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSaddress(SERV_ITER iter,
                                              char*     address);


/** @brief              Set custom files to load role and domain from,
 *                      respectively
 *  @param roleFile[in] To change role file, pass it here. To use current
 *                      role file, pass NULL
 *  @param domainFile[in]  To change domain file, pass it here. To use current
 *                      domain file, pass NULL
 *  @return             0 - something went wrong, values not changed
 *                      1 - success                                          */
NCBI_XCONNECT_EXPORT
int/*bool*/ g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(const char*  roleFile,
                                                       const char* domainFile);


///////////////////////////////////////////////////////////////////////////////
//                             GLOBAL VARIABLES                              //
///////////////////////////////////////////////////////////////////////////////

/** @brief              Table of all functions to mock, used solely for
 *                      unit testing purposes
 *  @see                SLBOS_Functions                                      */
NCBI_XCONNECT_EXPORT
SLBOS_Functions* g_LBOS_UnitTesting_GetLBOSFuncs(void);

/** @brief          Check whether LBOS mapper is turned ON or OFF
 *  @return         address of static variable s_LBOS_TurnedOn
    @see            SERV_LBOS_Open()                                         */
NCBI_XCONNECT_EXPORT
int* g_LBOS_UnitTesting_PowerStatus(void);


/** @brief          Check whether LBOS mapper is turned ON or OFF
 *  @return         address of static variable s_LBOS_TurnedOn
    @see            SERV_LBOS_Open()                                         */
NCBI_XCONNECT_EXPORT
int* g_LBOS_UnitTesting_InitStatus(void);


/** @brief          List of addresses of LBOS that is maintained in actual 
                    state
 *  @return         address of static variable s_LBOS_InstancesList
    @see            SERV_LBOS_Open(), s_LBOS_FillCandidates()                */
NCBI_XCONNECT_EXPORT
char** g_LBOS_UnitTesting_InstancesList(void);


#ifdef __cplusplus
} /* extern "C" */


#endif /*__cplusplus*/

#endif /* CONNECT___NCBI_LBOSP__H */
