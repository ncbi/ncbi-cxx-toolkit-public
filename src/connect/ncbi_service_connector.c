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
 *   Implementation of CONNECTOR for a named service
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/10/07 22:14:07  lavr
 * Initial revision, placeholder mostly
 *
 * ==========================================================================
 */

#include <connect/ncbi_service.h>
#include <connect/ncbi_service_connector.h>
#include <stdlib.h>

typedef struct SServiceConnectorTag {
    SERV_ITER           iter;
    EClientMode         mode;
    SConnNetInfo        *info;
} SServiceConnector;

/*
 * INTERNALS: Implementation of virtual functions
 */

#ifdef __cplusplus
extern "C" {
    static const char* s_VT_GetType(void* connector);
    static EIO_Status  s_VT_Connect(void*           connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Wait(void*           connector,
                                 EIO_Event       event,
                                 const STimeout* timeout);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status  s_VT_WaitAsync(void*                   connector,
                                      FConnectorAsyncHandler  func,
                                      SConnectorAsyncHandler* data);
#  endif
    static EIO_Status  s_VT_Write(void*           connector,
                                  const void*     buf,
                                  size_t          size,
                                  size_t*         n_written,
                                  const STimeout* timeout);
    static EIO_Status  s_VT_Flush(void*           connector,
                                  const STimeout* timeout);
    static EIO_Status  s_VT_Read(void*           connector,
                                 void*           buf,
                                 size_t          size,
                                 size_t*         n_read,
                                 const STimeout* timeout);
    static EIO_Status  s_VT_Close(CONNECTOR       connector,
                                  const STimeout* timeout);
} /* extern "C" */
#endif /* __cplusplus */



static const char* s_VT_GetType
(void* connector)
{
    return "SERVICE";
}


static EIO_Status s_VT_Connect
(void*           connector,
 const STimeout* timeout)
{
    return eIO_NotSupported;
}


static EIO_Status s_VT_Wait
(void*           connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    return eIO_Success;
}


#ifdef IMPLEMENTED__CONN_WaitAsync
static EIO_Status s_VT_WaitAsync
(void*                   connector,
 FConnectorAsyncHandler  func,
 SConnectorAsyncHandler* data)
{
    return eIO_NotSupported;
}
#endif


static EIO_Status s_VT_Write
(void*           connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    return eIO_NotSupported;
}


static EIO_Status s_VT_Flush
(void*           connector,
 const STimeout* timeout)
{
    return eIO_NotSupported;
}


static EIO_Status s_VT_Read
(void*           connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    return eIO_NotSupported;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;

    ConnNetInfo_Destroy(&uuu->info);
    SERV_Close(uuu->iter);
    free(uuu);
    free(connector);
    return eIO_Success;
}

/*
 * EXTERNALS: Connector constructors
 */

extern CONNECTOR SERVICE_CreateConnector
(const char*          service)
{
    return SERVICE_CreateConnectorEx(service, eClientModeUnspecified, 0);
}

extern CONNECTOR SERVICE_CreateConnectorEx
(const char*          service,
 EClientMode          mode,
 const SConnNetInfo   *info)
{
    CONNECTOR          ccc;
    SServiceConnector* xxx;

    if (!service)
        return 0;
    
    ccc = (SConnector*) malloc(sizeof(SConnector));
    xxx = (SServiceConnector*) malloc(sizeof(SServiceConnector));
    xxx->iter = 0;
    xxx->mode = mode;
    xxx->info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create(service);
    
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
