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
 * File Description:  TDS language command
 *
 */

#include <dbapi/driver/ftds/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_LangCmd::
//

CTDS_LangCmd::CTDS_LangCmd(CTDS_Connection* conn, DBPROCESS* cmd,
                           const string& lang_query,
                           unsigned int nof_params) :
    m_Connect(conn), m_Cmd(cmd), m_Query(lang_query), m_Params(nof_params)
{

    m_WasSent   =  false;
    m_HasFailed =  false;
    m_Res       =  0;
    m_RowCount  = -1;
    m_Status    =  0;
}


bool CTDS_LangCmd::More(const string& query_text)
{
    m_Query.append(query_text);
    return true;
}


bool CTDS_LangCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
    return
        m_Params.BindParam(CDB_Params::kNoParamNumber, param_name, param_ptr);
}


bool CTDS_LangCmd::SetParam(const string& param_name, CDB_Object* param_ptr)
{
    return
        m_Params.SetParam(CDB_Params::kNoParamNumber, param_name, param_ptr);
}


bool CTDS_LangCmd::Send()
{
    if (m_WasSent)
        Cancel();

    m_HasFailed = false;

    if (!x_AssignParams()) {
        dbfreebuf(m_Cmd);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 220003, "CTDS_LangCmd::Send",
                           "cannot assign params");
    }

    if (dbcmd(m_Cmd, (char*)(m_Query.c_str())) != SUCCEED) {
        dbfreebuf(m_Cmd);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 220001, "CTDS_LangCmd::Send",
                           "dbcmd failed");
    }

    if (dbsqlsend(m_Cmd) != SUCCEED) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 220005, "CTDS_LangCmd::Send",
                           "dbsqlsend failed");
    }

    m_WasSent = true;
    m_Status = 0;
    return true;
}


bool CTDS_LangCmd::WasSent() const
{
    return m_WasSent;
}


bool CTDS_LangCmd::Cancel()
{
    if (m_WasSent) {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }
        m_WasSent = false;
        return (dbcancel(m_Cmd) == SUCCEED);
    }

    dbfreebuf(m_Cmd);
    m_Query.erase();
    return true;
}


bool CTDS_LangCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CTDS_LangCmd::Result()
{
    if (m_Res) {
        delete m_Res;
        m_Res = 0;
    }

    if (!m_WasSent) {
        throw CDB_ClientEx(eDB_Error, 220010, "CTDS_LangCmd::Result",
                           "a command has to be sent first");
    }

    if (m_Status == 0) {
        m_Status = 0x1;
        if (dbsqlok(m_Cmd) != SUCCEED) {
            m_WasSent = false;
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 220011, "CTDS_LangCmd::Result",
                               "dbsqlok failed");
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        m_Res = new CTDS_ComputeResult(m_Cmd, &m_Status);
        return Create_Result(*m_Res);
    }

    while ((m_Status & 0x1) != 0) {
        if ((m_Status & 0x20) != 0) { // check for return parameters from exec
            m_Status ^= 0x20;
            int n;
            if ((n = dbnumrets(m_Cmd)) > 0) {
                m_Res = new CTDS_ParamResult(m_Cmd, n);
                return Create_Result(*m_Res);
            }
        }

        if ((m_Status & 0x40) != 0) { // check for ret status
            m_Status ^= 0x40;
            if (dbhasretstat(m_Cmd)) {
                m_Res = new CTDS_StatusResult(m_Cmd);
                return Create_Result(*m_Res);
            }
        }
        switch (dbresults(m_Cmd)) {
        case SUCCEED:
            m_Status |= 0x60;
            if (DBCMDROW(m_Cmd) == SUCCEED) { // we could get rows in result
                if (dbnumcols(m_Cmd) == 1) {
                    int ct = dbcoltype(m_Cmd, 1);
                    if ((ct == SYBTEXT) || (ct == SYBIMAGE)) {
                        m_Res = new CTDS_BlobResult(m_Cmd);
                    }
                }
                if (!m_Res)
                    m_Res = new CTDS_RowResult(m_Cmd, &m_Status);
                return Create_Result(*m_Res);
            } else {
                m_RowCount = DBCOUNT(m_Cmd);
                continue;
            }
        case NO_MORE_RESULTS:
            m_Status = 2;
            break;
        default:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Warning, 220016, "CTDS_LangCmd::Result",
                               "an error was encountered by server");
        }
        break;
    }

    m_WasSent = false;
    return 0;
}


bool CTDS_LangCmd::HasMoreResults() const
{
    return m_WasSent;
}


bool CTDS_LangCmd::HasFailed() const
{
    return m_HasFailed;
}


int CTDS_LangCmd::RowCount() const
{
    return m_RowCount;
}


void CTDS_LangCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTDS_LangCmd::~CTDS_LangCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_WasSent)
        Cancel();
}


bool CTDS_LangCmd::x_AssignParams()
{
    static const char s_hexnum[] = "0123456789ABCDEF";

    for (unsigned int n = 0; n < m_Params.NofParams(); n++) {
        const string& name  =  m_Params.GetParamName(n);
        CDB_Object&   param = *m_Params.GetParam(n);
        char          val_buffer[1024];
        const char*   type;
        string        cmd;

        if (name.length() > kTDSMaxNameLen)
            return false;

        switch (param.GetType()) {
        case eDB_Int:
            type = "int";
            break;
        case eDB_SmallInt:
            type = "smallint";
            break;
        case eDB_TinyInt:
            type = "tinyint";
            break;
        case eDB_BigInt:
            type = "numeric";
            break;
        case eDB_Char:
        case eDB_VarChar:
            type = "varchar(255)";
            break;
        case eDB_Binary:
        case eDB_VarBinary:
            type = "varbinary(255)";
            break;
        case eDB_Float:
            type = "real";
            break;
        case eDB_Double:
            type = "float";
            break;
        case eDB_SmallDateTime:
            type = "smalldatetime";
            break;
        case eDB_DateTime:
            type = "datetime";
            break;            
        default:
            return false;
        }

        cmd += "declare" + name + ' ' + type + "\nselect " + name + " = ";

        if (!param.IsNULL()) {
            switch (param.GetType()) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                sprintf(val_buffer, "%d\n", val.Value());
                break;
            }
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                sprintf(val_buffer, "%d\n", (int) val.Value());
                break;
            }
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                sprintf(val_buffer, "%d\n", (int) val.Value());
                break;
            }
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                sprintf(val_buffer, "%lld\n", val.Value());
                break;
            }
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                const char* c = val.Value(); // No more than 255 chars
                size_t i = 0;
                val_buffer[i++] = '"';
                while (*c) {
                    if (*c == '"')
                        val_buffer[i++] = '"';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '"';
                val_buffer[i++] = '\n';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                const char* c = val.Value();
                size_t i = 0;
                val_buffer[i++] = '"';
                while (*c) {
                    if (*c == '"')
                        val_buffer[i++] = '"';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '"';
                val_buffer[i++] = '\n';
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
                val_buffer[i++] = '\n';
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
                val_buffer[i++] = '\n';
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                sprintf(val_buffer, "%E\n", (double) val.Value());
                break;
            }
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                sprintf(val_buffer, "%E\n", val.Value());
                break;
            }
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                strcpy(val_buffer, val.Value().AsString("M/D/Y h:m").c_str());
                break;
            }
            case eDB_DateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                strcpy(val_buffer,
                       val.Value().AsString("M/D/Y h:m:s:S").c_str());
                break;
            }
            default:
                return false; // dummy for compiler
            }
            cmd += val_buffer;
        } else
            cmd += "NULL\n";
        if (dbcmd(m_Cmd, (char*) cmd.c_str()) != SUCCEED)
            return false;
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
