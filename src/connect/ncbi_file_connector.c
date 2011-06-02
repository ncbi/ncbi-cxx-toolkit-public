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
    const char*    inp_filename;
    const char*    out_filename;
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
    const char*     mode;

    assert(!xxx->finp  &&  !xxx->fout);

    /* open file for output */
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
    if (!xxx->out_filename
        ||  !(xxx->fout = fopen(xxx->out_filename, mode))) {
        return eIO_Unknown;
    }
    if (xxx->attr.w_mode == eFCM_Seek  &&  xxx->attr.w_pos
        &&  fseek(xxx->fout, xxx->attr.w_pos, SEEK_SET) != 0) {
        fclose(xxx->fout);
        xxx->fout = 0;
        return eIO_Unknown;
    }

    /* open file for input */
    if (!xxx->inp_filename
        ||  !(xxx->finp = fopen(xxx->inp_filename, "rb"))) {
        fclose(xxx->fout);
        xxx->fout = 0;
        return eIO_Unknown;
    }
    if (xxx->attr.r_pos
        &&  fseek(xxx->finp, xxx->attr.r_pos, SEEK_SET) != 0) {
        fclose(xxx->finp);
        fclose(xxx->fout);
        xxx->finp = 0;
        xxx->fout = 0;
        return eIO_Unknown;
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
static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    assert(xxx->fout  &&  *n_written == 0);

    if (!size)
        return eIO_Success;

    *n_written = fwrite(buf, 1, size, xxx->fout);

    return *n_written ? eIO_Success : eIO_Unknown;
}


/*ARGSUSED*/
static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    assert(xxx->fout);

    return fflush(xxx->fout) != 0 ? eIO_Unknown : eIO_Success;
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

    assert(xxx->finp  &&  *n_read == 0);

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
        assert(xxx->finp);
        return feof(xxx->finp) ? eIO_Closed
            : ferror(xxx->finp) ? eIO_Unknown : eIO_Success;
    case eIO_Write:
        assert(xxx->fout);
        return ferror(xxx->fout) ? eIO_Unknown : eIO_Success;
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


/*ARGSUSED*/
static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;
    EIO_Status status;

    assert(xxx->finp  &&  xxx->fout);

    if (fclose(xxx->finp) != 0)
        status = eIO_Unknown;
    xxx->finp = 0;
    if (fclose(xxx->fout) != 0)
        status = eIO_Unknown;
    xxx->fout = 0;
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

    if (xxx->inp_file_name) {
        free((void*) xxx->inp_file_name);
        xxx->inp_file_name = 0;
    }
    if (xxx->out_file_name) {
        free((void*) xxx->out_file_name);
        xxx->out_file_name = 0;
    }
    free(xxx);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FILE_CreateConnector
(const char* inp_filename,
 const char* out_filename)
{
    static const SFILE_ConnAttr def_attr = { 0, eFCM_Truncate, 0 };

    return FILE_CreateConnectorEx(inp_filename, out_filename, &def_attr);
}


extern CONNECTOR FILE_CreateConnectorEx
(const char*           inp_filename,
 const char*           out_filename,
 const SFILE_ConnAttr* attr)
{
    CONNECTOR       ccc;
    SFileConnector* xxx;

    if (!inp_filename  ||  !out_filename)
        return 0;
    if (!(ccc = (SConnector*)     malloc(sizeof(SConnector))))
        return 0;
    if (!(xxx = (SFileConnector*) malloc(sizeof(*xxx)))) {
        free(ccc);
        return 0;
    }

    /* initialize internal data structures */
    xxx->inp_file_name = strdup(inp_file_name);
    xxx->out_file_name = strdup(out_file_name);
    xxx->finp          = 0;
    xxx->fout          = 0;

    memcpy(&xxx->attr, attr, sizeof(xxx->attr));

    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}
