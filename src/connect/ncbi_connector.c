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
 *   This is generally not for the public use.
 *   Implementation of functions of meta-connector.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.6  2002/04/26 16:31:06  lavr
 * Minor style changes in call-by-pointer functions
 *
 * Revision 6.5  2002/03/22 22:17:29  lavr
 * No <stdlib.h> needed in here, removed
 *
 * Revision 6.4  2001/03/02 20:07:56  lavr
 * Typo fixed
 *
 * Revision 6.3  2001/01/25 16:57:08  lavr
 * METACONN_Remove revoked call to free() with connector:
 * connector's DESTROY method is now (back) responsible to call free().
 *
 * Revision 6.2  2001/01/12 23:51:38  lavr
 * Message logging modified for use LOG facility only
 *
 * Revision 6.1  2000/12/29 17:49:29  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "ncbi_priv.h"
#include <connect/ncbi_connector.h>


/* Standard logging message
 */
#define METACONN_LOG(level, descr)                      \
  CORE_LOGF(level,                                      \
            ("%s (connector \"%s\", error \"%s\")",     \
            descr, (*meta->get_type)(meta->c_get_type), \
            IO_StatusStr(status)))


extern EIO_Status METACONN_Remove
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    if (connector) {
        CONNECTOR x_conn;
        
        for (x_conn = meta->list; x_conn; x_conn = x_conn->next)
            if (x_conn == connector)
                break;
        if (!x_conn) {
            EIO_Status status = eIO_Unknown;
            METACONN_LOG(eLOG_Error,
                         "[METACONN_Remove]  Connector is not in connection");
            return status;
        }
    }

    while (meta->list) {
        CONNECTOR victim = meta->list;

        meta->list = victim->next;
        victim->meta = 0;
        victim->next = 0;
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
    assert(connector && meta);

    if (connector->next || !connector->setup) {
        EIO_Status status = eIO_Unknown;
        METACONN_LOG(eLOG_Error,
                     "[METACONN_Add]  Input connector is in use/uninitable");
        return status;
    }

    connector->setup(meta, connector);
    connector->meta = meta;
    connector->next = meta->list;
    meta->list = connector;

    return eIO_Success;
}
