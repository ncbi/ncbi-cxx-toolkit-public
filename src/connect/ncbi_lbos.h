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
 *   and tells hosts which can provide with specific service
*/


#include "ncbi_servicep.h"
#include <connect/ncbi_http_connector.h>
#include <string.h>
#include <stdlib.h>

#define ZK_MAPPER_NAME  "LBOS"      /**< name of mapper */


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

/** @brief The first method to try to find LBOS before default algorithm of
 *         going through methods (maybe even through the same emthod again)
 */
typedef enum {
    eLBOSFindMethod_none,           /*< do not search. Used to skip
                                        "custom host" method */
    eLBOSFindMethod_custom_host,    /*< Use custom address provided by
                                       s_SetLBOSaddress()*/
    eLBOSFindMethod_registry,       /*< Use value from registry (default)*/
    eLBOSFindMethod_etc_ncbi_domain,/*< Use value from /etc/ncbi/domain */
    eLBOSFindMethod_127_0_0_1       /*< Use 127.0.0.1 from /etc/ncbi/domain */
} ELBOSFindMethod;


/** @brief For use in function SetFindError
 */
typedef enum {
    eLBOSFindError_failNCBIROLEDOMAIN,/**< fail at finding LBOS via
                                           /etc/ncbi/role + /etc/ncbi/domain */
    eLBOSFindError_failREGISTRY,      /**< fail at finding LBOS via registry */
    eLBOSFindError_failFind           /**< fail at finding server (404 error)*/
} ELBOSFindError;


/*#ifdef _DEBUG */
/*unit testing section to check how LBOS behaves at different types of errors*/
typedef struct {
    SSERV_Info*         info;        /*< stores only IP and port for now */
    double              status;      /*< not used in this mapper yet     */
} SLBOS_Candidate;
/*#endif */ /* #ifdef _DEBUG */

typedef struct {
    SConnNetInfo*       net_info;
    SLBOS_Candidate*    cand;
    const char*         lbos;        /*< LBOS host:port or IP:port */
    ELBOSFindMethod     find_method; /*< how we find LBOS. Mainly for testing*/
    size_t              pos_cand;    /*< current candidate*/
    size_t              n_cand;      /*< full number of candidates*/
    size_t              a_cand;      /*< space for candidates*/
    int/*bool*/         fail;        /**< Not yet used. Add description if
                                                                         will*/
} SLBOS_Data;


/** @brief For use in function g_LBOS_Announce
 */
typedef enum {
    ELBOSAnnounceResult_Success         = 0,
    ELBOSAnnounceResult_NoLBOS          = 1,
    ELBOSAnnounceResult_DNSResolveError = 2,
    ELBOSAnnounceResult_InvalidArgs     = 3,
    ELBOSAnnounceResult_LBOSError     = 3
} ELBOSAnnounceResult;


/** @brief       Set method how to find LBOS. Default is eLBOSFindMethod_
 *               registry
 *  @param[in]   method
 */
int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod (SERV_ITER iter,
                                                       ELBOSFindMethod method);

int/*bool*/ g_LBOS_UnitTesting_SetLBOSaddress (SERV_ITER iter, char* address);
int/*bool*/ g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles (const char* roleFile,
                                                        const char* domainFile);
char** g_LBOS_getLBOSAddresses ();
char** g_LBOS_getLBOSAddressesEx(ELBOSFindMethod, const char* );

int/*bool*/ g_LBOS_SetLBOSFindError (SERV_ITER iter,
                                                    ELBOSFindError find_error);

const SSERV_VTable* SERV_LBOS_Open(SERV_ITER            iter,
                                   const SConnNetInfo*  net_info,
                                   SSERV_Info**         info);

int   g_LBOS_CheckIterator(SERV_ITER iter, int/*bool*/ should_have_data);
int   g_StringIsNullOrEmpty(const char* str);
char* g_LBOS_ComposeLBOSAddress();
/*ELBOSAnnounceResult  g_LBOS_AnnounceEx(const char*, const char*, unsigned short,
                        const char*, unsigned int*, unsigned short*);*/
/*int*/ /* bool*/ /*g_LBOS_DeannounceSelf();*/

typedef struct {
    SSERV_Info** (*ResolveIPPort) (const char* lbos_address,
            const char* serviceName, SConnNetInfo* net_info);
    EIO_Status (*Read) (CONN    conn, void*   line, size_t  size,
            size_t* n_read, EIO_ReadMethod how); /* This function pointer
            is not used only by external functions, but also by
            s_LBOS_UrlReadAll */
    char* (*ComposeLBOSAddress)();
    void (*FillCandidates) (SLBOS_Data* data, const char* service);
    SLBOS_Data* (*ConstructData) (int candidatesCapacity);
    void (*DestroyData) (SLBOS_Data* data);
    SSERV_Info* (*GetNextInfo) (SERV_ITER iter, HOST_INFO* host_info);
    /*ELBOSAnnounceResult (*AnnounceEx)(const char* service,
                                          const char* version,
                                          unsigned short port,
                                          const char* healthcheck_url,
                                          unsigned int* LBOS_host,
                                          unsigned short* LBOS_port);
    ELBOSDeannounceResult (*Deannounce)(const char* lbos_hostport,
                                            const char* service,
                                            const char* version,
                                            unsigned short port,
                                            const char* ip);*/
} SLBOS_functions;

extern SLBOS_functions g_lbos_funcs;

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/

#endif /* CONNECT___NCBI_LBOS__H */
