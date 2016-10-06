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

/// @file seqdb_demo.cpp
/// Task demonstration application for SeqDB.

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <corelib/ncbimtx.hpp>
#include <unistd.h>
#include <sstream>

BEGIN_NCBI_SCOPE

DEFINE_STATIC_MUTEX(s_mutex);


/// Demo case base class.
class ISeqDBDemoCase : public CObject {
public:
    /// Destructor
    virtual ~ISeqDBDemoCase()
    {
    }

    /// Show description for this test case.
    virtual void DisplayHelp() = 0;

    /// Run this test case.
    virtual void Run() = 0;
};


/// Demo for GetSequence() methods.
class CSeqDBDemo_GetSequence : public ISeqDBDemoCase {
public:
    /// Destructor
    virtual ~CSeqDBDemo_GetSequence()
    {
    }

    /// Show description for this test case.
    virtual void DisplayHelp()
    {
        cout << "    GetSequence() provides a basic interface to fetch\n"
             << "    a sequence from a SeqDB object given an OID.\n";
    }

    /// Run this test case.
    virtual void Run()
    {
        CSeqDB nr("nr", CSeqDB::eProtein);

        TGi gi = GI_CONST(129295);
        int oid(0);

        if (nr.GiToOid(gi, oid)) {
            const char * buffer(0);
            unsigned length = nr.GetSequence(oid, & buffer);

            int alanine = 1; // encoding for the amino acid alanine
            unsigned count_a = 0;

            for(unsigned i = 0; i<length; i++) {
                if (buffer[i] == alanine) {
                    ++ count_a;
                }
            }

            cout << "    Found " << count_a
                 << " alanines in sequence with GI " << gi
                 << "." << endl;

            nr.RetSequence(& buffer);
        } else {
            cout << "    Failed: could not find GI " << gi << endl;
        }
    }
};


/// Demo for simple (single threaded) iteration methods.
class CSeqDBDemo_SimpleIteration : public ISeqDBDemoCase {
public:
    /// Destructor
    virtual ~CSeqDBDemo_SimpleIteration()
    {
    }

    /// Show description for this test case.
    virtual void DisplayHelp()
    {
        cout << "    CheckOrFindOID() provides a simple OID based iteration\n"
             << "    over the database.  The method works well as the test\n"
             << "    clause of a for loop.  This example counts the number\n"
             << "    of sequences in the \"swissprot\" database, displaying\n"
             << "    the count and the combined length of the first 1000.\n";
    }

    /// Run this test case.
    virtual void Run()
    {
        CSeqDB sp("swissprot", CSeqDB::eProtein);

        int oid_count = 0;
        int length_1000 = 0;

        for(int oid = 0; sp.CheckOrFindOID(oid); oid++) {
            if (oid_count++ < 1000) {
                length_1000 += sp.GetSeqLength(oid);
            }
        }

        int measured = (oid_count > 1000) ? 1000 : oid_count;

        cout << "    Number of swissprot sequences in (from iteration):  "
             << oid_count << endl;

        cout << "    Number of sequences in swissprot (from index file): "
             << sp.GetNumSeqs() << endl;

        cout << "    Combined length of the first " << measured
             << " sequences: " << length_1000 << endl;
    }
};


/// Demo for chunk iteration methods (single-threaded).
class CSeqDBDemo_ChunkIteration : public ISeqDBDemoCase {
public:
    /// Destructor
    virtual ~CSeqDBDemo_ChunkIteration()
    {
    }

    /// Show description for this test case.
    virtual void DisplayHelp()
    {
        cout << "    GetNextOIDChunk() provides versatile iteration meant\n"
             << "    for multithreaded applications.  Each thread fetches\n"
             << "    a set of OIDs to work with, only returning for more\n"
             << "    when done with that set.  SeqDB guarantees that all\n"
             << "    OIDs will be assigned, and no OID will be returned\n"
             << "    more than once.\n\n"
             << "    The data will be returned in one of two forms, either\n"
             << "    as a pair of numbers representing a range of OIDs, or\n"
             << "    in a vector.  The number of OIDs desired is indicated\n"
             << "    by setting the size of the vector on input.\n";
    }

    /// Run this test case.
    virtual void Run()
    {
        CSeqDB sp("swissprot", CSeqDB::eProtein);

        // Summary data to collect

        int oid_count = 0;
        int length_1000 = 0;

        // This many OIDs will be fetched at once.

        int at_a_time = 1000;

        vector<int> oids;

        // These will be set to a range if SeqDB chooses to return one.

        int begin(0), end(0);

        // In a real multithreaded application, each thread might have
        // code like the loop inside the "iteration shell" markers,
        // all using a reference to the same SeqDB object and the same
        // local_state variable (if needed).  The locking inside SeqDB
        // will prevent simultaneous access - each thread will get
        // seperate slices of the OID range.


        // This (local_state) variable is only needed if more than one
        // iteration will be done.  The GetNextOIDChunk() call would
        // work the same way (in this example) with the last parameter
        // omitted, because we only iterate over the database once.
        //
        // Multiple, simultaneous, independant, iterations (ie. each
        // getting every OID), would need more than one "local_state"
        // variable.  Iterations pulling OIDs from the same collection
        // would need to share one variable.  If only three parameters
        // are specified to the GetNextOIDChunk() method, the method
        // uses a variable which is a field of the SeqDB object.

        int local_state = 0;

        // --- Start of iteration "shell" ---

        bool done = false;

        while(! done) {

            // The GetNextOIDChunk() uses the third argument to
            // determine how many OIDs the user needs, even if the
            // begin/end variables are used to return the range.  In
            // other words, at_a_time is an INPUT to this call, and
            // begin, end, and the size and contents of oids, are all
            // outputs.  local_state keeps track of the position in
            // the iteration - the only user adjustment of it should
            // be to reset it to 0 at the beginning of each iteration.

            switch(sp.GetNextOIDChunk(begin, end, at_a_time, oids, & local_state)) {
            case CSeqDB::eOidList:
                for(int index = 0; index < (int)oids.size(); index++) {
                    x_UseOID(sp, oids[index], oid_count, length_1000);
                }
                done = oids.empty();
                break;

            case CSeqDB::eOidRange:
                for(int oid = begin; oid < end; oid++) {
                    x_UseOID(sp, oid, oid_count, length_1000);
                }
                done = (begin == end);
                break;
            }
        }

        // --- End of iteration "shell" ---

        unsigned measured = (oid_count > 1000) ? 1000 : oid_count;

        cout << "    Sequences in swissprot (counted during iteration): "
             << oid_count << endl;
        cout << "    Sequences in swissprot (from database index file): "
             << sp.GetNumSeqs() << endl;
        cout << "    Combined length of the first " << measured
             << " sequences: " << length_1000 << endl;
    }

private:
    /// Use this OID as part of the set.
    void x_UseOID(
            CSeqDB& sp,
            int     oid,
            int&    oid_count,
            int&    length_1000
    )
    {
        if (oid_count++ < 1000) {
            length_1000 += sp.GetSeqLength(oid);
        }
    }
};


class CSeqDBDemo_Thread : public CThread
{
public:
    CSeqDBDemo_Thread(
            int index,
            int at_a_time,
            int max_length
    );

    virtual void* Main(void);
    virtual void OnExit(void);
    const int GetIndex(void) const;

    static void Init(CSeqDB& db, bool oid_shuffle, bool use_ambigs)
    {
        sm_sp = &db;
        sm_is_protein = (db.GetSequenceType() == CSeqDB::eProtein);
        sm_oid_shuffle = oid_shuffle;
        sm_use_ambigs = use_ambigs;
    }

    bool IsRunning(void) const
    {
        return m_Running;
    }

private:
    static CSeqDB* sm_sp;
    static bool    sm_is_protein;
    static bool    sm_oid_shuffle;
    static bool    sm_use_ambigs;
    static int     sm_oid_state;

    const int m_Index;
    const int m_AtATime;
    const int m_MaxLength;
    bool      m_Running;
    char*     m_Temp;
    // int* m_HeapVar;

    /// Use this OID as part of the set.
    long x_UseOID(
            int letter,
            int oid
    )
    {
        static const int kNuclCode = kSeqDBNuclNcbiNA8;

        static const int kLoops = 1;

        static char* mask = NULL;
        static char* lets = NULL;

        if (mask == NULL) {
            mask = new char[4];
            lets = new char[4];
            for (int i = 0; i < 4; ++i) {
                mask[i] = static_cast<char>(0x03 << (i << 1));
                lets[i] = static_cast<char>((letter & 0x03) << (i << 1));
            }
        }

        long count = 0;

        const char* buffer = NULL;
        if (!sm_is_protein  &&  sm_use_ambigs) {

            int length = sm_sp->GetAmbigSeq(oid, &buffer, kNuclCode);
            if (length > 0) {

                // Artificially inflate the amount of "work" being performed
                // on each oid.

                for (int loop = 0; loop < kLoops; ++loop) {
                    count = 0L;
                    const char* p = buffer;
                    for (int i = 0; i < length; ++i) {
                        if (*p++ == letter) {
                            ++count;
                        }
                    }
                }

            }
            sm_sp->RetAmbigSeq(&buffer);

        } else {

            int length = sm_sp->GetSequence(oid, &buffer);
            if (length > 0) {

                // Artificially inflate the amount of "work" being performed
                // on each oid.

                for (int loop = 0; loop < kLoops; ++loop) {
                    count = 0L;
                    const char* p = buffer;
                    if (sm_is_protein) {
                        for (int i = 0; i < length; ++i) {
                            if (*p++ == letter) {
                                ++count;
                            }
                        }
                    } else {
                        int nbytes = (length + 3) / 4;
                        for (int i = 0; i < nbytes; ++i) {
                            char c = *p++;
                            for (int j = 0; j < 4; ++j) {
                                if ((c & mask[j]) == lets[j]) {
                                    ++count;
                                }
                            }
                        }
                    }
                }

            }
            sm_sp->RetSequence(&buffer);

        }

        return count;
    }
};

CSeqDB* CSeqDBDemo_Thread::sm_sp          = NULL;
bool    CSeqDBDemo_Thread::sm_is_protein  = false;
bool    CSeqDBDemo_Thread::sm_oid_shuffle = false;
bool    CSeqDBDemo_Thread::sm_use_ambigs  = false;
int     CSeqDBDemo_Thread::sm_oid_state   = 0;

CSeqDBDemo_Thread::CSeqDBDemo_Thread(
        int index,
        int at_a_time,
        int max_length
) : m_Index(index), m_AtATime(at_a_time), m_MaxLength(max_length),
        m_Running(true), m_Temp(NULL)
{
}

void* CSeqDBDemo_Thread::Main(void)
{
    long* retval = new long;

    // m_HeapVar = new int;
    // *m_HeapVar = 12345;

    // Summary data to collect
    long count = 0;

    // This will be set to a collection of oids
    // if SeqDB chooses to return one.
    vector<int> oids;

    // These will be set to a range if SeqDB chooses to return one.
    int oid_begin(0), oid_end(0);

    bool done = false;
    while (!done) {

        // The GetNextOIDChunk() uses the third argument to
        // determine how many OIDs the user needs, even if the
        // begin/end variables are used to return the range.  In
        // other words, at_a_time is an INPUT to this call, and
        // begin, end, and the size and contents of oids, are all
        // outputs.  local_state keeps track of the position in
        // the iteration - the only user adjustment of it should
        // be to reset it to 0 at the beginning of each iteration.

        const int letter = 1;

        if (sm_use_ambigs) {
            if (
                    sm_sp->GetNextOIDChunk(
                            oid_begin,
                            oid_end,
                            m_AtATime,
                            oids,
                            &sm_oid_state
                    ) == CSeqDB::eOidRange
            ) {
                oids.clear();
                for (int oid = oid_begin; oid < oid_end; ++oid) {
                    oids.push_back(oid);
                }
            }
        } else {
            if (
                    sm_sp->GetNextOIDChunk(
                            oid_begin,
                            oid_end,
                            m_AtATime,
                            oids,
                            &sm_oid_state
                    ) == CSeqDB::eOidRange
            ) {
                oids.clear();
                for (int oid = oid_begin; oid < oid_end; ++oid) {
                    oids.push_back(oid);
                }
            }
        }

        if (sm_oid_shuffle) {
            random_shuffle(oids.begin(), oids.end());
        }
        ITERATE(vector<int>, oid, oids) {
            count += x_UseOID(letter, *oid);
        }
        done = oids.empty();

    }

    std::ostringstream oss;
    oss << "Thread " << m_Index << " says: "
            << count << " occurrences of A" << endl;

    s_mutex.Lock();
    cout << oss.str() << flush;
    s_mutex.Unlock();

    *retval = count;
    return retval;
}

void CSeqDBDemo_Thread::OnExit(void)
{
    // Delete here anything declared on the heap in Main,
    // except for the return value.  All such items should
    // have their pointers as data members of this class.

    // delete m_HeapVar;

    if (m_Temp != NULL) {
        delete [] m_Temp;
    }

    m_Running = false;
}

const int CSeqDBDemo_Thread::GetIndex(void) const
{
    return m_Index;
}


/// Demo for chunk iteration methods (multi-threaded).
class CSeqDBDemo_Threaded : public ISeqDBDemoCase {
private:

    static string sm_DbName;
    static int    sm_NumThreads;
    static int    sm_OidBatchSize;
    static bool   sm_ListVols;
    static bool   sm_NoMmap;
    static bool   sm_OidShuffle;
    static bool   sm_UseAmbigs;

public:

    /// Destructor
    virtual ~CSeqDBDemo_Threaded()
    {
    }

    static void SetDbName(const string& dbname)
    {
        sm_DbName = dbname;
    }

    static void SetNumThreads(const int nthreads)
    {
        sm_NumThreads = nthreads;
    }

    static void SetListVols(void)
    {
        sm_ListVols = true;
    }

    static void SetNoMmap(void)
    {
        sm_NoMmap = true;
    }

    static void SetOidShuffle(void)
    {
        sm_OidShuffle = true;
    }

    static void SetUseAmbigs(void)
    {
        sm_UseAmbigs = true;
    }

    static void SetOidBatchSize(const int num)
    {
        sm_OidBatchSize = num;
    }

    /// Show description for this test case.
    virtual void DisplayHelp()
    {
        cout << "    GetNextOIDChunk() provides versatile iteration meant\n"
             << "    for multithreaded applications.  Each thread fetches\n"
             << "    a set of OIDs to work with, only returning for more\n"
             << "    when done with that set.  SeqDB guarantees that all\n"
             << "    OIDs will be assigned, and no OID will be returned\n"
             << "    more than once.\n\n"
             << "    The data will be returned in one of two forms, either\n"
             << "    as a pair of numbers representing a range of OIDs, or\n"
             << "    in a vector.  The number of OIDs desired is indicated\n"
             << "    by setting the size of the vector on input.\n";
    }

    /// Run this test case.
    virtual void Run()
    {
        // Open and configure access to the database.
        CSeqDB sp(sm_DbName, CSeqDB::eUnknown, 0, 0, !sm_NoMmap, NULL);
        sp.SetNumberOfThreads(sm_NumThreads, true);
        if (sp.GetSequenceType() == CSeqDB::eProtein) {
                cout << "Sequence type is PROTEIN" << endl;
        } else {
                cout << "Sequence type is NUCLEOTIDE" << endl;
        }

        // Set the database for the thread class.
        CSeqDBDemo_Thread::Init(sp, sm_OidShuffle, sm_UseAmbigs);

        if (sm_ListVols) {
            // Show the base names of all volumes.
            vector<string> paths;
            bool recursive = true;
            sp.FindVolumePaths(paths, recursive);
            cout << "Volume paths:" << endl;
            ITERATE(vector<string>, path, paths) {
                cout << "\t" << *path << endl;
            }
        }

        // Get the length of the largest sequence in the database.
        int max_length = sp.GetMaxLength();

        // The locking inside SeqDB will prevent simultaneous access -
        // each thread will get separate slices of the OID range.
        //
        // The (local_state) variable is only needed if more than one
        // iteration will be done.  The GetNextOIDChunk() call would
        // work the same way (in this example) with the last parameter
        // omitted, because we only iterate over the database once.
        //
        // Multiple, simultaneous, independent, iterations (ie. each
        // getting every OID), would need more than one "local_state"
        // variable.  Iterations pulling OIDs from the same collection
        // would need to share one variable.  If only three parameters
        // are specified to the GetNextOIDChunk() method, the method
        // uses a variable which is a field of the SeqDB object.

        vector<CSeqDBDemo_Thread*> threads;

        for (int index = 1; index <= sm_NumThreads; ++index) {
            CSeqDBDemo_Thread* thread(
                    new CSeqDBDemo_Thread(
                            index,
                            sm_OidBatchSize,
                            max_length
                    )
            );
            threads.push_back(thread);
        }

        NON_CONST_ITERATE(vector<CSeqDBDemo_Thread*>, thread, threads) {
            (*thread)->Run();
        }

        enum ETimeUnits {
            eNsec = 1, eUsec = 1000, eMsec = 1000 * 1000
        };

        timespec time_delay, time_rem;
        time_delay.tv_sec = 0;
        time_delay.tv_nsec = 100 * eMsec;

        long sumval = 0;
        vector<CSeqDBDemo_Thread*>::iterator thr;
        while (!threads.empty()) {
            bool found = false;
            NON_CONST_ITERATE(
                    vector<CSeqDBDemo_Thread*>,
                    thread,
                    threads
            ) {
                CSeqDBDemo_Thread* th = *thread;
                if (!th->IsRunning()) {
                    thr = thread;
                    found = true;
                    break;
                }
            }
            if (found) {
                long* retval;
                (*thr)->Join(reinterpret_cast<void**>(&retval));
                s_mutex.Lock();
                cout << "Thread " << (*thr)->GetIndex() << " returned "
                        << *retval << endl;
                s_mutex.Unlock();
                threads.erase(thr);
                sumval += *retval;
            } else {
                nanosleep(&time_delay, &time_rem);
            }
        }
        cout << "Threads combined returned " << sumval << endl;
    }
};

string CSeqDBDemo_Threaded::sm_DbName       ("swissprot");
int    CSeqDBDemo_Threaded::sm_NumThreads   (4);
int    CSeqDBDemo_Threaded::sm_OidBatchSize (100);
bool   CSeqDBDemo_Threaded::sm_ListVols     (false);
bool   CSeqDBDemo_Threaded::sm_NoMmap       (false);
bool   CSeqDBDemo_Threaded::sm_OidShuffle   (false);
bool   CSeqDBDemo_Threaded::sm_UseAmbigs    (false);


/// Demo for fetching a bioseq from a seqid methods.
class CSeqDBDemo_SeqidToBioseq : public ISeqDBDemoCase {
public:
    /// Destructor
    virtual ~CSeqDBDemo_SeqidToBioseq()
    {
    }

    /// Show description for this test case.
    virtual void DisplayHelp()
    {
        cout << "    SeqidToBioseq() provides a basic interface to fetch\n"
             << "    sequences from a SeqDB object.  Given a Seq-id, the\n"
             << "    method returns the first matching CBioseq found in\n"
             << "    the database.\n";
    }

    /// Run this test case.
    virtual void Run()
    {
        CSeqDB sp("swissprot", CSeqDB::eProtein);

        string str("gi|129295");

        CSeq_id seqid(str);
        CRef<CBioseq> bs = sp.SeqidToBioseq(seqid);

        if (! bs.Empty()) {
            if (bs->CanGetInst()) {
                if (bs->GetInst().CanGetLength()) {
                    cout << "    Length of sequence \"" << str
                         << "\": " << bs->GetInst().GetLength() << endl;
                } else {
                    cout << "    Failed: could not get length from CSeq_inst."
                         << endl;
                }
            } else {
                cout << "    Failed: could not get CSeq_inst from CBioseq."
                     << endl;
            }
        } else {
            cout << "    Failed: could not get CBioseq from SeqDB." << endl;
        }
    }
};


/// Run one or more test cases.
extern "C"
int main(int argc, char ** argv)
{
    // Build a set of available test options

    typedef map< string, CRef<ISeqDBDemoCase> > TDemoSet;
    TDemoSet demo_set;

    demo_set["-get-sequence"].Reset(new CSeqDBDemo_GetSequence);
    demo_set["-iteration-simple"].Reset(new CSeqDBDemo_SimpleIteration);
    demo_set["-iteration-chunk"].Reset(new CSeqDBDemo_ChunkIteration);
    demo_set["-iteration-threaded"].Reset(new CSeqDBDemo_Threaded);
    demo_set["-seqid-to-bioseq"].Reset(new CSeqDBDemo_SeqidToBioseq);

    // Additional options recognized:
    // These are used only by "-iteration-threaded" test.
    // -db <database>
    // -list_vols
    // -no_mmap
    // -num_threads <number of threads>
    // -oid_batch <number of oids>
    // -shuffle_oids
    // -use_ambigs (only applies to nucleotide searches)

    // Create a list for the tests which will be queued.
    list< CRef<ISeqDBDemoCase> > demo_list;

    // Parse and attempt to run everything the user throws at us.

    bool display_help = false;

    cout << endl;
    if (argc > 1) {
        for (int arg = 1; arg < argc; arg++) {
            char* args = argv[arg];
            if (string(args) == "-db") {
                if (++arg == argc) {
                    cout << "** No database name specified. **\n" << endl;
                    return -1;
                } else {
                    CSeqDBDemo_Threaded::SetDbName(string(argv[arg]));
                }
            } else if (string(args) == "-num_threads") {
                if (++arg == argc) {
                    cout << "** Number of threads not given. **\n" << endl;
                    return -1;
                } else {
                    CSeqDBDemo_Threaded::SetNumThreads(atoi(argv[arg]));
                }
            } else if (string(args) == "-oid_batch") {
                if (++arg == argc) {
                    cout << "** Number of oids not given. **\n" << endl;
                    return -1;
                } else {
                    CSeqDBDemo_Threaded::SetOidBatchSize(atoi(argv[arg]));
                }
            } else if (string(args) == "-list_vols") {
                CSeqDBDemo_Threaded::SetListVols();
            } else if (string(args) == "-no_mmap") {
                CSeqDBDemo_Threaded::SetNoMmap();
            } else if (string(args) == "-shuffle_oids") {
                CSeqDBDemo_Threaded::SetOidShuffle();
            } else if (string(args) == "-use_ambigs") {
                CSeqDBDemo_Threaded::SetUseAmbigs();
            } else {
                TDemoSet::iterator it = demo_set.find(string(args));
                if (it == demo_set.end()) {
                    cout << "** Sorry, option [" << argv[arg]
                         << "] was not found. **\n" << endl;
                    display_help = true;
                } else {
                    cout << "Queueing test [" << argv[arg] << "]:" << endl;
                    demo_list.push_back(it->second);
                    cout << endl;
                }
            }
        }
    } else {
        cout << "  This application is meant to be read (as source code),\n"
             << "  stepped through in a debugger, or as a minimal test app.\n"
             << "  It demonstrates use of the CSeqDB library API to perform\n"
             << "  simple and/or common blast database operations.\n";

        display_help = true;
    }

    // If there was a problem, display usage messages

    if (display_help) {
        cout << "\nAvailable options:\n\n";

        NON_CONST_ITERATE(TDemoSet, demo, demo_set) {
            cout << demo->first << ":\n";
            demo->second->DisplayHelp();
            cout << endl;
        }

        return -1;
    }

    // Run the queued demos.

    NON_CONST_ITERATE(list<CRef<ISeqDBDemoCase> >, it, demo_list) {
        (*it)->Run();
    }

    return 0;
}

END_NCBI_SCOPE

