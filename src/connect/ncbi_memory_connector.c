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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   In-memory CONNECTOR
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 */

#include <connect/ncbi_memory_connector.h>
#include <assert.h>
#include <stdlib.h>


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    BUF                  buf;
    unsigned int/*bool*/ own_buf;
    EIO_Status           r_status;
    EIO_Status           w_status;
} SMemoryConnector;


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
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
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (SMetaConnector* meta,
                                     CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "MEMORY";
}


/*ARGSUSED*/
static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;
    xxx->r_status = eIO_Success;
    xxx->w_status = eIO_Success;
    return eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;
    assert(event == eIO_Read  ||  event == eIO_Write);
    switch (event) {
    case eIO_Read:
        if (!BUF_Size(xxx->buf))
            return eIO_Closed;
        /*FALLTHRU*/
    case eIO_Write:
        return eIO_Success;
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


/*ARGSUSED*/
static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;

    if ( !size )
        return eIO_Success;

    if (BUF_Write(&xxx->buf, buf, size)) {
        *n_written    = size;
        xxx->w_status = eIO_Success;
    } else
        xxx->w_status = eIO_Unknown;

    return xxx->w_status;
}


/*ARGSUSED*/
static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;

    if ( !size )
        return eIO_Success;

    *n_read = BUF_Read(xxx->buf, buf, size);
    xxx->r_status = *n_read ? eIO_Success : eIO_Closed;

    return xxx->r_status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;
    switch (dir) {
    case eIO_Read:
        return xxx->r_status;
    case eIO_Write:
        return xxx->w_status;
    default:
        assert(0); /* should never happen as checked by connection */
        break;
    }
    return eIO_InvalidArg;
}


/*ARGSUSED*/
static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;
    BUF_Erase(xxx->buf);
    return eIO_Success;
}


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, wait,     s_VT_Wait,    connector);
    CONN_SET_METHOD(meta, write,    s_VT_Write,   connector);
    CONN_SET_METHOD(meta, flush,    0,            0);
    CONN_SET_METHOD(meta, read,     s_VT_Read,    connector);
    CONN_SET_METHOD(meta, status,   s_VT_Status,  connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    meta->default_timeout = kInfiniteTimeout;
}


static void s_Destroy
(CONNECTOR connector)
{
    SMemoryConnector* xxx = (SMemoryConnector*) connector->handle;
    connector->handle = 0;

    if (xxx->own_buf) {
        BUF_Destroy(xxx->buf);
        xxx->buf = 0;
    }
    free(xxx);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR MEMORY_CreateConnector(void)
{
    return MEMORY_CreateConnectorEx(0, 0);
}


extern CONNECTOR MEMORY_CreateConnectorEx(BUF                  buf,
                                          unsigned int/*bool*/ own_buf)
{
    CONNECTOR         ccc;
    SMemoryConnector* xxx;

    if (!(ccc = (SConnector*) malloc(sizeof(SConnector))))
        return 0;
    if (!(xxx = (SMemoryConnector*) malloc(sizeof(*xxx)))) {
        free(ccc);
        return 0;
    }

    /* initialize internal data structures */
    xxx->buf     = buf;
    xxx->own_buf = buf ? own_buf : 1/*true*/;

    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}
