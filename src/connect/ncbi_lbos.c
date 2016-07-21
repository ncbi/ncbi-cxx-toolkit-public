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
 *   A client for service discovery API based on LBOS.
 *   LBOS is a client for ZooKeeper cloud-based DB.
 *   LBOS allows to announce, deannounce and resolve services.
 */


#include "ncbi_ansi_ext.h"
#include <connect/ncbi_http_connector.h>
#include "ncbi_lbosp.h"
#include "ncbi_priv.h"
#include <stdlib.h> /* free, realloc, calloc, malloc */
#include <ctype.h> /* isdigit */
#include "parson.h"
#define            kHostportStringLength     (16+1+5)/**<
                                                     strlen("255.255.255.255")+
                                                     strlen(":") +
                                                     strlen("65535")         */
#define            kMaxLineSize              1024  /**< used to build strings*/
                                                         

#define            NCBI_USE_ERRCODE_X        Connect_LBSM /**< Used in
                                                               CORE_LOG*_X  */

#ifdef NCBI_COMPILER_MSVC
#   define LBOS_STRTOK strtok_s
#else
#   define LBOS_STRTOK strtok_r
#endif 
/*/////////////////////////////////////////////////////////////////////////////
//                         STATIC VARIABLES                                  //
/////////////////////////////////////////////////////////////////////////////*/

#ifdef NCBI_OS_MSWIN
    static const char* kLbosresolverFile      = "C:\\Apps\\Admin_Installs\\etc"
                                                "\\ncbi\\lbosresolver";
#else 
    static const char* kLbosresolverFile      = "/etc/ncbi/lbosresolver";
#endif

static const char* kLBOSQuery                 = "/lbos/v3/services/"
                                                "?format=json&show=all&q=";

/*
 * LBOS registry section for announcement
 */
static const char* kLBOSAnnouncementSection    = "LBOS_ANNOUNCEMENT";
static const char* kLBOSServiceVariable        = "SERVICE";
static const char* kLBOSVersionVariable        = "VERSION";
static const char* kLBOSServerHostVariable     = "HOST";
static const char* kLBOSPortVariable           = "PORT";
static const char* kLBOSHealthcheckUrlVariable = "HEALTHCHECK";
static const char* kLBOSMetaVariable           = "meta";


static SConnNetInfo* s_EmptyNetInfo         = NULL; /* Do not change..       */
static       char*   s_LBOS_Lbosresolver    = NULL; /*          ..after init */
static const int     kInitialCandidatesCount = 1;   /* For initial memory
                                                       allocation            */
static       int     s_LBOS_TurnedOn         = 1;   /* If LBOS cannot 
                                                       resolve even itself,
                                                       we turn it off for
                                                       performance           */
static       int     s_LBOS_Init             = 0;   /* If client has already
                                                       been initialized      */

static 
struct SLBOS_AnnounceHandle_Tag* 
                   s_LBOS_AnnouncedServers      = NULL;
static 
unsigned int       s_LBOS_AnnouncedServersNum   = 0;
static 
unsigned int       s_LBOS_AnnouncedServersAlloc = 0;
/*/////////////////////////////////////////////////////////////////////////////
//                     STATIC FUNCTION DECLARATIONS                          //
/////////////////////////////////////////////////////////////////////////////*/
static SSERV_Info**   s_LBOS_ResolveIPPort (const char*   lbos_address,
                                            const char*   serviceName,
                                            SConnNetInfo* net_info);
static char *         s_LBOS_UrlReadAll    (SConnNetInfo* net_info, 
                                            const char*   url, 
                                            int*          status_code,
                                            char**        status_message);
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
#if defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN
static const char*    s_LBOS_ReadLbosresolver(void);
#endif /* defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN */
static 
EHTTP_HeaderParse     s_LBOS_ParseHeader   (const char*   header,
                                            void*         /* SLBOS_UserData* */
                                                          response,
                                            int           server_error);
/*LBOS is intended to answer in 0.5 sec, or is considered dead*/
static const STimeout   kLBOSTimeout       =  {10, 500000};
static char*            s_LBOS_Instance    =  NULL;
static char*            s_LBOS_DTABLocal   =  NULL;

static 
unsigned short s_LBOS_Announce(const char*             service,
                               const char*             version,
                               const char*             host,
                               unsigned short          port,
                               const char*             healthcheck_url,
                               const char*             meta,
                               char**                  lbos_answer,
                               char**                  http_status_message);


/*/////////////////////////////////////////////////////////////////////////////
//                     UNIT TESTING VIRTUAL FUNCTION TABLE                   //
/////////////////////////////////////////////////////////////////////////////*/
static SLBOS_Functions s_LBOS_funcs = {
                s_LBOS_ResolveIPPort
                ,CONN_Read
                ,s_LBOS_FillCandidates
                ,s_LBOS_DestroyData
                ,s_LBOS_GetNextInfo
                ,s_LBOS_Initialize
                ,s_LBOS_UrlReadAll
                ,s_LBOS_ParseHeader
                ,SOCK_GetLocalHostAddress
                ,s_LBOS_Announce
};


/** Get pointer to s_LBOS_MyPort */
///////////////////////////////////////////////////////////////////////////////
//                  INTERNAL STORAGE OF ANNOUNCED SERVERS                    //
///////////////////////////////////////////////////////////////////////////////
/** Add server to list of announced servers.
 * @attention
 *  Intended to be used inside critical section, with caller having
 *  locked already, so does not lock.  */
static int  s_LBOS_FindAnnouncedServer(const char*            service,
                                       const char*            version,
                                       unsigned short         port,
                                       const char*            host)
{
    /* For convenience, we use some aliases. Not references, because this 
     * is not C++ */
    struct SLBOS_AnnounceHandle_Tag** arr;
    unsigned int* count;
    int retval;
    unsigned int i = 0;

    arr = &s_LBOS_AnnouncedServers;
    if (*arr == NULL)
        return -1;
    count = &s_LBOS_AnnouncedServersNum;
    retval = -1;

    /* Just iterate and compare */    
    for (i = 0;  i < *count;  i++) {
        if (strcmp(service, (*arr)[i].service) == 0 
            &&
            strcmp(version, (*arr)[i].version) == 0 
            &&
            strcmp(host, (*arr)[i].host) == 0 
            && 
            (*arr)[i].port == port)
        {
            retval = i;
        }
    }

    return retval;
}


/** Remove server from list of announced servers.
 * @attention
 *  Intended to be used inside critical section, with caller having
 *  locked already, so does not lock.  */
static 
int/*bool*/ s_LBOS_RemoveAnnouncedServer(const char*          service,
                                         const char*          version,
                                         unsigned short       port,
                                         const char*          host)
{
    int return_code;
    struct SLBOS_AnnounceHandle_Tag** arr;
    unsigned int* count;
    int pos;


    /* If server was announced using local IP, it should be deleted
     * with IP "0.0.0.0"                                    */
    if (g_LBOS_StringIsNullOrEmpty(host)) {
        host = "0.0.0.0"; /* we safely reassign, because caller should care 
                             about memory leaks */
    }
    return_code = 1;
    /* For convenience, we use some aliases. Not references, because this 
     * is not C++ */
    arr = &s_LBOS_AnnouncedServers;
    if (*arr == NULL) {
        return_code = 0;
        goto unlock_and_return;
    }
    count = &s_LBOS_AnnouncedServersNum;

    /* Find node to delete*/
    pos = s_LBOS_FindAnnouncedServer(service, version, port, host);
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
    assert(return_code == 0 || return_code == 1);
    return return_code;
}


/** Add server to list of announced servers.
 * @attention
 *  Intended to be used inside critical section, with caller having
 *  locked already, so does not lock.  */
static 
int/*bool*/ s_LBOS_AddAnnouncedServer(const char*            service,
                                     const char*             version,
                                     unsigned short          port,
                                     const char*             healthcheck_url)
{
    int return_code;
    struct SLBOS_AnnounceHandle_Tag handle;
    SConnNetInfo * healthcheck_info;
    /* For convenience, we use some aliases. Not references, because this
     * is not C++ */
    struct SLBOS_AnnounceHandle_Tag** arr = &s_LBOS_AnnouncedServers;
    unsigned int* count = &s_LBOS_AnnouncedServersNum;
    unsigned int* alloc = &s_LBOS_AnnouncedServersAlloc;

    /* First we create object without using static variables, and then lock
     * critical section and copy object to static array. This will allow to 
     * to use most of multithreading */
    return_code = 1;
    /* extract host from healthcheck url */
    healthcheck_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    healthcheck_info->host[0] = '\0'; /* to be sure that it will be
                                           * overridden                      */
    ConnNetInfo_ParseURL(healthcheck_info, healthcheck_url);

    /* Create new element of list*/
    handle.host = strdup(healthcheck_info->host);
    handle.port = port;
    handle.version = strdup(version);
    handle.service = strdup(service);

    /* We search for the same server being already announced                 */  
    s_LBOS_RemoveAnnouncedServer(service, version, port,
                                 healthcheck_info->host);
    ConnNetInfo_Destroy(healthcheck_info);

    /* Allocate more space, if needed */
    if (*arr == NULL || *count == *alloc)
    {
        int new_size = *alloc*2 + 1;
        struct SLBOS_AnnounceHandle_Tag* realloc_result = 
            (struct SLBOS_AnnounceHandle_Tag*)realloc(*arr,
                new_size * sizeof(struct SLBOS_AnnounceHandle_Tag));
        if (realloc_result != NULL) {
            *arr = realloc_result;
            *alloc = new_size;
        } else {
            free(handle.version);
            free(handle.service);
            free(handle.host);
            return_code = 0;
            goto clear_and_exit;
        }
    }
    assert(arr != NULL);
    /* Copy handle to the end of array*/
    (*count)++;
    (*arr)[*count - 1] = handle;

clear_and_exit:
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
        return 1;
    }
    return 0;
}


/** Case-insensitive string lookup. 
 * @note
 *  Supposes that the string can be changed! No const modifier for dest      
 * @note
 *  If dest or lookup is NULL or empty, returns NULL */
const char* g_LBOS_strcasestr(const char* dest, const char* lookup)
{
    char* dest_lwr;
    char* lookup_lwr;
    const char* result;

    if (g_LBOS_StringIsNullOrEmpty(dest) || g_LBOS_StringIsNullOrEmpty(lookup)) 
    {
        return NULL;
    }
    dest_lwr = strlwr(strdup(dest));
    lookup_lwr = strlwr(strdup(lookup));

    result = strstr(dest_lwr, lookup_lwr);
    /* Set the same offset, but in original string */
    if (result != NULL) {
        result = dest + (result - dest_lwr);
    }

    free(dest_lwr);
    free(lookup_lwr);

    return result;
}


/**  Concatenate two C-strings and assign result to the left
 *  @param dest[in, out]
 *   Address of string to which you want to append another string. 
 *   This address will be realloc()'ed and returned, so dest should point to 
 *   heap and can be a result of function like strdup("my string").
 *   If it is NULL, a new string will be created.
 *  @param to_append[in]
 *   String that will be copied to the end of dest. 
 *   It will not be changed. If it is NULL or empty, and dest is not NULL,
 *   dest will be returned. If dest is NULL, empty string will be returned.
 *  @param dest_length[in]
 *   Can be NULL. Optimizes performance on sequence of concatenations like
 *   g_LBOS_StringConcat(g_LBOS_StringConcat(str,to_app1,&len), to_app2,&len).
 *   Value of data_length on first call should be 0.
 *  @return 
 *   reallocated dest on success, NULL if error happened and dest was 
 *   not changed         */
char*   g_LBOS_StringConcat(char*       dest, 
                            const char* to_append,
                            size_t*     dest_length)
{
    char* realloc_result;
    size_t dest_length_local = 0; /* not to handle if dest_length is NULL */
    size_t append_len;

    if (dest_length != NULL) {
        dest_length_local = *dest_length;
    }
    if (dest == NULL) {
        dest_length_local = 0;
    } else if (dest_length_local == 0) {
        dest_length_local = strlen(dest);
    }
    append_len = 0;
    if (!g_LBOS_StringIsNullOrEmpty(to_append)) {
        /* All done, in this case */
        append_len = strlen(to_append);
    }
    realloc_result = (char*)realloc(dest, dest_length_local + append_len + 1);
    if (realloc_result == NULL) {
        CORE_LOG_X(eLBOS_MemAlloc, eLOG_Critical, 
                    "g_LBOS_StringConcat: No RAM. Returning NULL.");
        free(dest);
        return NULL;
    }
    dest = realloc_result;
    memcpy(dest + dest_length_local, to_append, append_len);
    dest[dest_length_local + append_len] = '\0';
    dest_length_local += append_len;
    if (dest_length != NULL) {
        *dest_length = dest_length_local;
    }
    return dest;
}


char*   g_LBOS_StringNConcat(char*       dest,
                             const char* to_append,
                             size_t*     dest_length,
                             size_t      count)
{
    char* buf = (char*)malloc((count + 1) * sizeof(char));
    char* result;

    if (buf == NULL) {
        CORE_LOG_X(eLBOS_MemAlloc, eLOG_Critical, 
                 "g_LBOS_StringConcat: No RAM. Returning NULL.");
        free(buf);
        free(dest);
        return NULL;
    }
    memcpy(buf, to_append, count);
    buf[count] = '\0';
    result = g_LBOS_StringConcat(dest, buf, dest_length);
    free(buf);
    return result;
}


/* Wrapper of CORE_REG_GET that eliminates the need to know the length of the
 * parameter being read*/
char*   g_LBOS_RegGet(const char* section,
                      const char* name,
                      const char* def_value)
{
    size_t    totalBufSize     = kMaxLineSize;
    char*     realloc_result   = NULL;
    char*     buf              = (char*)malloc(totalBufSize * sizeof(char));

    if (buf == NULL) {
        CORE_LOG_X(eLBOS_MemAlloc, eLOG_Critical, 
                   "g_LBOS_RegGet: No RAM. Returning NULL.");
        return buf;
    }
    for (;;) {
        CORE_REG_GET(section, name, buf, totalBufSize, def_value);
        /* If we had enough space allocated */
        if (strlen(buf) < totalBufSize-1) {
            break;
        }
        /* If we (possibly) did not have enough space allocated
           then add space to buffer  */
        realloc_result = (char*)realloc(buf, sizeof(char)*(totalBufSize * 2));
        if (realloc_result == NULL) {
            CORE_LOG_X(eLBOS_MemAlloc, eLOG_Warning, 
                       "g_LBOS_RegGet: Buffer overflow while reading from "
                       "registry. Returning string at its maximum size");
            return buf;
        } else {
            buf = realloc_result;
            totalBufSize *= 2;
        }
    }
    return buf;
}


/** Checks iterator, fact that iterator belongs to this mapper, iterator data.
 * Only debug function.                                                      */
int/*bool*/ g_LBOS_CheckIterator(SERV_ITER              iter,
                                 ELBOSIteratorCheckType should_have_data)
{
    assert(iter != NULL);  /* we can do nothing if this happens */
    if (should_have_data == ELBOSIteratorCheckType_MustHaveData) {
        if (iter->data == NULL) {
            return 0;
        }
        assert(((SLBOS_Data*)iter->data)->a_cand >= 
            ((SLBOS_Data*)iter->data)->n_cand);
        assert(((SLBOS_Data*)iter->data)->pos_cand <=
            ((SLBOS_Data*)iter->data)->n_cand);
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


/**  This function is needed to get LBOS hostname in different situations.
 *  @param priority_find_method[in]
 *   The first method to try to find LBOS. If it fails, default order of
 *   methods will be used to find LBOS.
 *   Default order is:
 *   1) registry value of [CONN]->lbos;
 *   2) 127.0.0.1:8080;
 *   3) LBOS for current /etc/ncbi/{role, domain}.
 *   To not specify default method, use eLBOSFindMethod_None.
 *  @param lbos_addr[in]
 *   If priority_find_method is eLBOS_FindMethod_CustomHost, then LBOS is
 *   first looked for at hostname:port specified in this variable.           */
char* g_LBOS_GetLBOSAddressEx (ELBOSFindMethod priority_find_method,
                                const char* lbos_addr)
{
#if defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN
    const char* lbosaddress = NULL; /* for const strings */
#endif /* defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN */
    char* lbosaddress_temp = NULL;  /* for non-const strings */
    char* address = NULL;
    /* List of methods used, in their order */
    ELBOSFindMethod find_method_order[] = {
               priority_find_method /* eLBOSFindMethod_None, if not specified*/
               , eLBOS_FindMethod_Registry
               , eLBOS_FindMethod_Lbosresolve
    };
    size_t find_method_iter;
    size_t find_method_count = 
        sizeof(find_method_order) / sizeof(ELBOSFindMethod);
    CORE_LOG(eLOG_Trace, "Getting LBOS addresses...");
    /* Iterate through methods of finding LBOS address */
    for (find_method_iter = 0;  
         find_method_iter < find_method_count;
         ++find_method_iter)
    {
        /* If LBOS address has been filled - we're done */
        if (address != NULL)
            break;
        switch (find_method_order[find_method_iter]) {
        case eLBOSFindMethod_None :
            break;
        case eLBOS_FindMethod_CustomHost :
            if (g_LBOS_StringIsNullOrEmpty(lbos_addr)) {
                CORE_LOG_X(1, eLOG_Warning, "Use of custom LBOS address was "
                           "asked for, but no custom address was supplied. "
                           "Using default LBOS.");
                break;
            }
            address = strdup(lbos_addr);
            if (address == NULL) {
                CORE_LOG_X(1, eLOG_Warning, "Did not manage to copy custom "
                           "LBOS address. Probably insufficient RAM.");
            }
            break;
        case eLBOS_FindMethod_Lbosresolve :
#if defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN
            lbosaddress = s_LBOS_ReadLbosresolver();
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find LBOS using "
                           "/etc/ncbi/lbosresolve failed");
            } else {
                address = strdup(lbosaddress);
            }
#endif /* defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN */
            break;
        case eLBOS_FindMethod_Registry:
            lbosaddress_temp = g_LBOS_RegGet("CONN", "lbos", NULL);
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress_temp)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find LBOS in "
                                            "registry [CONN]lbos failed. "
                                            "Using address in "
                                            "/etc/ncbi/lbosresolver");
                free(lbosaddress_temp); /* just in case */
                lbosaddress_temp = NULL;
                break;
            }
            address = lbosaddress_temp;
            break;
        }
    }
    return address;
}


/* Get possible addresses of LBOS in default order */
char* g_LBOS_GetLBOSAddress(void)
{
    return g_LBOS_GetLBOSAddressEx(eLBOSFindMethod_None, NULL);
}


/*/////////////////////////////////////////////////////////////////////////////
//                    STATIC CONVENIENCE FUNCTIONS                           //
/////////////////////////////////////////////////////////////////////////////*/
#if 0 /* deprecated, remove on or after July 9 2016 */
/** Get role of current host. Returned string is read-only, may reside in 
 * 'data' memory area                                                        */
static const char* s_LBOS_ReadRole()
{
    /*
     * If no role previously read, fill it. Of course, not on MSWIN
     */
#ifdef NCBI_OS_LINUX
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
#endif /* #ifdef NCBI_OS_LINUX */
    return s_LBOS_CurrentRole;
}

#endif /* 0 */

#if defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN
/**  Read contents of lbosresolver
 *
 *  @warning 
 *   Do not modify or clear the returned string! Returned string is 
 *   read-only  
 */
static const char* s_LBOS_ReadLbosresolver(void)
{
    if (s_LBOS_Lbosresolver != NULL) {
        return s_LBOS_Lbosresolver;
    }

    FILE* lbosresolver_file;
    size_t len;
    char str[kMaxLineSize];
    char* read_result; /* during function will become equal either NULL
                        or str, do not free() */
    if ((lbosresolver_file = fopen(kLbosresolverFile, "r")) == NULL) {
        CORE_LOGF(eLOG_Warning, ("LBOS mapper: "
                                 "could not open file %s",
                                 kLbosresolverFile));
        return NULL;
    }
    read_result = fgets(str, sizeof(str), lbosresolver_file);
    fclose(lbosresolver_file);
    if (read_result == NULL) {
        CORE_LOG(eLOG_Warning, "s_LBOS_ReadLBOSResolve: "
                               "memory allocation failed");
        return NULL;
    }
    len = strlen(str);
    assert(len);
    if (g_LBOS_StringIsNullOrEmpty(str)) {
        /* No domain recognized */
        CORE_LOGF(eLOG_Warning,
                  ("LBOS mapper: file %s is empty, no LBOS address available",
                   kLbosresolverFile));
        free(read_result);
        return NULL;
    }
    /*We remove unnecessary '/n' and probably '/r'   */
    if (str[len - 1] == '\n') {
        if (--len && str[len - 1] == '\r') {
            --len;
        }
        str[len] = '\0';
    }
    CORE_LOCK_WRITE;
    /* Check one more time that no other thread managed to fill
        * static variable ahead of this thread. If this happened,
        * release memory */
    if (s_LBOS_Lbosresolver == NULL)
        /* We skip "http://" and "/lbos" */
        str[strlen(str) - strlen("/lbos")] = '\0';
        s_LBOS_Lbosresolver = strdup(str + 7);
    CORE_UNLOCK;
    
    return s_LBOS_Lbosresolver;
}
#endif  /* defined NCBI_OS_LINUX || defined NCBI_OS_MSWIN */


/**  Take original string and return URL-encoded string.
 *  @attention
 *   Original string is untouched. Caller is responsible for freeing
 *   allocated space.
 */
static char* s_LBOS_URLEncode (const char* to_encode)
{
    /* If all symbols are escape, our string will take triple space */
    size_t encoded_string_buf_size = strlen(to_encode)*3 + 1;
    char* encoded_string = (char*)calloc(encoded_string_buf_size, 
                                         sizeof(char));
    size_t src_read, dst_written; /* strange things needed by URL_Encode */
    URL_Encode(to_encode, strlen(to_encode), &src_read,
               encoded_string, encoded_string_buf_size, &dst_written);
    return encoded_string;
}


/**  Function to convert legacy service names for LBOS.
 *   Takes service name and if it does not start with '/', prepends "/Legacy"
 *   to it. 
 *  @attention 
 *   Original string is untouched. Caller is responsible for freeing
 *   allocated space.
 *  @warning
 *   to_modify MUST be a valid non-empty C-string 
 */
char* s_LBOS_ModifyServiceName (const char* to_modify)
{
    /* If all symbols are escape, our string will take triple space */
    static const char* prefix = "/Legacy/";
    if (to_modify[0] == '/')
        return strdup(to_modify);
    /* We deal with legacy service name. First, get "/Legacy/" prefix, then
     * change to_modify to lower register, concatenate them and return */
    char* modified_str = strdup(prefix);
    char* service_lwr = strlwr(strdup(to_modify));
    modified_str = g_LBOS_StringConcat(modified_str, service_lwr, NULL);
    free(service_lwr);
    return modified_str;
}


/** @brief Just connect and return connection
 *
 *  Internal function to create connection, which simplifies process
 *  by ignoring many settings and making them default
 */
static CONN s_LBOS_ConnectURL(SConnNetInfo* net_info, const char* url,
                              SLBOS_UserData* user_data)
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
    if (!(connector = HTTP_CreateConnectorEx(net_info, flags,
                      g_LBOS_UnitTesting_GetLBOSFuncs()->ParseHeader,
                      user_data/*data*/, 0, 
                      0/*cleanup*/))) 
    {
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
    CONN_SetTimeout(conn, eIO_Open,      &kLBOSTimeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, &kLBOSTimeout);
    return conn;
}


/** If you are sure that all answer will fit in one char*, use this function
 * to read all input. Must be free()'d by the caller.
 */
static char * s_LBOS_UrlReadAll(SConnNetInfo*   net_info, 
                                const char*     url, 
                                int*            status_code,
                                char**          status_message)
{
    SLBOS_UserData user_data; /* used to store HTTP response code and 
                                 HTTP header, HTTP header only for unit 
                                 testing */
    /* Status of reading*/
    EIO_Status    status = eIO_Timeout;
    /* how much how been read to the moment*/
    size_t        totalRead        = 0;
    /* how much we suppose one line takes*/
    size_t        oneLineSize      = kMaxLineSize;
    /* how much bytes was read in one turn*/
    size_t        bytesRead        = 0;    
    /* Connection to LBOS. We do not know if LBOS really exists at url */
    CONN          conn;
    char*         buf;
    size_t        totalBufSize;
    char*         realloc_result;

    /* Not to handle case when 'status_code' is NULL, we use internal variable,
     * and only try to set 'status_code' in the end of this function (and if it 
     * is NULL, we do not set it) */
    user_data.http_response_code = 0;
    /* The same for status_message */
    user_data.http_status_mesage = NULL;
    net_info->max_try = 1; /* we do not need to try more than once */
    /* Set HTTP header for unit testing. Pointer to already existing string. */
    user_data.header = net_info->http_user_header;
    /* Set content length to the maximum possible value */
    user_data.content_length = (size_t)-1; 
    conn = s_LBOS_ConnectURL(net_info, url, &user_data);
    if (conn == NULL) {
        return NULL;
    }
    CONN_SetUserData(conn, &user_data);
    buf              = (char*)calloc(oneLineSize, sizeof(char));
    /* Total length of buffer. We already have oneLineSize because of calloc */
    totalBufSize     = oneLineSize;    
    if (buf == NULL)
    {
        CORE_LOG(eLOG_Critical, "s_LBOS_UrlReadAll: No RAM. "
                                "Returning NULL.");
        CONN_Close(conn);
        return NULL;
    }
    do {
        /* If there is no LBOS, we will know about it here */
        status = g_LBOS_UnitTesting_GetLBOSFuncs()->
                          Read(conn, buf + strlen(buf),
                               totalBufSize - totalRead - 1 /* for \0 */,
                               &bytesRead, eIO_ReadPlain);
        if (user_data.http_response_code != 200) {
            CORE_LOGF(eLOG_Critical, ("s_LBOS_UrlReadAll: LBOS returned "
                                      "status code %d", 
                                      user_data.http_response_code));
        }
        if (status_code != NULL) {
            *status_code = user_data.http_response_code;
        }
        if (status_message != NULL && user_data.http_status_mesage != NULL) {
            *status_message = strdup(user_data.http_status_mesage);
        }
        free(user_data.http_status_mesage); /* not needed anymore*/
        user_data.http_status_mesage = NULL;
        /* If could not connect, fail now */
        if (user_data.http_response_code == 0) {
            free(buf);
            CONN_Close(conn);
            return NULL;
        }
        totalRead += bytesRead;

        buf[totalRead] = 0; /* force end of string */

        /* IF we still have to read - then add space to buffer, if needed  */
        if ( status == eIO_Success && totalBufSize < totalRead * 2 )
        {
            realloc_result = (char*)realloc(buf, 
                                            sizeof(char) * (totalBufSize * 2));
            if (realloc_result == NULL) {
                CORE_LOG(eLOG_Warning, "s_LBOS_UrlReadAll: Buffer "
                                        "overflow. Returning string at its "
                                        "maximum size");
                return buf;
            } else {
                buf = realloc_result;
                totalBufSize *= 2;
            }
        }
    } while (status == eIO_Success);
    /*In the end we shrink buffer to the minimal needed size*/
    if ( !(realloc_result = (char*) realloc(buf,
                                           sizeof(char) * (strlen(buf) + 1))) ) 
    {
        CORE_LOG(eLOG_Warning, "s_LBOS_UrlReadAll: Buffer shrink error, using "
                               "original stirng");
    }  else  {
        buf = realloc_result;
    }
    CONN_Close(conn);
    return buf;
}


/**   Resolve service name to one of the hosts implementing service.
 *   Uses LBZK at specified IP and port.
 */
static SSERV_Info** s_LBOS_ResolveIPPort(const char* lbos_address,
                                         const char* service_name,
                                         SConnNetInfo* net_info)
{ 
    SSERV_Info** infos;
    size_t infos_count;
    size_t infos_capacity;
    char* servicename_url_encoded = NULL;
    size_t url_length;
    char * url;
    char * lbos_answer; /*to write down LBOS's answer*/
    const char* user_dtab = NULL; 
    size_t length;
    size_t user_dtab_length;
    char* new_dtab = NULL;
    char* user_dtab_end;
    /* Allocate space for answer (will be expanded later, if needed) */
    infos = (SSERV_Info**)calloc(2, sizeof(SSERV_Info*));
    if (infos == NULL) {
        CORE_LOG(eLOG_Critical, "s_LBOS_ResolveIPPort: No RAM. "
                                "Returning NULL.");
        return NULL;
    }
    infos_count = 0;
    infos_capacity = 1;
    /* Update HTTP Header with local DTabs from registry and revert after we 
     * finished reading from URL */
    /* First, we look if there is DTab-Local already in header*/
    char* old_header = net_info->http_user_header ?
            strdup(net_info->http_user_header) : NULL;
    user_dtab = g_LBOS_strcasestr(net_info->http_user_header, "DTab-local:");
    /* If there is an already defined local DTab, we mix it with one from 
     * registry */
    if (user_dtab != NULL) 
    {
        /* Move start after name of tag*/
        user_dtab += strlen("DTab-Local:");
        if (user_dtab[1] == ' ') {
            user_dtab++;
        }
        /* Find end of line */
        user_dtab_end = strchr(user_dtab, '\n');
        if (user_dtab_end[-1] == '\r')  {
            user_dtab_end--;
        }
        /* Create new string that includes first DTabs from registry and then
         * DTabs from HTTP requests */
        length = 0;
        user_dtab_length = user_dtab_end - user_dtab;
        new_dtab = NULL;
        new_dtab = g_LBOS_StringNConcat(g_LBOS_StringConcat(
            g_LBOS_StringConcat(g_LBOS_StringConcat(
            /*dest*/   /*to append*/       /*length*/   /*count*/
            new_dtab,   "DTab-local: ",     &length), 
                        s_LBOS_DTABLocal,   &length),
                        ";",                &length),
                        user_dtab,          &length,    user_dtab_length);
        ConnNetInfo_OverrideUserHeader(net_info, new_dtab);
        free(new_dtab);
    } else {
        /* Set default Dtab from registry */
        size_t length = 0;
        char* new_dtab = NULL;
        new_dtab = g_LBOS_StringConcat(g_LBOS_StringConcat(
            /*dest*/   /*to append*/       /*length*/   /*count*/
            new_dtab,   "DTab-local: ",     &length), 
                        s_LBOS_DTABLocal,   &length);
        ConnNetInfo_OverrideUserHeader(net_info, new_dtab);
        free(new_dtab);
    }
    servicename_url_encoded = s_LBOS_ModifyServiceName(service_name);
   /*encode service name to url encoding (change ' ' to %20, '/' to %2f, etc.)*/
    url_length = strlen("http://")  + strlen(lbos_address) +
                 strlen(kLBOSQuery) + strlen(servicename_url_encoded);
    url = (char*)malloc(sizeof(char) * url_length + 1); /** to make up
                                                   LBOS query URI to connect*/
    if (url == NULL)
    {
        CORE_LOG(eLOG_Critical, "s_LBOS_ResolveIPPort: No RAM. "
                                "Returning NULL.");
        free(infos);
        free(old_header);
        free(servicename_url_encoded);
        return NULL;
    }
    sprintf(url, "%s%s%s%s", "http://", lbos_address, kLBOSQuery,
            servicename_url_encoded);
    lbos_answer = s_LBOS_UrlReadAll(net_info, url, NULL, NULL);
    /* Revert header */
    ConnNetInfo_OverrideUserHeader(net_info, old_header);
    free(old_header);
    free(url);
    free(servicename_url_encoded);
    /* If no connection */
    if (lbos_answer == NULL) {
        free(infos);
        return NULL;
    }
    x_JSON_Value  *root_value;
    x_JSON_Object *root_obj;
    x_JSON_Object *services;
    x_JSON_Array  *serviceEndpoints;
    x_JSON_Object *serviceEndpoint;
    unsigned int j = 0;
    root_value = x_json_parse_string(lbos_answer);
    if (x_json_value_get_type(root_value) != JSONObject) {
        goto clean_and_exit;
    }
    root_obj = x_json_value_get_object(root_value);
    services = x_json_object_get_object(root_obj, "services");
    /* Get endpoints for the first service name. 
     * Note: Multiple service resolution is not supported intentionally 
     * (yet). */
    serviceEndpoints = 
        x_json_object_get_array(services, x_json_object_get_name(services, 0));
    /* Iterate through endpoints */
    for (j = 0;  j < x_json_array_get_count(serviceEndpoints);  j++) {
        const char *host, *rate, *extra, *type;
        char* server_description;
        const char* descr_format = "%s %s:%u %s Regular R=%s L=no T=25";
        int port;
        serviceEndpoint = x_json_array_get_object(serviceEndpoints, j);
        host = x_json_object_dotget_string(serviceEndpoint,
                                         "serviceEndpoint.host");
        if (host == NULL) {
            continue;
        }
        port = (int)x_json_object_dotget_number(serviceEndpoint,
                                              "serviceEndpoint.port");
        /* -------------rate------------- */
        rate = x_json_object_dotget_string(serviceEndpoint,
                                         "meta.rate");
        rate = !g_LBOS_StringIsNullOrEmpty(rate) ? rate : "1";
        /* -------------type------------- */
        type = x_json_object_dotget_string(serviceEndpoint,
                                         "meta.type");
        type = !g_LBOS_StringIsNullOrEmpty(type) ? type : "STANDALONE";
        /* -------------extra------------- */
        extra = x_json_object_dotget_string(serviceEndpoint,
                                         "meta.extra");
        extra = !g_LBOS_StringIsNullOrEmpty(extra) ? extra : "";

        /* Examples: 
           HTTP - accn2gi
           DNS  - alndbasn_lb
           STANDALONE - aligndb_dbldd 
           HTTP_POST - taxservice3test
           NCBID - taxservice/mapviewaugust2011*/
        size_t length; /* used to count size to allocate for server_description,
                          and then for g_LBOS_StringConcat*/
        /* Occasionally, we are not able to allocate memory */
        length = strlen(descr_format) + strlen(type) + strlen(host) + 
                 5 /*length of port*/ + strlen(extra) + strlen(rate);
        server_description = malloc(sizeof(char) * length);
        sprintf(server_description, descr_format, type, host, 
                port, extra, rate);
        SSERV_Info * info = SERV_ReadInfoEx(server_description,service_name, 0);
        free(server_description);
        if (info == NULL) {
            continue;
        }
        if (infos_capacity <= infos_count + 1) {
            SSERV_Info** realloc_result = 
                (SSERV_Info**)realloc(infos, sizeof(SSERV_Info*) * 
                                             (infos_capacity*2 + 1));
            if (realloc_result == NULL) {
                /* If error with realloc, return as much as could 
                 * allocate for */
                infos_count--; /* Will just rewrite last info with NULL 
                                * to mark end of array*/
                free(info);
                break;
            } else { /* If realloc successful */
                infos = realloc_result;
                infos_capacity = infos_capacity*2 + 1;
            }
        }
        infos[infos_count++] = info;
    }

clean_and_exit:
    x_json_value_free(root_value);
    free(lbos_answer);
    /* Shuffle list with Durstenfeld's shuffle algorithm 
     * (also credits go to Fisher and Yates, and Knuth) */
    if (infos_count > 1) {
        size_t i;
        for (i = 0; i < infos_count - 1; i++) {
            size_t j = i + ( rand() % (infos_count - i) );
            if (i == j) continue; /* not swapping the item with itself */
            SSERV_Info* t = infos[j];
            infos[j] = infos[i];
            infos[i] = t;
        }
    }
    /* Set last element this NULL, finalizing the array ...*/
    infos[infos_count] = NULL;

    return infos; 
}


/** Given an empty iterator which has service name in its "name" field
 * we fill it with all servers which were found by asking LBOS
 * @param[in,out] iter
 *  We need name from it, then set "data" field with all servers. All
 *  previous "data" will be overwritten, causing possible memory leak.
 */
static void s_LBOS_FillCandidates(SLBOS_Data* data, const char* service)
{
    unsigned int    i;
    SSERV_Info**    hostports_array = 0;
    char*           lbos_address    = NULL; /* We copy LBOS address to  */

    /* We suppose that number of addresses is constant (and so
       is position of NULL), so no mutex is necessary */
    if (s_LBOS_Instance == NULL) return;
    lbos_address = s_LBOS_Instance;
    CORE_LOGF_X(1, eLOG_Trace, ("Trying to find servers of \"%s\" with "
                "LBOS at %s", service, lbos_address));
    hostports_array = 
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(lbos_address, service,
                                                         data->net_info);
    if (hostports_array == NULL) {
        CORE_LOGF_X(1, eLOG_Trace, ("Ho servers of \"%s\" found by LBOS",
                    service));
        return;
    }
    for (i = 0;  hostports_array[i] != NULL;  i++) continue;
    CORE_LOGF_X(1, eLOG_Trace, ("Found %u servers of \"%s\" with "
                "LBOS at %s", i, service, lbos_address));
    /* If we received answer from LBOS, we fill candidates */
    if (hostports_array != NULL) {
        SLBOS_Candidate* realloc_result;
        /* To allocate space once and forever, let's quickly find the number of
         * received addresses...
         */
        for (i = 0;  hostports_array[i] != NULL;  ++i) continue;
        /* ...and allocate space */
        realloc_result = (SLBOS_Candidate*)realloc(data->cand,
                                           sizeof(SLBOS_Candidate) * (i + 1));
        if (realloc_result == NULL) {
            CORE_LOGF_X(1, eLOG_Warning, 
                        ("s_LBOS_FillCandidates: Could not "
                        "allocate space for all candidates, "
                        "will use as much as was allocated "
                        "initially: %du",
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
    /*If we did not find answer from LBOS, we just finish*/
}



static int s_LBOS_CheckAnnounceArgs(const char* service,
                                    const char* version,
                                    const char* host,
                                    unsigned short port,
                                    const char* healthcheck_url,
                                    char** lbos_answer)
{
    unsigned short i;
    if (g_LBOS_StringIsNullOrEmpty(healthcheck_url)) {
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
    /* Greedy check for host - host should consist of alphanumerics and dots,
     * or be NULL */
    if (!g_LBOS_StringIsNullOrEmpty(host)) {
        for (i = 0;  i < strlen(host);  i++) {
            if (!isalnum(host[i]) && (host[i] != '.')) {
                CORE_LOG(eLOG_Critical, "Error with announcement, " 
                                        "ip has incorrect format "
                                        "(only digits and dots are allowed). "
                                        "Please provide resolved IP to "
                                        "avoid this error");
                return 0;
            }
        }
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
                                "no variable provided to save LBOS answer.");
        return 0;
    }
    return 1;
}


static int s_LBOS_CheckDeannounceArgs(const char*    service,
                                      const char*    version,
                                      const char*    host,
                                      unsigned short port)
{
    if (!g_LBOS_StringIsNullOrEmpty(host) && strstr(host, ":") != NULL) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for de-announcement, "
                                "please check that \"host\" parameter does "
                                "not contain protocol or port");
        return 0;
    }
    if (port < 1 || port > 65535) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for de-announcement, "
                                "incorrect port.");
        return 0;
    }
    if (g_LBOS_StringIsNullOrEmpty(version)) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for de-announcement, "
                                "no version specified.");
        return 0;
    }
    if (g_LBOS_StringIsNullOrEmpty(service)) {
        CORE_LOG(eLOG_Critical, "Invalid argument passed for de-announcement, "
                                "no service name specified.");
        return 0;
    }
    return 1;
}


/** Iterator request through LBOSes and return the best answer (ideally, one
 * that has status code 200)
 * This function is TODO
 */
static unsigned short s_LBOS_PerformRequest(const char* request,
                                            char**      lbos_answer,
                                            char**      http_status_message,
                                            TReqMethod  req_method,
                                            const char* service)
{
    SConnNetInfo*  net_info;
    char*          buf;
    char*          lbos_address;
    int            status_code;
    char*          query;
    char*          status_message  = NULL;
    size_t         length           = 0;
    /* We try all LBOSes until we get 200. If we receive error status code
     * instead of 200, we save it. So, if no LBOS returns 200, we
     * return error code. If we receive nothing (LBOS is not present), then
     * we save nothing.
     */
    net_info             = ConnNetInfo_Clone(s_EmptyNetInfo);
    net_info->req_method = req_method;
    buf                  = NULL;
    status_code          = 0;
    lbos_address         = s_LBOS_Instance;
    query                = g_LBOS_StringConcat(g_LBOS_StringConcat(
                                strdup("http://"), lbos_address, &length),
                                request, &length);
    length               = strlen(query);
    buf                  = s_LBOS_UrlReadAll(net_info, query, &status_code, 
                                             &status_message);
    free(query);
    if (lbos_answer != NULL && !g_LBOS_StringIsNullOrEmpty(buf)) {
        *lbos_answer = strdup(buf);
    }
    free(buf);
    if (http_status_message != NULL && status_message != NULL) {
        *http_status_message = strdup(status_message);
    }
    free(status_message);

    if (status_code == 0) {
        status_code = eLBOS_LbosNotFound;
    }
    /* Cleanup */
    ConnNetInfo_Destroy(net_info);
    return status_code;
}

/**  Find "0.0.0.0" in healthcheck_url and replace it with IP of current host
 *  @attention
 *   Original string is untouched. Caller is responsible for freeing
 *   allocated space.
 */
static char* s_LBOS_Replace0000WithIP(const char* healthcheck_url)
{
    size_t          chars_to_copy;
    const char*     query;
    char            hostname[kMaxLineSize];
    size_t          length;
    /* new url with replaced "0.0.0.0" (if needed) */
    char*           my_healthcheck_url; 
    unsigned int    local_host_ip;
    const char*     replace_pos; /* to check if there is 0.0.0.0 */
    if (healthcheck_url == NULL)
        return NULL;
    /* By 'const' said that we will not touch healthcheck_url, but actually 
     * we need to be able to change it, so we copy it */
    /* If we need to insert local IP instead of 0.0.0.0, we will */
    if ((replace_pos = strstr(healthcheck_url, "0.0.0.0")) == NULL) {
        my_healthcheck_url = strdup(healthcheck_url);
        return(my_healthcheck_url);
    }
    my_healthcheck_url = (char*)calloc(kMaxLineSize, sizeof(char));
    if (my_healthcheck_url == NULL) {
        CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
                               "not enough RAM.");
        return NULL;
    }
    chars_to_copy   = replace_pos - healthcheck_url;
    query           = replace_pos + strlen("0.0.0.0");
    local_host_ip = g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr(eDefault);
    if (local_host_ip == 0) {
        CORE_LOG(eLOG_Warning,
                 "Error with announcement, cannot find local IP.");
        free(my_healthcheck_url);
        return NULL;
    }
    SOCK_HostPortToString(local_host_ip, 0, hostname, kMaxLineSize - 1);
    if (strlen(hostname) == 0) {
        CORE_LOG(eLOG_Warning,
                 "Error with announcement, cannot find local IP.");
        free(my_healthcheck_url);
        return NULL;
    }
    length = strlen(my_healthcheck_url);
    my_healthcheck_url =
        g_LBOS_StringConcat(g_LBOS_StringConcat(g_LBOS_StringNConcat(
        my_healthcheck_url, healthcheck_url,   &length,   chars_to_copy),
                            strlwr(hostname),  &length),
                            query,             &length);

    return my_healthcheck_url;
}


static int s_TurnOn()
{
    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return 0;
    }
    return 1;
}

/*/////////////////////////////////////////////////////////////////////////////
//                             UNIT TESTING                                  //
/////////////////////////////////////////////////////////////////////////////*/
/** Check whether LBOS client is turned ON or OFF
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

/**  Check whether LBOS client has been initialized already
 *  @return
 *   Address of static variable s_LBOS_Init
 *  @see
 *   SERV_LBOS_Open()                                                        */
int* g_LBOS_UnitTesting_InitStatus(void)
{
    return &s_LBOS_Init;
}


/**  List of addresses of LBOS that is maintained in actual state.
 *  @return
 *   address of static variable s_LBOS_InstancesList.
 *  @see
 *   SERV_LBOS_Open(), s_LBOS_FillCandidates()                               */
char** g_LBOS_UnitTesting_Instance(void)
{
    return &s_LBOS_Instance;
}


/**  Pointer to s_LBOS_CurrentRole
 *  @return
 *   address of static variable s_LBOS_CurrentRole.
 *  @see                                                                     */
char** g_LBOS_UnitTesting_Lbosresolver(void)
{
    return &s_LBOS_Lbosresolver;
}


int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod (SERV_ITER       iter,
                                                  ELBOSFindMethod method)
{
    SLBOS_Data* data;
    assert(g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData));
    data = (SLBOS_Data*) iter->data;
    data->find_method = method;
    return 1;
}


int/*bool*/ g_LBOS_UnitTesting_SetLBOSResolverFile(const char* resolverfile)
{
    if (resolverfile != NULL) {
        kLbosresolverFile = resolverfile;
        return 1;
    }
    return 0;
}


/**  Set custom address for LBOS. Can be either hostname:port or IP:port.
 *   Intended mostly for testing.
 *  @param[in] iter
 *   Where to set address for LBOS. Change is made only for this iterator */
int/*bool*/ g_LBOS_UnitTesting_SetLBOSaddress (SERV_ITER iter, char* address) {
    SLBOS_Data* data;
    assert(g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData));
    data = (SLBOS_Data*) iter->data;
    data->lbos_addr = address;
    return 1;
}


struct SLBOS_AnnounceHandle_Tag** g_LBOS_UnitTesting_GetAnnouncedServers(void)
{
    return &s_LBOS_AnnouncedServers;
}


int g_LBOS_UnitTesting_GetAnnouncedServersNum(void)
{
    return s_LBOS_AnnouncedServersNum;
}


int g_LBOS_UnitTesting_FindAnnouncedServer(const char*             service,
                                           const char*             version,
                                           unsigned short          port,
                                           const char*             host) 
{
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
        CORE_LOG_X(1, eLOG_Error, "Could not allocate memory for LBOS mapper");
        return NULL;
    }
    /*
     * We consider that there will never be more than 20 candidates, which
     * is not many for us
     */
    data->a_cand        = candidatesCapacity;
    data->pos_cand      = 0;
    data->n_cand        = 0;
    data->lbos_addr     = NULL;
    data->find_method   = eLBOSFindMethod_None;
    data->cand          = (SLBOS_Candidate*)calloc(candidatesCapacity, 
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
        for (i = data->pos_cand;  i < data->n_cand;  i++) {
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
/**  This function tests existence of the application that should always be
 *  found - LBOS itself. If it is not found, we turn LBOS off.               */
static void s_LBOS_Initialize(void)
{
    const char*     service     = "/lbos";
    SConnNetInfo*   net_info;
    SERV_ITER       iter;
    CORE_LOCK_WRITE;
        if (s_LBOS_Instance == NULL) {
            s_LBOS_Instance = g_LBOS_GetLBOSAddress();
        }
        if (s_EmptyNetInfo == NULL) {
            s_EmptyNetInfo = ConnNetInfo_Create(NULL);
        }
    CORE_UNLOCK;
    s_LBOS_TurnedOn = 1; /* To ensure that initialization does
                            not depend on this variable */
    s_LBOS_Init     = 1;

    /* 
     * Load DTAB Local from registry
     */
    free(s_LBOS_DTABLocal);
    s_LBOS_DTABLocal = g_LBOS_RegGet("CONN", "DTAB", NULL);
    if (g_LBOS_StringIsNullOrEmpty(s_LBOS_DTABLocal)) {
        CORE_LOG_X(1, eLOG_Trace, "No DTAB in registry");
    } else {
        CORE_LOGF_X(1, eLOG_Trace, 
                   ("DTAB from registry: %s ", s_LBOS_DTABLocal));
    }

    /* Check On/Off status */
    char* lbos_toggle = g_LBOS_RegGet("CONN", "LBOS_ENABLE", NULL);
    int lbos_toggled = ConnNetInfo_Boolean(lbos_toggle);
    free(lbos_toggle);
    if (lbos_toggled) {
        CORE_LOG_X(1, eLOG_Note, "LBOS is turned ON in config.");
    } else {
        CORE_LOG_X(1, eLOG_Warning, 
                   "LBOS is NOT turned ON in config! Please provide "
                   "[CONN]LBOS_ENABLE=1");
        s_LBOS_TurnedOn = 0;
        return;
    }
    /*
     * Try to find LBOS
     */
    iter = (SERV_ITER)calloc(1, sizeof(*iter));
    assert(iter != NULL);  /* we can do nothing if this happens */
    iter->name  = service;
    net_info    = ConnNetInfo_Clone(s_EmptyNetInfo);
    iter->op    = SERV_LBOS_Open(iter, net_info, NULL);
    ConnNetInfo_Destroy(net_info);
    if (iter->op == NULL) {
        CORE_LOGF_X(1, eLOG_Warning, 
                    ("Could not connect to LBOS, or "
                     "http://%s/lbos/text/mlresolve?name=%%2flbos "
                     "is empty. Turning LBOS off in this "
                     "process.", s_LBOS_Instance));
        s_LBOS_TurnedOn = 0;
    } else {
        s_LBOS_Close(iter);
        s_LBOS_TurnedOn = 1;
    }
    free(iter);

}


/** After we receive answer from dispd.cgi, we parse header
 * to get all hosts.                                                         */
static EHTTP_HeaderParse s_LBOS_ParseHeader(const char*      header,
                                            void* /* SLBOS_UserData* */ 
                                                             response,
                                            int              server_error)
{
    SLBOS_UserData* response_output;
    int             status_code = 0/*success code if any*/;
    /* For all we know, status message ends before \r\n */
    char*           header_end;
    char*           status_message;
    unsigned int    content_length;
    char*           content_length_pos;

    if (response == NULL) {
        /* We do not intervent in the process by default */
        return eHTTP_HeaderSuccess;
    }
    response_output = (SLBOS_UserData*)response;
    header_end      = strstr(header, "\r\n");
    status_message  = (char*)calloc(header_end-header, sizeof(char));

    if (sscanf(header, "%*s %d %[^\r]\r\n", &status_code,
               status_message) < 1)
    {
        if (response != NULL) {
            response_output->http_response_code = 503; /* server error */
        }
        free(status_message);
        return eHTTP_HeaderError;
    }
    if (status_code != 200) {
        CORE_LOGF(eLOG_Critical, ("s_LBOS_UrlReadAll: LBOS returned status "
                  "code %d", status_code));
    }
    char* temp_header = strlwr(strdup(header)); /* we cannot modify
                                                 * original */
    /* We want to be sure that found "content-length" is the tag itself,
     * not a value as if in "Some-Tag: content-length: 2". So we check for
     * either \n before tag, or it should be positioned at the very
     * beginning of the string
     */
    content_length_pos = strstr(temp_header, "content-length: ");
    if (
            content_length_pos != NULL
            &&
            (
                (*(content_length_pos - 1) == '\n')
                ||
                (content_length_pos == temp_header)
            )
        )
    {
        sscanf(content_length_pos + strlen("content-length: "),
               "%u", &content_length);
    }
    /* If we could not read "content-length", we do not have any
     * estimation of the upper bound  */
    else {
        content_length = (unsigned int)(-1);
    }
    free(temp_header);
    /* check for empty document */
    response_output->http_response_code = status_code;
    response_output->http_status_mesage = status_message;
    response_output->content_length     = content_length;
    return eHTTP_HeaderSuccess;
}


static void s_LBOS_Reset(SERV_ITER iter)
{
    size_t i;
    SLBOS_Data* data;
    /*
     * First, we need to check if it is our iterator. It would be bad
     * to mess with other mapper's iterator
     */
    assert(g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_NoCheck));
    /*
     * Check passed, it is LBOS's iterator. Now we can be sure that we know
     * how to reset it
     */
    data = (SLBOS_Data*) iter->data;
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
                CORE_LOG(eLOG_Critical, "s_LBOS_Reset: No RAM. "
                                        "Failed to create iterator.");
                data->a_cand   = 0;
                data->n_cand   = 0;
                data->pos_cand = 0;
            }
#if defined(_DEBUG)  &&  !defined(NDEBUG)
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


/** Not implemented in LBOS client. */
static int/*bool*/ s_LBOS_Feedback (SERV_ITER a, double b, int c)
{
    return 0;
}


/** Not implemented in LBOS client. */
static int/*bool*/ s_LBOS_Update(SERV_ITER iter, const char* text, int code)
{
    return 1;
}


static void s_LBOS_Close (SERV_ITER iter)
{
    SLBOS_Data* data;
    /*
     * First, we need to check if it is our iterator. It would be bad
     * to mess with other mapper's iterator
     */
    assert(g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData));
    data = (SLBOS_Data*) iter->data;
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
    SLBOS_Data* data;
    /*
     * We store snapshot of LBOS answer in memory. So we just go from last
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

    assert(iter != NULL);  /* we can do nothing if this happens */

    if (iter->data == NULL) {
        iter->data = s_LBOS_ConstructData(kInitialCandidatesCount);
    }

    data = (SLBOS_Data*)(iter->data); /** set typed variable
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
        return data->cand[data->pos_cand-1].info;
    }  else  {
        return NULL;
    }
}


/** Creates iterator and fills it with found servers.
 * @param[in,out] iter
 *  Pointer to iterator. It is read and rewritten
 * @param[out] net_info
 *  Connection information.
 * @param[out] info
 *  Pointer to variable to return pointer to info about server. */
const SSERV_VTable* SERV_LBOS_Open( SERV_ITER            iter,
                                    const SConnNetInfo*  net_info,
                                    SSERV_Info**         info     )
{
    SLBOS_Data* data;
    char* new_name = NULL; /* if we need to add dbaf */
    const char* orig_serv_name = iter->name; /* we may modify name with dbaf */
    if (!s_TurnOn()) {
        return NULL;
    }
    /*
     * First, we need to check arguments
     */
    assert(iter != NULL);  /* we can do nothing if this happens */

    /* Check that iter is not a mask - LBOS cannot work with masks */
    if (iter->ismask) {
        CORE_LOG(eLOG_Warning, "Mask was provided instead of service name. "
            "Masks are not supported in LBOS.");
        return NULL;
    }

    /* Check that service name is provided - otherwise there is nothing to 
     * search for */
    if (iter->name == NULL) {
        CORE_LOG(eLOG_Warning, "\"iter->name\" is null, not able "
                               "to continue SERV_LBOS_Open");
        return NULL;
    }

    /* If dbaf is defined, we construct new service name and assign it 
     * to iter */
    if ( iter->arg  &&  (strcmp(iter->arg, "dbaf") == 0)  &&  iter->val ) {
        size_t length = 0;
        new_name = 
            g_LBOS_StringConcat(g_LBOS_StringConcat(g_LBOS_StringConcat(
                                NULL, iter->name, &length),
                                      "/",        &length),
                                      iter->val,  &length);
        if (new_name == NULL) {
            CORE_LOG(eLOG_Warning, "Could not concatenate dbaf with service "
                                   "name, probably not enough RAM. Searching "
                                   "for service without dbaf");
        } else {
            iter->name = new_name;
        }
    }
    /*
     * Arguments OK, start work
     */
    if (info != NULL) {
        *info = NULL;
    }
    data = s_LBOS_ConstructData(kInitialCandidatesCount);
    if(net_info == NULL) {
        CORE_LOG(eLOG_Warning,
                 "Parameter \"net_info\" is null, creating net info. "
                 "Please, fix the code and provide net_info.");
        data->net_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    } else {
        data->net_info = ConnNetInfo_Clone(net_info);
    }
    // Check if CONNECT_Init() has been run before
    if (g_CORE_GetRequestDtab == NULL) {
        CORE_LOG(eLOG_Critical, 
                 "LBOS FAIL! Please run CONNECT_Init() prior to using LBOS!\n"
                 "Example:\n"
                 "CNcbiRegistry& config = CNcbiApplication::Instance()"
                 "->GetConfig();\n"
                 "CONNECT_Init(&config);\n"
                 "LBOS::Announce(...);");
        s_LBOS_DestroyData(data);
        if (iter->name != orig_serv_name) {
            free(new_name);
            iter->name = orig_serv_name;
        }
        return NULL;
    }
    const char* request_dtab = g_CORE_GetRequestDtab();
    if (!g_LBOS_StringIsNullOrEmpty(request_dtab)) {
        /* Add a semicolon to separate DTabs */
        ConnNetInfo_ExtendUserHeader(data->net_info, "DTab-Local: ;");
        ConnNetInfo_ExtendUserHeader(data->net_info, request_dtab);
    }
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates(data, iter->name);
    /* Connect to LBOS, read what is needed, build iter, info, host_info
     */
    if (!data->n_cand) {
        s_LBOS_DestroyData(data);
        if (iter->name != orig_serv_name) {
            free(new_name);
            iter->name = orig_serv_name;
        }
        return NULL;
    }
    /* Something was found, now we can use iter */

    /*Just explicitly mention here to do something with it*/
    iter->data = data;
    if (iter->name != orig_serv_name) {
        free(new_name);
        iter->name = orig_serv_name;
    }
    return &s_lbos_op;
}


/*/////////////////////////////////////////////////////////////////////////////
//                        ANNOUNCEMENT/DEANNOUCEMENT                         //
/////////////////////////////////////////////////////////////////////////////*/
/** For the unit testing this function is moved from LBOS_Announce
 *  (interception of
 * parameters). For full description see LBOS_Announce
 * @sa 
 *  LBOS_Announce()                                                        */
static 
unsigned short s_LBOS_Announce(const char*             service,
                               const char*             version,
                               const char*             host,
                               unsigned short          port,
                               const char*             healthcheck_url,
#ifdef LBOS_METADATA
                               const char*             meta_args,
#endif /* LBOS_METADATA */
                               /* lbos_answer is never NULL  */
                               char**                  lbos_answer,
                               char**                  http_status_message)
    {
    char*           lbos_address;
    char*           lbos_addr; /* to store address of LBOS that 
                                  did the announcement*/
    int             status_code;
    char*           status_message;
    SConnNetInfo*   net_info;
    const char*     query_format        = NULL;
    char*           buf                 = NULL; /* for answer from LBOS */
    int             parsed_symbols = 0;
    assert(!g_LBOS_StringIsNullOrEmpty(host));

    if (!s_TurnOn())
        return eLBOS_Disabled;

    lbos_address         = s_LBOS_Instance;
    status_code          = 0;
    status_message       = NULL;
    net_info             = ConnNetInfo_Clone(s_EmptyNetInfo);
    net_info->req_method = eReqMethod_Put;
    /* Format for announcement request. "ip" parameter is optional and  
     * will be added separately, if provided */
    query_format         = "http://%s/lbos/v3/services%s?version=%s&"
                                                        "port=%hu&"
                                                        "check=%s&"
                                                        "ip=%s&"
                                                        "format=json";
    /*
     * Let's try announce 
     */
    char* query;
    /* We do not count extra 1 byte for \0 because we still have extra 
     * bytes because of %s placeholders in query_format */
    query = (char*)calloc(strlen(query_format) + 
                          strlen(lbos_address) + 
                          strlen(service) + strlen(version) +
                          5/* port */ + strlen(healthcheck_url) +
                          strlen(host),
                          sizeof(char));
    sprintf(query, query_format,
            lbos_address, service, version, port, healthcheck_url, host);
    if (!g_LBOS_StringIsNullOrEmpty(meta_args)) {
        query = g_LBOS_StringConcat(g_LBOS_StringConcat(
            query, "&",       NULL), 
                   meta_args, NULL);
    }
    buf = s_LBOS_UrlReadAll(net_info, query, &status_code, &status_message);
    free(query);
    if (!g_LBOS_StringIsNullOrEmpty(buf)) {
        /* If this function is not able to parse LBOS output, original LBOS
         * response will be available to the caller. Otherwise, content of 
         * lbos_answer will be replaced with parsed IP address of LBOS watcher 
         * a bit later */
        *lbos_answer = strdup(buf);
    }
    if (http_status_message != NULL && status_message != NULL) {
        *http_status_message = strdup(status_message);
    }
    free(status_message);
    switch (status_code) {
    case 0:
        /* If no LBOS found */
        CORE_LOG(eLOG_Warning, "Announce failed. No LBOS found.");
        status_code = eLBOS_LbosNotFound;
        break;
    case eLBOS_NotFound: case eLBOS_BadRequest: case eLBOS_Server:
        /* If announced server has a broken healthcheck */
        CORE_LOGF(eLOG_Warning, ("Announce failed. "
                                 "LBOS returned error code %d.", status_code));
        break;
    case eLBOS_Success:
        /* If we announced successfully and status_code is 200,
         * let's extract LBOS address */
        lbos_addr = (char*)calloc(kMaxLineSize, sizeof(char)); /* will not be 
                                                                * free()'d */
        if (lbos_addr == NULL) {
            CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
                                   "not enough RAM.");
            status_code = eLBOS_MemAlloc; 
            break;
        }
        if (buf != NULL) {
            parsed_symbols = sscanf(buf, "{\"watcher\":\"%[^\"]\"}", 
                                    lbos_addr);
        }
        if (parsed_symbols != 1) {
            CORE_LOG(eLOG_Warning, "g_LBOS_Announce: LBOS answered 200 OK, but "
                                   "output could not be parsed");
            free(lbos_addr);
            status_code = eLBOS_Protocol; 
            break;
        } 
        /* If announce finished with success, we parsed it to extract LBOS 
         * ip:port. We free() original output and replace it with ip:port */
        free(*lbos_answer);
        *lbos_answer = lbos_addr;
        /* If we could not announce, it is really bad */
        break;
    default:
        CORE_LOGF(eLOG_Warning, ("Announce failed. "
                                 "LBOS returned error code %d. "
                                 "LBOS answer: %s.", status_code, buf));
    }
    /* Cleanup */
    free(buf);
    ConnNetInfo_Destroy(net_info);
    return status_code;
}


unsigned short LBOS_Announce(const char*    service,
                             const char*    version,
                             const char*    host,
                             unsigned short port,
                             const char*    healthcheck_url,
#ifdef LBOS_METADATA
                             const char*    meta_args,
#endif /* LBOS_METADATA */
                             char**         lbos_answer,
                             char**         http_status_message)
{
    char*           my_healthcheck_url      = NULL;
    char*           healthcheck_encoded     = NULL;
    char*           my_host                 = NULL;
    char*           service_encoded         = NULL;
    char*           version_encoded         = NULL;
    unsigned short  result;

    /*
     * First we check input arguments
     */
    if (s_LBOS_CheckAnnounceArgs(service, version, host, port, healthcheck_url,
                                 lbos_answer) == 0)
    {
        return eLBOS_InvalidArgs;
    }
    /*
     * Pre-assign variables
     */
    *lbos_answer = NULL;
    /* Check if we need to replace 0.0.0.0 with local IP, and do it if needed*/
    my_healthcheck_url = s_LBOS_Replace0000WithIP(healthcheck_url);

	/*my_healthcheck_url = strdup(healthcheck_url); */
    if (my_healthcheck_url == NULL) {
        result = eLBOS_DNSResolve;
        goto clean_and_exit;
    }
    /* If host provided separately from healthcheck URL, check if we need to
     * replace 0.0.0.0 with local IP, and do it if needed                    */
    if (!g_LBOS_StringIsNullOrEmpty(host)) {
        my_host = s_LBOS_Replace0000WithIP(host);
    }
    else { /* If host was NOT provided, we append local IP to query,
           * just in case */
        SConnNetInfo * healthcheck_info;
        healthcheck_info = ConnNetInfo_Clone(s_EmptyNetInfo);
        healthcheck_info->host[0] = '\0'; /* to be sure that it will be
                                          * overridden                      */
        /* Save info about host */
        ConnNetInfo_ParseURL(healthcheck_info, my_healthcheck_url);
        my_host = strdup(healthcheck_info->host);
        /* If we could not parse healthcheck URL, throw "Invalid Arguments" */
        if (g_LBOS_StringIsNullOrEmpty(my_host)) {
            ConnNetInfo_Destroy(healthcheck_info);
            CORE_LOG_X(eLBOS_InvalidArgs, eLOG_Critical, 
                       "Could not parse host from healthcheck URL. Please set "
                       "ip of the announced server explicitly.");
            result = eLBOS_InvalidArgs;
            goto clean_and_exit;
        }
        ConnNetInfo_Destroy(healthcheck_info);
    }

    healthcheck_encoded  = s_LBOS_URLEncode(my_healthcheck_url);
    service_encoded      = s_LBOS_ModifyServiceName(service);
    version_encoded      = s_LBOS_URLEncode(version);

    /* Announce */
    result = 
            g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx(service_encoded, 
                                                          version_encoded, 
                                                          my_host,
                                                          port,
                                                          healthcheck_encoded, 
                                                          meta_args,
                                                          lbos_answer,
                                                          http_status_message);     
    if (result == eLBOS_Success) {
        CORE_LOCK_WRITE;
        s_LBOS_AddAnnouncedServer(service, version, port, healthcheck_url);
        CORE_UNLOCK;
    }

    /* Cleanup */
clean_and_exit:
    free(healthcheck_encoded);
    free(my_healthcheck_url);
    free(my_host);
    free(version_encoded);
    free(service_encoded);
    return result;
}


unsigned short LBOS_AnnounceFromRegistry(const char*  registry_section,
                                         char**       lbos_answer,
                                         char**       http_status_message)
{
    unsigned short  result      = eLBOS_Success;
    size_t          i           = 0;
    unsigned int    port;
    char*           srvc;
    char*           vers;
    char*           port_str;
    char*           hlth;
    char*           host;
    char*           meta;

    if (g_LBOS_StringIsNullOrEmpty(registry_section)) {
        registry_section = kLBOSAnnouncementSection;
    }
    srvc      = g_LBOS_RegGet(registry_section, kLBOSServiceVariable, NULL);
    vers      = g_LBOS_RegGet(registry_section, kLBOSVersionVariable, NULL);
    port_str  = g_LBOS_RegGet(registry_section, kLBOSPortVariable, NULL);
    host      = g_LBOS_RegGet(registry_section, kLBOSServerHostVariable, NULL);
    hlth      = g_LBOS_RegGet(registry_section, kLBOSHealthcheckUrlVariable, 
                              NULL);
    meta      = g_LBOS_RegGet(registry_section, kLBOSMetaVariable, 
                              NULL);
    
    /* Check port that it is a number of max 5 digits and no other symbols   */
    for (i = 0;  i < strlen(port_str);  i++) {
        if (!isdigit(port_str[i])) {
            CORE_LOGF_X(eLBOS_InvalidArgs, eLOG_Warning, 
                        ("Port \"%s\" in section %s is invalid", port_str, 
                        registry_section));
            result = eLBOS_InvalidArgs;
            goto clean_and_exit;
        }
    }
    if (strlen(port_str) > 5 || (sscanf(port_str, "%d", &port) != 1) ||
        port < 1 || port > 65535) 
    {
        result = eLBOS_InvalidArgs;
        goto clean_and_exit;
    }    

    /* Announce */    
    result = LBOS_Announce(srvc, vers, host, (unsigned short)port, hlth, meta,
                           lbos_answer, http_status_message);    
    if (result == eLBOS_Success) {
        CORE_LOCK_WRITE;
        s_LBOS_AddAnnouncedServer(srvc, vers, port, hlth);
        CORE_UNLOCK;
    }

    /* Cleanup */
clean_and_exit:
    free(srvc);
    free(vers);
    free(port_str);
    free(hlth);
    free(host);
    free(meta);
    return result;
}

/* Separated from LBOS_Deannounce to easier control memory allocated for 
 * variables */
unsigned short s_LBOS_Deannounce(const char*      service,
                               const char*        version,
                               const char*        host,
                               unsigned short     port,
                               char**             lbos_answer,
                               char**             http_status_message,
                               SConnNetInfo*      net_info)
{
    const char*    query_format;
    char*          lbos_address;
    char*          status_message = NULL;
    char*          buf;
    int            status_code;
    lbos_address = s_LBOS_Instance;
    status_code = 0;
    buf = NULL;
    /* ip */
    query_format = "http://%s/lbos/v3/services%s?version=%s&"
                                                 "port=%hu&"
                                                 "ip=%s";
    /*
    * Try deannounce
    */
    char* query;
    assert(!g_LBOS_StringIsNullOrEmpty(host));
    /* We do not count extra 1 byte for \0 because we still have extra 
        * bytes because of %s placeholders */
    query = (char*)calloc(strlen(query_format) +
                          strlen(lbos_address) + strlen(service) +
                          strlen(version) + 5/*port*/ + strlen(host),
                          sizeof(char));
    sprintf(query, query_format, 
            lbos_address, service, version, port, host);
    buf = s_LBOS_UrlReadAll(net_info, query, &status_code, 
                            &status_message);
    free(query);
    if (lbos_answer != NULL && !g_LBOS_StringIsNullOrEmpty(buf)) {
        *lbos_answer = strdup(buf);
    }
    free(buf);
    if (http_status_message != NULL && status_message != NULL) {
        *http_status_message = strdup(status_message);
    }
    free(status_message);

    if (status_code == 0) {
        status_code = eLBOS_LbosNotFound;
    }
    return status_code;
}


unsigned short LBOS_Deannounce(const char*        service,
                               const char*        version,
                               const char*        host,
                               unsigned short     port,
                               char**             lbos_answer,
                               char**             http_status_message)
{ 
    SConnNetInfo*   net_info;
    char*           service_encoded;
    char*           version_encoded;
    char*           my_host;
    unsigned short  retval;
    /*
     * First we check input arguments
     */
    if (s_LBOS_CheckDeannounceArgs(service, version, host, port) == 0) {
        return eLBOS_InvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (!s_TurnOn()) {
        return eLBOS_Disabled;
    }
    /*
     * If we are here, arguments are good!
     */
    if (!g_LBOS_StringIsNullOrEmpty(host)) {
        my_host = s_LBOS_Replace0000WithIP(host);
    }
    else { /* If host was NOT provided, we append local IP to query,
           * just in case */
        my_host = s_LBOS_Replace0000WithIP("0.0.0.0");
        if (g_LBOS_StringIsNullOrEmpty(my_host)){
            CORE_LOG_X(eLBOS_DNSResolve, eLOG_Critical,
                       "Did not manage to get local IP address.");
            free(my_host);
            return eLBOS_DNSResolve;
        }
    }
    net_info             = ConnNetInfo_Clone(s_EmptyNetInfo);
    net_info->req_method = eReqMethod_Delete;
    service_encoded      = s_LBOS_ModifyServiceName(service);
    version_encoded      = s_LBOS_URLEncode(version);
    
    retval = s_LBOS_Deannounce(service_encoded, version_encoded, 
                               my_host, port, lbos_answer, 
                               http_status_message, net_info);

    /* If eLBOS_NotFound or eLBOS_Success - we delete server from local storage
    * as no longer existing */
    if (retval == eLBOS_NotFound || retval == eLBOS_Success) {
        CORE_LOCK_WRITE;
        s_LBOS_RemoveAnnouncedServer(service, version, port, host);
        CORE_UNLOCK;
    }

    /*
     * Cleanup
     */
    free(version_encoded);
    free(service_encoded);
    free(my_host);
    ConnNetInfo_Destroy(net_info);

    return retval;
}


/** Deannounce all announced servers.
 * @note    
 *  Though this function is mt-safe, you should fully recognize the fact 
 *  results of simultaneous multiple de-announcement of the same service can be 
 *  unpredictable. For example, if service has just been de-announced in 
 *  different thread, this thread will return error "service is not announced".

 * @return 
 *  1 - all services de-announced successfully 
 *  0 - at least one server could not be de-announced */
void LBOS_DeannounceAll()
{
    struct SLBOS_AnnounceHandle_Tag**   arr;
    struct SLBOS_AnnounceHandle_Tag*    local_arr;
    unsigned int                        servers;
    unsigned int                        i;

    CORE_LOCK_READ;
    arr       = &s_LBOS_AnnouncedServers;
    servers   = s_LBOS_AnnouncedServersNum;
    local_arr = 
              (struct SLBOS_AnnounceHandle_Tag*)calloc(servers, sizeof(**arr)); 
    if (local_arr == NULL) {
        CORE_LOG_X(eLBOS_MemAlloc, eLOG_Warning, 
                   "RAM error. Cancelling deannounce all.");
        CORE_UNLOCK;
        return;
    }
    /* 
     * Copy servers list to local variable in case other thread 
     * deletes it or wants to announce new server which we would not want to 
     * deannounce, since it will be announced after call of this function.
     */
    for (i = 0;  i < servers;  i++) {
        local_arr[i].version        = strdup((*arr)[i].version);
        local_arr[i].service        = strdup((*arr)[i].service);
        local_arr[i].port           =        (*arr)[i].port;
        if (strcmp((*arr)[i].host, "0.0.0.0") == 0) {
        /* If host is "0.0.0.0", we do not provide it to LBOS in
         * HTTP request (because it returns error in this case).
         * With no host provided, LBOS understands that we want
         * to deannounce server from local host, which is the same
         * as 0.0.0.0
         */
            local_arr[i].host       = NULL;
        } else {
            local_arr[i].host       = strdup((*arr)[i].host);
        }
    }
    CORE_UNLOCK;
    for (i = 0;  i < servers;  i++) {
        /* Deannounce */
        LBOS_Deannounce(local_arr[i].service,
                        local_arr[i].version,
                        local_arr[i].host,
                        local_arr[i].port,
                        NULL,
                        NULL);
        /* Cleanup */
        free(local_arr[i].version);
        free(local_arr[i].host);
        free(local_arr[i].service);
    }
    free(local_arr);
    return;
}


/*/////////////////////////////////////////////////////////////////////////////
//                             LBOS CONFIGURATION                            //
/////////////////////////////////////////////////////////////////////////////*/
static int s_LBOS_CheckConfArgs(const char* service, const char** lbos_answer)
{
    unsigned int i;
    if (g_LBOS_StringIsNullOrEmpty(service)  ||  lbos_answer == NULL) {
        CORE_LOG_X(eLBOS_InvalidArgs, eLOG_Warning, 
                    "s_LBOS_CheckConfArgs: service is NULL or lbos_answer "
                    "is NULL");
        return 0;
    }
    for (i = 0;  i < strlen(service);  i++) {
        if (isspace(service[i])) {
            CORE_LOGF_X(eLBOS_InvalidArgs, eLOG_Warning, 
                        ("s_LBOS_CheckConfArgs: service "
                        "\"%s\" contains invalid character", service));
            return 0;
        }
    }
    return 1;
}


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
unsigned short LBOS_ServiceVersionGet(const char*  service,
                                      char**       lbos_answer,
                                      char**       http_status_message)
{
    char*          service_encoded;
    const char*    query_format;
    char*          query;
    unsigned short return_code;
    /*
     * First we check input arguments
     */
    if (!s_LBOS_CheckConfArgs(service, (const char**)lbos_answer)) {
        return eLBOS_InvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (!s_TurnOn()) {
        return eLBOS_Disabled;
    }
    
    /*
     * Arguments are good! Let's do the request
     */
    service_encoded = s_LBOS_ModifyServiceName(service);
    query_format = "/lbos/v3/conf%s?format=xml";
    query = (char*)calloc(strlen(query_format) +
                          strlen(service_encoded),
                          sizeof(char));
    sprintf(query, query_format, service_encoded);
    return_code = s_LBOS_PerformRequest(query,
                                        lbos_answer,
                                        http_status_message,
                                        eReqMethod_Get,
                                        service);
    /*
     * Cleanup
     */
    free(query);
    free(service_encoded);
    return return_code;
}


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
unsigned short LBOS_ServiceVersionSet(const char*  service,
                                      const char*  new_version,
                                      char**       lbos_answer,
                                      char**       http_status_message)
{
    char*          service_encoded;
    const char*    query_format;
    char*          query;
    unsigned short return_code;
    /*
     * First we check input arguments
     */
    if (!s_LBOS_CheckConfArgs(service, (const char**)lbos_answer)) {
        return eLBOS_InvalidArgs;
    }
    if (g_LBOS_StringIsNullOrEmpty(new_version)) {
        CORE_LOG_X(eLBOS_InvalidArgs, eLOG_Warning, 
                   "LBOS_ServiceVersionSet: new_version is empty. "
                   "If you want to delete service config, use "
                   "LBOS_ServiceVersionDelete");
        return eLBOS_InvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (!s_TurnOn()) {
        return eLBOS_Disabled;
    }
    /*
     * Arguments are good! Let's do the request
     */
    service_encoded = s_LBOS_ModifyServiceName(service);
    query_format = "/lbos/v3/conf%s?version=%s&format=xml";
    query = (char*)calloc(strlen(query_format) +
                          strlen(service_encoded) +
                          strlen(new_version),
                          sizeof(char));
    sprintf(query, query_format, service_encoded, new_version);
    return_code = s_LBOS_PerformRequest(query,
                                        lbos_answer,
                                        http_status_message,
                                        eReqMethod_Put,
                                        service);
    /*
     * Cleanup
     */
    free(service_encoded);
    free(query);
    return return_code;
}

/** This service can be used to remove service from configuration. Current
 * version will be empty. Previous version shows deleted version.
 */
unsigned short LBOS_ServiceVersionDelete(const char*  service,
                                         char**       lbos_answer,
                                         char**       http_status_message)
{
    char*          service_encoded;
    const char*    query_format;
    char*          query;
    unsigned short return_code;
    /*
     * First we check input arguments
     */
    if (!s_LBOS_CheckConfArgs(service, (const char**)lbos_answer)) {
        return eLBOS_InvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (!s_TurnOn()) {
        return eLBOS_Disabled;
    }
    /*
     * Arguments are good! Let's do the request
     */
    service_encoded = s_LBOS_ModifyServiceName(service);
    query_format = "/lbos/v3/conf%s?format=xml";
    query = (char*)calloc(strlen(query_format) +
                          strlen(service_encoded),
                          sizeof(char));
    sprintf(query, query_format, service_encoded);
    return_code = s_LBOS_PerformRequest(query,
                                        lbos_answer,
                                        http_status_message,
                                        eReqMethod_Delete,
                                        service);
    /*
     * Cleanup
     */
    free(service_encoded);
    free(query);
    return return_code;
}


#undef NCBI_USE_ERRCODE_X

