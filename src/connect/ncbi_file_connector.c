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
 * Author:  Vladimir Alekseyev, Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for a FILE stream
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/04/12 15:18:13  vakatov
 * Initial revision
 *
 * ==========================================================================
 */


#include <connect/ncbi_file_connector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
    static const char* s_VT_GetType(void* connector);
    static EIO_Status s_VT_Connect(void*           connector,
                                   const STimeout* timeout);
    static EIO_Status s_VT_Wait(void*           connector,
                                EIO_Event       event,
                                const STimeout* timeout);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
    static EIO_Status s_VT_Write(void*           connector,
                                 const void*     buf,
                                 size_t          size,
                                 size_t*         n_written,
                                 const STimeout* timeout);
    static EIO_Status s_VT_Flush(void*           connector,
                                 const STimeout* timeout);
    static EIO_Status s_VT_Read(void*           connector,
                                void*           buf,
                                size_t          size,
                                size_t*         n_read,
                                const STimeout* timeout);
    static EIO_Status s_VT_Close(CONNECTOR       connector,
                                 const STimeout* timeout);
} /* extern "C" */
#endif /* __cplusplus */



static const char* s_VT_GetType
(void* connector)
{
    return "FILE";
}


static EIO_Status s_VT_Connect
(void*           connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector;
    const char*     mode;

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


static EIO_Status s_VT_Write
(void*           connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector;

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


static EIO_Status s_VT_Read
(void*           connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector;

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


static EIO_Status s_VT_Wait
(void*           connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    return eIO_Success;
}


static EIO_Status s_VT_Flush
(void*           connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector;

    if (fflush(xxx->fout) != 0)
        return eIO_Unknown;

    return eIO_Success;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFileConnector* xxx = (SFileConnector*) connector->handle;

    fclose(xxx->finp);
    fclose(xxx->fout);

    free(xxx->inp_file_name);
    free(xxx->out_file_name);
    free(xxx);
    free(connector);

    return eIO_Success;
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
    xxx->inp_file_name  =
        strcpy((char*) malloc(strlen(inp_file_name) + 1), inp_file_name);
    xxx->out_file_name =
        strcpy((char*) malloc(strlen(out_file_name) + 1), out_file_name);
    xxx->finp = xxx->fout = 0;

    memcpy(&xxx->attr, attr, sizeof(SFileConnAttr));

    /* initialize handle */
    ccc->handle = xxx;

    /* initialize virtual table */ 
    ccc->vtable.get_type   = s_VT_GetType;
    ccc->vtable.connect    = s_VT_Connect;
    ccc->vtable.wait       = s_VT_Wait;
    ccc->vtable.write      = s_VT_Write;
    ccc->vtable.flush      = s_VT_Flush;
    ccc->vtable.read       = s_VT_Read;
    ccc->vtable.close      = s_VT_Close;

    /* done */
    return ccc;
}
