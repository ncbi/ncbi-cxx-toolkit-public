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
#include <objtools/readers/seqdb/seqdb.hpp>
#include <objects/seq/Seq_inst.hpp>

BEGIN_NCBI_SCOPE

class ISeqDBDemoCase : public CObject {
public:
    virtual ~ISeqDBDemoCase()
    {
    }
    virtual void DisplayHelp() = 0;
    virtual void Run() = 0;
};

class CSeqDBDemo_GetSequence : public ISeqDBDemoCase {
public:
    virtual ~CSeqDBDemo_GetSequence()
    {
    }
    
    virtual void DisplayHelp()
    {
        cout << "    GetSequence() provides a basic interface to fetch\n"
             << "    a sequence from a SeqDB object given an OID.\n";
    }
    
    virtual void Run()
    {
        CSeqDB nr("nr", CSeqDB::eProtein);
        
        int gi(129295);
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

class CSeqDBDemo_SimpleIteration : public ISeqDBDemoCase {
public:
    virtual ~CSeqDBDemo_SimpleIteration()
    {
    }
    
    virtual void DisplayHelp()
    {
        cout << "    CheckOrFindOID() provides a simple OID based iteration\n"
             << "    over the database.  The method works well as the test\n"
             << "    clause of a for loop.  This example counts the number\n"
             << "    of sequences in the \"swissprot\" database, displaying\n"
             << "    the count and the combined length of the first 1000.\n";
    }
    
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

class CSeqDBDemo_ChunkIteration : public ISeqDBDemoCase {
public:
    virtual ~CSeqDBDemo_ChunkIteration()
    {
    }
    
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
    
    virtual void Run()
    {
        CSeqDB sp("swissprot", CSeqDB::eProtein);
        
        // Summary data to collect
        
        int oid_count = 0;
        int length_1000 = 0;
        
        // This many OIDs will be fetched at once.
        
        int at_a_time = 1000;
        
        vector<int> oids;
        oids.resize(at_a_time);
        
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
            
            // The GetNextOIDChunk() uses the length of oids to
            // determine how many OIDs the user needs, even if the
            // begin/end variables are used to return the range.  In
            // other words, oids.size() is an INPUT to this call, and
            // begin, end, and the size and contents of oids, are all
            // outputs.  local_state keeps track of the position in
            // the iteration - the only user adjustment of it should
            // be to reset it to 0 at the beginning of each iteration.
            
            switch(sp.GetNextOIDChunk(begin, end, oids, & local_state)) {
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
    void x_UseOID(CSeqDB   & sp,
                  int        oid,
                  int      & oid_count,
                  int      & length_1000)
    {
        if (oid_count++ < 1000) {
            length_1000 += sp.GetSeqLength(oid);
        }
    }
};

class CSeqDBDemo_SeqidToBioseq : public ISeqDBDemoCase {
public:
    virtual ~CSeqDBDemo_SeqidToBioseq()
    {
    }
    
    virtual void DisplayHelp()
    {
        cout << "    SeqidToBioseq() provides a basic interface to fetch\n"
             << "    sequences from a SeqDB object.  Given a Seq-id, the\n"
             << "    method returns the first matching CBioseq found in\n"
             << "    the database.\n";
    }
    
    virtual void Run()
    {
        CSeqDB sp("swissprot", CSeqDB::eProtein);
        
        string str("gi|129295|");
        
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

extern "C"
int main(int argc, char ** argv)
{
    // Build a set of command line options
    
    typedef map< string, CRef<ISeqDBDemoCase> > TDemoSet;
    TDemoSet demo_set;
    
    demo_set["-get-sequence"].Reset(new CSeqDBDemo_GetSequence);
    demo_set["-iteration-simple"].Reset(new CSeqDBDemo_SimpleIteration);
    demo_set["-iteration-chunk"].Reset(new CSeqDBDemo_ChunkIteration);
    demo_set["-seqid-to-bioseq"].Reset(new CSeqDBDemo_SeqidToBioseq);
    
    // Parse and attempt to run everything the user throws at us
    
    bool display_help = false;
    
    cout << endl;
    if (argc > 1) {
        for(int arg = 1; arg < argc; arg++) {
            TDemoSet::iterator it = demo_set.find(string(argv[arg]));
            
            if (it == demo_set.end()) {
                cout << "** Sorry, option [" << argv[arg]
                     << "] was not found. **\n" << endl;
                display_help = true;
            } else {
                cout << "Running test [" << argv[arg] << "]:" << endl;
                it->second->Run();
                cout << endl;
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
    }
    
    return 0;
}

END_NCBI_SCOPE

