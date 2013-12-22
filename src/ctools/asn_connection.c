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
 *   Build C Toolkit ASN.1 streams on top of CONN (connection).
 *
 */

#include <ctools/asn_connection.h>
#include "error_codes.h"


#define NCBI_USE_ERRCODE_X   Ctools_ASN


#ifdef __cplusplus
extern "C" {
    static Int2 LIBCALLBACK s_AsnRead (Pointer conn, CharPtr buf, Uint2 len);
    static Int2 LIBCALLBACK s_AsnWrite(Pointer conn, CharPtr buf, Uint2 len);
    static EIO_Status s_AsnClose(CONN conn, ECONN_Callback type, void* data);
}
#endif


static Int2 LIBCALLBACK s_AsnRead(Pointer conn, CharPtr buf, Uint2 len)
{
    size_t n_read;
    CONN_Read((CONN) conn, buf, len, &n_read, eIO_ReadPlain);
    return (Int2) n_read;
}


static Int2 LIBCALLBACK s_AsnWrite(Pointer conn, CharPtr buf, Uint2 len)
{
    size_t n_written;
    CONN_Write((CONN) conn, buf, len, &n_written, eIO_WritePersist);
    return (Int2) n_written;
}


struct SAsnConn_Cbdata {
    SCONN_Callback cb;
    AsnIoPtr       aip;
};


static EIO_Status s_AsnClose(CONN conn, TCONN_Callback type, void* data)
{
    EIO_Status status;
    struct SAsnConn_Cbdata* cbdata = (struct SAsnConn_Cbdata*) data;

    assert(type == eCONN_OnClose  &&  cbdata  &&  cbdata->aip);
    AsnIoFree(cbdata->aip, FALSE/*not a file - don't close*/);
    status = cbdata->cb.func
        ? cbdata->cb.func(conn, type, cbdata->cb.data)
        : eIO_Success;
    free(cbdata);
    return status;
}


static int/*bool*/ s_AsnSetCloseCb(CONN conn, AsnIoPtr aip)
{
    struct SAsnConn_Cbdata* cbdata =
        (struct SAsnConn_Cbdata*) malloc(sizeof(*cbdata));

    if ( cbdata ) {
        SCONN_Callback cb;
        cbdata->aip = aip;
        cb.func     = s_AsnClose;
        cb.data     = cbdata;
        CONN_SetCallback(conn, eCONN_OnClose, &cb, &cbdata->cb);
        return 1/*success*/;
    }

    CORE_LOG_X(1, eLOG_Error,
               "Cannot create close callback for ASN.1 CONN-based stream");
    return 0/*failure*/;
}


AsnIoPtr CreateAsnConn(CONN               conn,
                       EAsnConn_Direction direction,
                       EAsnConn_Format    fmt)
{
    /* NB: Do not use ASNIO_{TEXT|BIN}_{IN|OUT} because they subsume FILE */
    AsnIoPtr aip;
    int type;

    switch (fmt) {
    case eAsnConn_Binary:
        type = ASNIO_BIN;
        break;
    case eAsnConn_Text:
        type = ASNIO_TEXT;
        break;
    default:
        return 0;
    }

    switch (direction) {
    case eAsnConn_Input:
        aip = AsnIoNew(type | ASNIO_IN,  0, (void*) conn, s_AsnRead, 0);
        break;
    case eAsnConn_Output:
        aip = AsnIoNew(type | ASNIO_OUT, 0, (void*) conn, 0, s_AsnWrite);
        break;
    default:
        return 0;
    }

    if (aip  &&  !s_AsnSetCloseCb(conn, aip)) {
        AsnIoFree(aip, FALSE/*not a file*/);
        aip = 0;
    }
    return aip;
}


CONN CreateAsnConn_ServiceEx(const char*           service,
                             EAsnConn_Format       input_fmt,
                             AsnIoPtr*             input,
                             EAsnConn_Format       output_fmt,
                             AsnIoPtr*             output,
                             TSERV_Type            type,
                             const SConnNetInfo*   net_info,
                             const SSERVICE_Extra* params)
{
    CONN conn;
    CONNECTOR c = SERVICE_CreateConnectorEx(service, type, net_info, params);
    if (!c  ||  CONN_Create(c, &conn) != eIO_Success)
        return 0/*failed*/;
    assert(conn);

    if (input)
        *input  = CreateAsnConn(conn, eAsnConn_Input,  input_fmt);

    if (output)
        *output = CreateAsnConn(conn, eAsnConn_Output, output_fmt);

    return conn;
}


CONN CreateAsnConn_Service(const char*     service,
                           EAsnConn_Format input_fmt,
                           AsnIoPtr*       input,
                           EAsnConn_Format output_fmt,
                           AsnIoPtr*       output)
{
    return CreateAsnConn_ServiceEx(service, input_fmt, input,
                                   output_fmt, output, fSERV_Any, 0, 0);
}
