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
    CPipe*              pipe;      // pipe handle; NULL if not connected yet
    string              cmd;       // program to execute
    vector<string>      args;      // program arguments
    CPipe::TCreateFlags flags;     // pipe create flags
    bool                is_open;   // true if pipe is open
    bool                own_pipe;  // true if pipe is owned
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
    ITERATE (vector<string>, arg, xxx->args) {
        if ( !cmd_line.empty() ) {
            cmd_line += " ";
        }
        bool quote = arg->find(' ') != NPOS ? true : false;
        if ( quote ) {
            cmd_line += '"';
        }
        cmd_line     += *arg;
        if ( quote ) {
            cmd_line += '"';
        }
    }
    return strdup(cmd_line.c_str());
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* /*timeout*/)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;

    _ASSERT(!xxx->is_open);

    if (!xxx->pipe) {
        return eIO_Unknown;
    }
    EIO_Status status = xxx->pipe->Open(xxx->cmd, xxx->args, xxx->flags);
    if (status == eIO_Success) {
        xxx->is_open = true;
    }
    return status;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    _ASSERT(event == eIO_Read  ||  event == eIO_Write);
    _ASSERT(xxx->is_open  &&  xxx->pipe);
    CPipe::TChildPollMask what = 0;
    if (event & eIO_Read)
        what |= CPipe::fDefault;
    if (event & eIO_Write)
        what |= CPipe::fStdIn;
    return xxx->pipe->Poll(what, timeout) ? eIO_Success : eIO_Unknown;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    _ASSERT(xxx->is_open  &&  xxx->pipe);
    if (xxx->pipe->SetTimeout(eIO_Write, timeout) != eIO_Success) {
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
    _ASSERT(xxx->is_open  &&  xxx->pipe);
    if (xxx->pipe->SetTimeout(eIO_Read, timeout) != eIO_Success) {
        return eIO_Unknown;
    }
    return xxx->pipe->Read(buf, size, n_read);
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    _ASSERT(xxx->is_open  &&  xxx->pipe);
    return xxx->pipe->Status(dir);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    _ASSERT(xxx->is_open  &&  xxx->pipe);
    xxx->is_open = false;
    xxx->pipe->SetTimeout(eIO_Close, timeout);
    return xxx->pipe->Close();
}


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    // Initialize virtual table
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, descr,    s_VT_Descr,   connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, wait,     s_VT_Wait,    connector);
    CONN_SET_METHOD(meta, write,    s_VT_Write,   connector);
    CONN_SET_METHOD(meta, flush,    0,            0);
    CONN_SET_METHOD(meta, read,     s_VT_Read,    connector);
    CONN_SET_METHOD(meta, status,   s_VT_Status,  connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    meta->default_timeout = kInfiniteTimeout;
}


static void s_Destroy(CONNECTOR connector)
{
    SPipeConnector* xxx = (SPipeConnector*) connector->handle;
    connector->handle = 0;

    if (xxx->own_pipe) {
        delete xxx->pipe;
    }
    xxx->pipe = 0;
    delete xxx;
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
 CPipe*                pipe,
 EOwnership            own_pipe)
{
    CONNECTOR       ccc;
    SPipeConnector* xxx;

    if (!(ccc = (SConnector*) malloc(sizeof(SConnector))))
        return 0;

    // Initialize internal data structures
    xxx           = new SPipeConnector;
    xxx->pipe     = pipe ? pipe                       : new CPipe;
    xxx->cmd      = cmd;
    xxx->args     = args;
    xxx->flags    = create_flags;
    xxx->is_open  = false;
    xxx->own_pipe = pipe ? own_pipe == eTakeOwnership : true;

    // Initialize connector data
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


END_NCBI_SCOPE
