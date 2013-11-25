/* $Id$
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
 * Author:  Vladimir Alekseyev, Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Implement CONNECTOR for a FILE stream
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_assert.h"
#include <connect/ncbi_file_connector.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NCBI_OS_MSWIN
#  define fseek  _fseeki64
#endif /*NCBI_OS_MSWIN*/


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    const char*    ifname;
    const char*    ofname;
    FILE*          finp;
    FILE*          fout;
    SFILE_ConnAttr attr;
} SFileConnector;


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
    static char*       s_VT_Descr   (CONNECTOR       connector);
    static EIO_Status  s_VT_Open    (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Wait    (CONNECTOR       connector,
                                     EIO_Event       event,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Write   (CONNECTOR       connector,
                                     const void*     buf,
                                     size_t          size,
                                     size_t*         n_written,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Flush   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "FILE";
}


static char* s_VT_Descr
(CONNECTOR connector)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    if (xxx->ifname  &&  xxx->ofname) {
        size_t ifnlen = strlen(xxx->ifname);
        size_t ofnlen = strlen(xxx->ofname);
        char* descr = (char*) malloc(ifnlen + ofnlen + 3);
        if (descr) {
            memcpy(descr + 1,      xxx->ifname, ifnlen++);
            descr[ifnlen++] = '>';
            memcpy(descr + ifnlen, xxx->ofname, ++ofnlen);
            descr[0] = '<';
            return descr;
        }
    } else if (xxx->ifname) {
        return strdup(xxx->ifname);
    } else if (xxx->ofname) {
        return strdup(xxx->ofname);
    }
    return 0;
}


/*ARGSUSED*/
static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* unused)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    const char*     mode;

    assert(!xxx->finp  &&  !xxx->fout);

    /* open file for output */
    if (xxx->ofname) {
        switch ( xxx->attr.w_mode ) {
        case eFCM_Truncate:
            mode = "wb";
            break;
        case eFCM_Seek:
            mode = "r+b";
            break;
        case eFCM_Append:
            mode = "ab";
            break;
        default:
            return eIO_InvalidArg;
        }
        if (!(xxx->fout = fopen(xxx->ofname, mode)))
            return eIO_Unknown;
        if (xxx->attr.w_mode == eFCM_Seek  &&  xxx->attr.w_pos
            &&  fseek(xxx->fout, xxx->attr.w_pos, SEEK_SET) != 0) {
            fclose(xxx->fout);
            xxx->fout = 0;
            return eIO_Unknown;
        }
    }

    /* open file for input */
    if (xxx->ifname) {
        if (!(xxx->finp = fopen(xxx->ifname, "rb"))) {
            if (xxx->fout) {
                fclose(xxx->fout);
                xxx->fout = 0;
            }
            return eIO_Unknown;
        }
        if (xxx->attr.r_pos
            &&  fseek(xxx->finp, xxx->attr.r_pos, SEEK_SET) != 0) {
            fclose(xxx->finp);
            xxx->finp = 0;
            if (xxx->fout) {
                fclose(xxx->fout);
                xxx->fout = 0;
            }
            return eIO_Unknown;
        }
    }

    assert(xxx->finp  ||  xxx->fout);
    return eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    return eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* unused)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    assert(*n_written == 0);

    if (!xxx->fout)
        return eIO_Closed;
    if (!size)
        return eIO_Success;

    *n_written = fwrite(buf, 1, size, xxx->fout);

    return *n_written ? eIO_Success : eIO_Unknown;
}


/*ARGSUSED*/
static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* unused)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    if (!xxx->fout)
        return eIO_Closed;

    return fflush(xxx->fout) != 0 ? eIO_Unknown : eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* unused)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    assert(*n_read == 0);

    if (!xxx->finp)
        return eIO_Closed;
    if (!size)
        return eIO_Success;

    *n_read = fread(buf, 1, size, xxx->finp);

    return *n_read ? eIO_Success : feof(xxx->finp) ? eIO_Closed : eIO_Unknown;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    switch (dir) {
    case eIO_Read:
        return !xxx->finp ? eIO_Closed
            : feof(xxx->finp) ? eIO_Closed
            : ferror(xxx->finp) ? eIO_Unknown : eIO_Success;
    case eIO_Write:
        return !xxx->fout ? eIO_Closed
            : ferror(xxx->fout) ? eIO_Unknown : eIO_Success;
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


/*ARGSUSED*/
static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* unused)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    assert(xxx->finp  ||  xxx->fout);

    if (xxx->finp) {
        if (fclose(xxx->finp) != 0)
            status = eIO_Unknown;
        xxx->finp = 0;
    }
    if (xxx->fout) {
        if (fclose(xxx->fout) != 0)
            status = eIO_Unknown;
        xxx->fout = 0;
    }
    return status;
}


static void s_Setup
(CONNECTOR connector)
{
    SMetaConnector* meta = connector->meta;

    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, descr,    s_VT_Descr,   connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, wait,     s_VT_Wait,    connector);
    CONN_SET_METHOD(meta, write,    s_VT_Write,   connector);
    CONN_SET_METHOD(meta, flush,    s_VT_Flush,   connector);
    CONN_SET_METHOD(meta, read,     s_VT_Read,    connector);
    CONN_SET_METHOD(meta, status,   s_VT_Status,  connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    meta->default_timeout = kInfiniteTimeout;
}


static void s_Destroy
(CONNECTOR connector)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    connector->handle = 0;

    assert(!xxx->finp  &&  !xxx->fout);
    xxx->ifname = 0;
    xxx->ofname = 0;
    free(xxx);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FILE_CreateConnector
(const char* ifname,
 const char* ofname)
{
    return FILE_CreateConnectorEx(ifname, ofname, NULL);
}


extern CONNECTOR FILE_CreateConnectorEx
(const char*           ifname,
 const char*           ofname,
 const SFILE_ConnAttr* attr)
{
    /* In fact, this is a whole-zero init */
    static const SFILE_ConnAttr def_attr = { eFCM_Truncate };

    CONNECTOR       ccc;
    SFileConnector* xxx;
    char*           str;
    size_t          ifnlen = ifname  &&  *ifname ? strlen(ifname) + 1 : 0;
    size_t          ofnlen = ofname  &&  *ofname ? strlen(ofname) + 1 : 0;

    if (!(ifnlen | ofnlen))
        return 0;
    if (!(ccc = (SConnector*) malloc(sizeof(SConnector))))
        return 0;
    if (!(xxx = (SFileConnector*) malloc(sizeof(*xxx) + ifnlen + ofnlen))) {
        free(ccc);
        return 0;
    }

    /* initialize internal data structures */
    str  = (char*) xxx + sizeof(*xxx);
    xxx->ifname = (const char*)(ifnlen ? memcpy(str, ifname, ifnlen) : 0);
    str += ifnlen;
    xxx->ofname = (const char*)(ofnlen ? memcpy(str, ofname, ofnlen) : 0);
    xxx->finp   = 0;
    xxx->fout   = 0;
    if (xxx->ofname)
        memcpy(&xxx->attr, attr ? attr : &def_attr, sizeof(xxx->attr));
    else
        memset(&xxx->attr, 0,                       sizeof(xxx->attr));

    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}
