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
 * File Description:  TDS cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_Ftds8_Cmds


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_CursorCmd::
//

CTDS_CursorCmd::CTDS_CursorCmd(CTDS_Connection& conn,
                               DBPROCESS* cmd,
                               const string& cursor_name,
                               const string& query,
                               unsigned int nof_params) :
    CDBL_Cmd(conn, cmd),
    impl::CBaseCmd(conn, cursor_name, query, nof_params),
    m_LCmd(NULL),
    m_Res(NULL)
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

CDB_Result* CTDS_CursorCmd::OpenCursor()
{
    const bool connected_to_MSSQLServer = GetConnection().GetCDriverContext().ConnectedToMSSQLServer();

    // need to close it first
    CloseCursor();

    SetHasFailed(false);

    // declare the cursor
    SetHasFailed(!x_AssignParams());
    CHECK_DRIVER_ERROR(
        HasFailed(),
        "Cannot assign params." + GetDbgInfo(),
        222003 );


    m_LCmd = 0;

    string buff;
    if ( connected_to_MSSQLServer ) {
        string cur_feat;

        if(for_update_of(GetCombinedQuery())) {
            cur_feat = " cursor FORWARD_ONLY SCROLL_LOCKS for ";
        } else {
            cur_feat = " cursor FORWARD_ONLY for ";
        }

        buff = "declare " + GetCmdName() + cur_feat + GetCombinedQuery();
    } else {
        // Sybase ...

        buff = "declare " + GetCmdName() + " cursor for " + GetCombinedQuery();
    }

    try {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));

        cmd->Send();
        cmd->DumpResults();
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Failed to declare cursor." + GetDbgInfo(), 222001 );
    }

    SetCursorDeclared();

    // open the cursor
    m_LCmd = 0;
    buff = "open " + GetCmdName();

    try {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));

        cmd->Send();
        cmd->DumpResults();
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Failed to open cursor." + GetDbgInfo(), 222002 );
    }

    SetCursorOpen();

    m_LCmd = 0;
    buff = "fetch " + GetCmdName();

    m_LCmd = GetConnection().LangCmd(buff);
    m_Res = new CTDS_CursorResult(GetConnection(), m_LCmd);

    return Create_Result(*GetResultSet());
}


bool CTDS_CursorCmd::Update(const string&, const string& upd_query)
{
    if (!CursorIsOpen())
        return false;

    try {
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
        }

        string buff = upd_query + " where current of " + GetCmdName();
        const auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
#if 0
        while (cmd->HasMoreResults()) {
            CDB_Result* r = cmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
#endif
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Update failed." + GetDbgInfo(), 222004 );
    }

    return true;
}

I_ITDescriptor* CTDS_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || (m_Res == 0)) {
        return 0;
    }
    while(static_cast<unsigned int>(m_Res->CurrentItemNo()) < item_num) {
        if(!m_Res->SkipItem()) return 0;
    }

    I_ITDescriptor* desc= new CTDS_ITDescriptor(GetConnection(), GetCmd(), item_num+1);
    return desc;
}

bool CTDS_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    if(desc) {
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }

        return GetConnection().x_SendData(*desc, data, log_it);
    }
    return false;
}

CDB_SendDataCmd* CTDS_CursorCmd::SendDataCmd(unsigned int item_num, size_t size,
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

bool CTDS_CursorCmd::Delete(const string& table_name)
{
    if (!CursorIsOpen())
        return false;

    CDB_LangCmd* cmd = 0;

    try {
    while(m_LCmd->HasMoreResults()) {
        CDB_Result* r= m_LCmd->Result();
        if(r) delete r;
    }

        string buff = "delete " + table_name + " where current of " + GetCmdName();
        cmd = GetConnection().LangCmd(buff);
        cmd->Send();
        cmd->DumpResults();
#if 0
        while (cmd->HasMoreResults()) {
            CDB_Result* r = cmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
#endif
        delete cmd;
    } catch ( const CDB_Exception& e ) {
        if (cmd)
            delete cmd;
        DATABASE_DRIVER_ERROR_EX( e, "Update failed." + GetDbgInfo(), 222004 );
    }

    return true;
}


int CTDS_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CTDS_CursorCmd::CloseCursor()
{
    if (!CursorIsOpen())
        return false;

    if (m_Res) {
        delete m_Res;
        m_Res = 0;
    }

    if (m_LCmd)
        delete m_LCmd;

    if (CursorIsOpen()) {
        string buff = "close " + GetCmdName();
        m_LCmd = 0;
        try {
            m_LCmd = GetConnection().LangCmd(buff);
            m_LCmd->Send();
            m_LCmd->DumpResults();
#if 0
            while (m_LCmd->HasMoreResults()) {
                CDB_Result* r = m_LCmd->Result();
                if (r) {
                    while (r->Fetch())
                        ;
                    delete r;
                }
            }
#endif
            delete m_LCmd;
        } catch ( const CDB_Exception& e ) {
            if (m_LCmd)
                delete m_LCmd;
            m_LCmd = 0;
            DATABASE_DRIVER_ERROR_EX( e, "Failed to close cursor." + GetDbgInfo(), 222003 );
        }

        SetCursorOpen(false);
        m_LCmd = 0;
    }

    if (CursorIsDeclared()) {
        string buff = "deallocate " + GetCmdName();
        m_LCmd = 0;
        try {
            m_LCmd = GetConnection().LangCmd(buff);
            m_LCmd->Send();
            m_LCmd->DumpResults();
#if 0
            while (m_LCmd->HasMoreResults()) {
                CDB_Result* r = m_LCmd->Result();
                if (r) {
                    while (r->Fetch())
                        ;
                    delete r;
                }
            }
#endif
            delete m_LCmd;
        } catch ( const CDB_Exception& e) {
            if (m_LCmd)
                delete m_LCmd;
            m_LCmd = 0;
            DATABASE_DRIVER_ERROR_EX( e, "Failed to deallocate cursor." + GetDbgInfo(), 222003 );
        }

        SetCursorDeclared(false);
        m_LCmd = 0;
    }

    return true;
}


CTDS_CursorCmd::~CTDS_CursorCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        CloseCursor();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


bool CTDS_CursorCmd::x_AssignParams()
{
    static const char s_hexnum[] = "0123456789ABCDEF";

    m_CombinedQuery = GetQuery();

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
                string s8 = NStr::Int8ToString(val.Value());
                s8.copy(val_buffer, s8.size());
                val_buffer[s8.size()] = '\0';
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
        m_CombinedQuery = g_SubstituteParam(m_CombinedQuery, name, val_buffer);
    }

    return true;
}


END_NCBI_SCOPE


