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


CMySQL_Connection::CMySQL_Connection(CMySQLContext& cntx,
                                     const string&  srv_name,
                                     const string&  user_name,
                                     const string&  passwd) :
    impl::CConnection(cntx),
    m_IsOpen(false)
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

    m_IsOpen = true;
}


CMySQL_Connection::~CMySQL_Connection()
{
    try {
        // Close is a virtual function but it is safe to call it from a destructor
        // because it is defined in this class.
        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CMySQL_Connection::IsAlive()
{
    return false;
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
    // close all commands first
    DeleteAllCommands();

    return true;
}


I_DriverContext::TConnectionMode CMySQL_Connection::ConnectMode() const
{
    return 0;
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

bool CMySQL_Connection::Close(void)
{
    if (m_IsOpen) {
        Refresh();

        mysql_close(&m_MySQL);

        m_IsOpen = false;

        return true;
    }

    return false;
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2006/11/28 20:08:07  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.14  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.13  2006/06/05 21:06:58  ssikorsk
 * Deleted CMySQL_Connection::Release;
 * Replaced "m_BR = 0" with "CDB_BaseEnt::Release()";
 *
 * Revision 1.12  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.11  2006/06/05 14:44:56  ssikorsk
 * Moved methods PushMsgHandler, PopMsgHandler and DropCmd into I_Connection.
 *
 * Revision 1.10  2006/05/15 19:39:05  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.9  2006/04/05 14:32:25  ssikorsk
 * Implemented CMySQL_Connection::Close
 *
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
