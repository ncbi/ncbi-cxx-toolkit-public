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

#include <dbapi/driver/ftds/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_CursorCmd::
//

CTDS_CursorCmd::CTDS_CursorCmd(CTDS_Connection* con, DBPROCESS* cmd,
                               const string& cursor_name, const string& query,
                               unsigned int nof_params) :
    m_Connect(con), m_Cmd(cmd), m_Name(cursor_name), m_LCmd(0), m_Query(query),
    m_Params(nof_params), m_IsOpen(false), m_HasFailed(false),
    m_IsDeclared(false), m_Res(0), m_RowCount(-1)
{
}


bool CTDS_CursorCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
    return
        m_Params.BindParam(CDB_Params::kNoParamNumber, param_name, param_ptr);
}


CDB_Result* CTDS_CursorCmd::Open()
{
    if (m_IsOpen) { // need to close it first
        Close();
    }

    m_HasFailed = false;

    // declare the cursor
    if (!x_AssignParams()) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 222003, "CTDS_CursorCmd::Open",
                           "cannot assign params");
    }


    m_LCmd = 0;
    string buff = "declare " + m_Name + " cursor for " + m_Query;

    try {
        m_LCmd = m_Connect->LangCmd(buff);
        m_LCmd->Send();
        while (m_LCmd->HasMoreResults()) {
            CDB_Result* r = m_LCmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
        delete m_LCmd;
    } catch (CDB_Exception& e) {
        if (m_LCmd) {
            delete m_LCmd;
            m_LCmd = 0;
        }
        throw CDB_ClientEx(eDB_Error, 222001, "CTDS_CursorCmd::Open",
                           "failed to declare cursor");
    }
    m_IsDeclared = true;

    // open the cursor
    m_LCmd = 0;
    buff = "open " + m_Name;

    try {
        m_LCmd = m_Connect->LangCmd(buff);
        m_LCmd->Send();
        while (m_LCmd->HasMoreResults()) {
            CDB_Result* r = m_LCmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
        delete m_LCmd;
    } catch (CDB_Exception& e) {
        if (m_LCmd) {
            delete m_LCmd;
            m_LCmd = 0;
        }
        throw CDB_ClientEx(eDB_Error, 222002, "CTDS_CursorCmd::Open",
                           "failed to open cursor");
    }
    m_IsOpen = true;

    m_LCmd = 0;
    buff = "fetch " + m_Name;

    m_LCmd = m_Connect->LangCmd(buff);
    m_Res = new CTDS_CursorResult(m_LCmd);
    return Create_Result(*m_Res);
}


bool CTDS_CursorCmd::Update(const string&, const string& upd_query)
{
    if (!m_IsOpen)
        return false;

    CDB_LangCmd* cmd = 0;

    try {
	while(m_LCmd->HasMoreResults()) {
	    CDB_Result* r= m_LCmd->Result();
	    if(r) delete r;
	}

        string buff = upd_query + " where current of " + m_Name;
        cmd = m_Connect->LangCmd(buff);
        cmd->Send();
        while (cmd->HasMoreResults()) {
            CDB_Result* r = cmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
        delete cmd;
    } catch (CDB_Exception& e) {
        if (cmd)
            delete cmd;
        throw CDB_ClientEx(eDB_Error, 222004, "CTDS_CursorCmd::Update",
                           "update failed");
    }

    return true;
}


bool CTDS_CursorCmd::Delete(const string& table_name)
{
    if (!m_IsOpen)
        return false;

    CDB_LangCmd* cmd = 0;

    try {
	while(m_LCmd->HasMoreResults()) {
	    CDB_Result* r= m_LCmd->Result();
	    if(r) delete r;
	}

        string buff = "delete " + table_name + " where current of " + m_Name;
        cmd = m_Connect->LangCmd(buff);
        cmd->Send();
        while (cmd->HasMoreResults()) {
            CDB_Result* r = cmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
        delete cmd;
    } catch (CDB_Exception& e) {
        if (cmd)
            delete cmd;
        throw CDB_ClientEx(eDB_Error, 222004, "CTDS_CursorCmd::Update",
                           "update failed");
    }

    return true;
}


int CTDS_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CTDS_CursorCmd::Close()
{
    if (!m_IsOpen)
        return false;

    if (m_Res) {
        delete m_Res;
        m_Res = 0;
    }

    if (m_LCmd)
        delete m_LCmd;

    if (m_IsOpen) {
        string buff = "close " + m_Name;
        m_LCmd = 0;
        try {
            m_LCmd = m_Connect->LangCmd(buff);
            m_LCmd->Send();
            while (m_LCmd->HasMoreResults()) {
                CDB_Result* r = m_LCmd->Result();
                if (r) {
                    while (r->Fetch())
                        ;
                    delete r;
                }
            }
            delete m_LCmd;
        } catch (CDB_Exception& e) {
            if (m_LCmd)
                delete m_LCmd;
            m_LCmd = 0;
            throw CDB_ClientEx(eDB_Error, 222003, "CTDS_CursorCmd::Close",
                               "failed to close cursor");
        }

        m_IsOpen = false;
        m_LCmd = 0;
    }

    if (m_IsDeclared) {
        string buff = "deallocate " + m_Name;
        m_LCmd = 0;
        try {
            m_LCmd = m_Connect->LangCmd(buff);
            m_LCmd->Send();
            while (m_LCmd->HasMoreResults()) {
                CDB_Result* r = m_LCmd->Result();
                if (r) {
                    while (r->Fetch())
                        ;
                    delete r;
                }
            }
            delete m_LCmd;
        } catch (CDB_Exception& e) {
            if (m_LCmd)
                delete m_LCmd;
            m_LCmd = 0;
            throw CDB_ClientEx(eDB_Error, 222003, "CTDS_CursorCmd::Close",
                               "failed to deallocate cursor");
        }

        m_IsDeclared = false;
        m_LCmd = 0;
    }

    return true;
}


void CTDS_CursorCmd::Release()
{
    m_BR = 0;
    if (m_IsOpen) {
        Close();
        m_IsOpen = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTDS_CursorCmd::~CTDS_CursorCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_IsOpen)
        Close();
}


bool CTDS_CursorCmd::x_AssignParams()
{
    static const char s_hexnum[] = "0123456789ABCDEF";

    for (unsigned int n = 0; n < m_Params.NofParams(); n++) {
        const string& name = m_Params.GetParamName(n);
        if (name.empty())
            continue;
        CDB_Object& param = *m_Params.GetParam(n);
        char val_buffer[1024];

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
                val_buffer[i++] = '"';
                while (*c) {
                    if (*c == '"')
                        val_buffer[i++] = '"';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '"';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                const char* c = val.Value(); // NB: 255 bytes at most
                size_t i = 0;
                val_buffer[i++] = '"';
                while (*c) {
                    if (*c == '"')
                        val_buffer[i++] = '"';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '"';
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
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                string t = val.Value().AsString("M/D/Y h:m:s:S");
                sprintf(val_buffer, "'%s'", t.c_str());
                break;
            }
            default:
                return false;
            }
        } else
            strcpy(val_buffer, "NULL");

        // substitute the param
        g_SubstituteParam(m_Query, name, val_buffer);
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
