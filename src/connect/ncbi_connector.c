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
 *   Implementation of functions of meta-connector.
 *   This is generally not a public interface.
 *
 */

#include "ncbi_priv.h"
#include <connect/ncbi_connector.h>

#define NCBI_USE_ERRCODE_X   Connect_Conn

/* Standard logging message
 */
#define METACONN_LOG(subcode, level, message)                   \
  CORE_LOGF_X(subcode, level,                                   \
              ("%s (connector \"%s\", error \"%s\")", message,  \
               meta->get_type                                   \
               ? meta->get_type(meta->c_get_type)               \
               : "UNDEF", IO_StatusStr(status)))


extern EIO_Status METACONN_Remove
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    assert(meta);

    if (connector) {
        CONNECTOR x_conn;
        
        for (x_conn = meta->list;  x_conn;  x_conn = x_conn->next) {
            if (x_conn == connector)
                break;
        }
        if (!x_conn) {
            EIO_Status status = eIO_Unknown;
            METACONN_LOG(34, eLOG_Error,
                         "[METACONN_Remove]  Connector is not in connection");
            return status;
        }
    }

    while (meta->list) {
        CONNECTOR victim = meta->list;
        meta->list       = victim->next;
        victim->meta     = 0;
        victim->next     = 0;
        if (victim->destroy)
            victim->destroy(victim);
        if (victim == connector)
            break;
    }
    return eIO_Success;
}


extern EIO_Status METACONN_Add
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    assert(meta  &&  connector);

    if (connector->next  ||  !connector->setup) {
        EIO_Status status = eIO_Unknown;
        METACONN_LOG(33, eLOG_Error,
                     "[METACONN_Add]  Connector is in use/uninitable");
        return status;
    }

    connector->setup(meta, connector);
    connector->meta = meta;
    connector->next = meta->list;
    meta->list = connector;
    return eIO_Success;
}
