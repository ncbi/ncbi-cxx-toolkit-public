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
 * File Description:  DBLib language command
 *
 */
#include <ncbi_pch.hpp>

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_LangCmd::
//

CDBL_LangCmd::CDBL_LangCmd(CDBL_Connection* conn, DBPROCESS* cmd,
                           const string& lang_query,
                           unsigned int nof_params) :
    CDBL_Cmd( conn, cmd ),
    impl::CBaseCmd(lang_query, nof_params),
    m_Res( 0 ),
    m_Status( 0 )
{
}


bool CDBL_LangCmd::Send()
{
    Cancel();

    m_HasFailed = false;

    if (!x_AssignParams()) {
        dbfreebuf(GetCmd());
        CheckFunctCall();
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "cannot assign params", 220003 );
    }

    if (Check(dbcmd(GetCmd(), (char*)(GetQuery().c_str()))) != SUCCEED) {
        dbfreebuf(GetCmd());
        CheckFunctCall();
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "dbcmd failed", 220001 );
    }


    // Timeout is already set by CDBLibContext ...
    // GetConnection().x_SetTimeout();

    m_HasFailed = Check(dbsqlsend(GetCmd())) != SUCCEED;
    CHECK_DRIVER_ERROR(
        m_HasFailed,
        "dbsqlsend failed",
        220005 );

    m_WasSent = true;
    m_Status = 0;
    return true;
}


bool CDBL_LangCmd::WasSent() const
{
    return m_WasSent;
}


bool CDBL_LangCmd::Cancel()
{
    if (m_WasSent) {
        if ( GetResultSet() ) {
#if 1 && defined(FTDS_IN_USE)
            while (GetResultSet()->Fetch())
               continue;
#endif
            ClearResultSet();
        }
        m_WasSent = false;

        dbfreebuf(GetCmd());
        CheckFunctCall();
        // m_Query.erase();

        return (Check(dbcancel(GetCmd())) == SUCCEED);
    }
    return true;
}


bool CDBL_LangCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CDBL_LangCmd::Result()
{
    if ( GetResultSet() ) {
        if(m_RowCount < 0) {
            m_RowCount = DBCOUNT(GetCmd());
        }

#if 1 && defined(FTDS_IN_USE)
        // This Fetch is required by FreeTDS v063
        // Useful stuff
        while (GetResultSet()->Fetch())
            continue;
#endif

        ClearResultSet();
    }

    CHECK_DRIVER_ERROR( !m_WasSent, "a command has to be sent first", 220010 );

    if (m_Status == 0) {
        m_Status = 0x1;
        if (Check(dbsqlok(GetCmd())) != SUCCEED) {
            m_WasSent = false;
            m_HasFailed = true;
            DATABASE_DRIVER_ERROR( "dbsqlok failed", 220011 );
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        SetResultSet( new CDBL_ComputeResult(GetConnection(), GetCmd(), &m_Status) );
        m_RowCount= 1;

        return Create_Result(*GetResultSet());
    }

    while ((m_Status & 0x1) != 0) {
#if !defined(FTDS_LOGIC)
        if ((m_Status & 0x20) != 0) { // check for return parameters from exec
            m_Status ^= 0x20;
            int n = Check(dbnumrets(GetCmd()));
            if (n > 0) {
                SetResultSet( new CDBL_ParamResult(GetConnection(), GetCmd(), n) );
                m_RowCount= 1;

                return Create_Result(*GetResultSet());
            }
        }

        if ((m_Status & 0x40) != 0) { // check for ret status
            m_Status ^= 0x40;
            DBBOOL has_return_status = Check(dbhasretstat(GetCmd()));
            if (has_return_status) {
                SetResultSet( new CDBL_StatusResult(GetConnection(), GetCmd()) );
                m_RowCount= 1;

                return Create_Result(*GetResultSet());
            }
        }
#endif

        RETCODE rc = Check(dbresults(GetCmd()));

        switch (rc) {
        case SUCCEED:
#if !defined(FTDS_LOGIC)
            m_Status |= 0x60;
#endif
            if (DBCMDROW(GetCmd()) == SUCCEED) { // we could get rows in result

// This optimization is currently unavailable for MS dblib...
#ifndef MS_DBLIB_IN_USE /*Text,Image*/
                if (Check(dbnumcols(GetCmd())) == 1) {
                    int ct = Check(dbcoltype(GetCmd(), 1));
                    if ((ct == SYBTEXT) || (ct == SYBIMAGE)) {
                        SetResultSet( new CDBL_BlobResult(GetConnection(), GetCmd()) );
                    }
                }
#endif
                if ( !GetResultSet() ) {
                    SetResultSet( new CDBL_RowResult(GetConnection(), GetCmd(), &m_Status) );
                }

                m_RowCount= -1;

                return Create_Result(*GetResultSet());
            } else {
                m_RowCount = DBCOUNT(GetCmd());
                continue;
            }
        case NO_MORE_RESULTS:
            m_Status = 2;
            break;
        default:
            m_HasFailed = true;
            CHECK_DRIVER_WARNING( m_HasFailed, "an error was encountered by server", 220016 );
        }
        break;
    }

#if defined(FTDS_LOGIC)
    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_Status == 2) {
        m_Status = 4;
        int n = Check(dbnumrets(GetCmd()));
        if (n > 0) {
            SetResultSet( new CTDS_ParamResult(GetConnection(), GetCmd(), n) );
            m_RowCount = 1;

            return Create_Result(*GetResultSet());
        }
    }

    if (m_Status == 4) {
        m_Status = 6;
        DBBOOL has_return_status = Check(dbhasretstat(GetCmd()));
        if (has_return_status) {
            SetResultSet( new CTDS_StatusResult(GetConnection(), GetCmd()) );
            m_RowCount = 1;

            return Create_Result(*GetResultSet());
        }
    }
#endif

    m_WasSent = false;
    return 0;
}


bool CDBL_LangCmd::HasMoreResults() const
{
    return m_WasSent;
}

void CDBL_LangCmd::DumpResults()
{
    while(m_WasSent) {
        auto_ptr<CDB_Result> dbres( Result() );

        if( dbres.get() ) {
            if(GetConnection().GetResultProcessor()) {
                GetConnection().GetResultProcessor()->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch())
                    continue;
            }
        }
    }
}

bool CDBL_LangCmd::HasFailed() const
{
    return m_HasFailed;
}


int CDBL_LangCmd::RowCount() const
{
    return (m_RowCount < 0)? DBCOUNT(GetCmd()) : m_RowCount;
}


CDBL_LangCmd::~CDBL_LangCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CDBL_LangCmd::x_AssignParams()
{
    static const char s_hexnum[] = "0123456789ABCDEF";

    for (unsigned int n = 0; n < GetParams().NofParams(); n++) {
        if(GetParams().GetParamStatus(n) == 0) continue;
        const string& name  =  GetParams().GetParamName(n);
        CDB_Object&   param = *GetParams().GetParam(n);
        char          val_buffer[16*1024];
        const char*   type;
        string        cmd;

        if (name.length() > kDBLibMaxNameLen)
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
                sprintf(val_buffer, "'%s'\n",
            val.Value().AsString("M/D/Y h:m").c_str());
                break;
            }
            case eDB_DateTime: {
                CDB_DateTime& val =
                    dynamic_cast<CDB_DateTime&> (param);
                sprintf(val_buffer, "'%s:%.3d'\n",
            val.Value().AsString("M/D/Y h:m:s").c_str(),
            (int)(val.Value().NanoSecond()/1000000));
                break;
            }
            default:
                return false; // dummy for compiler
            }
            cmd += val_buffer;
        } else
            cmd += "NULL\n";
        if (Check(dbcmd(GetCmd(), (char*) cmd.c_str())) != SUCCEED)
            return false;
    }
    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.34  2006/11/28 20:08:08  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.33  2006/11/20 18:15:58  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.32  2006/09/21 16:20:48  ssikorsk
 * CDBL_Connection::Check --> CheckDead.
 *
 * Revision 1.31  2006/09/20 20:36:44  ssikorsk
 * Removed extra check with dbnumrets and dbhasretstat.
 *
 * Revision 1.30  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.29  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.28  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.27  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.26  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.25  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.24  2006/05/04 20:12:17  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.23  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.22  2005/12/06 19:30:32  ssikorsk
 * Revamp code to use GetResultSet/SetResultSet/ClearResultSet
 * methods instead of raw data access.
 *
 * Revision 1.21  2005/10/31 12:24:32  ssikorsk
 * Do not use separate include files for the msdblib driver.
 *
 * Revision 1.20  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.19  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.18  2005/08/09 14:54:37  ssikorsk
 * Stylistic changes.
 *
 * Revision 1.17  2005/07/07 20:34:16  vasilche
 * Added #include <stdio.h> for sprintf().
 *
 * Revision 1.16  2005/07/07 19:12:55  ssikorsk
 * Improved to support a ftds driver
 *
 * Revision 1.15  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.14  2004/05/18 18:30:36  gorelenk
 * PCH <ncbi_pch.hpp> moved to correct place .
 *
 * Revision 1.13  2003/06/05 16:01:13  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.12  2003/05/16 20:25:20  soussov
 * adds code to skip parameter if it was not set
 *
 * Revision 1.11  2002/07/22 20:02:59  soussov
 * fixes the RowCount calculations
 *
 * Revision 1.10  2002/07/02 16:05:49  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.9  2002/01/11 20:11:43  vakatov
 * Fixed CVS logs from prev. revision that messed up the compilation
 *
 * Revision 1.8  2002/01/10 22:05:52  sapojnik
 * MS-specific workarounds needed to use blobs via I_ITDescriptor
 * (see Text,Image)
 *
 * Revision 1.7  2002/01/08 18:10:18  sapojnik
 * Syabse to MSSQL name translations moved to interface_p.hpp
 *
 * Revision 1.6  2001/12/18 19:29:22  soussov
 * adds conversion from nanosecs to milisecs for datetime args
 *
 * Revision 1.5  2001/12/18 17:56:53  soussov
 * copy-paste bug in datetime processing fixed
 *
 * Revision 1.4  2001/12/18 16:47:11  soussov
 * fixes bug in datetime argument processing
 *
 * Revision 1.3  2001/12/13 23:48:44  soussov
 * inserts space after declare
 *
 * Revision 1.2  2001/10/24 16:41:13  lavr
 * File description changed to be of the same style as in other files
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
