/*  $Id$
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
#include <connect/ncbi_file_connector.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    char*         inp_file_name;
    char*         out_file_name;
    FILE*         finp;
    FILE*         fout;
    SFileConnAttr attr;
} SFileConnector;


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
    static void        s_Setup      (SMetaConnector* meta,
                                     CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "FILE";
}


/*ARGSUSED*/
static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    const char*     mode = 0;

    /* if connected already, and trying to reconnect */
    if (xxx->finp  &&  xxx->fout)
        return eIO_Success;

    /* open file for output */
    switch ( xxx->attr.w_mode ) {
    case eFCM_Truncate:
        mode = "wb";  break;
    case eFCM_Seek: /* mode = "rb+"; break; */
    case eFCM_Append:
        mode = "ab";  break;
    }

    xxx->fout = fopen(xxx->out_file_name, mode);
    if ( !xxx->fout ) {
        return eIO_Unknown;
    }

    /* open file for input */
    xxx->finp = fopen(xxx->inp_file_name, "rb");
    if ( !xxx->finp ) {
        fclose(xxx->fout);
        xxx->fout = 0;
        return eIO_Unknown;
    }

    /* Due to the shortage of portable 'fseek' call
     * ignore read/write positions so far;
     * only 0/EOF are in use for writing, and only 0 for reading
     */
    return eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    if ( !size ) {
        *n_written = 0;
        return eIO_Success;
    }

    *n_written = fwrite(buf, 1, size, xxx->fout);
    if ( !*n_written ) {
        return eIO_Unknown;
    }

    return eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    if ( !size ) {
        *n_read = 0;
        return eIO_Success;
    }

    *n_read = fread(buf, 1, size, xxx->finp);
    if ( !*n_read ) {
        return feof(xxx->finp) ? eIO_Closed : eIO_Unknown;
    }

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
static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    if (fflush(xxx->fout) != 0)
        return eIO_Unknown;

    return eIO_Success;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    assert(dir == eIO_Read || dir == eIO_Write);
    if (dir == eIO_Read)
        return feof(xxx->finp) ? eIO_Closed :
        (ferror(xxx->finp) ? eIO_Unknown : eIO_Success);
    else
        return ferror(xxx->fout) ?  eIO_Unknown : eIO_Success;
}


/*ARGSUSED*/
static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    fclose(xxx->finp);
    xxx->finp = 0;
    fclose(xxx->fout);
    xxx->fout = 0;
    return eIO_Success;
}


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      s_VT_Flush,     connector);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
    meta->default_timeout = 0/*infinite*/;
}


static void s_Destroy
(CONNECTOR connector)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    
    free(xxx->inp_file_name);
    free(xxx->out_file_name);
    free(xxx);
    connector->handle = 0;
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FILE_CreateConnector
(const char* inp_file_name,
 const char* out_file_name)
{
    static const SFileConnAttr def_attr = { 0, eFCM_Truncate, 0 };

    return FILE_CreateConnectorEx(inp_file_name, out_file_name, &def_attr);
}


extern CONNECTOR FILE_CreateConnectorEx
(const char*          inp_file_name,
 const char*          out_file_name,
 const SFileConnAttr* attr)
{
    CONNECTOR       ccc = (SConnector*) malloc(sizeof(SConnector));
    SFileConnector* xxx = (SFileConnector*) malloc(sizeof(SFileConnector));

    /* initialize internal data structures */
    xxx->inp_file_name = strdup(inp_file_name);
    xxx->out_file_name = strdup(out_file_name);
    xxx->finp          = 0;
    xxx->fout          = 0;

    memcpy(&xxx->attr, attr, sizeof(SFileConnAttr));

    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.13  2005/04/20 18:15:59  lavr
 * +<assert.h>
 *
 * Revision 6.12  2003/05/31 05:14:56  lavr
 * Add ARGSUSED where args are meant to be unused
 *
 * Revision 6.11  2003/05/14 03:55:53  lavr
 * Slight VT table reformatting
 *
 * Revision 6.10  2002/10/28 15:46:20  lavr
 * Use "ncbi_ansi_ext.h" privately
 *
 * Revision 6.9  2002/10/22 15:11:24  lavr
 * Zero connector's handle to crash if revisited
 *
 * Revision 6.8  2002/09/24 15:06:00  lavr
 * Log moved to end
 *
 * Revision 6.7  2002/04/26 16:32:36  lavr
 * Added setting of default timeout in meta-connector's setup routine
 *
 * Revision 6.6  2001/12/04 15:56:35  lavr
 * Use strdup() instead of explicit strcpy(malloc(...), ...)
 *
 * Revision 6.5  2001/01/25 17:04:43  lavr
 * Reversed:: DESTROY method calls free() to delete connector structure
 *
 * Revision 6.4  2001/01/23 23:11:20  lavr
 * Status virtual method implemented
 *
 * Revision 6.3  2001/01/11 16:38:16  lavr
 * free(connector) removed from s_Destroy function
 * (now always called from outside, in METACONN_Remove)
 *
 * Revision 6.2  2000/12/29 17:55:53  lavr
 * Adapted for use of new connector structure.
 *
 * Revision 6.1  2000/04/12 15:18:13  vakatov
 * Initial revision
 *
 * ==========================================================================
 */
