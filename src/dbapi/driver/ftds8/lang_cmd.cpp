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

#include <ncbi_pch.hpp>
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
        DATABASE_DRIVER_ERROR( "cannot assign params", 220003 );
    }

    if (dbcmd(m_Cmd, (char*)(m_Query.c_str())) != SUCCEED) {
        dbfreebuf(m_Cmd);
        m_HasFailed = true;
        DATABASE_DRIVER_FATAL( "dbcmd failed", 220001 );
    }

    
    m_Connect->TDS_SetTimeout();

    m_HasFailed = dbsqlsend(m_Cmd) != SUCCEED;
    CHECK_DRIVER_ERROR( 
        m_HasFailed,
        "dbsqlsend failed", 
        220005 );

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
        if(m_RowCount < 0) {
            m_RowCount= DBCOUNT(m_Cmd);
        }
        delete m_Res;
        m_Res = 0;
    }

    CHECK_DRIVER_ERROR( 
        !m_WasSent,
        "a command has to be sent first", 
        220010 );

    if (m_Status == 0) {
        m_Status = 0x1;
        if (dbsqlok(m_Cmd) != SUCCEED) {
            m_WasSent = false;
            m_HasFailed = true;
            DATABASE_DRIVER_ERROR( "dbsqlok failed", 220011 );
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        m_Res = new CTDS_ComputeResult(m_Cmd, &m_Status);
        m_RowCount= 1;
        return Create_Result(*m_Res);
    }

    while ((m_Status & 0x1) != 0) {
        switch (dbresults(m_Cmd)) {
        case SUCCEED:
            if (DBCMDROW(m_Cmd) == SUCCEED) { // we may get rows in this result
                m_Res = new CTDS_RowResult(m_Cmd, &m_Status);
                m_RowCount = -1;
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
            DATABASE_DRIVER_WARNING( "error encountered in command execution", 221016 );
        }
        break;
    }

    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_Status == 2) {
        m_Status = 4;
        int n = dbnumrets(m_Cmd);
        if (n > 0) {
            m_Res = new CTDS_ParamResult(m_Cmd, n);
            m_RowCount = 1;
            return Create_Result(*m_Res);
        }
    }

    if (m_Status == 4) {
        m_Status = 6;
        if (dbhasretstat(m_Cmd)) {
            m_Res = new CTDS_StatusResult(m_Cmd);
            m_RowCount = 1;
            return Create_Result(*m_Res);
        }
    }

    m_WasSent = false;
    return 0;
}

bool CTDS_LangCmd::HasMoreResults() const
{
    return m_WasSent;
}

void CTDS_LangCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_WasSent) {
        dbres= Result();
        if(dbres) {
            if(m_Connect->m_ResProc) {
                m_Connect->m_ResProc->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch());
            }
            delete dbres;
        }
    }
}

bool CTDS_LangCmd::HasFailed() const
{
    return m_HasFailed;
}


int CTDS_LangCmd::RowCount() const
{
    return (m_RowCount < 0)? DBCOUNT(m_Cmd) : m_RowCount;
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
        if(m_Params.GetParamStatus(n) == 0) continue;
        const string& name  =  m_Params.GetParamName(n);
        CDB_Object&   param = *m_Params.GetParam(n);
        char          val_buffer[16*1024];
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
        case eDB_LongChar: {
            CDB_LongChar& lc = dynamic_cast<CDB_LongChar&> (param);
            sprintf(val_buffer, "varchar(%d)", lc.Size());
            type= val_buffer;
            break;
        }
        case eDB_Binary:
        case eDB_VarBinary:
            type = "varbinary(255)";
            break;
        case eDB_LongBinary: {
            CDB_LongBinary& lb = dynamic_cast<CDB_LongBinary&> (param);
            if(lb.DataSize()*2 > (sizeof(val_buffer) - 4)) return false;
            sprintf(val_buffer, "varbinary(%d)", lb.Size());
            type= val_buffer;
            break;
        }
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

        cmd += "declare " + name + ' ' + type + "\nselect " + name + " = ";

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
                val_buffer[i++] = '\'';
                while (*c) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '\'';
                val_buffer[i++] = '\n';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                const char* c = val.Value();
                size_t i = 0;
                val_buffer[i++] = '\'';
                while (*c) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '\'';
                val_buffer[i++] = '\n';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
                const char* c = val.Value();
                size_t i = 0;
                val_buffer[i++] = '\'';
                while (*c && (i < (sizeof(val_buffer)-3))) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                val_buffer[i++] = '\'';
                val_buffer[i++] = '\n';
                val_buffer[i] = '\0';
                if(*c != '\0') return false;
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
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.DataSize();
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
                sprintf(val_buffer, "'%s'\n",val.Value().AsString("M/D/Y h:m").c_str());
                break;
            }
            case eDB_DateTime: {
                CDB_DateTime& val =
                    dynamic_cast<CDB_DateTime&> (param);
                sprintf(val_buffer,  "'%s:%.3d'\n",   val.Value().AsString("M/D/Y h:m:s").c_str(),
            (int)(val.Value().NanoSecond()/1000000));
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
 * Revision 1.14  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.13  2004/05/17 21:13:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.12  2003/06/05 16:01:40  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.11  2003/05/16 20:26:09  soussov
 * adds code to skip parameter if it was not set
 *
 * Revision 1.10  2003/04/29 21:15:03  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.9  2002/09/19 22:28:20  soussov
 * changs order of result processing
 *
 * Revision 1.8  2002/07/22 20:11:07  soussov
 * fixes the RowCount calculations
 *
 * Revision 1.7  2002/01/14 20:38:49  soussov
 * timeout support for tds added
 *
 * Revision 1.6  2001/12/18 19:29:08  soussov
 * adds conversion from nanosecs to milisecs for datetime args
 *
 * Revision 1.5  2001/12/18 17:56:39  soussov
 * copy-paste bug in datetime processing fixed
 *
 * Revision 1.4  2001/12/18 16:42:44  soussov
 * fixes bug in datetime argument processing
 *
 * Revision 1.3  2001/12/13 23:40:53  soussov
 * changes double quotes to single quotes in SQL queries
 *
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
