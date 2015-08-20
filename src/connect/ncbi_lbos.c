#ifndef CONNECT___NCBI_LBOS__C
#define CONNECT___NCBI_LBOS__C
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
 *   and tells hosts which can provide with specific service
 */


#include "ncbi_ansi_ext.h"
#include <connect/ncbi_http_connector.h>
#include "ncbi_lbosp.h"
#include "ncbi_priv.h"
#include <stdlib.h>

#ifdef NCBI_OS_MSWIN
#endif

#define            kHostportStringLength      (16+1+5)/**<
                                                     strlen("255.255.255.255")+
                                                     strlen(":") +
                                                     strlen("65535")         */
#define            kMaxLineSize               1024  /**< used to read
                                                         from socket         */

#define            NCBI_USE_ERRCODE_X         Connect_LBSM /**< Used in
                                                                CORE_LOG*_X  */

/*/////////////////////////////////////////////////////////////////////////////
//                         STATIC VARIABLES                                  //
/////////////////////////////////////////////////////////////////////////////*/

static const size_t kLBOSAddresses   =        7; /**< How many addresses of
                                                      lbos to expect initially,
                                                      plus one for NULL.     */
static const char* kRoleFile         =        "/etc/ncbi/role";
static const char* kDomainFile       =        "/etc/ncbi/domain";
static const char* kLBOSQuery        =        "/lbos/text/mlresolve?name=";
static       char* s_LBOS_CurrentDomain   =   NULL; /* Do not change..       */
static       char* s_LBOS_CurrentRole     =   NULL; /*          ..after init */
static const
unsigned short int kDefaultLBOSPort  =        8080;  /**< if by
                                                     /etc/ncbi{role, domain} */
static const int   kInitialCandidatesCount =  1; /**< For initial memory
                                                      allocation             */
static       int   s_LBOS_TurnedOn   =        1; /* If lbos cannot resolve
                                                    even itself,
                                                    we turn it off for
                                                    performance              */

static       int   s_LBOS_Init       =        0; /* If mapper has already
                                                     been initialized        */

static 
struct SLBOS_AnnounceHandle_Tag* 
                   s_LBOS_AnnouncedServers = NULL;
static 
unsigned int       s_LBOS_AnnouncedServersNum = 0;
static 
unsigned int       s_LBOS_AnnouncedServersAlloc = 0;
/*/////////////////////////////////////////////////////////////////////////////
//                     STATIC FUNCTION DECLARATIONS                          //
/////////////////////////////////////////////////////////////////////////////*/
static SSERV_Info**   s_LBOS_ResolveIPPort (const char*   lbos_address,
                                            const char*   serviceName,
                                            SConnNetInfo* net_info);
static char *         s_LBOS_UrlReadAll    (SConnNetInfo* net_info,
                                            const char*   url);
static void           s_LBOS_FillCandidates(SLBOS_Data*   data,
                                            const char*   service);
static SLBOS_Data*    s_LBOS_ConstructData (size_t        candidatesCapacity);
static void           s_LBOS_DestroyData   (SLBOS_Data*   data);
static SSERV_Info*    s_LBOS_GetNextInfo   (SERV_ITER     iter,
                                            HOST_INFO*    host_info);
static void           s_LBOS_Initialize    (void);
static int/*bool*/    s_LBOS_Feedback      (SERV_ITER     a,
                                            double        b,
                                            int           c);
static void           s_LBOS_Close         (SERV_ITER     iter);
static void           s_LBOS_Reset         (SERV_ITER     iter);
static int/*bool*/    s_LBOS_Update        (SERV_ITER     iter,
                                            const char*   text,
                                            int           code);
static const char*    s_LBOS_ReadDomain    (void);
static const char*    s_LBOS_ReadRole      (void);
static 
EHTTP_HeaderParse      s_LBOS_ParseHeader  (const char*   header,
                                            void* /*int**/response,
                                            int           server_error);
/*lbos is intended to answer in 0.5 sec, or is considered dead*/
static const STimeout   s_LBOS_Timeout       =  {2, 500000};
static char**           s_LBOS_InstancesList =  NULL;/** Not to get 404 errors
                                                        on every resolve, we
                                                        will remember the last
                                                        successful lbos and
                                                        put it first         */
static char*            s_LBOS_DTABLocal     =  NULL;

static 
ELBOS_Result s_LBOS_Announce(const char*             service,
                             const char*             version,
                             unsigned short          port,
                             const char*             healthcheck_url,
                             char**                  lbos_answer);


/*/////////////////////////////////////////////////////////////////////////////
//                     UNIT TESTING VIRTUAL FUNCTION TABLE                   //
/////////////////////////////////////////////////////////////////////////////*/
static SLBOS_Functions s_LBOS_funcs = {
                s_LBOS_ResolveIPPort
                ,CONN_Read
                ,g_LBOS_ComposeLBOSAddress
                ,s_LBOS_FillCandidates
                ,s_LBOS_DestroyData
                ,s_LBOS_GetNextInfo
                ,s_LBOS_Initialize
                ,s_LBOS_UrlReadAll
                ,s_LBOS_ParseHeader
                ,SOCK_gethostbyaddrEx
                ,s_LBOS_Announce
};


/** Get pointer to s_LBOS_MyPort */
///////////////////////////////////////////////////////////////////////////////
//                  INTERNAL STORAGE OF ANNOUNCED SERVERS                    //
///////////////////////////////////////////////////////////////////////////////
/** Add service to list of announced servers. 
 * NOTE: Intended to be used inside critical section, with caller having
 *       locked already, so does not lock.  */
static int  s_LBOS_FindAnnouncedServer(const char*            service,
                                      const char*             version,
                                      unsigned short          port,
                                      const char*             host)
{
    /* For convenience, we use some aliases. Not references, because this 
     * is not C++ */
    struct SLBOS_AnnounceHandle_Tag** arr = &s_LBOS_AnnouncedServers;
    if (*arr == NULL)
        return -1;
    unsigned int* count = &s_LBOS_AnnouncedServersNum;

    /* Just iterate and compare */
    unsigned int i = 0;
    for (i = 0;  i < *count; i++) {
        if (strcmp(service, (*arr)[i].service) == 0 
            &&
            strcmp(version, (*arr)[i].version) == 0 
            &&
            strcmp(host, (*arr)[i].host) == 0 
            && 
            (*arr)[i].port == port)
        {
            return i;
        }
    }
    return -1;
}


static 
int/*bool*/ s_LBOS_RemoveAnnouncedServer(const char*          service,
                                         const char*          version,
                                         unsigned short       port,
                                         const char*          host)
{
    int return_code = 1;
    CORE_LOCK_WRITE;
    /* For convenience, we use some aliases. Not references, because this 
     * is not C++ */
    struct SLBOS_AnnounceHandle_Tag** arr = &s_LBOS_AnnouncedServers;
    if (*arr == NULL)
        return -1;
    unsigned int* count = &s_LBOS_AnnouncedServersNum;

    /* Find node to delete*/
    int pos = s_LBOS_FindAnnouncedServer(service, version, port, host);
    if (pos == -1) {/* no such service was announced */
        return_code = 0;
        goto unlock_and_return;
    }

    /* Erase node data */
    free((*arr)[pos].version);
    free((*arr)[pos].service);
    free((*arr)[pos].host);

    /* Move all nodes to fill erased position */
    memmove(*arr + pos, 
            *arr + pos + 1, 
            sizeof(**arr) * (*count - pos -1 ));
    (*count)--;
unlock_and_return:
    CORE_UNLOCK;
    return return_code;
}


static 
int/*bool*/ s_LBOS_AddAnnouncedServer(const char*            service,
                                     const char*             version,
                                     unsigned short          port,
                                     const char*             healthcheck_url)
{
    /* First we create object without using static variables, and then lock
     * critical section and copy object to static array. This will allow to 
     * to use most of multithreading */
    int return_code = 1;
    /* extract host from healthcheck url */
    SConnNetInfo * healthcheck_info = ConnNetInfo_Create(NULL);
    healthcheck_info->host[0] = '\0'; /* to be sure that it will be
                                           * overridden                      */
    ConnNetInfo_ParseURL(healthcheck_info, healthcheck_url);

    /* Create new element of list*/
    struct SLBOS_AnnounceHandle_Tag handle;
    handle.host = strdup(healthcheck_info->host);
    handle.port = port;
    handle.version = strdup(version);
    handle.service = strdup(service);

    CORE_LOCK_WRITE;
    /* For convenience, we use some aliases. Not references, because this
     * is not C++ */
    struct SLBOS_AnnounceHandle_Tag** arr = &s_LBOS_AnnouncedServers;
    unsigned int* count = &s_LBOS_AnnouncedServersNum;
    unsigned int* alloc = &s_LBOS_AnnouncedServersAlloc;

    /* We search for the same server being already announced                 */  
    s_LBOS_RemoveAnnouncedServer(service, version, port,
                                 healthcheck_info->host);

    /* Allocate more space, if needed */
    if (*count == *alloc)
    {
        int new_size = *alloc*2 + 1;
        struct SLBOS_AnnounceHandle_Tag* realloc_result = 
            (struct SLBOS_AnnounceHandle_Tag*)realloc(*arr,
                           new_size * sizeof(struct SLBOS_AnnounceHandle_Tag));
        if (realloc_result != NULL) {
            *arr = realloc_result;
            *alloc = new_size;
        } else {
            return_code = 0;
            goto clear_and_exit;
        }
    }
    /* Copy handle to the end of array*/
    (*count)++;
    (*arr)[*count - 1] = handle;

clear_and_exit:
    ConnNetInfo_Destroy(healthcheck_info);
    CORE_UNLOCK;
    return return_code;
}


/*/////////////////////////////////////////////////////////////////////////////
//                  SERVICE MAPPING VIRTUAL FUNCTION TABLE                   //
/////////////////////////////////////////////////////////////////////////////*/
static const SSERV_VTable s_lbos_op =  {
        s_LBOS_GetNextInfo, /**< Use open()'ed iterator to get next found
                             *   server*/
        s_LBOS_Feedback,    /**< Not used                                    */
        s_LBOS_Update,      /**< Not used                                    */
        s_LBOS_Reset,       /**< Free iterator's 'data'                      */
        s_LBOS_Close,       /**< Close iterator and free its 'data'          */
        "lbos"              /**< name of mapper                              */
};


/*/////////////////////////////////////////////////////////////////////////////
//                    GLOBAL CONVENIENCE FUNCTIONS                           //
/////////////////////////////////////////////////////////////////////////////*/
/** Check C-string if it is not NULL and contains at least one symbol        */
int/*bool*/ g_LBOS_StringIsNullOrEmpty(const char* str)
{
    if ((str == NULL) || (strlen(str) == 0)) {
        return (1);
    }
    return (0);
}


/**  We compose lbos hostname based on /etc/ncbi/domain and /etc/ncbi/role.  */
char* g_LBOS_ComposeLBOSAddress(void)
{
    const char *role   = s_LBOS_ReadRole(), 
               *domain = s_LBOS_ReadDomain();
    if (role == NULL || domain == NULL) 
        return NULL;
    
    char* site = (char*)calloc(kMaxLineSize, sizeof(char));
    if (site == NULL) {
        CORE_LOG(eLOG_Warning, "s_LBOS_ComposeLBOSAddress: "
                                 "memory allocation failed");
        return NULL;
    }

    sprintf(site, "lbos.%s.%s", role, domain);
    /*Release unneeded resources for this C-string */
    char* realloc_result;
    if ((realloc_result = (char*)realloc(site,
                                 sizeof(char) * (strlen(site) + 1))) == NULL) {
        CORE_LOG(eLOG_Warning, "s_LBOS_ComposeLBOSAddress: problem with "
                "realloc while trimming string, using untrimmed string");
    }
    else {
        site = realloc_result;
    }
    return (site);
}


/** Checks iterator, fact that iterator belongs to this mapper, iterator data.
 * Only debug function.                                                      */
int/*bool*/ g_LBOS_CheckIterator(SERV_ITER              iter,
                         ELBOSIteratorCheckType should_have_data)
{
    if (iter == NULL)
        return 0;
    if (should_have_data == ELBOSIteratorCheckType_MustHaveData) {
        if (iter->data == NULL) return 0;
        SLBOS_Data* data = (SLBOS_Data*)iter->data;
        assert(data->a_cand >= data->n_cand);
        assert(data->pos_cand <= data->n_cand);
    }
    if (should_have_data == ELBOSIteratorCheckType_DataMustBeNULL 
        && iter->data != NULL) {
        return 0;
    }
    if (strcmp(iter->op->mapper, s_lbos_op.mapper) != 0) {
        return 0;
    }
    return 1;
}


/** Given address to lbos instance, check if it resides in the same domain
 * (be-md, st-va, ac-va, or-wa) as current machine                           */
int/*bool*/ g_LBOS_CheckDomain(const char* lbos_address)
{
    if ( (
            (strstr(lbos_address, ".be-md.") != NULL) 
             || 
             (strstr(lbos_address, ".st-va.") != NULL) 
             ||
             (strstr(lbos_address, ".or-wa.") != NULL)
             ||
             (strstr(lbos_address, ".ac-va.") != NULL)
         )
         &&
         (
            !g_LBOS_StringIsNullOrEmpty(s_LBOS_ReadDomain())
         )
       )
    {
        /* If we can perform check, we return result of check */
        return (strstr(lbos_address, s_LBOS_ReadDomain()) != NULL);
    }
    /* If we cannot perform check, we allow any domain */
    return 1;
}


/**  This function is needed to get lbos hostname in different situations.
 *  @param priority_find_method[in]
 *   The first method to try to find lbos. If it fails, default order of
 *   methods will be used to find lbos.
 *   Default order is:
 *   1) registry value of [CONN]->lbos;
 *   2) 127.0.0.1:8080;
 *   3) lbos for current /etc/ncbi/{role, domain}.
 *   To not specify default method, use eLBOSFindMethod_None.
 *  @param lbos[in]
 *   If priority_find_method is eLBOSFindMethod_CustomHost, then lbos is
 *   first looked for at hostname:port specified in this variable.           */
char** g_LBOS_GetLBOSAddressesEx (ELBOSFindMethod priority_find_method,
                                  const char* lbos_addr)
{
    char** addresses = (char**)malloc(sizeof(char*) *
            kLBOSAddresses);
    /* Section for custom-set address*/
    CORE_LOG_X(1, eLOG_Trace, "Getting lbos addresses...");
    char* lbosaddress = NULL;
    size_t lbosaddresses_count = 0;
    ELBOSFindMethod find_method_order[] = {
               priority_find_method /* eLBOSFindMethod_None, if not specified*/
               , eLBOSFindMethod_Registry
               , eLBOSFindMethod_127001
               , eLBOSFindMethod_EtcNcbiDomain
               };
    size_t find_method_iter;
    size_t find_method_count = sizeof(find_method_order)/sizeof(ELBOSFindMethod);
    for (find_method_iter = 0; find_method_iter < find_method_count;
            ++find_method_iter) {
        switch (find_method_order[find_method_iter]) {
        case eLBOSFindMethod_None :
            break;
        case eLBOSFindMethod_CustomHost :
            if (g_LBOS_StringIsNullOrEmpty(lbos_addr)) {
                CORE_LOG_X(1, eLOG_Warning, "Use of custom lbos address was "
                        "asked for, but no custom address was supplied. Using "
                        "default lbos.");
                break;
            }
            lbosaddress = strdup(lbos_addr);
            if (lbosaddress != NULL) {
                addresses[lbosaddresses_count++] = lbosaddress;
            }
            break;
        case eLBOSFindMethod_127001 :
            lbosaddress = (char*)calloc(kHostportStringLength, sizeof(char));
            if (lbosaddress != NULL) {
                sprintf(lbosaddress, "127.0.0.1:8080");
                addresses[lbosaddresses_count++] = lbosaddress;
            }
            break;
        case eLBOSFindMethod_EtcNcbiDomain :
#ifndef NCBI_OS_MSWIN
            lbosaddress = g_LBOS_UnitTesting_GetLBOSFuncs()->
                                                          ComposeLBOSAddress();
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find lbos using "
                        "/etc/ncbi{role, domain} failed");
            } else {
                addresses[lbosaddresses_count++] = lbosaddress;
                /* Also we must check alternative port, 8080 */
                char* lbos_alternative = calloc(strlen(lbosaddress) + 8, 
                                                sizeof(char));
                if (lbos_alternative == NULL)  {
                    CORE_LOG_X(1, eLOG_Warning,
                               "Memory allocation problem while generating "
                               "lbos {role, domain} address");
                } else {
                    sprintf(lbos_alternative, "%s:%hu", lbosaddress,
                            kDefaultLBOSPort);
                    addresses[lbosaddresses_count++] = lbos_alternative;
                }
            }
#endif
            break;
        case eLBOSFindMethod_Registry:
            lbosaddress = (char*)calloc(kMaxLineSize, sizeof(char));
            CORE_REG_GET("CONN", "lbos", lbosaddress, kMaxLineSize, NULL);
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find lbos using "
                        "registry failed");
                break;
            } 
            addresses[lbosaddresses_count++] = lbosaddress;
            break;
        }
    }
    addresses[lbosaddresses_count++] = NULL;
    return addresses;
}


/* Get possible addresses of lbos in default order */
char** g_LBOS_GetLBOSAddresses(void)
{
    return g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_None, NULL);
}


/*/////////////////////////////////////////////////////////////////////////////
//                    STATIC CONVENIENCE FUNCTIONS                           //
/////////////////////////////////////////////////////////////////////////////*/
/** Get role of current host. No control over returned string! Just read it. */
static const char* s_LBOS_ReadRole()
{
    /*
     * If no role previously read, fill it. Of course, not on MSWIN
     */
#ifndef NCBI_OS_MSWIN
    if (s_LBOS_CurrentRole == NULL) {
        size_t len;
        char str[kMaxLineSize];
        char* read_result; /* during function will become equal either NULL 
                              or str, do not free() */
        FILE* role_file;
        char *role; /* will be used to set s_LBOS_CurrentRole, do not free() */
        if ((role_file = fopen(kRoleFile, "r")) == NULL) {
            /* No role recognized */
            CORE_LOGF(eLOG_Warning, ("s_LBOS_ReadRole: "
                                     "could not open role file %s",
                                     kRoleFile));
            return NULL;
        }
        read_result = fgets(str, sizeof(str), role_file);
        fclose(role_file);
        if (read_result == NULL) {
            CORE_LOG(eLOG_Warning, "s_LBOS_ReadRole: "
                                     "memory allocation failed");
            return NULL;
        }
        len = strlen(str);
        assert(len);
        /*We remove unnecessary '/n' and probably '/r'   */
        if (str[len - 1] == '\n') {
            if (--len && str[len - 1] == '\r')
                --len;
            str[len] = '\0';
        }
        if (strncasecmp(str, "try", 3) == 0)
            role = "try";
        else {
            if (strncasecmp(str, "qa", 2) == 0)
                role = "qa";
            else {
                if (strncasecmp(str, "dev", 3) == 0)
                    role = "dev";
                else {
                    if (strncasecmp(str, "prod", 4) == 0)
                        role = "prod";
                    else {
                        /* No role recognized */
                        CORE_LOGF(eLOG_Warning,
                            ("s_LBOS_ComposeLBOSAddress"
                                ": could not recognize role [%s] in %s",
                                str, kRoleFile));
                        return NULL;
                    }
                }
            }
        }
        CORE_LOCK_WRITE;
        /* Check one more time that no other thread managed to fill
            * static variable ahead of this thread. If this happened,
            * release memory */
        if (s_LBOS_CurrentRole == NULL)
            s_LBOS_CurrentRole = strdup(role);
        CORE_UNLOCK;
    }
#endif /* #ifndef NCBI_OS_MSWIN */
    return s_LBOS_CurrentRole;
}


/** Get domain of current host. No control over returned string! Just 
 * read it.                                                                  */
static const char* s_LBOS_ReadDomain()
{
    /*
     * If no domain previously read, fill it. Of course, not on MSWIN
     */
#ifndef NCBI_OS_MSWIN
    if (s_LBOS_CurrentDomain == NULL) {
        size_t len;
        char str[kMaxLineSize ];
        char* read_result; /* during function will become equal either NULL 
                              or str, do not free() */
        FILE* domain_file;
        if ((domain_file = fopen(kDomainFile, "r")) == NULL) {
            CORE_LOGF(eLOG_Warning, ("s_LBOS_ReadDomain: "
                                     "could not open domain file %s", 
                                     kDomainFile));
            return NULL;
        }
        read_result = fgets(str, sizeof(str), domain_file);
        fclose(domain_file);
        if (read_result == NULL) {
            CORE_LOG(eLOG_Warning, "s_LBOS_ReadDomain: "
                                     "memory allocation failed");
            return NULL;
        } 
        len = strlen(str);
        assert(len);
        /*We remove unnecessary '/n' and probably '/r'   */
        if (str[len - 1] == '\n') {
            if (--len && str[len - 1] == '\r')
                --len;
            str[len] = '\0';
        }
        if (g_LBOS_StringIsNullOrEmpty(str)) {
            /* No domain recognized */
            CORE_LOGF(eLOG_Warning, ("s_LBOS_ComposeLBOSAddress"
                ": domain file %s is empty, skipping this "
                "method", kDomainFile));
            free(read_result);
            return NULL;
        }
        CORE_LOCK_WRITE;
        /* Check one more time that no other thread managed to fill
            * static variable ahead of this thread. If this happened,
            * release memory */
        if (s_LBOS_CurrentDomain == NULL)
            s_LBOS_CurrentDomain = strdup(str);
        CORE_UNLOCK;
    }
#endif /*#ifndef NCBI_OS_MSWIN*/
    return s_LBOS_CurrentDomain;
}


/**   Takes original string and returns URL-encoded string. Original string
 *  is untouched.
 */
char* s_LBOS_URLEncode (const char* to_encode)
{
    /* If all symbols are escape, our string will take triple space */
    size_t encoded_string_buf_size = strlen(to_encode)*3 + 1;
    char* encoded_string = (char*)calloc(encoded_string_buf_size, 
                                         sizeof(char));
    size_t src_read, dst_written; /*strange things needed by URL_Encode*/
    URL_Encode(to_encode, strlen(to_encode), &src_read,
               encoded_string, encoded_string_buf_size, &dst_written);
    return encoded_string;
}


/** @brief Just connect and return connection
 *
 *  Internal function to create connection, which simplifies process
 *  by ignoring many settings and making them default
 */
static CONN s_LBOS_ConnectURL(SConnNetInfo* net_info, const char* url,
                              int* responseCode)
{
    THTTP_Flags         flags        = fHTTP_AutoReconnect | fHTTP_Flushable;
    CONN                conn;
    CONNECTOR           connector;

    CORE_LOGF(eLOG_Note, ("Parsing URL \"%s\"", url));
    if (!ConnNetInfo_ParseURL(net_info, url)) {
        CORE_LOG(eLOG_Warning, "Cannot parse URL");
        return NULL;
    }
    CORE_LOGF(eLOG_Note, ("Creating HTTP%s connector",
            &"S"[net_info->scheme != eURL_Https]));
    if (!(connector = HTTP_CreateConnectorEx(net_info, 
                                             flags,
                                             g_LBOS_UnitTesting_GetLBOSFuncs()
                                                                 ->ParseHeader,
                                             responseCode/*data*/, 
                                             0,
                                             0/*cleanup*/))) {
        CORE_LOG(eLOG_Warning, "Cannot create HTTP connector");
        return NULL;
    }
    CORE_LOG(eLOG_Note, "Creating connection");

    if (CONN_Create(connector, &conn) != eIO_Success) {
        CORE_LOG(eLOG_Warning, "Cannot create connection");
        return NULL;
    }
    /*
     * We define very little timeout so that we can quickly iterate
     * through LBOSes (they should answer in no more than 0.5 second)
     */
    CONN_SetTimeout(conn, eIO_Open,      &s_LBOS_Timeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, &s_LBOS_Timeout);
    return (conn);
}


/** If you are sure that all answer will fit in one char*, use this function
 * to read all input. Must be free()'d by the caller.
 */
static char * s_LBOS_UrlReadAll(SConnNetInfo* net_info, const char* url)
{
    /* Connector to lbos. We do not know if lbos really exists at url */
    CONN          connection       = s_LBOS_ConnectURL(net_info, url, NULL);
    if (connection == NULL) {
        return NULL;
    }
    /* Status of reading*/
    EIO_Status    status;
    /* how much how been read to the moment*/
    size_t        totalRead        = 0;
    /* how much we suppose one line takes*/
    size_t        oneLineSize      = kMaxLineSize;
    /* how much bytes was read in one turn*/
    size_t        bytesRead        = 0;
    char*         buf              = (char*)calloc(oneLineSize, sizeof(char));
    /* Total length of buffer. We already add one line because of calloc */
    size_t           totalBufSize     = oneLineSize;
    char*         realloc_result;
    if (buf == NULL)
    {
        CORE_LOG(eLOG_Warning, "s_LBOS_UrlReadAll: No RAM. "
            "Returning NULL.");
        return NULL;
    }
    do {
        /* If there is no lbos, we will know about it here */
        status = g_LBOS_UnitTesting_GetLBOSFuncs()->
                          Read(connection, buf + strlen(buf),
                               totalBufSize - totalRead - 1 /* for \0 */,
                               &bytesRead, eIO_ReadPlain);
        totalRead += bytesRead;
        buf[totalRead] = 0; /* force end of string */
        /*If there is next line coming, let's finish this one*/
        if (status == eIO_Success)
        {
            /* Add space to buffer, if needed  */
            if (totalBufSize < totalRead * 2) {
                realloc_result = (char*) realloc(buf, sizeof(char) *
                                                 (totalBufSize * 2));
                if (realloc_result == NULL) {
                    CORE_LOG(eLOG_Warning, "s_LBOS_UrlReadAll: Buffer "
                             "overflow. Returning string at its maximum size");
                    return (buf);
                } else {
                    buf = realloc_result;
                    totalBufSize *= 2;
                }
            }
        }
    } while (status == eIO_Success);
    /*In the end we cut buffer to the minimal needed size*/
    if (!(realloc_result = (char*) realloc(buf,
                                          sizeof(char) * (strlen(buf) + 1)))) {
        CORE_LOG(eLOG_Warning, "s_LBOS_UrlReadAll: Buffer shrink error, using "
                "original stirng");
    }  else  {
        buf = realloc_result;
    }
    CONN_Close(connection);
    return (buf);
}


/**   Resolve service name to one of the hosts implementing service.
 *   Uses LBZK at specified IP and port.
 */
static SSERV_Info** s_LBOS_ResolveIPPort(const char* lbos_address,
        const char* serviceName, SConnNetInfo* net_info)
{
    /*Access server at the specified IP and port and receive answer to
     * std::stringstream*/
    SSERV_Info** infos = (SSERV_Info**)calloc(2, sizeof(SSERV_Info*));
    if (infos == NULL) {
        CORE_LOG(eLOG_Warning, "s_LBOS_ResolveIPPort: No RAM. "
                               "Returning NULL.");
        return NULL;
    }
    size_t infos_count = 0;
    size_t infos_capacity = 1;
    char* servicename_url_encoded = s_LBOS_URLEncode(serviceName);
    /*encode service
       namename to url encoding (change ' ' to %20, '/' to %2f, etc.)*/
    size_t url_length = strlen("http://") + strlen(lbos_address) +
            strlen(kLBOSQuery) + strlen(servicename_url_encoded);
    char * url = (char*)malloc(sizeof(char) * url_length + 1); /** to make up
                                                   lbos query URI to connect*/
    if (url == NULL)
    {
        CORE_LOG(eLOG_Warning, "s_LBOS_ResolveIPPort: No RAM. "
            "Returning NULL.");
        return NULL;
    }
    char * lbzk_answer; /*to write down lbos's answer*/
    sprintf(url, "%s%s%s%s", "http://", lbos_address, kLBOSQuery,
            servicename_url_encoded);
    lbzk_answer = s_LBOS_UrlReadAll(net_info, url);
    free(url);
    free(servicename_url_encoded);
    /*
     * We read all the answer, find host:port in the answer and fill
     * 'hostports', occasionally reallocationg array, if needed
     */
    if (!g_LBOS_StringIsNullOrEmpty(lbzk_answer)) {
        char* token = NULL, *saveptr = NULL, *str = NULL;
        for(str = lbzk_answer  ;  ;  str = NULL) {
#ifdef NCBI_COMPILER_MSVC
            token = strtok_s(str, "\n", &saveptr);
#else
            token = strtok_r(str, "\n", &saveptr);
#endif
            if (token == NULL) break;
            SSERV_Info * info = SERV_ReadInfoEx(token, serviceName, 0);
            /* Occasionally, the info returned by lbos can be invalid. */
            if (info == NULL) 
                continue;
            /* We check if we have at least two more places: one for current
             * info and one for finalizing NULL
             */
            if (infos_capacity <= infos_count + 1) {
                SSERV_Info** realloc_result = (SSERV_Info**)realloc(infos,
                        sizeof(SSERV_Info*) * (infos_capacity*2 + 1));
                if (realloc_result == NULL) {
                    /* If error with realloc, return as much as could allocate
                     * for*/
                    infos_count--; /*Will just rewrite last info with NULL to
                                     mark end of array*/
                    break;
                } else { /*If realloc successful */
                    infos = realloc_result;
                    infos_capacity = infos_capacity*2 + 1;
                }
            }
            infos[infos_count++] = info;
            char hostport[kHostportStringLength]; //just for debug
            SOCK_HostPortToString(info->host, info->port, hostport,
                    kHostportStringLength);
            /*Copy IP from stream to the result char array*/
            CORE_LOGF(eLOG_Note, ("Resolved to: [%s]", hostport));
        }
    }
    if (lbzk_answer) {
        free(lbzk_answer);
    }
    if (infos_count > 0) {
        /* ... and set this NULL, finalizing the array ...*/
        infos[infos_count] = NULL;
    } else {
        /*... or just deleting it, if nothing was found*/
        free(infos);
        infos = NULL;
    }

    return infos; /*notice that if lbos did not answer, this is NULL*/
}


/** Given an empty iterator which has service name in its "name" field
 * we fill it with all servers which were found by asking lbos
 * @param[in,out] iter
 *  We need name from it, then set "data" field with all servers. All
 *  previous "data" will be overwritten, causing possible memory leak.
 */
static void s_LBOS_FillCandidates(SLBOS_Data* data, const char* service)
{
    SSERV_Info** hostports_array = 0;
    /*
     * We iterate through known LBOSes to find working instance
     */
    unsigned int i = 0;

    /* We copy all lbos addresses to  */
    char** lbos_addresses = NULL;
    unsigned int addresses_count = 0;
    /* We suppose that number of addresses is constant (and so
       is position of NULL), so no mutex is necessary */
    while (s_LBOS_InstancesList[++addresses_count] != NULL) continue;
    lbos_addresses = (char**)calloc(addresses_count + 1, sizeof(char*));
    if (lbos_addresses == NULL) {
        CORE_LOG(eLOG_Warning, "s_LBOS_FillCandidates: No RAM. "
            "Returning.");
        return;
    }
    /* We can either delete this list or assign to s_LBOS_InstancesList,
     * but anyway we have to prepare */
    char** to_delete = lbos_addresses;
    CORE_LOCK_READ;
    for (i = 0; i < addresses_count; i++) {
        lbos_addresses[i] = strdup(s_LBOS_InstancesList[i]);
        /* If strdup returned NULL */
        if (lbos_addresses[i] == NULL) {
            CORE_UNLOCK;
            CORE_LOG(eLOG_Warning, "s_LBOS_FillCandidates: No RAM. "
                "Returning.");
            /*Free() what has been allocated*/
            size_t j = 0;
            for (j = 0;  j < i;  j++) {
                free(lbos_addresses[j]);
            }
            free(lbos_addresses);
            return;
        }
    }
    CORE_UNLOCK;
    lbos_addresses[addresses_count] = NULL;
    i = 0;
    do
    {
        CORE_LOGF_X(1, eLOG_Trace, ("Trying to find servers of \"%s\" with "
                "lbos at %s", service, lbos_addresses[i]));
        hostports_array = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            lbos_addresses[i], service,
                data->net_info);
        i++;
    } while (hostports_array == NULL  &&  lbos_addresses[i] != NULL);
    size_t last_instance = i-1; /* lbos instance with which we finished 
                                 * our search. It can be good or we just 
                                 * ran out of instances */
    i = 0;
    if (hostports_array != NULL) {
        while (hostports_array[++i] != NULL) continue;
        CORE_LOGF_X(1, eLOG_Trace, ("Found %u servers of \"%s\" with "
            "lbos at %s", i, service, lbos_addresses[last_instance]));
        if (last_instance != 0 && last_instance < addresses_count) {
            /* If we did not succeed with first lbos in list, let's swap
             * first lbos with good lbos. We will use CORE_LOCK_WRITE only
             in the very last moment */
            char* temp = lbos_addresses[last_instance];
            lbos_addresses[last_instance] = lbos_addresses[0];
            lbos_addresses[0] = temp;
            if (strcmp(lbos_addresses[0], s_LBOS_InstancesList[0])  !=  0) {
                /* By default, we free() locally created list */
                int/*bool*/ swapped = 0; /* to see which thread actually
                                            did swap */
                CORE_LOCK_WRITE;
                if (strcmp(lbos_addresses[0], s_LBOS_InstancesList[0]) != 0) {
                /* If nobody has swapped static list before us, We assign
                 * static list with our list, and delete previous static list
                 * instead of local list */
                    to_delete = s_LBOS_InstancesList;
                    swapped = 1;
                    s_LBOS_InstancesList = lbos_addresses;
                }
                CORE_UNLOCK;
                if (swapped) {
                    CORE_LOGF_X(1, eLOG_Trace, (
                        "Swapping lbos order, changing \"%s\" (good) and "
                        "%s (bad)",
                        lbos_addresses[last_instance],
                        lbos_addresses[0]));
                }
            }
        }
    } else {
        CORE_LOGF_X(1, eLOG_Trace, ("Ho servers of \"%s\" found by lbos",
                service));
    }
    assert(to_delete != NULL);
    for (i = 0; to_delete[i] != NULL; i++) {
        free(to_delete[i]);
        to_delete[i] = NULL;
    }
    free(to_delete);
    /*If we received answer from lbos, we fill candidates */
    if (hostports_array != NULL) {
        /* To allocate space once and forever, let's quickly find the number of
         * received addresses...
         */
        for (i = 0;  hostports_array[i] != NULL;  ++i) continue;
        /* ...and allocate space */
        SLBOS_Candidate* realloc_result = (SLBOS_Candidate*)realloc(data->cand,
                                            sizeof(SLBOS_Candidate) * (i + 1));
        if (realloc_result == NULL) {
            CORE_LOGF_X(1, eLOG_Warning, ("s_LBOS_FillCandidates: Could not "
                    "allocate space for all candidates, will use as much as "
                    "was allocated initially: %du",
                    (unsigned int)data->a_cand));
        } else {
            data->cand = realloc_result;
            data->a_cand = i + 1;
        }
        for (i = 0;  hostports_array[i] != NULL && i < data->a_cand;  i++) {
            data->cand[i].info = hostports_array[i];
            data->n_cand++;
        }
        free(hostports_array);
    }
    /*If we did not find answer from lbos, we just finish*/
}



static int s_LBOS_CheckAnnounceArgs(const char* service,
                                    const char* version,
                                    unsigned short port,
                                    const char* healthcheck_url,
                                    char** lbos_answer)
{
    if (healthcheck_url == NULL || strlen(healthcheck_url) == 0) {
        CORE_LOG(eLOG_Critical, "Error with announcement, "
            "no healthcheck_url specified.");
        return 0;
    }
    if ((strstr(healthcheck_url, "http://") != healthcheck_url)
        &&
        (strstr(healthcheck_url, "https://") != healthcheck_url)) {
        CORE_LOG(eLOG_Critical, "Error with announcement, missing http:// or "
                                "https:// in the beginning of healthcheck "
                                "URL.");
        return 0;
    }
    if (port < 1 || port > 65535) {
        CORE_LOG(eLOG_Critical, "Error with announcement, incorrect port.");
        return 0;
    }
    if (g_LBOS_StringIsNullOrEmpty(version)) {
        CORE_LOG(eLOG_Critical, "Error with announcement, "
                                "no version specified.");
        return 0;
    }
    if (g_LBOS_StringIsNullOrEmpty(service)) {
        CORE_LOG(eLOG_Critical, "Error with announcement, "
                                "no service name specified.");
        return 0;
    }
    if (lbos_answer == NULL) {
        CORE_LOG(eLOG_Critical, "Error with announcement, "
                                "no variable provided to save lbos answer.");
        return 0;
    }
    return 1;
}


static int s_LBOS_CheckDeannounceArgs(
    const char* service,
    const char* version,
    const char* host,
    unsigned short port)
{
    if (g_LBOS_StringIsNullOrEmpty(host)) {
        CORE_LOG(eLOG_Critical, "Error with announcement, "
            "no host specified.");
        return 0;
    }
    if (!g_LBOS_StringIsNullOrEmpty(host) && strstr(host, ":") != NULL) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for deannouncement, "
                                "please check that \"host\" parameter does "
                                "not contain protocol or port");
        return 0;
    }
    if (port < 1 || port > 65535) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for deannouncement, "
                                "incorrect port.");
        return 0;
    }
    if (g_LBOS_StringIsNullOrEmpty(version)) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for deannouncement, "
                                "no version specified.");
        return 0;
    }
    if (g_LBOS_StringIsNullOrEmpty(service)) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for deannouncement, "
                                "no service name specified.");
        return 0;
    }
    return 1;
}


static char* s_LBOS_Replace000WithIP(const char* healthcheck_url)
{ 
    /* By 'const' said that we will not touch healthcheck_url, but actually 
     * we need to be able to change it, so we copy it */
    char* my_healthcheck_url; /* new url with replaced "0.0.0.0" (if needed) */
    const char* replace_pos;/* to check if there is 0.0.0.0 */
    /* If we need to insert local IP instead of 0.0.0.0, we will */
    if ((replace_pos = strstr(healthcheck_url, "0.0.0.0")) == NULL) {
        my_healthcheck_url = strdup(healthcheck_url);
    } else {
        my_healthcheck_url = (char*)calloc(kMaxLineSize, sizeof(char));
        if (my_healthcheck_url == NULL) {
            CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
                                   "not enough RAM.");
            return NULL;
        }
        size_t chars_to_copy = replace_pos - healthcheck_url;
        strncpy(my_healthcheck_url, healthcheck_url, chars_to_copy);
        my_healthcheck_url[kMaxLineSize-1] = '\0'; /* you better do it
                                                    * after strncpy...    */
        char hostname[kMaxLineSize];
        if (g_LBOS_UnitTesting_GetLBOSFuncs()
                 ->GetHostByAddr(0, hostname, kMaxLineSize - 1, eOn) == NULL) {
            CORE_LOG(eLOG_Warning,  "Error with announcement, "
                                    "cannot find local IP.");
            free(my_healthcheck_url);
            return NULL;
        }
        strcat(my_healthcheck_url, strlwr(hostname));
        strcat(my_healthcheck_url, replace_pos + strlen("0.0.0.0"));
    }
    return my_healthcheck_url;
}


/*/////////////////////////////////////////////////////////////////////////////
//                             UNIT TESTING                                  //
/////////////////////////////////////////////////////////////////////////////*/
/** Check whether lbos mapper is turned ON or OFF
 * @return
 *  address of static variable s_LBOS_TurnedOn
 * @see
 *  SERV_LBOS_Open()                                                         */
int* g_LBOS_UnitTesting_PowerStatus(void)
{
    return &s_LBOS_TurnedOn;
}


SLBOS_Functions* g_LBOS_UnitTesting_GetLBOSFuncs(void)
{
    return &s_LBOS_funcs;
}

/**  Check whether lbos mapper is turned ON or OFF
 *  @return
 *   Address of static variable s_LBOS_Init
 *  @see
 *   SERV_LBOS_Open()                                                        */
int* g_LBOS_UnitTesting_InitStatus(void)
{
    return &s_LBOS_Init;
}


/**  List of addresses of lbos that is maintained in actual state.
 *  @return
 *   address of static variable s_LBOS_InstancesList.
 *  @see
 *   SERV_LBOS_Open(), s_LBOS_FillCandidates()                               */
char*** g_LBOS_UnitTesting_InstancesList(void)
{
    return &s_LBOS_InstancesList;
}


/**  Pointer to s_LBOS_CurrentDomain
 *  @return
 *   address of static variable s_LBOS_CurrentDomain.
 *  @see                                                                     */
char** g_LBOS_UnitTesting_CurrentDomain(void)
{
    return &s_LBOS_CurrentDomain;
}


/**  Pointer to s_LBOS_CurrentRole
 *  @return
 *   address of static variable s_LBOS_CurrentRole.
 *  @see                                                                     */
char** g_LBOS_UnitTesting_CurrentRole(void)
{
    return &s_LBOS_CurrentRole;
}


int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod (SERV_ITER iter,
        ELBOSFindMethod method)
{
    if (!g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData)) {
        CORE_LOG_X(1, eLOG_Error, "Could not set lbos find method, problem "
                "with iterator.");
        return (0);
    }
    SLBOS_Data* data = (SLBOS_Data*) iter->data;
    data->find_method = method;
    return (1);
}


int/*bool*/ g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles (const char* roleFile,
        const char* domainFile)
{
    if (roleFile != NULL) {
        kRoleFile = roleFile;
    }
    if (domainFile != NULL) {
        kDomainFile = domainFile;
    }
    return 1;
}


/* UNIT TESTING*/
/**  Set custom address for lbos. Can be both hostname:port and IP:port.
 *  Intended mostly for testing.
 * @param[in] iter
 *  Where to set address for lbos. The settings is made for only one iterator*/
int/*bool*/ g_LBOS_UnitTesting_SetLBOSaddress (SERV_ITER iter, char* address) {
    if (!g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData)) {
        CORE_LOG_X(1, eLOG_Error, "Could not set lbos address, problem with "
                "iterator.");
        return (0);
    }
    SLBOS_Data* data = (SLBOS_Data*) iter->data;
    data->lbos_addr = address;
    return (1);
}


struct SLBOS_AnnounceHandle_Tag** g_LBOS_UnitTesting_GetAnnouncedServers(void)
{
    return &s_LBOS_AnnouncedServers;
}


int g_LBOS_UnitTesting_GetAnnouncedServersNum(void)
{
    return s_LBOS_AnnouncedServersNum;
}

int g_LBOS_UnitTesting_FindAnnouncedServer(const char*              service,
    const char*             version,
    unsigned short          port,
    const char*             host) {
    return s_LBOS_FindAnnouncedServer(service, version, port, host);
}


/*/////////////////////////////////////////////////////////////////////////////
//                      DATA CONSTRUCTOR/DESTRUCTOR                          //
/////////////////////////////////////////////////////////////////////////////*/
/** Constructor for SLBOS_Data. Returns pointer to new empty initialized
 * SLBOS_Data.                                                              */
static SLBOS_Data* s_LBOS_ConstructData(size_t candidatesCapacity)
{
    SLBOS_Data* data;

    if (!(data = (SLBOS_Data*) calloc(1, sizeof(SLBOS_Data))))
    {
        CORE_LOG_X(1, eLOG_Error, "Could not allocate memory for lbos mapper");
        return NULL;
    }
    /*
     * We consider that there will never be more than 20 candidates, which
     * is not many for us
     */
    data->a_cand = candidatesCapacity;
    data->pos_cand = 0;
    data->n_cand = 0;
    data->lbos_addr = NULL;
    data->find_method = eLBOSFindMethod_None;
    data->cand = (SLBOS_Candidate*)calloc(candidatesCapacity, 
                                          sizeof(SLBOS_Candidate));
    return data;
}


/** Destructor for SLBOS_Data.
 * @param[in] data_to_delete
 *  Please note that you have to set pointer to NULL yourself.               */
static void s_LBOS_DestroyData(SLBOS_Data* data)
{
    if (data == NULL) {
        return;
    }
    if (data->cand != NULL) {
        size_t i;
        for (i = data->pos_cand; i < data->n_cand; i++) {
            if (data->cand[i].info != NULL) {
                free(data->cand[i].info);
                data->cand[i].info = NULL;
            }
        }
        free(data->cand);
    }
    if (data->net_info) {
        ConnNetInfo_Destroy(data->net_info);
    }
    free(data);
}


/*/////////////////////////////////////////////////////////////////////////////
//                        MAIN MAPPER FUNCTIONS                              //
/////////////////////////////////////////////////////////////////////////////*/
/**  This function test existence of the application that should always be
 *  found - lbos itself. If it is not found, we turn lbos off.               */
static void s_LBOS_Initialize(void)
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter = (SERV_ITER)calloc(1, sizeof(*iter));
    if (iter == NULL) {
        CORE_LOG_X(1, eLOG_Warning, "RAM error. "
                                    "Turning lbos off in this process.");
        s_LBOS_TurnedOn = 0;
        return;
    }
    iter->name = service;
    net_info = ConnNetInfo_Create(service);
    iter->op = SERV_LBOS_Open(iter, net_info, NULL);
    ConnNetInfo_Destroy(net_info);
    if (iter->op == NULL) {
        CORE_LOG_X(1, eLOG_Warning, "Turning lbos off in this process.");
        s_LBOS_TurnedOn = 0;
    } else {
        s_LBOS_Close(iter);
        s_LBOS_TurnedOn = 1;
    }
    free(iter);

    /* 
     * Load DTAB Local from registry
     */
    free(s_LBOS_DTABLocal);
    s_LBOS_DTABLocal = (char*)calloc(kMaxLineSize, sizeof(char));
    if (s_LBOS_DTABLocal == NULL) {
        CORE_LOG_X(1, eLOG_Warning, "RAM error. "
                                    "Turning lbos off in this process.");
        s_LBOS_TurnedOn = 0;
        return;
    }
    CORE_REG_GET("CONN", "DTAB", s_LBOS_DTABLocal, kMaxLineSize, NULL);
    if (g_LBOS_StringIsNullOrEmpty(s_LBOS_DTABLocal)) {
        CORE_LOG_X(1, eLOG_Trace, "No DTAB in registry");
    } else {
        CORE_LOGF_X(1, eLOG_Trace, 
                   ("DTAB from registry: %s ", s_LBOS_DTABLocal));
        /* If string does not start with "DTab-Local:", we add it */
        if (strstr(s_LBOS_DTABLocal, "DTab-Local:") != s_LBOS_DTABLocal)
        {
            char* tempDTAB = (char*)calloc(strlen(s_LBOS_DTABLocal) + 
                                                    strlen("DTab-Local: ") + 1, 
                                    sizeof(char));
            if (tempDTAB == NULL) {
                CORE_LOG_X(1, eLOG_Warning, "RAM error. Turning lbos off in "
                                            "this process.");
                s_LBOS_TurnedOn = 0;
                return;
            }
            strcat(tempDTAB, "DTab-Local: ");
            strcat(tempDTAB, s_LBOS_DTABLocal);
            free(s_LBOS_DTABLocal);
            s_LBOS_DTABLocal = tempDTAB;
        }
    }
}


/** After we receive answer from dispd.cgi, we parse header
 * to get all hosts.                                                         */
static EHTTP_HeaderParse s_LBOS_ParseHeader(const char*      header,
                                            void* /* int* */ response,
                                            int              server_error)
{
    int* response_output = (int*)response;
    int code = 0/*success code if any*/;
    if (server_error) {
        if (response != NULL)
            *response_output = server_error;
        return eHTTP_HeaderError;
    }
    if (sscanf(header, "%*s %d", &code) < 1) {
        if (response != NULL)
            *response_output = 503; /* Some kind of server error */
        return eHTTP_HeaderError;
    }
    /* check for empty document */
    if (response != NULL)
        *response_output = code;
    return eHTTP_HeaderSuccess;
}


static void s_LBOS_Reset(SERV_ITER iter)
{
    size_t i;
    /*
     * First, we need to check if it is our iterator. It would be bad
     * to mess with other mapper's iterator
     */
    if (!g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_NoCheck)) {
        CORE_LOGF_X(1, eLOG_Error,
                ("Trying to reset iterator  of [%s] with [%s]",
                        iter->op->mapper, s_lbos_op.mapper));
        return;
    }
    /*
     * Check passed, it is lbos's iterator. Now we can be sure that we know
     * how to reset it
     */
    SLBOS_Data* data = (SLBOS_Data*) iter->data;
    if (data != NULL) {
        if (data->cand) {
            for (i = data->pos_cand;  i < data->n_cand;  i++)
                free(data->cand[i].info);
            free(data->cand);
            /*There will hardly be more than 20 candidates, so we allocate
             * memory for candidates straight away, no array of pointers
             */
            data->cand = (SLBOS_Candidate*)calloc(data->a_cand, 
                                                  sizeof(SLBOS_Candidate));
            if (data->cand == NULL) {
                CORE_LOG(eLOG_Warning, "s_LBOS_Reset: No RAM. "
                    "Failed to create iterator.");
                data->a_cand = 0;
                data->n_cand = 0;
                data->pos_cand = 0;
            }
#ifdef _DEBUG
            /** We check both that it returns zero and does not crash accessing
             * allocated memory  */
            for (i = 0;  i < data->n_cand;  i++) {
                assert(data->cand + i != NULL);
                data->cand[i].info = (SSERV_Info*)malloc(sizeof(SSERV_Info));
                free(data->cand[i].info);
                data->cand[i].info = NULL;
            }
#endif
        }
        data->n_cand = 0;
        data->pos_cand = 0;
    }
    return;
}


/** Not implemented in lbos mapper. */
static int/*bool*/ s_LBOS_Feedback (SERV_ITER a, double b, int c)
{
    return 0;
}


/** Not implemented in lbos mapper. */
static int/*bool*/ s_LBOS_Update(SERV_ITER iter, const char* text, int code)
{
    return 1;
}


static void s_LBOS_Close (SERV_ITER iter)
{
    /*
     * First, we need to check if it is our iterator. It would be bad
     * to mess with other mapper's iterator
     */
    if (!g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData)) {
        CORE_LOGF_X(1, eLOG_Error,
                ("Trying to close iterator of [%s] with %s",
                 iter->op->mapper, s_lbos_op.mapper));
        return;
    }
    SLBOS_Data* data = (SLBOS_Data*) iter->data;
    if(data->n_cand > 0) {
        /*s_Reset() has to be called before*/
        s_LBOS_Reset(iter);
    }
    s_LBOS_DestroyData((SLBOS_Data*)iter->data);
    iter->data = NULL;
    return;
}


/** 1) If iterator is valid - find and return next server;
 *  2) If iterator is outdated or empty - we update it and fill it with hosts
 *     and go to 1)                                                          */
static SSERV_Info* s_LBOS_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    /*
     * We store snapshot of lbos answer in memory. So we just go from last
     * known position. Position is stored in n_skip.
     * If we want to update list - well, we update all the list and start from
     * the beginning.
     */

    /* host_info is implemented only for lbsmd mapper, so we just set it to
     * NULL, if it even exists.
     */
    if (host_info) {
        *host_info = NULL; /*No host data*/
    }

    if (iter == NULL) { /* we can do nothing if this happens */
        return NULL;
    }

    if (iter->data == NULL) {
        iter->data = s_LBOS_ConstructData(kInitialCandidatesCount);
    }

    SLBOS_Data* data = (SLBOS_Data*)(iter->data); /** set typed variable
                                                     for convenience */

    if (data->n_cand == 0) { /**< this is possible after reset()*/
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates(data, iter->name);
    }
    /*
     * If there are some servers left to show, we move iterator and show next
     * Otherwise, return NULL
     */
    if (data->pos_cand < data->n_cand) {
        data->pos_cand++;
        return (SSERV_Info*)(data->cand[data->pos_cand-1].info);
    }  else  {
        return NULL;
    }
}


/** Creates iterator and fills it with found servers.
 * @param[in,out] iter
 *  Pointer to iterator. It is read and rewritten
 * @param[out] info
 *  Pointer to variable to return pointer to info about server.
 * @param[out] host_info
 *  To return info about host
 * @param[in[ no_dispd_follows
 *  To tell whether there will be dispd if all other methods fail.           */
const SSERV_VTable* SERV_LBOS_Open( SERV_ITER            iter,
                                    const SConnNetInfo*  net_info,
                                    SSERV_Info**         info     )
{
    if (s_LBOS_Init == 0) {
        CORE_LOCK_WRITE;
            if (s_LBOS_InstancesList == NULL) {
                s_LBOS_InstancesList = g_LBOS_GetLBOSAddresses();
            }
        CORE_UNLOCK;
        s_LBOS_TurnedOn = 1; /* To ensure that initialization does
                                not depend on this variable */
        s_LBOS_Init = 1;
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return NULL;
    }
    /*
     * First, we need to check arguments
     */
    if(iter == NULL) {
        CORE_LOG(eLOG_Warning, "Parameter \"iter\" is null, not able "
                "to continue SERV_LBOS_Open");
        return NULL;
    }
    if (iter->name == NULL) {
        CORE_LOG(eLOG_Warning, "\"iter->name\" is null, not able "
            "to continue SERV_LBOS_Open");
        return NULL;
    }
    /*
     * Arguments OK, start work
     */
    if (info != NULL) {
        *info = NULL;
    }
    SLBOS_Data* data = s_LBOS_ConstructData(kInitialCandidatesCount);
    if(net_info == NULL) {
        CORE_LOG(eLOG_Warning,
                 "Parameter \"net_info\" is null, creating net info. "
                 "Please, fix the code and provide net_info.");
        data->net_info = ConnNetInfo_Create(NULL);
    } else {
        data->net_info = ConnNetInfo_Clone(net_info);
    }

    /*
     * Check DTAB Local in registry
     */
    if (!g_LBOS_StringIsNullOrEmpty(s_LBOS_DTABLocal)) 
    {
        char* dtab = (char*)calloc(kMaxLineSize, sizeof(char));
        if (dtab != NULL) {
            strcat(dtab, s_LBOS_DTABLocal);
            strcat(dtab, "\r\n");
            ConnNetInfo_ExtendUserHeader(data->net_info, dtab);
            free(dtab);
        }
    };

    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates(data, iter->name);
    /* Connect to lbos, read what is needed, build iter, info, host_info
     */
    if (!data->n_cand) {
        s_LBOS_DestroyData(data);
        return NULL;
    }
    /* Something was found, now we can use iter */

    /*Just explicitly mention here to do something with it*/
    iter->data = data;

    return &s_lbos_op;
}


/*/////////////////////////////////////////////////////////////////////////////
//                        ANNOUNCEMENT/DEANNOUCEMENT                         //
/////////////////////////////////////////////////////////////////////////////*/
/** For unit testing was divided from g_LBOS_Announce (interception of 
 * parameters). For full description see g_LBOS_Announce      
 * @sa 
 *  g_LBOS_Announce()                                                        */
static 
ELBOS_Result s_LBOS_Announce(const char*             service,
                             const char*             version,
                             unsigned short          port,
                             const char*             healthcheck_url,
                             char**                  lbos_answer)
{
    if (s_LBOS_Init == 0) {
        CORE_LOCK_WRITE;
            if (s_LBOS_InstancesList == NULL) {
                s_LBOS_InstancesList = g_LBOS_GetLBOSAddresses();
            }
        CORE_UNLOCK;
        s_LBOS_TurnedOn = 1; /* To ensure that initialization does
                                not depend on this variable */
        s_LBOS_Init = 1;
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return eLBOS_Off;
    }
    char** lbos_addresses = g_LBOS_GetLBOSAddresses();
    int response_code = 0;
    SConnNetInfo* net_info = ConnNetInfo_Create(service);
    net_info->req_method = eReqMethod_Post;
    ELBOS_Result return_code = eLBOS_Success;
    char* buf = (char*)calloc(kMaxLineSize, sizeof(char));
    if (buf == NULL) {
        CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
            "not enough RAM.");
        return_code = eLBOS_ServerError;
        goto clear_and_exit;
    }
    /*
     * Let's try announce 
     */
    int i = 0;
    for (i = 0;  lbos_addresses[i] != NULL && response_code != 200;  ++i)
    {
        /* We have to check if specified lbos is from the same domain*/
        if (!g_LBOS_CheckDomain(lbos_addresses[i])) {
            CORE_LOGF_X(1, eLOG_Warning, ("[%s] is not from local domain [%s]. "
                                          "Announcement in foreign domain "
                                          "is not allowed.",
                                          lbos_addresses[i],
                                          s_LBOS_ReadDomain()));
            continue;
        }
        char query [kMaxLineSize];
        sprintf(query,
                "http://%s/lbos/json/announce?name=%s&"
                "version=%s&port=%hu&check=%s",
                lbos_addresses[i], service, version, port, 
                healthcheck_url);
        CONN conn = s_LBOS_ConnectURL(net_info,  query, &response_code);
        /* Let's check if lbos exists at this address */
        size_t bytesRead;
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read(conn, buf,
                          kMaxLineSize, &bytesRead, eIO_ReadPlain);
        
        CONN_Close(conn);
    }
    if (!g_LBOS_StringIsNullOrEmpty(buf)) {
        *lbos_answer = strdup(buf);
    } else {
        return_code = eLBOS_NoLBOS;
        goto clear_and_exit;
    }
    /* If no lbos found */
    if (response_code == 0) {
        CORE_LOG(eLOG_Warning, "g_LBOS_Announce: could not announce! "
                               "No lbos found.");
        return_code = eLBOS_NoLBOS;
        goto clear_and_exit;
    }
    /* If we could not announce, it is really bad */
    if (response_code != 200) {
        CORE_LOG(eLOG_Warning, "g_LBOS_Announce: could not announce! "
                               "lbos returned error code. "
                               "lbos answer was written to lbos_answer.");
        return_code = eLBOS_ServerError;
        goto clear_and_exit;
    }
    char* lbos_addr = (char*)calloc(kMaxLineSize, sizeof(char)); /* will not be 
                                                           * free()'d */
    if (lbos_addr == NULL) {
        CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
                               "not enough RAM.");
        return_code = eLBOS_ServerError;
        goto clear_and_exit;
    }
    int parsed_symbols = sscanf(buf, "{\"watcher\":\"%[^\"]\"}", lbos_addr);
    if (parsed_symbols != 1) {
        CORE_LOG(eLOG_Warning, "g_LBOS_Announce: lbos answered 200 OK, but "
                               "output could not be parsed");
        return_code = eLBOS_ServerError;
        goto clear_and_exit;
    } else {
    /* If announce finished with success, we parsed it to extract lbos ip:port.
     * We free() original output and replace it with ip:port                 */
        free(*lbos_answer);
        *lbos_answer = lbos_addr;
    }

    s_LBOS_AddAnnouncedServer(service, version, port, healthcheck_url);

    /* Cleanup */
    clear_and_exit:
        for (i = 0; lbos_addresses != NULL && lbos_addresses[i] != NULL; ++i) {
            free(lbos_addresses[i]);
            lbos_addresses[i] = NULL;
        }
        free(lbos_addresses);
        free(buf);
        ConnNetInfo_Destroy(net_info);
    return return_code;
}


ELBOS_Result LBOS_Announce(const char*             service,
                           const char*             version,
                           unsigned short          port,
                           const char*             healthcheck_url,
                           char**                  lbos_answer)
{
    /*
     * First we check input arguments
     */
    if (s_LBOS_CheckAnnounceArgs(service, version, port, healthcheck_url, 
                                                             lbos_answer) == 0)
    {
        return eLBOS_InvalidArgs;
    }
    /*
     * Pre-assign variables
     */
    *lbos_answer = NULL;
    /* Check if we need to replace 0.0.0.0 with local IP, and do it if needed*/
    char* my_healthcheck_url = s_LBOS_Replace000WithIP(healthcheck_url);
    if (my_healthcheck_url == NULL) {
        return eLBOS_DNSResolveError;
    }
    char* healthcheck_encoded  = s_LBOS_URLEncode(my_healthcheck_url);
    char* service_encoded      = s_LBOS_URLEncode(service);
    char* version_encoded      = s_LBOS_URLEncode(version);

    /* Announce */
    ELBOS_Result result = 
            g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx(service, 
                                                          version, 
                                                          port,
                                                          my_healthcheck_url, 
                                                          lbos_answer);     
    /* Cleanup */
    free(healthcheck_encoded);
    free(my_healthcheck_url);
    free(version_encoded);
    free(service_encoded);
    return result;
}


ELBOS_Result LBOS_Deannounce(const char*       service,
                            const char*        version,
                            const char*        host,
                            unsigned short     port)
{
    /*
     * First we check input arguments
     */
    if (s_LBOS_CheckDeannounceArgs(service, version, 
                                   host, port) == 0) {
        return eLBOS_InvalidArgs;
    }
    /*
     * Check if lbos is ON 
     */
    if (s_LBOS_Init == 0) {
        CORE_LOCK_WRITE;
            if (s_LBOS_InstancesList == NULL) {
                s_LBOS_InstancesList = g_LBOS_GetLBOSAddresses();
            }
        CORE_UNLOCK;
        s_LBOS_TurnedOn = 1; /* To ensure that initialization does
                                not depend on this variable */
        s_LBOS_Init = 1;
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return eLBOS_Off;
    }
    /*
     * If we are here, arguments are good!
     */
    SConnNetInfo* net_info = ConnNetInfo_Create(service);
    net_info->req_method = eReqMethod_Post;
    char buf[kMaxLineSize];
    char query[kMaxLineSize];
    char* service_encoded = s_LBOS_URLEncode(service);
    char* version_encoded = s_LBOS_URLEncode(version);
    int response_code = 0;
     EIO_Status read_status = eIO_Timeout; /* If no lbos addresses, 
                                            * then no lbos      */
    char** lbos_addresses = g_LBOS_GetLBOSAddresses();
    /*
     * Try deannounce
     */
    int i = 0;
    for (i = 0; lbos_addresses[i] != NULL && response_code != 200; ++i) {
        if (!g_LBOS_CheckDomain(lbos_addresses[i])) {
            CORE_LOGF_X(1, eLOG_Warning, ("[%s] is not from local domain "
                                          "[%s]. Deannouncement in foreign "
                                          "domain is not allowed.",
                                          lbos_addresses[i],
                                          s_LBOS_ReadDomain()));
            continue;
        }
        sprintf(query,
                "http://%s/lbos/json/conceal?name=%s&version=%s&port=%hu",
                lbos_addresses[i], service_encoded, version_encoded, port);
        /* If host was provided, we append it to query */
        if (!g_LBOS_StringIsNullOrEmpty(host)) {
            sprintf(query + strlen(query), "&ip=%s", host);
        }
        CONN conn = s_LBOS_ConnectURL(net_info, query, &response_code);
        /* Let's check if lbos exists at this address */
        size_t bytesRead;
        read_status =
            g_LBOS_UnitTesting_GetLBOSFuncs()->Read(conn, buf, kMaxLineSize,
                                                    &bytesRead, eIO_ReadPlain);
        CONN_Close(conn);
    }
    /*
     * Cleanup
     */
    for (i = 0; lbos_addresses[i] != NULL; ++i) {
        free(lbos_addresses[i]);
        lbos_addresses[i] = NULL;
    }
    free(lbos_addresses);
    free(version_encoded);
    free(service_encoded);
    ConnNetInfo_Destroy(net_info);
    /*
     * If we could not announce, it is really bad
     */
    if (read_status == eIO_Timeout || response_code == 0) {
        CORE_LOG(eLOG_Warning, "LBOS_Dennounce: could not deannounce! "
                               "lbos not found.");
        return eLBOS_NoLBOS;
    }
    if (response_code != 200) {
        CORE_LOG(eLOG_Warning, "LBOS_Dennounce: could not deannounce! "
                               "lbos returned error code.");
        return eLBOS_ServerError;
    }
    s_LBOS_RemoveAnnouncedServer(service, version, port, host);
    return eLBOS_Success;
}


/** Deannounce all announced servers.
 * @note    
 *  Though this function is mt-safe, you should fully recognize the fact 
 *  results of simultaneous multiple deannouncement of the same service can be 
 *  unpredictable. For example, if service has just been deannounced in 
 *  different thread, this thread will return error "service is not announced".

 * @return 
 *  1 - all services deannounced successfully 
 *  0 - at least one server could not be deannounced */
void LBOS_DeannounceAll()
{
    CORE_LOCK_READ;
    struct SLBOS_AnnounceHandle_Tag** arr = &s_LBOS_AnnouncedServers;
    unsigned int servers = s_LBOS_AnnouncedServersNum;
    struct SLBOS_AnnounceHandle_Tag* local_arr = 
              (struct SLBOS_AnnounceHandle_Tag*)calloc(servers, sizeof(**arr)); 
    if (local_arr == NULL) {
        CORE_LOG_X(1, eLOG_Warning, "RAM error. Cancelling deannounce all.");
        CORE_UNLOCK;
        return;
    }
    /* 
     * Copy servers list to local variable in case other thread 
     * deletes it or wants to announce new server which we would not want to 
     * deannounce, since it will be announced after call of this function.
     */
    unsigned int i;
    for(i = 0;  i < servers;  i++) {
        local_arr[i].version        = strdup((*arr)[i].version);
        local_arr[i].host           = strdup((*arr)[i].host);
        local_arr[i].service        = strdup((*arr)[i].service);
        local_arr[i].port           =        (*arr)[i].port;
    }
    CORE_UNLOCK;
    for(i = 0;  i < servers; i++) {
        /* Deannounce */
        LBOS_Deannounce(
                   local_arr[i].service,
                   local_arr[i].version,
                   local_arr[i].host,
                   local_arr[i].port);
        /* Cleanup */
        free(local_arr[i].version);
        free(local_arr[i].host);
        free(local_arr[i].service);
    }
    free(local_arr);
    return;
}


#undef NCBI_USE_ERRCODE_X

#endif /*CONNECT___NCBI_LBOS__C*/
