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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *     Build C Toolkit ASN streams on top of CONN (connection).
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/06/28 21:58:23  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_service_connector.h>
#include <ctools/asn_connection.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
    static Int2 LIBCALLBACK s_AsnRead(Pointer p, CharPtr buff, Uint2 len);
    static Int2 LIBCALLBACK s_AsnWrite(Pointer p, CharPtr buff, Uint2 len);
}
#endif


static Int2 LIBCALLBACK s_AsnRead(Pointer p, CharPtr buff, Uint2 len)
{
    size_t n_read = 0;
    CONN_Read((CONN) p, buff, len, &n_read, eIO_Plain);
    return (Int2) n_read;
}


static Int2 LIBCALLBACK s_AsnWrite(Pointer p, CharPtr buff, Uint2 len)
{
    size_t n_written = 0;
    CONN_Write((CONN) p, buff, len, &n_written);
    return (Int2) n_written;
}


struct SAsnConn_Cbdata {
    SCONN_Callback cb;
    AsnIoPtr       ptr;
};


static void s_CloseAsnConn(CONN conn, ECONN_Callback type, void* data)
{
    struct SAsnConn_Cbdata* cbdata = (struct SAsnConn_Cbdata*) data;
    
    assert(type == eCONN_OnClose);
    if (cbdata) {
        assert(cbdata->ptr);
        AsnIoFree(cbdata->ptr, 0/*not a file - don't close*/);
        if (cbdata->cb.func)
            (*cbdata->cb.func)(conn, type, cbdata->cb.data);
        free(cbdata);
    }
}


static void s_SetAsnConn_CloseCb(CONN conn, AsnIoPtr ptr)
{
    struct SAsnConn_Cbdata* cbdata = malloc(sizeof(*cbdata));
    SCONN_Callback cb = { s_CloseAsnConn, cbdata };

    assert(ptr);
    if (cbdata)
        cbdata->ptr = ptr;
    CONN_SetCallback(conn, eCONN_OnClose, &cb, cbdata ? &cbdata->cb : 0);
}


AsnIoPtr CreateAsnConn(CONN               conn,
                       EAsnConn_Direction direction,
                       EAsnConn_Format    fmt)
{
    AsnIoPtr ptr;
    int flags;

    switch (fmt) {
    case eAsnConn_Binary:
        flags = ASNIO_BIN;
        break;
    case eAsnConn_Text:
        flags = ASNIO_TEXT;
        break;
    default:
        return 0;
    }

    switch (direction) {
    case eAsnConn_Input:
        ptr = AsnIoNew(flags | ASNIO_IN, (FILE*) 0,
                       (void*) conn, s_AsnRead, (IoFuncType) 0);
        break;
    case eAsnConn_Output:
        ptr = AsnIoNew(flags | ASNIO_OUT, (FILE*) 0,
                       (void*) conn, (IoFuncType) 0, s_AsnWrite);
        break;
    default:
        return 0;
    }

    if (ptr)
        s_SetAsnConn_CloseCb(conn, ptr);

    return ptr;
}


CONN CreateAsnConn_ServiceEx(const char*         service_name,
                             EAsnConn_Format     input_fmt,
                             AsnIoPtr*           input,
                             EAsnConn_Format     output_fmt,
                             AsnIoPtr*           output,
                             TSERV_Type          service_type,
                             const SConnNetInfo* info)
{
    CONN conn;
    CONNECTOR c = SERVICE_CreateConnectorEx(service_name, service_type, info);
    if (!c || CONN_Create(c, &conn) != eIO_Success)
        return 0/*failed*/;
    assert(conn);

    if (input) {
        AsnIoPtr ptr = CreateAsnConn(conn, ASNIO_IN, input_fmt);
        if (ptr)
            s_SetAsnConn_CloseCb(conn, ptr);
        *input = ptr;
    }

    if (output) {
        AsnIoPtr ptr = CreateAsnConn(conn, ASNIO_OUT, output_fmt);
        if (ptr)
            s_SetAsnConn_CloseCb(conn, ptr);
        *output = ptr;
    }

    return conn;
}


CONN CreateAsnConn_Service(const char*     service_name,
                           EAsnConn_Format input_fmt,
                           AsnIoPtr*       input,
                           EAsnConn_Format output_fmt,
                           AsnIoPtr*       output)
{
    return CreateAsnConn_ServiceEx(service_name, input_fmt,
                                   input, output_fmt, output,
                                   fSERV_Any, 0);
}
