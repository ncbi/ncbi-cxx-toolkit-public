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
 * File Description:  CTLib cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

CTL_CursorCmd::CTL_CursorCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                             const string& cursor_name, const string& query,
                             unsigned int nof_params, unsigned int fetch_size) :
    CTL_Cmd(conn, cmd),
    impl::CCursorCmd(cursor_name, query, nof_params),
    m_FetchSize(fetch_size),
    m_Used(false)
{
}


CDB_Result* CTL_CursorCmd::Open()
{
    // need to close it first
    Close();

    if ( !m_Used ) {
        m_HasFailed = false;

        CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DECLARE,
                           const_cast<char*> (m_Name.c_str()), CS_NULLTERM,
                           const_cast<char*> (m_Query.c_str()), CS_NULLTERM,
                           CS_UNUSED),
                 "ct_cursor(DECLARE) failed", 122001);

        if (GetParams().NofParams() > 0) {
            // we do have the parameters
            // check if query is a select statement or a function call
            if (m_Query.find("select") != string::npos  ||
                m_Query.find("SELECT") != string::npos) {
                // this is a select
                m_HasFailed = !x_AssignParams(true);
                CHECK_DRIVER_ERROR( m_HasFailed, "cannot declare the params", 122003 );
            }
        }

        if (m_FetchSize > 1) {
            CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_ROWS, 0, CS_UNUSED,
                               0, CS_UNUSED, (CS_INT) m_FetchSize),
                     "ct_cursor(ROWS) failed", 122004);
        }
    }

    m_HasFailed = false;

    // open the cursor
    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_OPEN, 0, CS_UNUSED, 0, CS_UNUSED,
                       m_Used ? CS_RESTORE_OPEN : CS_UNUSED),
             "ct_cursor(open) failed", 122005);

    m_IsOpen = true;

    if (GetParams().NofParams() > 0) {
        // we do have the parameters
        m_HasFailed = !x_AssignParams(false);
        CHECK_DRIVER_ERROR( m_HasFailed, "cannot assign the params", 122003 );
    }

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122006);

    m_Used = true;

    // Process results ....
    for (;;) {
        CS_INT res_type;

        if (CheckSFBCP(ct_results(x_GetSybaseCmd(), &res_type),
                       "ct_result failed", 122013) == CS_END_RESULTS) {
            return NULL;
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE:
            // done with this command -- check the number of affected rows
            GetRowCount(&m_RowCount);
            continue;
        case CS_CMD_FAIL:
            // the command has failed -- check the number of affected rows
            GetRowCount(&m_RowCount);
            m_HasFailed = true;
            while (Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED) {
                continue;
            }
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 122016 );
        case CS_CURSOR_RESULT:
            SetResult(MakeCursorResult());
            break;
        default:
            continue;
        }

        return CTL_Cmd::CreateResult();
    }
}


bool CTL_CursorCmd::Update(const string& table_name, const string& upd_query)
{
    if ( !m_IsOpen ) {
        return false;
    }

    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_UPDATE,
                       const_cast<char*> (table_name.c_str()), CS_NULLTERM,
                       const_cast<char*> (upd_query.c_str()),  CS_NULLTERM,
                       CS_UNUSED),
             "ct_cursor(update) failed", 122030);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122032);

    // process the results
    return ProcessResults();
}

I_ITDescriptor* CTL_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || !HaveResult()) {
        return 0;
    }

    while ( static_cast<unsigned int>(GetResult().CurrentItemNo()) < item_num ) {
        if(!GetResult().SkipItem()) return 0;
    }

    I_ITDescriptor* desc= 0;
    if(static_cast<unsigned int>(GetResult().CurrentItemNo()) == item_num) {
        desc = GetResult().GetImageOrTextDescriptor();
    }
    else {
        auto_ptr<CTL_ITDescriptor> dsc(new CTL_ITDescriptor);

        bool rc = (Check(ct_data_info(x_GetSybaseCmd(), CS_GET, item_num+1, &dsc->m_Desc))
                   != CS_SUCCEED);

        CHECK_DRIVER_ERROR(
            rc,
            "ct_data_info failed",
            130010 );
        desc = dsc.release();
    }
    return desc;
}

bool CTL_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    return (desc) ? x_SendData(*desc, data, log_it) : false;
}

CDB_SendDataCmd* CTL_CursorCmd::SendDataCmd(unsigned int item_num, size_t size,
                        bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    return (desc) ? ConnSendDataCmd(*desc, size, log_it) : 0;
}

bool CTL_CursorCmd::Delete(const string& table_name)
{
    if ( !m_IsOpen ) {
        return false;
    }

    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DELETE,
                       const_cast<char*> (table_name.c_str()), CS_NULLTERM,
                       0, CS_UNUSED, CS_UNUSED),
             "ct_cursor(delete) failed", 122040);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122042);

    // process the results
    return ProcessResults();
}


int CTL_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CTL_CursorCmd::Close()
{
    if ( !m_IsOpen ) {
        return false;
    }

    DeleteResult();

    CheckSFB(ct_cursor(x_GetSybaseCmd(),
                         CS_CURSOR_CLOSE,
                         0,
                         CS_UNUSED,
                         0,
                         CS_UNUSED,
                         CS_UNUSED),
             "ct_cursor(close) failed", 122020);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122022);

    m_IsOpen = false;

    // Process results ...
    return ProcessResults();
}


void
CTL_CursorCmd::CloseForever(void)
{
    if (x_GetSybaseCmd()) {

        // ????
        DetachInterface();

        Close();

        if ( m_Used ) {
            // deallocate the cursor
            switch ( Check(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DEALLOC,
                               0, CS_UNUSED, 0, CS_UNUSED, CS_UNUSED)) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                // m_HasFailed = true;
                //throw CDB_ClientEx(eDiag_Fatal, 122050, "::~CTL_CursorCmd",
                //                   "ct_cursor(dealloc) failed");
#ifdef CS_BUSY
            case CS_BUSY:
                //throw CDB_ClientEx(eDiag_Error, 122051, "::~CTL_CursorCmd",
                //                   "the connection is busy");
#endif
                Check(ct_cmd_drop(x_GetSybaseCmd()));
                return;
            }

            // send this command
            switch ( Check(ct_send(x_GetSybaseCmd())) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                // m_HasFailed = true;
                // throw CDB_ClientEx(eDiag_Error, 122052, "::~CTL_CursorCmd",
                //                   "ct_send failed");
            case CS_CANCELED:
                // throw CDB_ClientEx(eDiag_Error, 122053, "::~CTL_CursorCmd",
                //                   "command was canceled");
#ifdef CS_BUSY
            case CS_BUSY:
#endif
            case CS_PENDING:
                // throw CDB_ClientEx(eDiag_Error, 122054, "::~CTL_CursorCmd",
                //                   "connection has another request pending");
                Check(ct_cmd_drop(x_GetSybaseCmd()));
                return;
            }

            // process the results
            try {
                ProcessResults();
            } catch(CDB_ClientEx const&)
            {
                // Just ignore ...
                _ASSERT(false);
            }

        }

        DropSybaseCmd();
    }
}


CDB_Result*
CTL_CursorCmd::CreateResult(impl::CResult& result)
{
    return Create_Result(result);
}


CTL_CursorCmd::~CTL_CursorCmd()
{
    try {
        DetachInterface();

        DropCmd(*this);

        CloseForever();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CTL_CursorCmd::x_AssignParams(bool declare_only)
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;
    param_fmt.status  = CS_INPUTVALUE;

    for (unsigned int i = 0;  i < GetParams().NofParams();  i++) {
        if(GetParams().GetParamStatus(i) == 0) continue;

        CDB_Object&   param = *GetParams().GetParam(i);
        const string& param_name = GetParams().GetParamName(i);
        CS_SMALLINT   indicator = (!declare_only  &&  param.IsNULL()) ? -1 : 0;

        if ( !AssignCmdParam(param, param_name, param_fmt, indicator, declare_only) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2006/11/28 20:08:09  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.26  2006/08/10 15:19:27  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.25  2006/08/02 15:16:27  ssikorsk
 * Revamp code to use CheckSFB and CheckSFBCP;
 * Revamp code to compile with FreeTDS ctlib implementation;
 *
 * Revision 1.24  2006/07/19 14:11:02  ssikorsk
 * Refactoring of CursorCmd.
 *
 * Revision 1.23  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.22  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.21  2006/06/19 19:11:44  ssikorsk
 * Replace C_ITDescriptorGuard with auto_ptr<I_ITDescriptor>
 *
 * Revision 1.20  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.19  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.18  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.17  2006/06/05 18:10:06  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.16  2006/05/03 15:10:36  ssikorsk
 * Implemented classs CTL_Cmd and CCTLExceptions;
 * Surrounded each native ctlib call with Check;
 *
 * Revision 1.15  2006/03/06 19:51:38  ssikorsk
 *
 * Added method Close/CloseForever to all context/command-aware classes.
 * Use getters to access Sybase's context and command handles.
 *
 * Revision 1.14  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.13  2005/10/31 12:29:14  ssikorsk
 * Get rid of warnings.
 *
 * Revision 1.12  2005/09/19 14:19:02  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.11  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.10  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.9  2004/05/17 21:12:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2003/06/05 16:00:31  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.7  2003/05/16 20:24:24  soussov
 * adds code to skip parameters if it was not set
 *
 * Revision 1.6  2002/09/16 16:34:16  soussov
 * add try catch when canceling in Release method
 *
 * Revision 1.5  2002/05/16 21:35:22  soussov
 * fixes the memory leak in text/image processing
 *
 * Revision 1.4  2002/03/26 15:34:38  soussov
 * new image/text operations added
 *
 * Revision 1.3  2001/11/06 17:59:55  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/09/25 16:29:57  soussov
 * fixed typo in CTL_CursorCmd::x_AssignParams
 *
 * Revision 1.1  2001/09/21 23:40:02  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
