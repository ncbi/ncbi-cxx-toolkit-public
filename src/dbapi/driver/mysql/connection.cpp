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
 * Author:  Anton Butanayev
 *
 * File Description:  Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


CMySQL_Connection::CMySQL_Connection(CMySQLContext* cntx,
                                     const string&  srv_name,
                                     const string&  user_name,
                                     const string&  passwd)
    : m_Context(cntx)
{
    if ( !mysql_init(&m_MySQL) ) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_init", 800001 );
    }

    if ( !mysql_real_connect(&m_MySQL,
                             srv_name.c_str(),
                             user_name.c_str(), passwd.c_str(),
                             NULL, 0, NULL, 0)) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_connect", 800002 );
    }
}


CMySQL_Connection::~CMySQL_Connection()
{
    mysql_close(&m_MySQL);
}


bool CMySQL_Connection::IsAlive()
{
    return false;
}


void CMySQL_Connection::Release()
{
}


CDB_SendDataCmd* CMySQL_Connection::SendDataCmd(I_ITDescriptor& /*descr_in*/,
                                                size_t          /*data_size*/,
                                                bool            /*log_it*/)
{
    return 0;
}


bool CMySQL_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Image&      /*img*/,
                                 bool            /*log_it*/)
{
    return true;
}


bool CMySQL_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Text&       /*txt*/,
                                 bool            /*log_it*/)
{
    return true;
}


bool CMySQL_Connection::Refresh()
{
    return true;
}


const string& CMySQL_Connection::ServerName() const
{
    return kEmptyStr;
}


const string& CMySQL_Connection::UserName() const
{
    return kEmptyStr;
}


const string& CMySQL_Connection::Password() const
{
    return kEmptyStr;
}


I_DriverContext::TConnectionMode CMySQL_Connection::ConnectMode() const
{
    return 0;
}


bool CMySQL_Connection::IsReusable() const
{
    return false;
}


const string& CMySQL_Connection::PoolName() const
{
    return kEmptyStr;
}


I_DriverContext* CMySQL_Connection::Context() const
{
    return m_Context;
}

void CMySQL_Connection::PushMsgHandler(CDB_UserHandler* /*h*/)
{
}


void CMySQL_Connection::PopMsgHandler (CDB_UserHandler* /*h*/)
{
}

CDB_ResultProcessor* CMySQL_Connection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    CDB_ResultProcessor* r= m_ResProc;
    m_ResProc= rp;
    return r;
}

CDB_LangCmd* CMySQL_Connection::LangCmd(const string& lang_query,
                                        unsigned int  nof_parms)
{
    return Create_LangCmd(*new CMySQL_LangCmd(this, lang_query, nof_parms));

}


CDB_RPCCmd *CMySQL_Connection::RPC(const string& /*rpc_name*/,
                                   unsigned int  /*nof_args*/)
{
    return 0;
}


CDB_BCPInCmd* CMySQL_Connection::BCPIn(const string& /*table_name*/,
                                       unsigned int  /*nof_columns*/)
{
    return 0;
}


CDB_CursorCmd *CMySQL_Connection::Cursor(const string& /*cursor_name*/,
                                         const string& /*query*/,
                                         unsigned int  /*nof_params*/,
                                         unsigned int  /*batch_size*/)
{
    return 0;
}

bool CMySQL_Connection::Abort()
{
    return false;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.7  2005/02/23 21:38:13  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.6  2004/05/17 21:15:34  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/03/24 19:46:53  vysokolo
 * addaed support of blob
 *
 * Revision 1.4  2003/06/05 16:02:48  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.3  2003/01/06 20:30:26  vakatov
 * Get rid of some redundant header(s).
 * Formally reformatted to closer meet C++ Toolkit/DBAPI style.
 *
 * Revision 1.2  2002/08/28 17:18:20  butanaev
 * Improved error handling, demo app.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
