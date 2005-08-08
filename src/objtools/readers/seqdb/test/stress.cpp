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
 * Author:  Kevin Bealer
 *
 */

/// "Stress" test routine.
/// 
/// This test is meant as a "pressure cooker" type test for SeqDB in
/// multithreaded mode.  It doesn't focus on testing any particular
/// type of request or usage scenario.  Instead, the test tries to
/// "break" SeqDB's thread safety by running lots of requests at high
/// speed from multiple threads.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>

USING_NCBI_SCOPE;


class CStressTestThread : public CThread {
public:
    CStressTestThread(CRef<CSeqDB> db, int num_iter, int oid_range);
    ~CStressTestThread();
    
    virtual void* Main(void);
    virtual void OnExit(void);
    
private:
    CRef<CSeqDB> m_Db;
    int          m_Count;
    int          m_OidRange;
};

string short_log(double x)
{
    // 36 levels
    char * z = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ#";
    
    double j = x;
    int i = 0;
    
    while((j > 10) && (i < (10+26+26))) {
        j = j * .9;
        i ++;
    }
    
    return string("<") + string(z, i, 1) + string(">");
}

void * CStressTestThread::Main(void)
{
    int qq = 0;
    
    vector<const char *> rawptr;
    vector< CRef<CBioseq> > keepers;
    
    rawptr.resize(10);
    keepers.resize(10);
    
    int keep_i = 0;
    
    for(int iter = 0; iter<m_Count; iter++) {
        switch(rand() % 4) {
        case 0:
            // GetSeqLength()
            qq += m_Db->GetSeqLength(rand() % m_OidRange);
            break;
            
        case 1:
            // GetBioseq()
            keep_i = keep_i ? (keep_i-1) : 9;
            keepers[keep_i] = m_Db->GetBioseq(rand() % m_OidRange);
            break;
            
        case 2:
            // GetSequence()
            {
                int raw_i = rand() % 10;
                
                if (rawptr[raw_i]) {
                    m_Db->RetSequence(& rawptr[raw_i]);
                }
                
                qq += m_Db->GetSequence(rand() % m_OidRange, & rawptr[raw_i]);
            }
            break;
            
        case 3:
            // GiToOid(), OidToGi(), SeqIdToOid()
            {
                int oid1 = rand() % m_OidRange;
                int gi(0);
                
                if (m_Db->OidToGi(oid1, gi)) {
                    string ident = string("gi|") + NStr::IntToString(gi);
                   
                    CSeq_id seqid(ident);
                   
                    int oid2(-1);
                   
                    if (m_Db->SeqidToOid(seqid, oid2)) {
                        if (oid1 != oid2) {
                            cout << "Error: oid #" << oid1 << ", gi " << gi
                                 << " failed three-way conversion test." << endl;
                        }
                    }
                }
                break;
            }
            break;
        }
    }
    
    for(int i = 0; i<10; i++) {
        if (rawptr[i]) {
            m_Db->RetSequence(& rawptr[i]);
        }
        keepers[i].Reset();
    }
    
    cout << short_log(double(qq) / m_Count) << flush;
    
    return NULL;
}

void CStressTestThread::OnExit(void)
{
}

CStressTestThread::CStressTestThread(CRef<CSeqDB> db, int num_iter, int oid_range)
    : m_Db(db), m_Count(num_iter), m_OidRange(oid_range)
{
}

CStressTestThread::~CStressTestThread()
{
}

void run_stress(CRef<CSeqDB> db, int num_threads, int num_iter, int oid_range)
{
    vector< CRef<CStressTestThread> > thread_set;
    
    for(int i = 0; i<num_threads; i++) {
        CRef<CStressTestThread> thread(new CStressTestThread(db, num_iter, oid_range));
        thread->Run();
        
        thread_set.push_back(thread);
    }
    
    for (int i = 0; i < num_threads; i++) {
        thread_set[i]->Join();
    }
}

class CStressArgs {
public:
    typedef map< string, string > TArgMap;
    
    void AddValues(const vector<string> & args)
    {
        for(size_t i = 0; i < args.size();) {
            string s1 = args[i++];
            
            if (i == args.size()) {
                m_Argmap[s1] = "T";
            } else {
                string s2 = args[i];
                
                switch(s2[0]) {
                case '-':
                    m_Argmap[s1] = "T";
                    break;
                    
                case '\\':
                    s2.erase(0);
                    
                default:
                    m_Argmap[s1] = s2;
                    i++;
                    break;
                }
            }
        }
    }
    
    bool Get(const string & key, int & value)
    {
        if (m_Argmap.find(key) != m_Argmap.end()) {
            value = NStr::StringToInt(m_Argmap[key]);
            return true;
        }
        return false;
    }
    
    bool Get(const string & key, string & value)
    {
        if (m_Argmap.find(key) != m_Argmap.end()) {
            value = m_Argmap[key];
            return true;
        }
        return false;
    }
    
    bool Get(const string & key, bool & value)
    {
        if (m_Argmap.find(key) != m_Argmap.end()) {
            value = (m_Argmap[key] == "T");
            return true;
        }
        return false;
    }
    
private:
    TArgMap m_Argmap;
};

bool read_arguments(vector<string>   & arglist,
                    int              & num_threads,
                    int              & max_iter,
                    int              & oid_range,
                    string           & dbname,
                    CSeqDB::ESeqType & seqtype)
{
    CStressArgs args;
    
    args.AddValues(arglist);
    
    bool flag = false;
    
    if (args.Get("-h", flag) ||
        args.Get("-?", flag) ||
        args.Get("-", flag)) {
        
        cout << "\nUsage:\n"
             << "  -h | -? | -     : this help message\n"
             << "  -threads <int>  : number of threads\n"
             << "  -max-iter <int> : number of iterations to run\n"
             << "  -oids <int>     : oid range to use (0..N)\n"
             << "  -db <string>    : database name\n"
             << "  -nucl           : database is nucleotide\n"
             << "\n" << endl;
        
        return false;
    }
    
    bool is_nucl = false;
    
    args.Get("-threads",  num_threads);
    args.Get("-max-iter", max_iter);
    args.Get("-oids",     oid_range);
    args.Get("-dbname",   dbname);
    
    if (args.Get("-nucl", is_nucl)) {
        seqtype = is_nucl ? CSeqDB::eNucleotide : CSeqDB::eProtein;
    }
    
    return true;
}

void stress_test_seqdb(const list<string> & args_list)
{
    int num_threads = 20;
    int max_iter    = 20000;
    int oid_range   = 1000000;
    
    string dbname("nr");
    CSeqDB::ESeqType seqtype(CSeqDB::eProtein);
    
    vector<string> argv(args_list.begin(), args_list.end());
    
    if (! read_arguments(argv,
                         num_threads,
                         max_iter,
                         oid_range,
                         dbname,
                         seqtype)) {
        return;
    }
    
    cout << "Settings:"
         << "\n  threads:   " << num_threads
         << "\n  max_iter:  " << max_iter
         << "\n  oid_range: " << oid_range
         << "\n  dbname:    " << dbname
         << "\n  seqtype:   " << seqtype
         << "\n" << endl;
    
    // SeqDB
    
    CRef<CSeqDB> db(new CSeqDB(dbname, seqtype));
    
    int count = 100;
    
    while(count < max_iter) {
        CStopWatch sw(true);
        
        cout << "Count: " << count << " ... " << flush;
        
        run_stress(db, num_threads, count, oid_range);
        
        cout << ": done, Elapsed: " << sw.Elapsed() << endl;
        
        
        count = count + (count/4);
    }
}


