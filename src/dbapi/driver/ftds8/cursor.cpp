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

CDB_Result* CTDS_CursorCmd::Open()
{
    if (m_IsOpen) { // need to close it first
        Close();
    }

    m_HasFailed = false;

    // declare the cursor
    m_HasFailed = !x_AssignParams();
    CHECK_DRIVER_ERROR( 
        m_HasFailed,
        "cannot assign params", 
        222003 );


    m_LCmd = 0;
    string cur_feat;
    if(for_update_of(m_Query)) {
        cur_feat= " cursor FORWARD_ONLY SCROLL_LOCKS for ";
    }
    else {
        cur_feat= " cursor FORWARD_ONLY for ";
    }
        
    string buff = "declare " + m_Name + cur_feat + m_Query;

    try {
        m_LCmd = m_Connect->LangCmd(buff);
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
    } catch (CDB_Exception&) {
        if (m_LCmd) {
            delete m_LCmd;
            m_LCmd = 0;
        }
        DATABASE_DRIVER_ERROR( "failed to declare cursor", 222001 );
    }
    m_IsDeclared = true;

    // open the cursor
    m_LCmd = 0;
    buff = "open " + m_Name;

    try {
        m_LCmd = m_Connect->LangCmd(buff);
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
    } catch (CDB_Exception&) {
        if (m_LCmd) {
            delete m_LCmd;
            m_LCmd = 0;
        }
        DATABASE_DRIVER_ERROR( "failed to open cursor", 222002 );
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

    try {
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
        }

        string buff = upd_query + " where current of " + m_Name;
        const auto_ptr<CDB_LangCmd> cmd(m_Connect->LangCmd(buff));
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
    } catch (CDB_Exception&) {
        DATABASE_DRIVER_ERROR( "update failed", 222004 );
    }

    return true;
}

I_ITDescriptor* CTDS_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || (m_Res == 0)) {
        return 0;
    }
    while(m_Res->CurrentItemNo() < item_num) {
        if(!m_Res->SkipItem()) return 0;
    }
    
    I_ITDescriptor* desc= new CTDS_ITDescriptor(m_Cmd, item_num+1);
    return desc;
}

bool CTDS_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data, 
                    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    C_ITDescriptorGuard d_guard(desc);

    if(desc) {
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }
        
        return m_Connect->x_SendData(*desc, data, log_it);
    }
    return false;
}

CDB_SendDataCmd* CTDS_CursorCmd::SendDataCmd(unsigned int item_num, size_t size, 
                        bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    C_ITDescriptorGuard d_guard(desc);

    if(desc) {
        m_LCmd->DumpResults();
#if 0
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }
#endif
        
        return m_Connect->SendDataCmd(*desc, size, log_it);
    }
    return 0;
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
    } catch (CDB_Exception&) {
        if (cmd)
            delete cmd;
        DATABASE_DRIVER_ERROR( "update failed", 222004 );
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
        } catch (CDB_Exception&) {
            if (m_LCmd)
                delete m_LCmd;
            m_LCmd = 0;
            DATABASE_DRIVER_ERROR( "failed to close cursor", 222003 );
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
        } catch (CDB_Exception&) {
            if (m_LCmd)
                delete m_LCmd;
            m_LCmd = 0;
            DATABASE_DRIVER_ERROR( "failed to deallocate cursor", 222003 );
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
        g_SubstituteParam(m_Query, name, val_buffer);
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.12  2004/12/10 15:26:11  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.11  2004/05/17 21:13:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.10  2003/06/05 16:01:40  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.9  2003/04/29 21:15:03  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.8  2003/02/28 23:27:24  soussov
 * fixes double quote bug in char/varchar parameters substitute
 *
 * Revision 1.7  2002/12/03 19:18:58  soussov
 * adopting the TDS8 cursors
 *
 * Revision 1.6  2002/05/16 21:36:45  soussov
 * fixes the memory leak in text/image processing
 *
 * Revision 1.5  2002/03/26 15:35:10  soussov
 * new image/text operations added
 *
 * Revision 1.4  2001/12/18 19:29:08  soussov
 * adds conversion from nanosecs to milisecs for datetime args
 *
 * Revision 1.3  2001/12/18 17:56:38  soussov
 * copy-paste bug in datetime processing fixed
 *
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
