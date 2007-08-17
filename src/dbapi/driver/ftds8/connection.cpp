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
 * Author:  Vladimir Soussov
 *
 * File Description:  TDS connection
 *
 */

#include <ncbi_pch.hpp>

#include "ftds8_utils.hpp"

#include <dbapi/driver/ftds/interfaces.hpp>
#include <string.h>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#endif


BEGIN_NCBI_SCOPE


CTDS_Connection::CTDS_Connection(CTDSContext& cntx,
                                 DBPROCESS* con,
                                 bool reusable,
                                 const string& pool_name) :
    impl::CConnection(cntx, false, reusable, pool_name),
    m_Link(con)
{
    dbsetuserdata(GetDBLibConnection(), (BYTE*) this);
    CheckFunctCall();
}


bool CTDS_Connection::IsAlive()
{
    return DBDEAD(GetDBLibConnection()) == FALSE;
}


CDB_LangCmd* CTDS_Connection::LangCmd(const string& lang_query,
                                      unsigned int nof_parms)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    SetExtraMsg(extra_msg);

    CTDS_LangCmd* lcmd = new CTDS_LangCmd(*this, GetDBLibConnection(), lang_query, nof_parms);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CTDS_Connection::RPC(const string& rpc_name, unsigned int nof_args)
{
    string extra_msg = "RPC Command: " + rpc_name;
    SetExtraMsg(extra_msg);

    CTDS_RPCCmd* rcmd = new CTDS_RPCCmd(*this, GetDBLibConnection(), rpc_name, nof_args);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CTDS_Connection::BCPIn(const string& table_name,
                                     unsigned int nof_cols)
{
    if (!IsBCPable()) {
        DATABASE_DRIVER_ERROR( "No bcp on this connection." + GetDbgInfo(),  210003 );
    }

    string extra_msg = "BCP Table: " + table_name;
    SetExtraMsg(extra_msg);

    CTDS_BCPInCmd* bcmd = new CTDS_BCPInCmd(*this, GetDBLibConnection(), table_name, nof_cols);
    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CTDS_Connection::Cursor(const string& cursor_name,
                                       const string& query,
                                       unsigned int nof_params,
                                       unsigned int)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExtraMsg(extra_msg);

    CTDS_CursorCmd* ccmd = new CTDS_CursorCmd(*this, GetDBLibConnection(), cursor_name,
                                              query, nof_params);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CTDS_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                              size_t data_size, bool log_it)
{
    if (data_size < 1) {
        DATABASE_DRIVER_ERROR( "Wrong (zero) data size." + GetDbgInfo(), 210092 );
    }

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CDBL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor(dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0) return false;
    }


    auto_ptr<I_ITDescriptor> d_guard(p_desc);

    CTDS_ITDescriptor& desc = p_desc? dynamic_cast<CTDS_ITDescriptor&> (*p_desc) :
        dynamic_cast<CTDS_ITDescriptor&> (descr_in);

    if (Check(dbwritetext(GetDBLibConnection(),
                    (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE,
                    static_cast<int>(data_size), 0)) != SUCCEED ||
        Check(dbsqlok(GetDBLibConnection())) != SUCCEED ||
        //        dbresults(GetDBLibConnection()) == FAIL) {
        x_Results(GetDBLibConnection()) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed." + GetDbgInfo(), 210093 );
    }

    CTDS_SendDataCmd* sd_cmd = new CTDS_SendDataCmd(*this, GetDBLibConnection(), data_size);
    return Create_SendDataCmd(*sd_cmd);
}


bool CTDS_Connection::SendData(I_ITDescriptor& desc,
                               CDB_Image& img, bool log_it){
    return x_SendData(desc, dynamic_cast<CDB_Stream&> (img), log_it);
}


bool CTDS_Connection::SendData(I_ITDescriptor& desc,
                               CDB_Text&  txt, bool log_it) {
    return x_SendData(desc, dynamic_cast<CDB_Stream&> (txt), log_it);
}


bool CTDS_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    // cancel all pending commands
    if (Check(dbcancel(GetDBLibConnection())) != CS_SUCCEED)
        return false;

    // check the connection status
    return DBDEAD(GetDBLibConnection()) == FALSE;
}


I_DriverContext::TConnectionMode CTDS_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( IsBCPable() ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( HasSecureLogin() ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}


CTDS_Connection::~CTDS_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CTDS_Connection::Abort()
{
    int fdr= DBIORDESC(GetDBLibConnection());
    int fdw= DBIOWDESC(GetDBLibConnection());
    if(fdr >= 0) {
        close(fdr);
    }
    if(fdw != fdr && fdw >= 0) {
        close(fdw);
    }
    return (fdr >= 0 || fdw >= 0);
}

bool CTDS_Connection::Close(void)
{
    if (GetDBLibConnection()) {
        Refresh();

        // Clean user data ...
        dbsetuserdata(GetDBLibConnection(), (BYTE*) NULL);

        dbclose(GetDBLibConnection());
        CheckFunctCall();

        m_Link = NULL;

        return true;
    }

    return false;
}

bool CTDS_Connection::x_SendData(I_ITDescriptor& descr_in,
                                 CDB_Stream& stream, bool log_it)
{
    size_t size = stream.Size();
    if (size < 1)
        return false;

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CDBL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor(dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0) return false;
    }


    auto_ptr<I_ITDescriptor> d_guard(p_desc);

    CTDS_ITDescriptor& desc = p_desc? dynamic_cast<CTDS_ITDescriptor&> (*p_desc) :
        dynamic_cast<CTDS_ITDescriptor&> (descr_in);
    // CTDS_ITDescriptor& desc = dynamic_cast<CTDS_ITDescriptor&> (descr_in);
    char buff[1800]; // maximal page size

    if (size <= sizeof(buff)) { // we could write a blob in one chunk
        size_t s = stream.Read(buff, sizeof(buff));
        if (Check(dbwritetext(GetDBLibConnection(), (char*) desc.m_ObjName.c_str(),
                        desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                        DBTXPLEN,
                        desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                        log_it ? TRUE : FALSE, (DBINT) s, (BYTE*) buff))
            != SUCCEED) {
            DATABASE_DRIVER_ERROR( "dbwritetext failed." + GetDbgInfo(), 210030 );
        }
        return true;
    }

    // write it in chunks
    if (Check(dbwritetext(GetDBLibConnection(), (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE, (DBINT) size, 0)) != SUCCEED ||
        Check(dbsqlok(GetDBLibConnection())) != SUCCEED ||
        //        dbresults(GetDBLibConnection()) == FAIL) {
        x_Results(GetDBLibConnection()) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed." + GetDbgInfo(), 210031 );
    }

    while (size > 0) {
        size_t s = stream.Read(buff, sizeof(buff));
        if (s < 1) {
            Check(dbcancel(GetDBLibConnection()));
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted." + GetDbgInfo(), 210032 );
        }
        if (Check(dbmoretext(GetDBLibConnection(), (DBINT) s, (BYTE*) buff)) != SUCCEED) {
            Check(dbcancel(GetDBLibConnection()));
            DATABASE_DRIVER_ERROR( "dbmoretext failed." + GetDbgInfo(), 210033 );
        }
        size -= s;
    }

    if (Check(dbsqlok(GetDBLibConnection())) != SUCCEED || x_Results(GetDBLibConnection()) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbsqlok/dbresults failed." + GetDbgInfo(), 110034 );
    }

    return true;
}

I_ITDescriptor* CTDS_Connection::x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in)
{
    string q= "set rowcount 1\nupdate ";
    q+= descr_in.TableName();
    q+= " set ";
    q+= descr_in.ColumnName();
    q+= "='0x0' where ";
    q+= descr_in.SearchConditions();
    q+= " \nselect ";
    q+= descr_in.ColumnName();
    q+= " from ";
    q+= descr_in.TableName();
    q+= " where ";
    q+= descr_in.SearchConditions();
    q+= " \nset rowcount 0";

    CDB_LangCmd* lcmd= LangCmd(q, 0);
    if(!lcmd->Send()) {
        DATABASE_DRIVER_ERROR( "Cannot send the language command." + GetDbgInfo(), 210035 );
    }

    CDB_Result* res;
    I_ITDescriptor* descr= 0;
    bool i;

    while(lcmd->HasMoreResults()) {
        res= lcmd->Result();
        if(res == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == 0)) {
            EDB_Type tt= res->ItemDataType(0);
            if(tt == eDB_Text || tt == eDB_Image) {
                while(res->Fetch()) {
                    res->ReadItem(&i, 1);

                    descr= new CTDS_ITDescriptor(*this, GetDBLibConnection(), descr_in);
                    // descr= res->GetImageOrTextDescriptor();
                    if(descr) break;
                }
            }
        }
        delete res;
    }
    delete lcmd;

    return descr;
}


RETCODE CTDS_Connection::x_Results(DBPROCESS* pLink)
{
    unsigned int x_Status= 0x1;
    CDB_Result* dbres;
    impl::CResult* res= 0;

    while ((x_Status & 0x1) != 0) {
        RETCODE rc = Check(dbresults(pLink));

        switch (rc) {
        case SUCCEED:
            if (DBCMDROW(pLink) == SUCCEED) { // we may get rows in this result
                if(!GetResultProcessor()) {
                    for(;;) {
                        switch(Check(dbnextrow(pLink))) {
                        case NO_MORE_ROWS:
                        case FAIL:
                        case BUF_FULL: break;
                        default: continue;
                        }
                        break;
                    }
                    continue;
                }
                res = new CTDS_RowResult(*this, pLink, &x_Status);
                if(res) {
                    dbres= Create_Result(*res);
                    GetResultProcessor()->ProcessResult(*dbres);
                    delete dbres;
                    delete res;
                }
                if ((x_Status & 0x10) != 0) { // we do have a compute result
                    res = new CTDS_ComputeResult(*this, pLink, &x_Status);
                    if(res) {
                        dbres= Create_Result(*res);
                        GetResultProcessor()->ProcessResult(*dbres);
                        delete dbres;
                        delete res;
                    }
                }
            }
            continue;

        case NO_MORE_RESULTS:
            x_Status = 2;
            break;
        default:
            return FAIL;
        }
        break;
    }

    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (GetResultProcessor() && x_Status == 2) {
        x_Status = 4;
        int n = Check(dbnumrets(pLink));
        if (n > 0) {
            res = new CTDS_ParamResult(*this, pLink, n);
            if(res) {
                dbres= Create_Result(*res);
                GetResultProcessor()->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
        }
    }

    if (GetResultProcessor() && x_Status == 4) {
        if (Check(dbhasretstat(pLink))) {
            res = new CTDS_StatusResult(*this, pLink);
            if(res) {
                dbres= Create_Result(*res);
                GetResultProcessor()->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
        }
    }
    return SUCCEED;
}

void CTDS_Connection::TDS_SetTimeout(void)
{
    GetDBLibConnection()->tds_socket->timeout= (TDS_INT)(Context()->GetTimeout());
}

RETCODE CTDS_Connection::CheckDead(RETCODE rc)
{
    if (rc == FAIL) {
        if (DBDEAD(GetDBLibConnection()) == TRUE) {
            CDB_ClientEx ex(DIAG_COMPILE_INFO,
                            0,
                            "Database connection is closed",
                            eDiag_Error,
                            220000);

            GetFTDS8ExceptionStorage().Accept(ex);
        } else {
            CDB_ClientEx ex(DIAG_COMPILE_INFO,
                            0,
                            "FreeTDS/dblib function call failed",
                            eDiag_Error,
                            220001);

            GetFTDS8ExceptionStorage().Accept(ex);
        }
    }

    CheckFunctCall();

    return rc;
}


void CTDS_Connection::CheckFunctCall(void)
{
    GetFTDS8ExceptionStorage().Handle(GetMsgHandlers());
}


void CTDS_Connection::CheckFunctCall(const string& extra_msg)
{
    GetFTDS8ExceptionStorage().Handle(GetMsgHandlers(), extra_msg);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_SendDataCmd::
//


CTDS_SendDataCmd::CTDS_SendDataCmd(CTDS_Connection& conn,
                                   DBPROCESS* cmd,
                                   size_t nof_bytes) :
    CDBL_Cmd(conn, cmd),
    impl::CSendDataCmd(conn, nof_bytes)
{
}


size_t CTDS_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    if (!pChunk  ||  !nof_bytes) {
        DATABASE_DRIVER_ERROR( "Wrong (zero) arguments." + GetDbgInfo(), 290000 );
    }
    if (!GetBytes2Go())
        return 0;

    if (nof_bytes > GetBytes2Go())
        nof_bytes = GetBytes2Go();

    if (Check(dbmoretext(GetCmd(), static_cast<DBINT>(nof_bytes), (BYTE*) pChunk))
        != SUCCEED) {
        Check(dbcancel(GetCmd()));
        DATABASE_DRIVER_ERROR( "dbmoretext failed." + GetDbgInfo(), 290001 );
    }

    SetBytes2Go(GetBytes2Go() - nof_bytes);

    if (GetBytes2Go() <= 0) {
        if (Check(dbsqlok(GetCmd())) != SUCCEED || GetConnection().x_Results(GetCmd()) == FAIL) {
            DATABASE_DRIVER_ERROR( "dbsqlok/results failed." + GetDbgInfo(), 290002 );
        }
    }

    return nof_bytes;
}


bool CTDS_SendDataCmd::Cancel(void)
{
    if (GetBytes2Go() > 0) {
        Check(dbcancel(GetCmd()));
        SetBytes2Go(0);
        return true;
    }

    return false;
}


CTDS_SendDataCmd::~CTDS_SendDataCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


END_NCBI_SCOPE


