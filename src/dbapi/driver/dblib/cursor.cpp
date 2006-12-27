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
 * File Description:  DBLib cursor command
 *
 */
#include <ncbi_pch.hpp>

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_CursorCmd::
//

CDBL_CursorCmd::CDBL_CursorCmd(CDBL_Connection* conn, DBPROCESS* cmd,
                               const string& cursor_name, const string& query,
                               unsigned int nof_params) :
    CDBL_Cmd( conn, cmd ),
    impl::CCursorCmd(cursor_name, query, nof_params),
    m_LCmd(0),
    m_Res(0)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExecCntxInfo(extra_msg);
}


static bool for_update_of(const string& q)
{
    if((q.find("update") == string::npos) &&
       (q.find("UPDATE") == string::npos))
        return false;

    if((q.find("for update") != string::npos) ||
       (q.find("FOR UPDATE") != string::npos))
        return true;

    // TODO: add more logic here to find "for update" clause
    return false;
}

CDB_Result* CDBL_CursorCmd::Open()
{
    const bool connected_to_MSSQLServer = GetConnection().GetCDriverContext().ConnectedToMSSQLServer();

    // We need to close it first
    Close();

    m_HasFailed = false;

    // declare the cursor
    m_HasFailed = !x_AssignParams();
    CHECK_DRIVER_ERROR(
        m_HasFailed,
        "cannot assign params",
        222003 );


    m_LCmd = 0;

    string buff;
    if ( connected_to_MSSQLServer ) {
        string cur_feat;

        if(for_update_of(GetQuery())) {
            cur_feat = " cursor FORWARD_ONLY SCROLL_LOCKS for ";
        } else {
            cur_feat = " cursor FORWARD_ONLY for ";
        }

        buff = "declare " + m_Name + cur_feat + GetQuery();
    } else {
        // Sybase ...

        buff = "declare " + m_Name + " cursor for " + GetQuery();
    }

    try {
        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );

        cmd->Send();
        cmd->DumpResults();
    } catch ( const CDB_Exception& e) {
        DATABASE_DRIVER_ERROR_EX( e, "failed to declare cursor", 222001 );
    }

    m_IsDeclared = true;

    // open the cursor
    m_LCmd = 0;
    buff = "open " + m_Name;

    try {
        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );

        cmd->Send();
        cmd->DumpResults();
    } catch ( const CDB_Exception& e) {
        DATABASE_DRIVER_ERROR_EX( e, "failed to open cursor", 222002 );
    }

    m_IsOpen = true;

    m_LCmd = 0;
    buff = "fetch " + m_Name;

    m_LCmd = GetConnection().LangCmd(buff);

    SetResultSet( new CDBL_CursorResult(GetConnection(), m_LCmd) );

    return Create_Result(*GetResultSet());
}


bool CDBL_CursorCmd::Update(const string&, const string& upd_query)
{
    if (!m_IsOpen)
        return false;

    try {
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
            // Not in ftds8 ...
//             if (r.get()) {
//                 while (r->Fetch())
//                     continue;
//             }
        }

        string buff = upd_query + " where current of " + m_Name;
        const auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
#if 0
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
            if (r.get()) {
                while (r->Fetch())
                    continue;
            }
        }
#endif
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "update failed", 222004 );
    }

    return true;
}

I_ITDescriptor* CDBL_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || (GetResultSet() == 0)) {
        return 0;
    }
    while ( static_cast<unsigned int>(GetResultSet()->CurrentItemNo()) < item_num ) {
        if(!GetResultSet()->SkipItem()) return 0;
    }

    I_ITDescriptor* desc= new CDBL_ITDescriptor(GetConnection(), GetCmd(), item_num+1);
    return desc;
}

bool CDBL_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                                     bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    if(desc) {
#ifdef FTDS_IN_USE
        // For some starnge reason this code does not work with Sybase dblib ...
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
            // Not in ftds8 ...
//             if (r.get()) {
//                 while (r->Fetch())
//                     continue;
//             }
        }

        // m_Res may not be deleted here. Checked.
//         delete m_Res;
//         m_Res = NULL;
#endif
        return GetConnection().x_SendData(*desc, data, log_it);
    }
    return false;
}

CDB_SendDataCmd* CDBL_CursorCmd::SendDataCmd(unsigned int item_num, size_t size,
                                             bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    if(desc) {
        m_LCmd->DumpResults();
#if 0
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }
#endif

        return GetConnection().SendDataCmd(*desc, size, log_it);
    }
    return 0;
}

bool CDBL_CursorCmd::Delete(const string& table_name)
{
    if (!m_IsOpen)
        return false;

    try {
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
            // Not in ftds8 ...
//             if (r.get()) {
//
//                 while (r->Fetch())
//                     continue;
//             }
        }

        string buff = "delete " + table_name + " where current of " + m_Name;
        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
#if 0
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
            if (r.get()) {
                while (r->Fetch())
                    continue;
            }
        }
#endif
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "update failed", 222004 );
    }

    return true;
}


int CDBL_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CDBL_CursorCmd::Close()
{
    if (!m_IsOpen)
        return false;

    ClearResultSet();

    if (m_LCmd) {
        delete m_LCmd;
        m_LCmd = NULL;
    }

    if (m_IsOpen) {
        string buff = "close " + m_Name;
        try {
            auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
#if 0
            while(m_LCmd->HasMoreResults()) {
                auto_ptr<CDB_Result> r(m_LCmd->Result());
                if (r.get()) {
                    while (r->Fetch())
                        continue;
                }
            }
#endif
        } catch ( const CDB_Exception& e ) {
            DATABASE_DRIVER_ERROR_EX( e, "failed to close cursor", 222003 );
        }

        m_IsOpen = false;
    }

    if (m_IsDeclared) {
        string buff;
        const bool connected_to_MSSQLServer = GetConnection().GetCDriverContext().ConnectedToMSSQLServer();

        if ( connected_to_MSSQLServer ) {
            buff = "deallocate " + m_Name;
        } else {
            buff = "deallocate cursor " + m_Name;
        }

        try {
            auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
#if 0
            while(m_LCmd->HasMoreResults()) {
                auto_ptr<CDB_Result> r(m_LCmd->Result());
                if (r.get()) {
                    while (r->Fetch())
                        continue;
                }
            }
#endif
        } catch ( const CDB_Exception& e) {
            DATABASE_DRIVER_ERROR_EX( e, "failed to deallocate cursor", 222003 );
        }

        m_IsDeclared = false;
    }

    return true;
}


CDBL_CursorCmd::~CDBL_CursorCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CDBL_CursorCmd::x_AssignParams()
{
    static const char s_hexnum[] = "0123456789ABCDEF";

    for (unsigned int n = 0; n < GetParams().NofParams(); n++) {
        const string& name = GetParams().GetParamName(n);
        if (name.empty())
            continue;
        CDB_Object& param = *GetParams().GetParam(n);
        char val_buffer[16*1024];

        if (!param.IsNULL()) {
            switch (param.GetType()) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                sprintf(val_buffer, "%d", val.Value());
                break;
            }
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                sprintf(val_buffer, "%d", (int) val.Value());
                break;
            }
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                sprintf(val_buffer, "%d", (int) val.Value());
                break;
            }
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                Int8 v8 = val.Value();
                sprintf(val_buffer, "%lld", v8);
                break;
            }
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                const char* c = val.Value(); // NB: 255 bytes at most
                size_t i = 0;
                val_buffer[i++] = '\'';
                while (*c) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '\'';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                const char* c = val.Value(); // NB: 255 bytes at most
                size_t i = 0;
                val_buffer[i++] = '\'';
                while (*c) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '\'';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
                const char* c = val.Value(); // NB: 255 bytes at most
                size_t i = 0;
                val_buffer[i++] = '\'';
                while (*c && (i < sizeof(val_buffer) - 2)) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                if(*c != '\0') return false;
                val_buffer[i++] = '\'';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.Size();
                val_buffer[i++] = '0';
                val_buffer[i++] = 'x';
                for (size_t j = 0; j < size; j++) {
                    val_buffer[i++] = s_hexnum[c[j] >> 4];
                    val_buffer[i++] = s_hexnum[c[j] & 0x0F];
                }
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.Size();
                val_buffer[i++] = '0';
                val_buffer[i++] = 'x';
                for (size_t j = 0; j < size; j++) {
                    val_buffer[i++] = s_hexnum[c[j] >> 4];
                    val_buffer[i++] = s_hexnum[c[j] & 0x0F];
                }
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.DataSize();
                if(size*2 > sizeof(val_buffer) - 4) return false;
                val_buffer[i++] = '0';
                val_buffer[i++] = 'x';
                for (size_t j = 0; j < size; j++) {
                    val_buffer[i++] = s_hexnum[c[j] >> 4];
                    val_buffer[i++] = s_hexnum[c[j] & 0x0F];
                }
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                sprintf(val_buffer, "%E", (double) val.Value());
                break;
            }
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                sprintf(val_buffer, "%E", val.Value());
                break;
            }
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                string t = val.Value().AsString("M/D/Y h:m");
                sprintf(val_buffer, "'%s'", t.c_str());
                break;
            }
            case eDB_DateTime: {
                CDB_DateTime& val =
                    dynamic_cast<CDB_DateTime&> (param);
                string t = val.Value().AsString("M/D/Y h:m:s");
                sprintf(val_buffer, "'%s:%.3d'", t.c_str(),
            (int)(val.Value().NanoSecond()/1000000));
                break;
            }
            default:
                return false;
            }
        } else
            strcpy(val_buffer, "NULL");

        // substitute the param
        g_SubstituteParam(GetQuery(), name, val_buffer);
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.34  2006/12/27 21:37:45  ssikorsk
 * Call SetExecCntxInfo() in constructor.
 *
 * Revision 1.33  2006/11/28 20:08:09  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.32  2006/11/20 18:15:58  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.31  2006/07/19 14:11:02  ssikorsk
 * Refactoring of CursorCmd.
 *
 * Revision 1.30  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.29  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.28  2006/06/19 19:11:44  ssikorsk
 * Replace C_ITDescriptorGuard with auto_ptr<I_ITDescriptor>
 *
 * Revision 1.27  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.26  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.25  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.24  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.23  2006/05/04 20:12:17  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.22  2005/12/06 19:29:21  ssikorsk
 * Revamp code to use GetResultSet/SetResultSet methods
 * instead of raw data access.
 *
 * Revision 1.21  2005/11/02 14:16:59  ssikorsk
 * Rethrow catched CDB_Exception to preserve useful information.
 *
 * Revision 1.20  2005/10/31 12:19:59  ssikorsk
 * Do not use separate include files for msdblib.
 *
 * Revision 1.19  2005/10/27 12:58:22  ssikorsk
 * Fixed "deallocate cursor" syntax in case of MS SQL Server
 *
 * Revision 1.18  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.17  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.16  2005/09/14 14:14:24  ssikorsk
 * Improved the CDBL_CursorCmd::Open method
 *
 * Revision 1.15  2005/07/07 20:34:16  vasilche
 * Added #include <stdio.h> for sprintf().
 *
 * Revision 1.14  2005/07/07 19:12:55  ssikorsk
 * Improved to support a ftds driver
 *
 * Revision 1.13  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.12  2004/05/18 18:30:36  gorelenk
 * PCH <ncbi_pch.hpp> moved to correct place .
 *
 * Revision 1.11  2003/06/05 16:01:13  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.10  2003/01/31 16:50:12  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.9  2002/07/02 16:05:49  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.8  2002/05/16 21:36:06  soussov
 * fixes the memory leak in text/image processing
 *
 * Revision 1.7  2002/03/26 15:37:52  soussov
 * new image/text operations added
 *
 * Revision 1.6  2002/01/08 18:10:18  sapojnik
 * Syabse to MSSQL name translations moved to interface_p.hpp
 *
 * Revision 1.5  2001/12/18 19:29:22  soussov
 * adds conversion from nanosecs to milisecs for datetime args
 *
 * Revision 1.4  2001/12/18 17:56:53  soussov
 * copy-paste bug in datetime processing fixed
 *
 * Revision 1.3  2001/10/24 16:41:13  lavr
 * File description changed to be of the same style as in other files
 *
 * Revision 1.2  2001/10/22 16:28:01  lavr
 * Default argument values removed
 * (mistakenly left while moving code from header files)
 *
 * Revision 1.1  2001/10/22 15:19:55  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
