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
 * Author:  Denis Vakatov, Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:
 *   Implement CONNECTOR for a pipe interprocess communication
 *   (based on the NCBI CPipe).
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_pipe_connector.hpp>


USING_NCBI_SCOPE;


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

// All internal data necessary to perform the (re)connect and i/o
typedef struct {
    CPipe*              pipe;        // pipe handle; NULL if not connected yet
    string              cmd;         // program to execute
    vector<string>      args;        // program arguments
    CPipe::TCreateFlags flags;       // pipe create flags
    bool                is_open;     // true if pipe is open
    bool                is_own_pipe; // true if pipe was created in constructor
} SPipeConnector;


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/


extern "C" {


static const char* s_VT_GetType
(CONNECTOR /*connector*/)
{
    return "PIPE";
}


static char* s_VT_Descr(CONNECTOR connector)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    string cmd_line(xxx->cmd);
    ITERATE (vector<string>, iter, xxx->args) {
        if ( !cmd_line.empty() ) {
            cmd_line += " ";
        }
        cmd_line += *iter;
    }
    size_t len = cmd_line.length() + 1/*EOL*/;
    char* buf = (char*) malloc(len);
    if (buf) {
        strcpy(buf, cmd_line.c_str());
    }
    return buf;
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* /*timeout*/)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;

    if (!xxx->pipe) {
        return eIO_Unknown;
    }
    // If connected, close previous session first
    if (xxx->is_open) {
        if (xxx->pipe->Close() != eIO_Success) {
            return eIO_Unknown;
        }
        xxx->is_open = false;
    }
    // Open new connection
    EIO_Status status = xxx->pipe->Open(xxx->cmd, xxx->args, xxx->flags);
    if (status == eIO_Success) {
        xxx->is_open = true;
    }
    return status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    return xxx->pipe ? xxx->pipe->Status(dir) : eIO_Success;
}


static EIO_Status s_VT_Wait
(CONNECTOR       /*connector*/,
 EIO_Event       /*event*/,
 const STimeout* /*timeout*/)
{
    /* NB: to be implemented */
    return eIO_Success;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;

    if (!xxx->is_open) {
        return eIO_Closed;
    }
    if (!xxx->pipe  ||  xxx->pipe->SetTimeout(eIO_Write, timeout) != 
        eIO_Success) {
        return eIO_Unknown;
    }
    return xxx->pipe->Write(buf, size, n_written);
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;

    if (!xxx->is_open) {
        return eIO_Closed;
    }
    if (!xxx->pipe  ||  xxx->pipe->SetTimeout(eIO_Read, timeout) !=
        eIO_Success) {
        return eIO_Unknown;
    }
    return xxx->pipe->Read(buf, size, n_read);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    EIO_Status status = eIO_Success;
    if (xxx->is_open) {
        xxx->pipe->SetTimeout(eIO_Close, timeout);
        int exitcode = 0;
        status = xxx->pipe->Close(&exitcode);
        xxx->is_open = false;
    }
    return status == eIO_Closed ? eIO_Success : status;
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


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    // Initialize virtual table
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, descr,      s_VT_Descr,     connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      0,              0);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
    meta->default_timeout = 0; // infinite
}


static void s_Destroy(CONNECTOR connector)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    if (xxx) {
        if (xxx->is_own_pipe) {
            delete xxx->pipe;
        }
        delete xxx;
    }
    connector->handle = 0;
    free(connector);
}


} /* extern "C" */


BEGIN_NCBI_SCOPE


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR PIPE_CreateConnector
(const string&         cmd,
 const vector<string>& args,
 CPipe::TCreateFlags   create_flags,
 CPipe*                pipe)
{
    CONNECTOR       ccc = (SConnector*) malloc(sizeof(SConnector));
    SPipeConnector* xxx =               new SPipeConnector();

    // Initialize internal data structures
    xxx->pipe        = pipe ? pipe  : new CPipe();
    xxx->cmd         = cmd;
    xxx->args        = args;
    xxx->flags       = create_flags;
    xxx->is_open     = false;
    xxx->is_own_pipe = pipe ? false : true;

    // Initialize connector data
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


END_NCBI_SCOPE
