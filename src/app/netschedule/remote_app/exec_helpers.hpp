#ifndef __REMOTE_APP_EXEC_HELPERS__HPP
#define __REMOTE_APP_EXEC_HELPERS__HPP

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
 * Authors: Maxim Didenko
 *
 * File Description:
 *
 */

#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE

class CFile;
bool CanExecRemoteApp(const CFile& file);


class CWorkerNodeJobContext;
bool ExecRemoteApp(const string& cmd, 
                   const vector<string>& args,
                   CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
                   int& exit_value,
                   CWorkerNodeJobContext& context,
                   int max_app_running_time,
                   int app_running_time,
                   int keep_alive_period,
                   const string& tmp_path,
                   const char* const env[] = 0);


class IRegistry;
class CRemoteAppParams 
{
public:
    CRemoteAppParams();

    void Load(const string& sec_name, const IRegistry&);

    const string& GetAppPath() const { return m_AppPath; }
    int GetMaxAppRunningTime() const { return m_MaxAppRunningTime; }
    int GetKeepAlivePeriod() const { return m_KeepAlivePeriod; }
    bool FailOnNonZeroExit() const { return m_FailOnNonZeroExit; }
    bool RunInSeparateDir() const { return m_RunInSeparateDir; }
    const string& GetTempDir() const { return m_TempDir; }

private:
    string m_AppPath;
    int m_MaxAppRunningTime;
    int m_KeepAlivePeriod;
    bool m_FailOnNonZeroExit;
    bool m_RunInSeparateDir;
    string m_TempDir;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/05/30 16:43:36  didenko
 * Moved the commonly used code to separate files.
 *
 * ===========================================================================
 */

#endif 

