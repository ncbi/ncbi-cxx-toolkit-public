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
 *   A service discovery API based on LBOS. LBOS is an adapter for ZooKeeper
 *   and tells hosts which can provide with specific service
 */


#include "ncbi_ansi_ext.h"
#include <connect/ncbi_http_connector.h>
#include "ncbi_lbosp.h"
#include "ncbi_priv.h"
#include <stdlib.h> /* free, realloc, calloc, malloc */
#include <ctype.h> /* isdigit */

#define            kHostportStringLength     (16+1+5)/**<
                                                     strlen("255.255.255.255")+
                                                     strlen(":") +
                                                     strlen("65535")         */
#define            kMaxLineSize              1024  /**< used to build strings*/
                                                         

#define            NCBI_USE_ERRCODE_X        Connect_LBSM /**< Used in
                                                               CORE_LOG*_X  */

/*/////////////////////////////////////////////////////////////////////////////
//                         STATIC VARIABLES                                  //
/////////////////////////////////////////////////////////////////////////////*/

static const size_t kLBOSAddresses           = 7; /**< Max addresses of
                                                       LBOS to expect,
                                                       plus one for NULL.    */
static const char* kRoleFile                  = "/etc/ncbi/role";
static const char* kDomainFile                = "/etc/ncbi/domain";
static const char* kLbosresolverFile          = "/etc/ncbi/lbosresolver";
static const char* kLBOSQuery                 = "/lbos/text/mlresolve?name=";


/*
 * LBOS registry section for announcement
 */
static const char* kLBOSAnnouncementSection    = "LBOS_ANNOUNCEMENT";
static const char* kLBOSServiceVariable        = "SERVICE";
static const char* kLBOSVersionVariable        = "VERSION";
static const char* kLBOSServerHostVariable     = "HOST";
static const char* kLBOSPortVariable           = "PORT";
static const char* kLBOSHealthcheckUrlVariable = "HEALTHCHECK";
static const char* kLBOSDomainVariable         = "lbos_domain";


static SConnNetInfo* s_EmptyNetInfo       =   NULL; /* Do..                  */
static       char* s_LBOS_CurrentDomain   =   NULL; /*  ..not..              */
static       char* s_LBOS_Lbosresolver    =   NULL; /*      ..change..       */
static       char* s_LBOS_CurrentRole     =   NULL; /*          ..after init */
static const int   kInitialCandidatesCount =  1; /**< For initial memory
                                                      allocation             */
static       int   s_LBOS_TurnedOn   =        1; /* If LBOS cannot resolve
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
static const char*    s_LBOS_ReadDomain    (void);
static const char*    s_LBOS_ReadRole      (void);
static const char*    s_LBOS_ReadLbosresolver(void);
static 
EHTTP_HeaderParse      s_LBOS_ParseHeader  (const char*   header,
                                            void* /* SLBOS_UserData* */
                                                          response,
                                            int           server_error);
/*LBOS is intended to answer in 0.5 sec, or is considered dead*/
static const STimeout   kLBOSTimeout       =  {10, 500000};
static char**           s_LBOS_InstancesList =  NULL;/** Not to get 404 errors
                                                        on every resolve, we
                                                        will remember the last
                                                        successful LBOS and
                                                        put it first         */
static char*            s_LBOS_DTABLocal     =  NULL;

static 
unsigned short s_LBOS_Announce(const char*             service,
                               const char*             version,
                               const char*             host,
                               unsigned short          port,
                               const char*             healthcheck_url,
                               char**                  lbos_answer,
                               char**                  http_status_message);


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
                                      const char*             version,
                                      unsigned short          port,
                                      const char*             host)
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
int/*bool*/ g_LBOS_StringIsNullOrEmpty(const char* const str)
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
 *   Address of string to which we will append another string. 
 *   This string will not be accessible in any case, success or error. 
 *   You can safely pass result of function like strdup("my string").
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
 *   1 if success, 0 if error happened and dest was not changed              */
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
        CORE_LOG(eLOG_Critical, "g_LBOS_StringConcat: No RAM. Returning NULL.");
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
#if 0
/* Get "tag: value" pair from HTTP header */
char* s_LBOS_GetHTTPHeader(const char* tag, const char* headers)
{
    char* tag_lwr = strlwr(strdup(tag));
    char* header_lwr = strlwr(strdup(headers));
    char* tag_start = header_lwr, *tag_end;
    while (tag_start != NULL)
    {
        /* We move bit by bit */
        tag_start = strstr(tag_start, tag_lwr);
        /* We want to be sure that found tag is the tag itself,
         * not a value as if in "Some-Tag: tag: 2". So we check for
         * either \n before tag, or it should be positioned at the very
         * beginning of the string
         */
        if  (
                tag_start != NULL
                &&
                (
                    (*(tag_start - 1) == '\n')
                    ||
                    (tag_start == header_lwr)
                )
            ) 
            {
                /* Look for the end of tag: value */
                while ()
            }
        else {
            tag_start = 

        if (d)
    }
}


char* g_LBOS_CombineDTabs(const char* primary_dtab, const char* secondary_dtab)
{
    char* primary_dtab, *primary_dtab_end, *new_dtab;
    int length, primary_dtab_length;
    primary_dtab += strlen("DTab-Local:");
    if (primary_dtab[1] == ' ') {
        primary_dtab++;
    }
    /* Find end of line */
    primary_dtab_end = strchr(primary_dtab, '\n');
    if (primary_dtab_end[-1] == '\r')  {
        primary_dtab_end--;
    }
    /* Create new string that includes first DTabs from registry and then
    * DTabs from HTTP requests */
    length = 0;
    primary_dtab_length = primary_dtab_end - primary_dtab;
    new_dtab = NULL;
    new_dtab = g_LBOS_StringNConcat(g_LBOS_StringConcat(
               g_LBOS_StringConcat(g_LBOS_StringConcat(
                 /*dest*/   /*to append*/     /*length*/  /*count*/
                 new_dtab,  "DTab-local: ",   &length),
                            secondary_dtab,   &length),
                            ";",              &length),
                            primary_dtab,     &length,    primary_dtab_length);
    return new_dtab;
}
#endif

char*   g_LBOS_StringNConcat(char*       dest,
                             const char* to_append,
                             size_t*     dest_length,
                             size_t      count)
{
    char* buf = (char*)malloc((count + 1) * sizeof(char));
    char* result;

    if (buf == NULL) {
        CORE_LOG(eLOG_Critical, "g_LBOS_StringConcat: No RAM. Returning NULL.");
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
        CORE_LOG(eLOG_Critical, "g_LBOS_RegGet: No RAM. Returning NULL.");
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
            CORE_LOG(eLOG_Warning, "g_LBOS_RegGet: Buffer "
                                    "overflow while reading from "
                                    "registry. Returning string at its "
                                    "maximum size");
            return buf;
        } else {
            buf = realloc_result;
            totalBufSize *= 2;
        }
    }
    return buf;
}


/**  We compose LBOS hostname based on /etc/ncbi/domain and /etc/ncbi/role.  */
char* g_LBOS_ComposeLBOSAddress(void)
{
    char* site = NULL;
    size_t length = 0;
    const char *role   = s_LBOS_ReadRole(), 
               *domain = s_LBOS_ReadDomain();

    if (role == NULL || domain == NULL) {
        return NULL;
    }
    site = g_LBOS_StringConcat(g_LBOS_StringConcat(
                g_LBOS_StringConcat(g_LBOS_StringConcat(
                /* dest */    /* to append */        /* length */
                site,          "lbos.",              &length),
                               role,                 &length),
                               ".",                  &length),
                               domain,               &length);
    if (site == NULL) {
        CORE_LOG(eLOG_Warning, "s_LBOS_ComposeLBOSAddress: "
                                 "memory allocation failed");
        return NULL;
    }
    return site;
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


/** Given address to LBOS instance, check if it resides in the same domain
 * (be-md, st-va, ac-va, or-wa) as current machine                           */
int/*bool*/ g_LBOS_CheckDomain(const char* lbos_address)
{
    return 1; /* we do not check domain */
    /* Though we cannot be 100% sure that sequence of numbers and dots is IP, 
     * it is what we hope for */
    int/*bool*/ is_ip = 1;
    unsigned short int i;
    for (i = 0; i < strlen(lbos_address); i++) {
        if (!isdigit(lbos_address[i]) && lbos_address[i] != '.') {
            is_ip = 0;
        }
    }
    /*  The check now is really simple - it searches for known domains in the
     * provided address. If domain is omitted in address - then we deny such 
     * address, just in case. If address contains 
     * non-standard domain - check will not work. */
    if ( !is_ip
         /* &&
         (
            (strstr(lbos_address, ".be-md.") != NULL) 
             || 
            (strstr(lbos_address, ".st-va.") != NULL)
             ||
            (strstr(lbos_address, ".or-wa.") != NULL)
             ||
            (strstr(lbos_address, ".ac-va.") != NULL)
         ) */
         &&
         (
            !g_LBOS_StringIsNullOrEmpty(s_LBOS_ReadDomain())
         )
         &&
         (
             strcmp(s_LBOS_ReadDomain(), "*") != 0
         )
       )
    {
        /* If we can perform check, we return result of check */
        return (strstr(lbos_address, s_LBOS_ReadDomain()) != NULL);
    }
    /* If we cannot perform check, we allow any domain */
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
 *  @param lbos[in]
 *   If priority_find_method is eLBOSFindMethod_CustomHost, then LBOS is
 *   first looked for at hostname:port specified in this variable.           */
char** g_LBOS_GetLBOSAddressesEx (ELBOSFindMethod priority_find_method,
                                  const char* lbos_addr)
{
    const char* lbosaddress = NULL; /* for const strings */
    char* lbosaddress_temp = NULL;  /* for non-const strings */
    size_t lbosaddresses_count = 0;
    char** addresses = (char**)calloc(kLBOSAddresses, sizeof(char*));
    if (addresses == NULL) {
        CORE_LOG_X(1, eLOG_Warning,
                   "Memory allocation problem while generating "
                   "LBOS addresses!");
        return NULL;
    }
    /* Section for custom-set address*/
    ELBOSFindMethod find_method_order[] = {
               priority_find_method /* eLBOSFindMethod_None, if not specified*/
               , eLBOSFindMethod_Registry
               , eLBOSFindMethod_Lbosresolve
    };
    size_t find_method_iter;
    size_t find_method_count = 
        sizeof(find_method_order) / sizeof(ELBOSFindMethod);
    CORE_LOG_X(1, eLOG_Trace, "Getting LBOS addresses...");
    for (find_method_iter = 0;  
         find_method_iter < find_method_count;
         ++find_method_iter)
    {
        if (lbosaddresses_count > 0)
            break;
        switch (find_method_order[find_method_iter]) {
        case eLBOSFindMethod_None :
            break;
        case eLBOSFindMethod_CustomHost :
            if (g_LBOS_StringIsNullOrEmpty(lbos_addr)) {
                CORE_LOG_X(1, eLOG_Warning, "Use of custom LBOS address was "
                        "asked for, but no custom address was supplied. Using "
                        "default LBOS.");
                break;
            }
            lbosaddress_temp = strdup(lbos_addr);
            if (lbosaddress_temp != NULL) {
                addresses[lbosaddresses_count++] = lbosaddress_temp;
            }
            break;
        case eLBOSFindMethod_127001 :
#if 0
            lbosaddress = (char*)malloc(kHostportStringLength * sizeof(char));
            if (lbosaddress != NULL) {
                sprintf(lbosaddress, "127.0.0.1:8080");
                addresses[lbosaddresses_count++] = lbosaddress;
            }
#endif /* 0 */
            break;
        case eLBOSFindMethod_EtcNcbiDomain :
#if 0
#ifndef NCBI_OS_MSWIN
            lbosaddress = g_LBOS_UnitTesting_GetLBOSFuncs()->
                ComposeLBOSAddress();
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find LBOS using "
                        "/etc/ncbi{role, domain} failed");
            } else {
                char* lbos_alternative = malloc((strlen(lbosaddress) + 8) * 
                    sizeof(char));
                addresses[lbosaddresses_count++] = lbosaddress;
                /* Also we must check alternative port, 8080 */
                if (lbos_alternative == NULL)  {
                    CORE_LOG_X(1, eLOG_Warning,
                               "Memory allocation problem while generating "
                               "LBOS {role, domain} address");
                } else {
                    sprintf(lbos_alternative, "%s:%hu", lbosaddress,
                            kDefaultLBOSPort);
                    addresses[lbosaddresses_count++] = lbos_alternative;
                }
            }
#endif /* NCBI_OS_MSWIN */
#endif /* 0 */
            break;
        case eLBOSFindMethod_Lbosresolve :
#ifdef NCBI_OS_LINUX
            lbosaddress = s_LBOS_ReadLbosresolver();
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find LBOS using "
                           "/etc/ncbi/lbosresolve failed");
            } else {
                addresses[lbosaddresses_count++] = strdup(lbosaddress);
            }
#endif
            break;
        case eLBOSFindMethod_Registry:
            lbosaddress_temp = g_LBOS_RegGet("CONN", "lbos", NULL);
            if (g_LBOS_StringIsNullOrEmpty(lbosaddress_temp)) {
                CORE_LOG_X(1, eLOG_Warning, "Trying to find LBOS using "
                        "registry failed");
                free(lbosaddress_temp); /* just in case */
                lbosaddress_temp = NULL;
                break;
            }
            addresses[lbosaddresses_count++] = lbosaddress_temp;
            break;
        }
    }
    addresses[1] = NULL; /* we need only one LBOS address */
    return addresses;
}


/* Get possible addresses of LBOS in default order */
char** g_LBOS_GetLBOSAddresses(void)
{
    return g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_None, NULL);
}


/*/////////////////////////////////////////////////////////////////////////////
//                    STATIC CONVENIENCE FUNCTIONS                           //
/////////////////////////////////////////////////////////////////////////////*/
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


/** Get domain of current host. Do not modify or clear the returned string!
 * Returned string is read-only, may reside in 'data' memory area             */
static const char* s_LBOS_ReadDomain()
{
    /*
     * If no domain has been previously read, fill it
     */
    if (s_LBOS_CurrentDomain == NULL) {
        char* registry_domain = NULL; /* if registry has domain specified
                                       * for LBOS First, we check registry
                                       * and then if nothing there, we check
                                       * /etc/ncbi/domain                     */
        registry_domain = g_LBOS_RegGet("CONN", kLBOSDomainVariable, NULL);
        CORE_LOCK_WRITE;
        if (!g_LBOS_StringIsNullOrEmpty(registry_domain)
                && s_LBOS_CurrentDomain == NULL)
        {
            s_LBOS_CurrentDomain = registry_domain;
        } else {
            free(registry_domain);
        }
        CORE_UNLOCK;
        if (s_LBOS_CurrentDomain != NULL)
            return s_LBOS_CurrentDomain;

#ifdef NCBI_OS_LINUX
        else { /* If nothing found in registry, check /etc/ncbi/domain */
            FILE* domain_file;
            size_t len;
            char str[kMaxLineSize];
            char* read_result; /* during function will become equal either NULL 
                               or str, do not free() */
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
                CORE_LOGF(eLOG_Warning,
                          ("s_LBOS_ComposeLBOSAddress: domain file"
                          "%s is empty, skipping this method",
                          kDomainFile));
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
#endif /* #ifdef NCBI_OS_LINUX */
    }
    return s_LBOS_CurrentDomain;
}


/** Get domain of current host. Do not modify or clear the returned string!
 * Returned string is read-only, may reside in 'data' memory area             */
static const char* s_LBOS_ReadLbosresolver(void)
{
#ifdef NCBI_OS_LINUX
    if (s_LBOS_Lbosresolver == NULL) {
        FILE* lbosresolver_file;
        size_t len;
        char str[kMaxLineSize];
        char* read_result; /* during function will become equal either NULL
                           or str, do not free() */
        if ((lbosresolver_file = fopen(kLbosresolverFile, "r")) == NULL) {
            CORE_LOGF(eLOG_Warning, ("LBOS mapper: "
                                     "could not open lbosresolve file %s",
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
        /*We remove unnecessary '/n' and probably '/r'   */
        if (str[len - 1] == '\n') {
            if (--len && str[len - 1] == '\r')
                --len;
            str[len] = '\0';
        }
        if (g_LBOS_StringIsNullOrEmpty(str)) {
            /* No domain recognized */
            CORE_LOGF(eLOG_Warning,
                      ("LBOS mapper: /etc/ncbi/lbosresolve file"
                      "%s is empty, no LBOS address available",
                      kLbosresolverFile));
            free(read_result);
            return NULL;
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
    }
#endif /* #ifdef NCBI_OS_LINUX */
    return s_LBOS_Lbosresolver;
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

    /* Not to handle case when status_code is NULL, we use internal variable,
       and only try to set status_code in the end of this function */
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
            CORE_LOGF(eLOG_Critical, ("s_LBOS_UrlReadAll: LBOS returned status "
                    "code %d", user_data.http_response_code));
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
#if 0
        /* If we have read enough and use PUT or DELETE methods,
         * exit this loop */
        if (
                totalRead >= user_data.content_length
                /*&&
                (   net_info->req_method == eReqMethod_Put
                    ||
                    net_info->req_method == eReqMethod_Delete
                )*/
            )
        {
            CONN_Flush(conn);
            CONN_SetTimeout(conn, eIO_Close, &kLBOSZeroTimeout);
            SOCK_CloseEx(sock, 0/*retain SOCK*/);
            break;
        }
#endif
        /* IF we still have to read - then add space to buffer, if needed  */
        if (status == eIO_Success && totalBufSize < totalRead * 2)
        {
            realloc_result = (char*)realloc(buf, sizeof(char) *
                                                            (totalBufSize * 2));
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
    if (!(realloc_result = (char*) realloc(buf,
                                          sizeof(char) * (strlen(buf) + 1)))) {
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
                                         const char* serviceName,
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
    char* token = NULL, *saveptr = NULL, *str = NULL;
#ifdef _DEBUG
    char hostport[kHostportStringLength]; //just for debug
#endif
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
    user_dtab = g_LBOS_strcasestr(net_info->http_user_header, 
                                              "DTab-local:");
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
    servicename_url_encoded = s_LBOS_URLEncode(serviceName);
  /*encode service name to url encoding (change ' ' to %20, '/' to %2f, etc.)*/
    url_length = strlen("http://") + strlen(lbos_address) +
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
    /*
     * We read all the answer, find host:port in the answer and fill
     * 'hostports', occasionally reallocating array, if needed
     */
    for (str = lbos_answer  ;  ;  str = NULL) {
        SSERV_Info * info;
#ifdef NCBI_COMPILER_MSVC
        token = strtok_s(str, "\n", &saveptr);
#else
        token = strtok_r(str, "\n", &saveptr);
#endif
        if (token == NULL) {
            break;
        }
        info = SERV_ReadInfoEx(token, serviceName, 0);
        /* Occasionally, the info returned by LBOS can be invalid. */
        if (info == NULL) {
            continue;
        }
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
#ifdef _DEBUG
        SOCK_HostPortToString(info->host, info->port, hostport,
                kHostportStringLength);
        /*Copy IP from stream to the result char array*/
        CORE_LOGF(eLOG_Note, ("Resolved to: [%s]", hostport));
#endif
    }
    free(lbos_answer);
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
    SSERV_Info** hostports_array = 0;
    char** to_delete;
    size_t last_instance;
    /*
     * We iterate through known LBOSes to find working instance
     */
    unsigned int i = 0;
    /* We copy all LBOS addresses to  */
    char** lbos_addresses = NULL;
    unsigned int addresses_count = 0;

    /* We suppose that number of addresses is constant (and so
       is position of NULL), so no mutex is necessary */
    while (s_LBOS_InstancesList[++addresses_count] != NULL) continue;
    lbos_addresses = (char**)calloc(addresses_count + 1, sizeof(char*));
    if (lbos_addresses == NULL) {
        CORE_LOG(eLOG_Critical, "s_LBOS_FillCandidates: No RAM. "
                                "Returning.");
        return;
    }
    /* We can either delete this list or assign to s_LBOS_InstancesList,
     * but anyway we have to prepare */
    to_delete = lbos_addresses;
    CORE_LOCK_READ;
    for (i = 0; i < addresses_count; i++) {
        size_t j = 0;
        lbos_addresses[i] = strdup(s_LBOS_InstancesList[i]);
        /* If strdup returned NULL */
        if (lbos_addresses[i] == NULL) {
            CORE_UNLOCK;
            CORE_LOG(eLOG_Critical, "s_LBOS_FillCandidates: No RAM. "
                                    "Returning.");
            /*Free() what has been allocated*/
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
                "LBOS at %s", service, lbos_addresses[i]));
        hostports_array = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            lbos_addresses[i], service,
            data->net_info);
        i++;
    } while (hostports_array == NULL  &&  lbos_addresses[i] != NULL);
    last_instance = i-1; /* LBOS instance with which we finished
                                 * our search. It can be good or we just 
                                 * ran out of instances */
    i = 0;
    if (hostports_array != NULL) {
        while (hostports_array[++i] != NULL) continue;
        CORE_LOGF_X(1, eLOG_Trace, ("Found %u servers of \"%s\" with "
            "LBOS at %s", i, service, lbos_addresses[last_instance]));
#if 0 /* we do not iterate through LBOSes */
        if (last_instance != 0 && last_instance < addresses_count) {
            /* If we did not succeed with first LBOS in list, let's swap
             * first LBOS with good LBOS. We will use CORE_LOCK_WRITE only
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
                        "Swapping LBOS order, changing \"%s\" (good) and "
                        "%s (bad)",
                        lbos_addresses[last_instance],
                        lbos_addresses[0]));
                }
            }
        }
#endif
    } else {
        CORE_LOGF_X(1, eLOG_Trace, ("Ho servers of \"%s\" found by LBOS",
                service));
    }
    assert(to_delete != NULL);
    for (i = 0;  to_delete[i] != NULL;  i++) {
        free(to_delete[i]);
        to_delete[i] = NULL;
    }
    free(to_delete);
    /*If we received answer from LBOS, we fill candidates */
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


static int s_LBOS_CheckDeannounceArgs(const char* service,
                                      const char* version,
                                      const char* host,
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
    unsigned int   i;
    SConnNetInfo*  net_info;
    char*          buf;
    char**         lbos_addresses;
    char*          status_message = NULL;
    int            status_code;
    /* We try all LBOSes until we get 200. If we receive error status code
     * instead of 200, we save it. So, if no LBOS returns 200, we
     * return error code. If we receive nothing (LBOS is not present), then
     * we save nothing.
     */
    char*          last_status_message; /* to remember the "best" answer */
    char*          last_buf = NULL;     /* to remember the "best" answer */
    int            last_code;
    last_status_message = NULL;
    net_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    net_info->req_method = req_method;
    buf = NULL;
    status_code = 0;
    lbos_addresses = s_LBOS_InstancesList;
    last_code = 0; /* remember last non-zero http response code to analyze
                    * it (we do not give up on first fail, but it is
                    * better to know error code than "no connection"
                    * from non-working LBOS, which can happen on next LBOS
                    * after getting
                    * real error */
    for (i = 0;  lbos_addresses[i] != NULL && last_code != 200;  ++i) {
        char* query;
        size_t length = 0;
        /* We deny foreign de-announcement*/
        /* Compare local domain and LBOS domain */
        if (!g_LBOS_CheckDomain(lbos_addresses[i])) {
            CORE_LOGF_X(1, eLOG_Error,
                        ("Could not verify that [%s] is in local domain [%s]. "
                        "Announcement in foreign domain is not allowed. "
                        "If you omitted [%s] in the LBOS address - please, "
                        "provide it.",
                        lbos_addresses[i], s_LBOS_ReadDomain(), 
                        s_LBOS_ReadDomain()));
            continue;
        }
        query = g_LBOS_StringConcat(g_LBOS_StringConcat(
                        strdup("http://"), lbos_addresses[i], &length),
                                           request,           &length);
        length = strlen(query);
        buf = s_LBOS_UrlReadAll(net_info, query, &status_code, &status_message);
        free(query);
        if (status_code != 0) {
            last_code = status_code;
        }
        if (buf != NULL) {
            free(last_buf);
            last_buf = strdup(buf);
            free(buf);
            buf = NULL;
        }
        if (status_message != NULL) {
            free(last_status_message);
            last_status_message = strdup(status_message);
            free(status_message);
            status_message = NULL;
        }
    }
    if (lbos_answer != NULL && !g_LBOS_StringIsNullOrEmpty(last_buf)) {
        *lbos_answer = strdup(last_buf);
    }
    free(last_buf);
    if (http_status_message != NULL && last_status_message != NULL) {
        *http_status_message = strdup(last_status_message);
    }
    free(last_status_message);

    if (last_code == 0) {
        last_code = kLBOSNoLBOS;
    }
    /* Cleanup */
    ConnNetInfo_Destroy(net_info);
    return last_code;
}


static char* s_LBOS_Replace0000WithIP(const char* healthcheck_url)
{ 
    if (healthcheck_url == NULL)
        return NULL;
    /* By 'const' said that we will not touch healthcheck_url, but actually 
     * we need to be able to change it, so we copy it */
    char* my_healthcheck_url; /* new url with replaced "0.0.0.0" (if needed) */
    const char* replace_pos;/* to check if there is 0.0.0.0 */
    /* If we need to insert local IP instead of 0.0.0.0, we will */
    if ((replace_pos = strstr(healthcheck_url, "0.0.0.0")) == NULL) {
        my_healthcheck_url = strdup(healthcheck_url);
        return(my_healthcheck_url);
    }
    size_t chars_to_copy;
    const char* query;
    char hostname[kMaxLineSize];
    size_t length;
    my_healthcheck_url = (char*)calloc(kMaxLineSize, sizeof(char));
    if (my_healthcheck_url == NULL) {
        CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
                               "not enough RAM.");
        return NULL;
    }
    chars_to_copy = replace_pos - healthcheck_url;
    query = replace_pos + strlen("0.0.0.0");
    unsigned int local_host_ip =
            g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr(eDefault);
    if (local_host_ip == 0) {
        CORE_LOG(eLOG_Warning,
                 "Error with announcement, cannot find local IP.");
        free(my_healthcheck_url);
        return NULL;
    }
    SOCK_HostPortToString(local_host_ip, 0, hostname, kMaxLineSize - 1);
    if (hostname == NULL) {
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


/*/////////////////////////////////////////////////////////////////////////////
//                             UNIT TESTING                                  //
/////////////////////////////////////////////////////////////////////////////*/
/** Check whether LBOS mapper is turned ON or OFF
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

/**  Check whether LBOS mapper is turned ON or OFF
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


int/*bool*/ g_LBOS_UnitTesting_SetLBOSFindMethod (SERV_ITER       iter,
                                                  ELBOSFindMethod method)
{
    SLBOS_Data* data;
    assert(g_LBOS_CheckIterator(iter, ELBOSIteratorCheckType_MustHaveData));
    data = (SLBOS_Data*) iter->data;
    data->find_method = method;
    return 1;
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
/**  Set custom address for LBOS. Can be both hostname:port and IP:port.
 *  Intended mostly for testing.
 * @param[in] iter
 *  Where to set address for LBOS. The settings is made for only one iterator*/
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
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    CORE_LOCK_WRITE;
        if (s_LBOS_InstancesList == NULL) {
            s_LBOS_InstancesList = g_LBOS_GetLBOSAddresses();
        }
        if (s_EmptyNetInfo == NULL) {
            s_EmptyNetInfo = ConnNetInfo_Create(NULL);
        }
    CORE_UNLOCK;
    s_LBOS_TurnedOn = 1; /* To ensure that initialization does
                            not depend on this variable */
    s_LBOS_Init = 1;

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

    /*
     * Try to find LBOS
     */
    iter = (SERV_ITER)calloc(1, sizeof(*iter));
    assert(iter != NULL);  /* we can do nothing if this happens */
    iter->name = service;
    net_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    iter->op = SERV_LBOS_Open(iter, net_info, NULL);
    ConnNetInfo_Destroy(net_info);
    if (iter->op == NULL) {
        CORE_LOG_X(1, eLOG_Warning, "Turning LBOS off in this process.");
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
    if (response == NULL) {
        /* We do not intervent in the process by default */
        return eHTTP_HeaderSuccess;
    }
    SLBOS_UserData* response_output = (SLBOS_UserData*)response;
    int status_code = 0/*success code if any*/;
    /* For all we know, status message ends before \r\n */
    char* header_end = strstr(header, "\r\n");
    char* status_message = (char*)calloc(header_end-header, sizeof(char));
    unsigned int content_length;
    char* content_length_pos;
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
               "%u",
               &content_length);
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
    response_output->content_length = content_length;
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
                data->a_cand = 0;
                data->n_cand = 0;
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


/** Not implemented in LBOS mapper. */
static int/*bool*/ s_LBOS_Feedback (SERV_ITER a, double b, int c)
{
    return 0;
}


/** Not implemented in LBOS mapper. */
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

    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return NULL;
    }
    /*
     * First, we need to check arguments
     */
    assert(iter != NULL);  /* we can do nothing if this happens */

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
    data = s_LBOS_ConstructData(kInitialCandidatesCount);
    if(net_info == NULL) {
        CORE_LOG(eLOG_Warning,
                 "Parameter \"net_info\" is null, creating net info. "
                 "Please, fix the code and provide net_info.");
        data->net_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    } else {
        data->net_info = ConnNetInfo_Clone(net_info);
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
                               /* lbos_answer is never NULL  */
                               char**                  lbos_answer,
                               char**                  http_status_message)
    {
    char** lbos_addresses;
    int status_code;
    char* status_message;
    char* last_status_message; /* for last existing LBOS (maybe malfunction)*/
    char* last_buf = NULL; /* for last existing LBOS (maybe malfunction)*/
    SConnNetInfo* net_info;
    const char* query_format = NULL;
    char* buf = NULL; /* for answer from LBOS */
    int last_code;
    char* lbos_addr = NULL;
    int parsed_symbols = 0;
    int i;
    size_t length;

    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return kLBOSOff;
    }
    lbos_addresses = s_LBOS_InstancesList;
    status_code = 0;
    status_message = NULL;
    last_status_message = NULL;
    net_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    net_info->req_method = eReqMethod_Post;
    query_format = "http://%s/lbos/json/announce?name=%s&"
                               "version=%s&port=%hu&check=%s";
    last_code = 0; /* remember last non-zero http response code to analyze 
                         it (we do not give up on first fail, but it is 
                         better to know error code than "no connection" 
                         from non-working LBOS, which can happen after getting
                         real error */
    /*
     * Let's try announce 
     */
    for (i = 0;  lbos_addresses[i] != NULL && last_code != 200;  ++i)
    {
        char* query;
        /* We deny foreign announcement*/
        /* Compare local domain and LBOS domain */
        if (!g_LBOS_CheckDomain(lbos_addresses[i])) {
            CORE_LOGF_X(1, eLOG_Warning, 
                        ("[%s] is not from local domain [%s]. "
                        "Announcement in foreign domain is not allowed.",
                        lbos_addresses[i], s_LBOS_ReadDomain()));                        
            continue;
        }
        query = (char*)calloc(strlen(query_format) + 
                                  strlen(lbos_addresses[i]) + 
                                  strlen(service) + strlen(version) +
                                  5/* port */ + strlen(healthcheck_url),
                              sizeof(char));
        sprintf(query,
                query_format,
                lbos_addresses[i], service, version, port, healthcheck_url);
        length = strlen(query);
        /* If host was provided, we append it to query */
        if (!g_LBOS_StringIsNullOrEmpty(host)) {
            query = g_LBOS_StringConcat(g_LBOS_StringConcat(
                                                    query,  "&ip=", &length),
                                                            host,   &length);
        }
        buf = s_LBOS_UrlReadAll(net_info, query, &status_code, 
                                &status_message);
        if (status_code != 0) {
            last_code = status_code;
        }
        if (buf != NULL) {
            free(last_buf);
            last_buf = strdup(buf); 
            free(buf);
            buf = NULL;
        }
        if (status_message != NULL) {
            free(last_status_message);
            last_status_message = strdup(status_message);
            free(status_message);
            status_message = NULL;
        }
        free(query);      
    }
    if (!g_LBOS_StringIsNullOrEmpty(last_buf)) {
        *lbos_answer = strdup(last_buf);
    }
    if (http_status_message != NULL && last_status_message != NULL) {
        *http_status_message = strdup(last_status_message);
    }
    free(last_status_message);
    /* If no LBOS found */
    if (last_code == 0) {
        CORE_LOG(eLOG_Warning, "Announce failed. No LBOS found.");
        last_code = kLBOSNoLBOS;
        goto clear_and_exit;
    }
    /* If announced server has broken healthcheck */
    if (last_code == kLBOSNotFound || last_code == kLBOSBadRequest) {
        CORE_LOGF(eLOG_Warning, ("Announce failed. "
                                 "LBOS returned error code %d.", last_code));
        goto clear_and_exit;
    }
    /* If we could not announce, it is really bad */
    if (last_code != 200) {
        CORE_LOGF(eLOG_Warning, ("Announce failed. "
                                 "LBOS returned error code %d. "
                                 "LBOS answer: %s.", last_code, last_buf));
        goto clear_and_exit;
    }
    /* If we announced successfully and status_code is 200,
     * let's extract LBOS address */
    lbos_addr = (char*)calloc(kMaxLineSize, sizeof(char)); /* will not be 
                                                            * free()'d */
    if (lbos_addr == NULL) {
        CORE_LOG(eLOG_Warning, "Failed memory allocation. Most likely, "
                               "not enough RAM.");
        last_code = kLBOSMemAllocError;
        goto clear_and_exit;
    }
    if (last_buf != NULL) {
        parsed_symbols = sscanf(last_buf, "{\"watcher\":\"%[^\"]\"}", 
                                lbos_addr);
    }
    if (parsed_symbols != 1) {
        CORE_LOG(eLOG_Warning, "g_LBOS_Announce: LBOS answered 200 OK, but "
                               "output could not be parsed");
        free(lbos_addr);
        last_code = kLBOSCorruptOutput;
        goto clear_and_exit;
    } else {
    /* If announce finished with success, we parsed it to extract LBOS ip:port.
     * We free() original output and replace it with ip:port                 */
        free(*lbos_answer);
        *lbos_answer = lbos_addr;
    }

    /* Cleanup */
    clear_and_exit:
        free(last_buf);
        ConnNetInfo_Destroy(net_info);
    return last_code;
}


unsigned short LBOS_Announce(const char*             service,
                             const char*             version,
                             const char*             host,
                             unsigned short          port,
                             const char*             healthcheck_url,
                             char**                  lbos_answer,
                             char**                  http_status_message)
{
    char* my_healthcheck_url = NULL;
    char* healthcheck_encoded = NULL;
    char* my_host = NULL;
    char* service_encoded = NULL;
    char* version_encoded = NULL;
    unsigned short result;

    /*
     * First we check input arguments
     */
    if (s_LBOS_CheckAnnounceArgs(service, version, host, port, healthcheck_url,
                                                             lbos_answer) == 0)
    {
        return kLBOSInvalidArgs;
    }
    /*
     * Pre-assign variables
     */
    *lbos_answer = NULL;
    /* Check if we need to replace 0.0.0.0 with local IP, and do it if needed*/
    my_healthcheck_url = s_LBOS_Replace0000WithIP(healthcheck_url);


	/*my_healthcheck_url = strdup(healthcheck_url); */
    if (my_healthcheck_url == NULL) {
        result = kLBOSDNSResolveError;
        goto clean_and_exit;
    }
    /* If host provided separately from healthcheck URL, check if we need to
     * replace 0.0.0.0 with local IP, and do it if needed                    */
    my_host = s_LBOS_Replace0000WithIP(host);

    healthcheck_encoded  = s_LBOS_URLEncode(my_healthcheck_url);
    service_encoded      = s_LBOS_URLEncode(service);
    version_encoded      = s_LBOS_URLEncode(version);

    /* Announce */
    result = 
            g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx(service_encoded, 
                                                          version_encoded, 
                                                          my_host,
                                                          port,
                                                          healthcheck_encoded, 
                                                          lbos_answer,
                                                          http_status_message);     
    if (result == kLBOSSuccess) {
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
    unsigned short result = kLBOSSuccess;
    unsigned int port;
    char* srvc;
    char* vers;
    char* port_str;
    char* hlth;
    char* host;
    size_t i = 0;

    if (g_LBOS_StringIsNullOrEmpty(registry_section)) {
        registry_section = kLBOSAnnouncementSection;
    }
    srvc = g_LBOS_RegGet(registry_section, kLBOSServiceVariable, NULL);
    vers = g_LBOS_RegGet(registry_section, kLBOSVersionVariable, NULL);
    port_str = g_LBOS_RegGet(registry_section, kLBOSPortVariable, NULL);
    hlth = g_LBOS_RegGet(registry_section, kLBOSHealthcheckUrlVariable, NULL);
    host = g_LBOS_RegGet(registry_section, kLBOSServerHostVariable, NULL);
    
    /* Check port that it is a number of max 5 digits and no other symbols   */
    for (i = 0;  i < strlen(port_str);  i++) {
        if (!isdigit(port_str[i])) {
            result = kLBOSInvalidArgs;
            goto clean_and_exit;
        }
    }
    if (strlen(port_str) > 5 || (sscanf(port_str, "%d", &port) != 1) ||
        port < 1 || port > 65535) 
    {
        result = kLBOSInvalidArgs;
        goto clean_and_exit;
    }    

    /* Announce */    
    result = LBOS_Announce(srvc, vers, host, (unsigned short)port, hlth,
                           lbos_answer, http_status_message);    
    if (result == kLBOSSuccess) {
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
    return result;
}


unsigned short s_LBOS_Deannounce(const char*      service,
                               const char*        version,
                               const char*        host,
                               unsigned short     port,
                               char**             lbos_answer,
                               char**             http_status_message,
                               SConnNetInfo*      net_info)
{
    const char*    query_format;
    char**         lbos_addresses;
    int            last_code;
    char*          status_message = NULL;
    unsigned int   i;
    char*          buf;
    int            status_code;
    char* last_status_message; /* for last existing LBOS (maybe malfunction)*/
    char* last_buf = NULL; /* for last existing LBOS (maybe malfunction)*/
    lbos_addresses = s_LBOS_InstancesList;
    last_code = 0; /* remember last non-zero http response code to analyze
                   it (we do not give up on first fail, but it is
                   better to know error code than "no connection"
                   from non-working LBOS, which can happen after getting
                   real error */
    last_status_message = NULL;
    status_code = 0;
    buf = NULL;
    query_format = "http://%s/lbos/json/conceal?name=%s&version=%s&port=%hu";
    /*
    * Try deannounce
    */
    for (i = 0;  lbos_addresses[i] != NULL && last_code != 200;  ++i) {
        char* query;
        size_t length;
        /* We deny foreign de-announcement*/
        /* Compare local domain and LBOS domain */
        if (!g_LBOS_CheckDomain(lbos_addresses[i])) {
            CORE_LOGF_X(1, eLOG_Warning,
                ("[%s] is not from local domain [%s]. "
                "Announcement in foreign domain is not allowed.",
                lbos_addresses[i], s_LBOS_ReadDomain()));
            continue;
        }
        query = (char*)calloc(strlen(query_format) +
                    strlen(lbos_addresses[i]) + strlen(service) +
                    strlen(version) + 5/*port*/, 
                sizeof(char));
        sprintf(query, query_format, lbos_addresses[i], service, version, port);
        length = strlen(query);
        /* If host was provided, we append it to query */
        if (!g_LBOS_StringIsNullOrEmpty(host)) {
            query = g_LBOS_StringConcat(g_LBOS_StringConcat(
                query, "&ip=", &length),
                host, &length);
        }
        else { /* If host was NOT provided, we append local IP to query,
               * just in case */
            char* local_ip = s_LBOS_Replace0000WithIP("0.0.0.0");
            query = g_LBOS_StringConcat(g_LBOS_StringConcat(
                query, "&ip=", &length),
                local_ip, &length);
            free(local_ip);
        }
        buf = s_LBOS_UrlReadAll(net_info, query, &status_code,
            &status_message);
        free(query);
        if (status_code != 0) {
            last_code = status_code;
        }
        if (buf != NULL) {
            free(last_buf);
            last_buf = strdup(buf);
            free(buf);
            buf = NULL;
        }
        if (status_message != NULL) {
            free(last_status_message);
            last_status_message = strdup(status_message);
            free(status_message);
            status_message = NULL;
        }
    }
    if (lbos_answer != NULL && !g_LBOS_StringIsNullOrEmpty(last_buf)) {
        *lbos_answer = strdup(last_buf);
    }
    free(last_buf);
    if (http_status_message != NULL && last_status_message != NULL) {
        *http_status_message = strdup(last_status_message);
    }
    free(last_status_message);

    if (last_code == 0) {
        last_code = kLBOSNoLBOS;
    }
    return last_code;
}


unsigned short LBOS_Deannounce(const char*        service,
                               const char*        version,
                               const char*        host,
                               unsigned short     port,
                               char**             lbos_answer,
                               char**             http_status_message)
{ 
    SConnNetInfo*  net_info;
    char*          service_encoded;
    char*          version_encoded;
    char*          my_host;
    /*
     * First we check input arguments
     */
    if (s_LBOS_CheckDeannounceArgs(service, version, host, port) == 0) {
        return kLBOSInvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return kLBOSOff;
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
    }
    net_info = ConnNetInfo_Clone(s_EmptyNetInfo);
    net_info->req_method = eReqMethod_Post;
    service_encoded = s_LBOS_URLEncode(service);
    version_encoded = s_LBOS_URLEncode(version);
    
    unsigned short retval = s_LBOS_Deannounce(service_encoded, version_encoded, 
                                              my_host, port, lbos_answer, 
                                              http_status_message, net_info);

    /* If kLBOSNotFound or kLBOSSuccess - we delete server from local storage
    * as no longer existing */
    if (retval == kLBOSNotFound || retval == kLBOSSuccess) {
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
    struct SLBOS_AnnounceHandle_Tag** arr;
    struct SLBOS_AnnounceHandle_Tag* local_arr;
    unsigned int servers;
    unsigned int i;

    CORE_LOCK_READ;
    arr = &s_LBOS_AnnouncedServers;
    servers = s_LBOS_AnnouncedServersNum;
    local_arr = 
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
    if (g_LBOS_StringIsNullOrEmpty(service) || lbos_answer == NULL) {
        return kLBOSInvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return kLBOSOff;
    }
    /*
     * Arguments are good! Let's do the request
     */
    service_encoded = s_LBOS_URLEncode(service);
    query_format = "/lbos/xml/configuration?name=%s";
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
    if (g_LBOS_StringIsNullOrEmpty(service) || lbos_answer == NULL
            || g_LBOS_StringIsNullOrEmpty(new_version)) {
        return kLBOSInvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return kLBOSOff;
    }
    /*
     * Arguments are good! Let's do the request
     */
    service_encoded = s_LBOS_URLEncode(service);
    query_format = "/lbos/xml/configuration?name=%s&version=%s";
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
    if (g_LBOS_StringIsNullOrEmpty(service) || lbos_answer == NULL) {
        return kLBOSInvalidArgs;
    }
    /*
     * Check if LBOS is ON
     */
    if (s_LBOS_Init == 0) {
        s_LBOS_funcs.Initialize();
    }
    if (s_LBOS_TurnedOn == 0) {
        return kLBOSOff;
    }
    /*
     * Arguments are good! Let's do the request
     */
    service_encoded = s_LBOS_URLEncode(service);
    query_format = "/lbos/xml/configuration?name=%s";
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

