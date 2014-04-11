/*  $Id$
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
 * Author: Sergey Sikorskiy
 *
 * File Description: Python DBAPI unit-test
 *
 * ===========================================================================
 */

#ifndef PYTHON_NCBI_DBAPI_TEST_H
#define PYTHON_NCBI_DBAPI_TEST_H

#include "../pythonpp/pythonpp_emb.hpp"

#include <corelib/test_boost.hpp>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////
class CTestArguments
{
public:
    CTestArguments(void);

public:
    typedef map<string, string> TDatabaseParameters;

    enum EServerType {
        eUnknown,   //< Server type is not known
        eSybase,    //< Sybase server
        eMsSql,     //< Microsoft SQL server
    };

    string GetDriverName(void) const
    {
        return m_DriverName;
    }

    string GetServerName(void) const
    {
        if (NStr::CompareNocase(m_ServerName, "MsSql") == 0) {
#ifdef HAVE_LIBCONNEXT
            return "DBAPI_MS_TEST";
#else
            return "MSDEV1";
#endif
        } else if (NStr::CompareNocase(m_ServerName, "Sybase") == 0) {
#ifdef HAVE_LIBCONNEXT
            return "DBAPI_SYB155_TEST";
#else
            return "DBAPI_DEV3";
#endif
        }
        return m_ServerName;
    }

    string GetUserName(void) const
    {
        return m_UserName;
    }

    string GetUserPassword(void) const
    {
        return m_UserPassword;
    }

    const TDatabaseParameters& GetDBParameters(void) const
    {
        return m_DatabaseParameters;
    }

    string GetDatabaseName(void) const
    {
        return m_DatabaseName;
    }

    string GetServerTypeStr(void) const;
    EServerType GetServerType(void) const;

private:
    void SetDatabaseParameters(void);

private:

    string m_DriverName;
    string m_ServerName;
    string m_UserName;
    string m_UserPassword;
    string m_DatabaseName;
    TDatabaseParameters m_DatabaseParameters;
};

END_NCBI_SCOPE

#endif  // PYTHON_NCBI_DBAPI_TEST_H

