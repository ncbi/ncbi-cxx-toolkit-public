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
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <dbapi/simple/sdbapi.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>


BEGIN_NCBI_SCOPE

class CMyThread : public CThread
{
public:
    CMyThread(int num) : m_Num(num) {}

    virtual void* Main(void) {
        //for (int i = 0; i < 10; ++i) {
        for (;;) {
            try {
                CDatabase db("MSDEV");
                db.Connect();
                CQuery q = db.NewQuery("select serverproperty('servername')");
                q.Execute();
                LOG_POST("Thread " << m_Num << " connected to " << q.begin()[1].AsString());
                try {
                    q.SetSql("drop table #t");
                    q.Execute();
                }
                catch (CSDB_Exception& _DEBUG_ARG(ex)) {
                    _TRACE(ex);
                    //
                }
                q.SetSql("create table #t (tt varchar(100))");
                q.Execute();
                q.SetSql("insert into #t values (@tt)");
                for (int i = 0; i < 500; ++i) {
                    q.SetParameter("@tt", i, eSDB_String);
                    q.Execute();
                }
                SleepMilliSec(100);
            }
            catch (CSDB_Exception& ex) {
                LOG_POST("Error from SDBAPI in thread " << m_Num << ": " << ex);
            }
        }
        return NULL;
    }

private:
    int m_Num;
};

class CMyApplication : public CNcbiApplication
{
public:
    int Run(void)
    {
        CMyThread* thr[10];
        for (int i = 0; i < 10; ++i) {
            thr[i] = new CMyThread(i);
            thr[i]->Run();
        }

        //for (int i = 0; i < 10; ++i) {
        for (;;) {
            string errMsg;
            list<string> servers;
            CSDBAPI::EMirrorStatus status = CSDBAPI::UpdateMirror("MSDEV", &servers, &errMsg);
            if(status == CSDBAPI::eMirror_Unavailable)
                ERR_POST("SQL mirror check failed, error: " << errMsg);
            else if(status == CSDBAPI::eMirror_NewMaster)
                ERR_POST(Warning << "Master server has been switched. New master: " 
                                 << (servers.empty() ? kEmptyStr : servers.front()));
            else if(status != CSDBAPI::eMirror_Steady)
                ERR_POST("DB connection pool unknown status.");
            SleepMilliSec(1000);
        }

        for (int i = 0; i < 10; ++i) {
            thr[i]->Join();
        }
        return 0;
    }
};

END_NCBI_SCOPE

int main(int argc, char* argv[])
{
    return ncbi::CMyApplication().AppMain(argc, argv);
}
