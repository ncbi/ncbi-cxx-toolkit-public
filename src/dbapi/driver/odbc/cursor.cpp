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
 * File Description:  ODBC cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmd::
//

CODBC_CursorCmd::CODBC_CursorCmd(CODBC_Connection* con, SQLHSTMT cmd,
                               const string& cursor_name, const string& query,
                               unsigned int nof_params) :
    m_CursCmd(con, cmd, "declare "+cursor_name+" cursor for "+query, nof_params),
    m_Connect(con), m_Name(cursor_name), m_LCmd(0), m_IsOpen(false), 
    m_HasFailed(false), m_IsDeclared(false), m_Res(0), m_RowCount(-1)
{
}


bool CODBC_CursorCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
    return
        m_CursCmd.BindParam(param_name, param_ptr);
}


CDB_Result* CODBC_CursorCmd::Open()
{
    if (m_IsOpen) { // need to close it first
        Close();
    }

    m_HasFailed = false;

    // declare the cursor
    try {
        m_CursCmd.Send();
        m_CursCmd.DumpResults();
#if 0
        while (m_CursCmd.HasMoreResults()) {
            CDB_Result* r = m_CursCmd.Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
#endif
    } catch (CDB_Exception& ) {
        DATABASE_DRIVER_ERROR( "failed to declare cursor", 422001 );
    }
    m_IsDeclared = true;

    // open the cursor
    m_LCmd = 0;
    // buff = "open " + m_Name;

    try {
        m_LCmd = m_Connect->xLangCmd("open " + m_Name);
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
        m_LCmd->Release();
    } catch (CDB_Exception& ) {
        if (m_LCmd) {
            m_LCmd->Release();
            m_LCmd = 0;
        }
        DATABASE_DRIVER_ERROR( "failed to open cursor", 422002 );
    }
    m_IsOpen = true;

    m_LCmd = 0;
    //buff = "fetch " + m_Name;

    m_LCmd = m_Connect->xLangCmd("fetch " + m_Name);
    m_Res = new CODBC_CursorResult(m_LCmd);
    return Create_Result(*m_Res);
}


bool CODBC_CursorCmd::Update(const string&, const string& upd_query)
{
    if (!m_IsOpen)
        return false;

    CDB_LangCmd* cmd = 0;

    try {
		m_LCmd->Cancel();
#if 0
		while(m_LCmd->HasMoreResults()) {
			CDB_Result* r= m_LCmd->Result();
			if(r) delete r;
		}
#endif
        string buff = upd_query + " where current of " + m_Name;
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
    } catch (CDB_Exception& ) {
        if (cmd)
            delete cmd;
        DATABASE_DRIVER_ERROR( "update failed", 422004 );
    }

    return true;
}

CDB_ITDescriptor* CODBC_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || (m_Res == 0) || (m_LCmd == 0)) {
        return 0;
    }
    string cond= "current of " + m_Name;
    return m_LCmd->m_Res->GetImageOrTextDescriptor(item_num, cond);
}

bool CODBC_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data, 
				    bool log_it)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return false;
    C_ITDescriptorGuard g((I_ITDescriptor*)desc);

	m_LCmd->Cancel();
    
    return (data.GetType() == eDB_Text)? 
        m_Connect->SendData(*desc, (CDB_Text&)data, log_it) :
        m_Connect->SendData(*desc, (CDB_Image&)data, log_it);
}

CDB_SendDataCmd* CODBC_CursorCmd::SendDataCmd(unsigned int item_num, size_t size, 
					    bool log_it)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return 0;
    C_ITDescriptorGuard g((I_ITDescriptor*)desc);

	m_LCmd->Cancel();

    return m_Connect->SendDataCmd((I_ITDescriptor&)*desc, size, log_it);
}					    

bool CODBC_CursorCmd::Delete(const string& table_name)
{
    if (!m_IsOpen)
        return false;

    CDB_LangCmd* cmd = 0;

    try {
		m_LCmd->Cancel();

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
    } catch (CDB_Exception& ) {
        if (cmd)
            delete cmd;
        DATABASE_DRIVER_ERROR( "update failed", 422004 );
    }

    return true;
}


int CODBC_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CODBC_CursorCmd::Close()
{
    if (!m_IsOpen)
        return false;

    if (m_Res) {
        delete m_Res;
        m_Res = 0;
    }

    if (m_LCmd) {
        m_LCmd->Release();
        m_LCmd= 0;
    }

    if (m_IsOpen) {
        string buff = "close " + m_Name;
        m_LCmd = 0;
        try {
            m_LCmd = m_Connect->xLangCmd(buff);
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
            m_LCmd->Release();
        } catch (CDB_Exception& ) {
            if (m_LCmd)
                m_LCmd->Release();
            m_LCmd = 0;
            DATABASE_DRIVER_ERROR( "failed to close cursor", 422003 );
        }

        m_IsOpen = false;
        m_LCmd = 0;
    }

    if (m_IsDeclared) {
        string buff = "deallocate " + m_Name;
        m_LCmd = 0;
        try {
            m_LCmd = m_Connect->xLangCmd(buff);
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
            m_LCmd->Release();
        } catch (CDB_Exception& ) {
            if (m_LCmd)
                m_LCmd->Release();
            m_LCmd = 0;
            DATABASE_DRIVER_ERROR( "failed to deallocate cursor", 422003 );
        }

        m_IsDeclared = false;
        m_LCmd = 0;
    }

    return true;
}


void CODBC_CursorCmd::Release()
{
    m_BR = 0;
    if (m_IsOpen) {
        Close();
        m_IsOpen = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CODBC_CursorCmd::~CODBC_CursorCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_IsOpen)
        Close();
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.4  2004/05/17 21:16:06  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/06/05 16:02:04  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.2  2003/01/31 16:51:03  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.1  2002/06/18 22:06:24  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
