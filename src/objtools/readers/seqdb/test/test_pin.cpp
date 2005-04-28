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

/// test_pin application.
/// 
/// This test application serves as a CVS managed tool bench and
/// scratch pad where I (Kevin Bealer) can keep routines that test the
/// stability and performance of, and new functionality added to, the
/// seqdb library.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>
#include <util/random_gen.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/blastdb/blastdb__.hpp>

#include <assert.h>

#if defined(NCBI_OS_UNIX)
#include <readdb.h>
#endif

volatile int gnum = 0;

USING_NCBI_SCOPE;

static void
s_TokenizeKeepDelims(const string   & input,
                     const string   & delim,
                     vector<string> & results,
                     vector<char>   & taken)
{
    results.clear();
    taken.clear();
    
    NStr::Tokenize(input, delim, results);
    
    int numres = (int) results.size();
    
    if ((! results.empty()) && results[numres-1].empty()) {
        results.resize(--numres);
    }
    
    {
        int orig_loc = 0;
        
        for(int i = 0; i < numres; i++) {
            orig_loc += (int) results[i].length();
            
            if (orig_loc < (int) input.size()) {
                taken.push_back(input[orig_loc++]);
            }
        }
    }
    
    {
        string reconstr;
        
        for(int i = 0; i < numres; i++) {
            reconstr += results[i];
            if (i < (int) taken.size()) {
                reconstr += taken[i];
            }
        }
    }
}

static void
s_RebuildTokens(vector<string> & results,
                vector<char>   & taken,
                string         & reconstr)
{
    for(int i = 0; i < (int)results.size(); i++) {
        reconstr += results[i];
        
        if (i < (int)taken.size()) {
            reconstr += taken[i];
        }
    }
}

static void
s_TokenizeAndGroupQuotes(const string   & inp,
                         vector<string> & results,
                         vector<char>   & taken)
{
    string delimchars("'\"");
    
    s_TokenizeKeepDelims(inp, delimchars, results, taken);
    
    // Allow embedding of ' within " and vice versa; does not (yet)
    // handle escaped quotes.
    
    for(unsigned i = 1; i < (results.size()-1); i++) {
        char d1 = delimchars[0];
        char d2 = delimchars[1];
        
        char delim = taken[i-1];
        char delim2 = (delim == d1) ? d2 : d1;
        
        while((i < taken.size()) &&
              (taken[i] == delim2) &&
              ((i+1) < results.size())) {
            
            results[i] += taken[i];
            results[i] += results[i+1];
            
            taken.erase(taken.begin() + i);
            results.erase(results.begin() + (i+1));
        }
        
        i++;
    }
}

static string
s_Asn1Transform(const string & inp)
{
    const int Nth = 80;
    
    vector<string> results;
    vector<char>   taken;
    
    s_TokenizeAndGroupQuotes(inp, results, taken);
    
    for(unsigned i = 0; i<results.size(); i++) {
        if (i & 1) {
            // Quoted area; remove newlines, then put back in after
            // every Nth character.
            
            results[i] = NStr::Replace(results[i], "\n", "");
            for(unsigned p = Nth; p < results[i].length(); p+= (Nth+1)) {
                results[i].insert(p, "\n");
            }
        } else {
            // Non-quoted area; remove spaces, tabs, and newlines.
            // Insert newlines back in after commas.
            
            results[i] = NStr::Replace(results[i], " ", "");
            results[i] = NStr::Replace(results[i], "\t", "");
            results[i] = NStr::Replace(results[i], "\n", "");
            results[i] = NStr::Replace(results[i], ",", ",\n");
        }
    }
    
    string recon;
    s_RebuildTokens(results, taken, recon);
    
    return recon;
}

static void s_MutateString(string & s_in)
{
    string s(s_in);
    
    static CRandom prng;
    static string  alph;
    
    if (alph.empty()) {
        string a;
        a += "0123456789";
        a += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        a += "abcdefghijklmnopqrstuvwxyz";
        alph.assign(a.data(), a.data() + a.size());
    }
    
    bool flip = true;
    
    while(flip && s.size()) {
        switch(prng.GetRand(0,3)) {
        case 0:
            {
                // Removal of character.
                int point = prng.GetRand(0, (int) s.size()-1);
                
                s.erase(s.begin() + point);
                break;
            }
            
        case 1:
            {
                // Insertion of character.
                
                int point = prng.GetRand(0, (int) s.size());
                int item  = prng.GetRand(0, (int) alph.size()-1);
                
                s.insert(point, 1, alph[item]);
                break;
            }
            
        case 2:
            {
                // Swap characters
                int point1 = prng.GetRand(0, (int) s.size()-1);
                int point2 = prng.GetRand(0, (int) s.size()-1);
                
                char ch = s[point1];
                s[point1] = s[point2];
                s[point2] = ch;
                break;
            }
            
        case 3:
            {
                // Mutate character
                
                int point = prng.GetRand(0, (int) s.size()-1);
                int item  = prng.GetRand(0, (int) alph.size()-1);
                
                s[point] = alph[item];
                break;
            }
        }
        
        if (prng.GetRand(0,1)) {
            if (s != s_in) {
                flip = false;
            }
        }
    }
    
//     cout << "changed [" << s_in << "]" << endl;
//     cout << "into... [" << s << "]" << endl;
    
    s_in = s;
}

/// A simple fixed value GI list class.

class CGiOidList : public CSeqDBGiList {
public:
    /// Build a GI list with three elements.
    CGiOidList()
    {
        m_GisOids.push_back(129295);
        m_GisOids.push_back(129297);
        m_GisOids.push_back(1071922);
    }
};


/// A simple vector based GI list class.

class CVectorGiList : public CSeqDBGiList {
public:
    /// Build a GI list from a vector.
    CVectorGiList(const vector<int> & v)
    {
        m_GisOids.resize(v.size());
        
        for(int i = 0; i<(int)v.size(); i++) {
            m_GisOids[i].gi = v[i];
        }
    }
};


/// Helper class for event timing.

class CTimedTask {
public:
    /// Start timing.
    /// @param msg Message to output at beginning and Mark().
    CTimedTask(string msg)
        : m_Msg(msg)
    {
        cout << "Executing task [" << m_Msg << "]..." << endl;
        m_Watch.Start();
        m_E1 = m_Watch.Elapsed();
    }
    
    /// Write timing output.
    double Mark()
    {
        double e2 = m_Watch.Elapsed();
        cout << "Completed task [" << m_Msg << "] in " << (e2-m_E1) << " seconds.\n" << endl;
        return e2-m_E1;
    }
    
private:
    /// Stop watch to keep time.
    CStopWatch m_Watch;
    
    /// Start time.
    double m_E1;
    
    /// Message for this task.
    string m_Msg;
};

int test1(int argc, char ** argv)
{
    string dbpath = "/net/fridge/vol/export/blast/db/blast";
    
    list<string> args;
    
    while(argc > 1) {
        args.push_front(string(argv[--argc]));
    }
    
    bool  use_mm        = true;
    bool  deletions     = true;
    int   num_display   = -1;
    int   num_itera     = 1;
    bool  look_seq      = false;
    bool  show_bioseq   = false;
    bool  show_fasta    = false;
    bool  get_bioseq    = false;
    bool  show_progress = true;
    bool  approx        = true;
    Uint8 membound      = 0;
    Uint8 slicesize     = 0;
    bool  defer_ret     = false;
    bool  x4mutate      = false;
    
    string dbname("nr");
    CSeqDB::ESeqType seqtype = CSeqDB::eProtein;
    
    bool failed      = false;
    
    while(! args.empty()) {
        string desc;
        
        string s = args.front();
        args.pop_front();
        
        if (s == "-mutexes") {
            /*Uint4 z = 0;*/
            
            CMutex a;
            CFastMutex b;
            
            CStopWatch sw(true);
            double spt1 = sw.Elapsed();
            
            for(int i = 0; i<10000000; i++) {
                CMutexGuard aa(a);
                
                gnum++;
            }
            
            double spt2 = sw.Elapsed();
            for(int i = 0; i<10000000; i++) {
                CFastMutexGuard bb(b);
                
                gnum++;
            }

            double spt3 = sw.Elapsed();
            for(int i = 0; i<10000000; i++) {
                gnum++;
            }

            double sptA = sw.Elapsed();
            for(int i = 0; i<10000000; i++) {
                CMutexGuard aa(a);
                
                gnum++;
            }
            
            double sptB = sw.Elapsed();
            for(int i = 0; i<10000000; i++) {
                CFastMutexGuard bb(b);
                
                gnum++;
            }
            
            double sptC = sw.Elapsed();
            for(int i = 0; i<10000000; i++) {
                gnum++;
            }
            
            double sptX = sw.Elapsed();
            
            cout << "\nS run1 = " << spt2 - spt1 << endl;
            cout <<   "S run2 = " << sptB - sptA << endl;
            cout << "\nF run1 = " << spt3 - spt2 << endl;
            cout <<   "F run2 = " << sptC - sptB << endl;
            cout << "\nC run1 = " << sptA - spt3 << endl;
            cout <<   "C run2 = " << sptX - sptC << endl;
            
            cout << "\nnothing " << gnum << endl;
            return 0;
        } else desc += " [-mutexes]";
        
        if (s == "-fromcpp") {
            const char * ge = getenv("BLASTDB");
            string pe("BLASTDB=/home/bealer/code/ut/internal/blast/unit_test/data:" + string(ge ? ge : ""));
            putenv((char*)pe.c_str());
            
            CSeqDB local1("seqp", CSeqDB::eProtein);
            CSeqDB local2("seqp", CSeqDB::eProtein, 0, 0, false);
            
            Int4 num1 = local1.GetNumSeqs();
            Int4 num2 = local1.GetNumSeqs();
            
            int rv = 0 | num1 | num2;
            
            assert(num1 >= 1);
            assert(num1 == num2);
            
            cout << "Test worked." << endl;
            return rv;
        } else desc += " [-fromcpp]";
        
        if (s == "-alphabeta") {
            
            // Note: this test is NOT EXPECTED to work, unless the
            // user has databases named "alpha" and "beta" somewhere
            // in their path.
            
            CSeqDB ab("alpha beta", CSeqDB::eProtein);
            
            for(int i = 0; i < ab.GetNumSeqs(); i++) {
                cout << "-----seq " << i << " length " << ab.GetSeqLength(i) << " ------------" << endl;
            }
            
            return 0;
        } else desc += " [-alphabeta]";
        
        if (s == "-bioseqs") {
            CSeqDB db(dbname, seqtype);
            
            unsigned gi (129297);                 // chicken receptor (gg)
            unsigned pig(123);                    // junctional adhesion molecule 3 (hs)
            string   str("sp|P35586|COCO_LIMPO"); // cocoonase (limulus polyphemus)
            
            CSeq_id  seqid(str);
            
            {
                cout << "--- gi " << gi << " ---" << endl;
                
                CRef<CBioseq> bs = db.GiToBioseq(gi);
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                *outpstr << *bs;
            }
            
            {
                cout << "\n--- pig " << pig << " ---" << endl;
                
                CRef<CBioseq> bs = db.PigToBioseq(pig);
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                *outpstr << *bs;
            }
            
            {
                cout << "\n--- seqid " << str << " ---" << endl;
                
                CRef<CBioseq> bs = db.SeqidToBioseq(seqid);
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                *outpstr << *bs;
                
                cout << endl;
            }
            
            return 0;
        } else desc += " [-bioseqs]";
        
        if ((s == "-gi2bs") || (s == "-gi2bs-target")) {
            CSeqDB db(dbname, seqtype);
            
            if (args.empty() || (! isdigit(args.begin()->c_str()[0]))) {
                cout << "The gi2bs command needs a GI to work with." << endl;
                return 1;
            }
            
            int gi = atoi(args.begin()->c_str());
            args.pop_front();
            
            int target_gi = (s == "-gi2bs-target") ? gi : 0;
            
            if (gi < 1) {
                cout << "The GI " << gi << " is not valid." << endl;
                return 1;
            }
            
            int oid(0);
            
            if (! db.GiToOid(gi, oid)) {
                cout << "The GI " << gi << " was not found." << endl;
                return 0;
            }
            
            CRef<CBioseq> bs = db.GetBioseq(oid, target_gi);
            
            cout << "--- gi " << gi << " ---" << endl;
            
            auto_ptr<CObjectOStream>
                outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
            
            *outpstr << *bs;
            
            return 0;
        } else desc += " [-gi2bs] [-gi2bs-target]";
        
        if ((s == "-gi2bs-ugl")) {
            if ((args.size() < 2) || (! isdigit(args.begin()->c_str()[0]))) {
                cout << "The gi2bs command needs a GI and user-gi-list filename to work with." << endl;
                return 1;
            }
            
            int gi = atoi(args.begin()->c_str());
            args.pop_front();
            
            CRef<CSeqDBGiList> gilist(new CSeqDBFileGiList(*args.begin()));
            args.pop_front();
            
            CSeqDB db(dbname, seqtype, 0, 0, true, gilist);
            
            int target_gi = 0;
            
            if (gi < 1) {
                cout << "The GI " << gi << " is not valid." << endl;
                return 1;
            }
            
            int oid(0);
            
            if (! db.GiToOid(gi, oid)) {
                cout << "The GI " << gi << " was not found." << endl;
                return 0;
            }
            
            CRef<CBioseq> bs = db.GetBioseq(oid, target_gi);
            
            cout << "--- gi " << gi << " ---" << endl;
            
            auto_ptr<CObjectOStream>
                outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
            
            *outpstr << *bs;
            
            return 0;
        } else desc += " [-gi2bs-ugl]";
        
        if (s == "-taxid") {
            int oid = 12;
            
            if (args.size() == 1) {
                oid = atoi((*args.begin()).c_str());
                args.pop_front();
            }
            
            cout << "Using oid: " << oid << endl;
            
            CSeqDB nr(dbname, seqtype);
            
            CRef<CBlast_def_line_set> bdls = nr.GetHdr(oid);
            
            vector<int> taxids;
            
            ITERATE(list< CRef<CBlast_def_line> >, iter, bdls->Get()) {
                taxids.push_back((**iter).GetTaxid());
            }
            
            ITERATE(vector<int>, it, taxids) {
                cout << "taxid: " << (*it) << endl;
            }
            
            return 0;
        } else desc += " [-gi2bs] [-gi2bs-target]";
        
        if ((s == "-paths") ||
            (s == "-paths-static") ||
            (s == "-paths-three")) {
            
            bool full_version = false;
            
            if (s == "-paths") {
                full_version = true;
            }
            
            if (s == "-paths-three") {
                cout << "Constructing prefetch version..." << endl;
                CSeqDB nr(dbname, seqtype);
                cout << "Done constructing prefetch version..." << endl;
                
                full_version = true;
            }
            
            CStopWatch sw(true);
            
            double e[4];
            
            vector<string> paths;
            vector<string> paths2;
            
            e[0] = sw.Elapsed();
            
            if (full_version) {
                CSeqDB nr(dbname, seqtype);
                
                e[1] = sw.Elapsed();
                
                nr.FindVolumePaths(paths);
            } else {
                e[1] = sw.Elapsed();
            }
            
            e[2] = sw.Elapsed();
            
            CSeqDB::FindVolumePaths(dbname, seqtype, paths2);
            
            e[3] = sw.Elapsed();
            
            cout << "\n-- non-static method "
                 << (e[1]-e[0]) << " to construct, "
                 << (e[2]-e[1]) << " to get paths." << endl;
            
            ITERATE(vector<string>, iter, paths) {
                cout << "    " << (*iter) << endl;
            }
            
            cout << "\n-- static method "
                 << (e[3]-e[2]) << " to get paths." << endl;
            
            ITERATE(vector<string>, iter, paths2) {
                cout << "    " << (*iter) << endl;
            }
            
            double ee2,ee3;
            
            for(int i = 0; i<10; i++) {
                ee2 = sw.Elapsed();
                
                CSeqDB::FindVolumePaths(dbname, seqtype, paths2);
                
                ee3 = sw.Elapsed();
                
                cout << "\n-- static method loop: "
                     << (ee3-ee2) << " to get paths." << endl;
            }
            
            return 0;
        } else desc += " [-paths | -paths-static | -paths-three]";
        
        if (s == "-here") {
            CSeqDB nr("tenth", CSeqDB::eProtein);
            
            for(int i = 0; i<100; i++) {
                CRef<CBioseq> bs = nr.GetBioseq(i);
                
                cout << "-----seq " << i << "------------" << endl;
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                
                *outpstr << *bs;
            }
            
            return 0;
        } else desc += " [-here]";
        
#if defined(NCBI_OS_UNIX)
        if ((s == "-bs9") ||
            (s == "-bs10") ||
            (s == "-bs-gi")) {
            // These require: -lncbitool -lz -lncbiobj -lncbi
            
            //const char * dbname1 = "nr";
            
            bool is_prot = true;
            int gi = 8;
            
            if (s == "-bs10") {
                is_prot = true;
                //dbname1 = "nr";
                //gi = 129291;
                gi = 2501325;
            }
            
            if ((s == "-bs-gi") && (! args.empty())) {
                is_prot = (seqtype == 'p');
                
                gi = atoi(args.front().c_str());
                args.pop_front();
            }
            
            ostringstream oss_fn;
            oss_fn << "." << dbname << "." << gi;
            
            vector<char> seqdb_data, readdb_data;
            string seqdb_bs, readdb_bs;
            
            {
                CSeqDB db(dbname, is_prot ? CSeqDB::eProtein : CSeqDB::eNucleotide);
                
                int oid(0);
                
                if (db.GiToOid(gi, oid)) {
                    CRef<CBioseq> bs = db.GetBioseq(oid);
                    
                    string fn = string("seqdb") + oss_fn.str() + ".txt";
                    ofstream outf(fn.c_str());
                    
                    auto_ptr<CObjectOStream> os1(CObjectOStream::Open(eSerial_AsnText, outf));
                    *os1 << *bs;
                    
                    cout << "seqdb: got bioseq." << endl;
                    
                    cout << "\n bs->inst->seq-data[0] = ";
                    
                    vector<char> byte_data;
                    
                    if (is_prot) {
                        byte_data = bs->GetInst().GetSeq_data().GetNcbistdaa().Get();
                    } else {
                        byte_data = bs->GetInst().GetSeq_data().GetNcbi4na().Get();
                    }
                    
                    cout << "Total bytes available: " << byte_data.size() << endl;
                    
                    cout << int(byte_data[0]) << endl;
                    
                    seqdb_data = byte_data;
                    
                    CMemoryFile mfile(fn);
                    seqdb_bs.assign((char*)mfile.GetPtr(), mfile.GetSize());
                } else {
                    cout << "seqdb: could not get bioseq." << endl;
                }
            }
            {
                ReadDBFILEPtr rdfp = readdb_new_ex2((char*) dbname.c_str(),
                                                    is_prot ? 1 : 0,
                                                    READDB_NEW_DO_TAXDB,
                                                    NULL,
                                                    NULL);
                
                int oid = readdb_gi2seq(rdfp, gi, 0);
                
                rdfp->gi_target = gi;
                
                BioseqPtr bsp = readdb_get_bioseq(rdfp, oid);
                
                if (bsp) {
                    string fn = string("readdb") + oss_fn.str() + ".txt";
                    
                    AsnIoPtr myaip = AsnIoOpen((char*) fn.c_str(), (char*)"w");
                    BioseqAsnWrite(bsp, myaip, NULL);
                    AsnIoClose(myaip);
                    
                    cout << "readdb: got bioseq." << endl;
                    
                    cout << "\n bs->inst->seq-data[0] = ";
                    
                    ByteStorePtr bstorep = bsp->seq_data;
                    
                    int bslen = bstorep->totlen;
                    
                    vector<char> byte_data;
                    byte_data.resize(bslen);
                    
                    cout << "Total bytes available: " << bslen << endl;
                    
                    // Annoyingly, this starts at the END, if you
                    // don't seek to the beginning.
                    
                    Nlm_BSSeek(bstorep, 0, SEEK_SET);
                    int i = BSRead(bstorep, & byte_data[0], bslen);
                    
                    cout << "Bytes read = " << i << endl;
                    
                    cout << int(byte_data[0]) << endl;
                    
                    readdb_data = byte_data;
                    
                    CMemoryFile mfile(fn);
                    readdb_bs.assign((char*)mfile.GetPtr(), mfile.GetSize());
                } else {
                    cout << "readdb: could not get bioseq." << endl;
                }
                
                BioseqFree(bsp);
            }
            
            int num_diffs = 0;
            
            for(int i = 0; i < (int)readdb_data.size(); i++) {
                unsigned R = unsigned(readdb_data[i]) & 0xFF;
                unsigned S = unsigned(seqdb_data[i])  & 0xFF;
                
                if (R != S) {
                    cout << "At location " << dec << i << ", Readdb has: " << hex << int(R) << " whereas SeqDB has: " << hex << int(S);
                    
                    if (R > S) {
                        cout << " (R += " << (R - S) << ")\n";
                    } else {
                        cout << " (S += " << (S - R) << ")\n";
                    }
                    
                    num_diffs ++;
                }
            }
            cout << "Num diffs: " << dec << num_diffs << endl;
            
            cout << "----ReadDB pre-transformed:----" << endl;
            cout << readdb_bs << endl;
            cout << "----SeqDB pre-transformed:----" << endl;
            cout << seqdb_bs << endl;
            
            string tran_r, tran_s;
            
            cout << "----ReadDB transformed:----" << endl;
            cout << (tran_r = s_Asn1Transform(readdb_bs)) << endl;
            cout << "----SeqDB transformed:----" << endl;
            cout << (tran_s = s_Asn1Transform(seqdb_bs)) << endl;
            cout << "------end-------" << endl;
            
            cout << "raw: " << ((seqdb_bs == readdb_bs) ? "eq" : "not eq") << endl;
            cout << "raw: " << ((tran_s == tran_r) ? "eq" : "not eq") << endl;
            
            return 0;
        } else desc += " [-bs9] [-bs9] [-bs-gi]";
#endif
        
        if (s == "-dyn") {
            CSeqDB db("nr", CSeqDB::eProtein);
            
            cout << "Num oids: " << db.GetNumSeqs() << endl;
            
            char * buf1 = 0;
            
            Int4 len = db.GetAmbigSeqAlloc(10,
                                           & buf1,
                                           kSeqDBNuclBlastNA8,
                                           eNew);
            
            cout << "Length (10): " << len << endl;
            
            delete[] buf1;
            
            return 0;
        } else desc += " [-dyn]";
        
#if defined(NCBI_OS_UNIX)
        if ((s == "-gi2oid") ||
            (s == "-gi2oidX") ||
            (s == "-gi2oidS") ||
            (s == "-gi2oidR")) {
            
            bool randomize = true;
            
            bool using_readdb     (s == "-gi2oidR");
            bool seperate_caching (s == "-gi2oidS");
            bool build_gilist     (s == "-gi2oidX");
            
            cout << "using_readdb:     " << (using_readdb     ? "T" : "F") << endl;
            cout << "seperate_caching: " << (seperate_caching ? "T" : "F") << endl;
            
            vector<int> gis;
            vector<int> oids;
            
            CStopWatch sw(true);
            
            CSeqDB db("nr", CSeqDB::eProtein);
            
            ReadDBFILEPtr rdfp =
                readdb_new_ex2((char*) "nr",
                               1, // prot
                               READDB_NEW_DO_TAXDB,
                               NULL,
                               NULL);
            
            int nseq = db.GetNumSeqs();
            
            double spt1(0.0);
            double spt1a(0.0);
            double spt2(0.0);
            
            if (build_gilist) {
                int jump = 10;

                cout << "Setting up (j" << jump << ")..." << endl;
                
                int i = 0;
                
                for(i = 0; i<nseq; i+= jump) {
                    oids.push_back(i);
                    gis.push_back(0);
                }
                
                cout << "Initial translation (w/ SeqDB)..." << endl;
                
                spt1 = sw.Elapsed();
                
                {
                    // Use a seperate object to avoid "internal" caching
                    // interference.
                    
                    CSeqDB db1("nr", CSeqDB::eProtein);
                    
                    CSeqDB & dbx(seperate_caching ? db1 : db);
                    
                    for(i = 0; i<(int)oids.size(); i++) {
                        dbx.OidToGi(oids[i], gis[i]);
                    }
                }
                
                spt1a = sw.Elapsed();
                
                if (randomize) {
                    cout << "Randomizing " << oids.size() << " entries ..." << endl;
                    
                    CRandom prng;
                    
                    for(int i2 = 0; i2 < (int)oids.size(); i2+= jump) {
                        int j = prng.GetRand(0, (int) oids.size()-1);
                    
                        int oid_tmp = oids[i2];
                        oids[i2] = oids[j];
                        oids[j] = oid_tmp;
                    
                        int gi_tmp = gis[i2];
                        gis[i2] = gis[j];
                        gis[j] = gi_tmp;
                    }
                }
                
                ofstream gilist("gilist.txt");
                
                for(int i2 = 0; i2<(int)oids.size(); i2++) {
                    gilist << gis[i2] << "\n";
                }
            } else {
                cout << "Reading from file..." << endl;
                
                spt1 = sw.Elapsed();
                
                ifstream gilist("gilist.txt");
                
                while(gilist) {
                    int gi(0);
                    
                    gilist >> gi;
                    gis.push_back(gi);
                }
                
                spt1a = sw.Elapsed();
                
                ITERATE(vector<int>, iter, gis) {
                    int oid(0);
                    db.GiToOid(*iter, oid);
                    
                    oids.push_back(oid);
                }
            }
            
            spt2 = sw.Elapsed();
            
            cout << "trans oid2gi: " << (spt1a - spt1) << ", randomize " << (spt2 - spt1a) << endl;
            
            cout << "Phase 2..." << endl;
            
            for(int iter = 0; iter<4; iter++) {
                double spt3 = sw.Elapsed();
                
                if (using_readdb) {
                    for(int i = 0; i<(int)oids.size(); i++) {
                        int oid(int(-1));
                        oid = readdb_gi2seq(rdfp, gis[i], 0);
                        
                        if (oid != oids[i]) {
                            cout << "Error, gi " << gis[i]
                                 << " should be oid " << oids[i]
                                 << " but was " << oid << endl;
                        }
                    }
                } else {
                    for(int i = 0; i<(int)oids.size(); i++) {
                        int oid(int(-1));
                        db.GiToOid(gis[i], oid);
                        
                        if (oid != oids[i]) {
                            cout << "Error, gi " << gis[i]
                                 << " should be oid " << oids[i]
                                 << " but was " << oid << endl;
                        }
                    }
                }
                double spt4 = sw.Elapsed();
                cout << "verif gi2oid [" << iter << "]: " << (spt4 - spt3) << endl;
            }
            
            return 0;
        } else desc += " [-gi2oid | -gi2oidR]";
        
        
        if (s == "-seqidtrans") {
            CSeqDB nr("nr", CSeqDB::eProtein);
            
            string seqid_str("gi|129295");
            
            vector<int> oids;
            CSeq_id seqid(seqid_str);
            
            nr.SeqidToOids(seqid, oids);
            
            cout << "\nTranslating Seqid: " << seqid_str << endl;
            
            for(int i = 0; i<(int)oids.size(); i++) {
                cout << "  found oid: " << oids[i] << endl;
            }
            
            cout << "\nTranslating gi 129295" << endl;
            int oid2((int)-1);
            
            nr.GiToOid(129295, oid2);
            
            cout << "  found oid: " << oid2 << "\n" << endl;
            
            return 0;
        } else desc += " [-gi2oid | -gi2oidR]";
        
        
        if (s == "-fastacmd") {
            dbname = "nr";
            seqtype = CSeqDB::eUnknown;
            string acc;
            
            while(! args.empty()) {
                string cmd(args.front());
                args.pop_front();
                
                bool has_value = true;
                
                if (args.empty() || (args.front()[0] == '-')) {
                    has_value = false;
                }
                
                bool success = false;
                
                if (has_value) {
                    success = true;
                    string value = args.front();
                    args.pop_front();
                    
                    if (cmd == "-d") {
                        dbname = value;
                    } else if (cmd == "-s") {
                        acc = value;
                    } else if (cmd == "-p") {
                        // Default is eUnknown.  Can specify protein
                        // or nucleotide directly, though.
                        
                        seqtype = (value == "T")
                            ? CSeqDB::eProtein
                            : CSeqDB::eNucleotide;
                    } else if (cmd == "-s") {
                        acc = value;
                    } else {
                        cout << "Fell off bottom of value test" << endl;
                        success = false;
                    }
                } else {
                    cout << "No value found, cmd=" << cmd << endl;
                }
                
                if (! success) {
                    cout << "Command [" << cmd << "] not supported yet." << endl;
                    return 0;
                }
            }
            
            if (acc.empty()) {
                cout << "For this command, [-s] option is mandatory." << endl;
                return 0;
            }
            
            CSeqDB db(dbname, seqtype);
            
            vector<int> oids;
            db.AccessionToOids(acc, oids);
            
            if (oids.empty()) {
                cout << "Could not find OIDs for Accession " << acc << endl;
            } else {
                ITERATE(vector<int>, iter, oids) {
                    cout << "Have OID " << (*iter)
                         << ", length " << db.GetSeqLength(*iter) << endl;
                }
            }
            
            return 0;
        } else desc += " [-fastacmd]";
        
        
        if (s == "-xlate3") {
            dbname = "nr";
            //string dbname("prot_dbs");
            string acc("AAK53449");
            //string acc("AAK53420"); //!!!!!!!!
            
            if ((! args.empty()) && ((*args.begin())[0] != '-')) {
                dbname = *args.begin();
                args.pop_front();
            }
            
            CSeqDB db(dbname, CSeqDB::eProtein);
            
            while((acc != "QUIT") && cin) {
                vector<int> oids;
                int oid(0), gi(0);
                
                db.AccessionToOids(acc, oids);
                
                if (! oids.empty()) {
                    cout << "Num oids returned " << oids.size() << endl;
                    
                    db.OidToGi(oid = oids[0], gi);
                    
                    if (oids.size() > 1) {
                        cout << "Multiple oids were returned ..." << endl;
                    }
                } else {
                    cout << "No oids were returned" << endl;
                }
                
                cout << "Acc [" << acc << "] is oid " << oid << ", which is gi " << gi << endl;
                
                {
                    ReadDBFILEPtr rdfp =
                        readdb_new_ex2((char*) dbname.c_str(),
                                       1, // prot
                                       READDB_NEW_DO_TAXDB,
                                       NULL,
                                       NULL);
                    
                    Int4 oid2 = readdb_acc2fasta(rdfp, (char*) acc.c_str());
                    
                    cout << "... readdb says oid = " << oid2 << endl;
                    
                    readdb_destruct(rdfp);
                }
                
                cout << "enter QUIT to quit:" << endl;
                cout << "enter acc: " << flush;
                cin >> acc;
                cout << endl << "... You entered accession [" << acc << "]" << endl;
            }
            return 0;
        } else desc += " [-xlate3]";
        
        if ((s == "-compare-bs") ||
            (s == "-compare-bs-o")) {
            
            bool use_ncbi_fmt = true;
            
            bool have_oid = false;
            
            int gi = 0;
            int oid = 0;
            
            int id = 0;
            
            if (args.empty() || ((*args.begin())[0] == '-')) {
                return 2;
            }
            
            if (s == "-compare-bs-o") {
                have_oid = true;
                oid = id;
            } else {
                gi = id;
            }
            
            // SeqDB
            
            CSeqDB db(dbname, seqtype);
            
            int s_oid = oid;
            int s_leng = 0;
            const char* s_data = 0;
            
            if (! have_oid) {
                if (! db.GiToOid(gi, s_oid)) {
                    cout << "Error: gi not found: " << gi << endl;
                    return 3;
                }
            }
            
            if (use_ncbi_fmt) {
                s_leng = db.GetAmbigSeq(s_oid, & s_data, kSeqDBNuclNcbiNA8);
            } else {
                s_leng = 2 + db.GetAmbigSeq(s_oid, & s_data, kSeqDBNuclBlastNA8);
            }
            
            // ReadDB
            
            ReadDBFILEPtr rdfp =
                readdb_new_ex2((char*) dbname.c_str(),
                               seqtype == 'p',
                               READDB_NEW_DO_TAXDB,
                               NULL,
                               NULL);
            
            int r_oid = oid;
            Int4 r_leng = 0;
            unsigned char* r_data = 0;
            
            if (! have_oid) {
                r_oid = readdb_gi2seq(rdfp, gi, 0);
            }
            
            readdb_get_sequence_ex(rdfp, r_oid, & r_data, & r_leng, use_ncbi_fmt ? 0 : 1);
            
            // Testing
            
            if (((int)r_leng) != ((int)s_leng)) {
                cout << "Failure, lengths differ: R(" << r_leng << "), S(" << s_leng << ")" << endl;
            }
            int x = 0;
            
            while((x < r_leng) || (x < int(s_leng))) {
                cout << x << ": ";
                
                unsigned rr = 9999;
                unsigned ss = 9999;
                
                bool has_r = false;
                if (x < r_leng) {
                    has_r = true;
                    cout << "R[" << (rr = unsigned(r_data[x])) << "]";
                }
                
                if (x < int(s_leng)) {
                    if (has_r) {
                        cout << ", ";
                    }
                    cout << "S[" << (ss = unsigned(s_data[x])) << "]";
                }
                
                if (use_ncbi_fmt) {
                    // test for bitfield format output
                    // WRONG:
                    // if (((rr >> 1) & rr) || ((ss >> 1) & ss)) {
                    
                    // RIGHT:
                    if (((rr - 1) & rr) | ((ss - 1) & ss)) {
                        cout << "  ++  ";
                    }
                } else {
                    // test for enumerated format output
                    if ((rr | ss) > 3) {
                        cout << "  ++  ";
                    }
                }
                
                if (rr != ss) {
                    cout << " !!!!";
                }

                cout << "." << endl;
                x++;
            }
            
            // Cleanup
            
            db.RetSequence(& s_data);
            
            readdb_destruct(rdfp);
            
            return 0;
        } else desc += " [-compare-bs]";
#endif
        
        if (s == "-x4mutate") {
            x4mutate = true;
            cerr << "Using mutation engine." << endl;
            continue;
        } else desc += " [-x4-mutate]";
        
#if defined(NCBI_OS_UNIX)
        if ((s == "-xlate4r") || 
            (s == "-xlate4rx")) {
            
            bool rx = (s == "-xlate4rx");
            
            ReadDBFILEPtr rdfp = readdb_new_ex2((char*) "nr",
                                                1, // prot
                                                READDB_NEW_DO_TAXDB,
                                                NULL,
                                                NULL);
            
            CNcbiIfstream idents("stringids.txt");
            
            while(idents) {
                string line;
                NcbiGetlineEOL(idents, line);
                
                if (line.empty()) {
                    break;
                }
                
                string linecpy = line;
                
                if (x4mutate) {
                    s_MutateString(line);
                }
                
                vector<int> oids;
                
                if (rx) {
                    oids.clear();
                    
                    int * data(0);
                    int   count(0);
                    
                    int result = readdb_acc2fastaEx(rdfp, (char*)line.c_str(), & data, & count);
                    
                    if ((result >= 0) && (count > 0)) {
                        for(int i = 0; i<count; i++) {
                            oids.push_back(data[i]);
                        }
                    }
                } else {
                    int oid1 = readdb_acc2fasta(rdfp, (char*)line.c_str());
                    
                    if (oid1 != -1) {
                        oids.resize(1);
                        oids[0] = oid1;
                    }
                }
                
                cout << "orig[" << linecpy << "] -> Acc [" << line << "] has oids: ";
                
                if (oids.size()) {
                    for(int i = 0; i < (int)oids.size(); i++) {
                        if (i) {
                            cout << ", ";
                        }
                        
                        cout << oids[i];
                    }
                } else {
                    cout << "NONE";
                }
                cout << endl;
            }
            
            return 0;
        } else desc += " [-xlate4r] [-xlate4rx]";
#endif
        
        if (s == "-xlate4") {
            CSeqDB db("nr", CSeqDB::eProtein);
            
            CNcbiIfstream idents("stringids.txt");
            
            while(idents) {
                string line;
                NcbiGetlineEOL(idents, line);
                
                if (line.empty()) {
                    break;
                }
                
                string linecpy(line);
                
                if (x4mutate) {
                    s_MutateString(line);
                }
                
                vector<int> oids;
                db.AccessionToOids(line, oids);
                
                cout << "orig[" << linecpy << "] -> Acc [" << line << "] has oids: ";
                
                if (oids.size()) {
                    for(int i = 0; i < (int)oids.size(); i++) {
                        if (i) {
                            cout << ", ";
                        }
                        
                        int gi(0);
                        db.OidToGi(oids[i], gi);
                        
                        cout << oids[i];
                    }
                } else {
                    cout << "NONE";
                }
                cout << endl;
            }
            
            return 0;
        } else desc += " [-xlate4]";
        
        if (s == "-gilist") {
            CStopWatch sw(true);
            
            double e1 = sw.Elapsed();
            CSeqDB db(dbname, seqtype);
            double e2 = sw.Elapsed();
            
            cout << "Time to construct: " << (e2 - e1) << endl;
            
            vector<int> oids;
            oids.resize(10000);
            int obegin(0), oend(0);
            
            int numseq = 0;
            
            int z1 = 0;
            
            while(1) {
                CSeqDB::EOidListType range_type =
                    db.GetNextOIDChunk(obegin, oend, oids);
                
                int num_found(0);
                
                if (range_type == CSeqDB::eOidList) {
                    num_found = (int) oids.size();
//                     ITERATE(vector<int>, iter, oids) {
//                         int iter_gi(0);
//                         db.OidToGi(*iter, iter_gi);
//                         cout << *iter << ":" << iter_gi << endl;
//                    }
                } else {
                    num_found = oend - obegin;
//                     for(int iter = obegin; iter < oend; iter++) {
//                         int iter_gi(0);
//                         db.OidToGi(iter, iter_gi);
//                         cout << iter << ":" << iter_gi << endl;
//                     }
                }
                
                if (! num_found) {
                    break;
                }
                
                numseq += num_found;
                
                if (numseq > z1) {
                    z1 = (numseq + numseq / 10);
                }
            }
            
            cout << "Found " << numseq << " oids in [" << dbname << "], title = " << db.GetTitle() << endl;
            cout << "Total oids in db  " << db.GetNumOIDs() << endl;
            
            return 0;
        } else desc += " [-gilist]";
        
        if (s == "-xlate2") {
            string dbname1("prot_dbs");
            
            if ((! args.empty()) && ((*args.begin())[0] != '-')) {
                dbname1 = *args.begin();
                args.pop_front();
            }
            
            CSeqDB db(dbname1, CSeqDB::eProtein);
            
            cout << "Num oids: " << db.GetNumSeqs() << endl;
            
            int pig(2201), oid(0), gi(0), pg2(0), oid2(0), len(0);
            bool b1, b2, b3, b4;
            
            b1  = db.PigToOid(pig, oid);
            b2  = db.OidToPig(oid, pg2);
            len = db.GetSeqLength(oid);
            
            b3  = db.OidToGi(oid, gi);
            b4  = db.GiToOid(gi, oid2);
            
            cout << "PIG: " << pig
                 << ", OID: " << oid
                 << ", length " << len
                 << ", pig2 = " << pg2
                 << ", gi   = " << gi
                 << ", oid2 = " << oid2
                 << endl;
            
            cout << "Dumping deflines:" << endl;
            {
                auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText, cout));
                CRef<CBlast_def_line_set> obj(db.GetHdr(oid));
                *os << *obj;
            }
            cout << "Done dumping deflines:" << endl;
            
            assert(b1);
            assert(b2);
            assert(pig == pg2);
            //assert(db.GetSeqLength(oid) == 87);
            
            assert(oid2 == oid);
            
            return 0;
            
#if 0
            cout << "PIG translations worked, trying bulk mode:" << endl;
            
            //int numseqs(db.GetNumSeqs());
            
            int i = 0;
            
            for(i = 0; i<10000; i++) {
                int OID = 0;
                
                if (db.PigToOid(i, OID)) {
                    cout << "pig " << i << " is oid " << OID << endl;
                } else {
                    cout << "For pig " << i << ", translation failed." << endl;
                }
            }
            
//             int shout_at = 0;
            
//             for(; i<numseqs; i++) {
//                 int pig = 0;
                
//                 if (i > shout_at) {
//                     shout_at = ((i * 4) / 3);
                    
//                     cout << "Computed translation for " << i << " sequences." << endl;
//                 }
                
//                 if (db.OidToPig(i, pig)) {
//                     cout << "OID " << i << " is pig " << pig << endl;
//                 } else {
//                     cout << "For oid " << i << " translation failed." << endl;
//                 }
//             }
            
            cout << "Translations tested up to " << i << endl;
            
            return 0;
#endif
        } else desc += " [-xlate2]";
        
        if (s == "-xlate") {
            string dbname1("nr pataa env_nr");
            
            if ((! args.empty()) && ((*args.begin())[0] != '-')) {
                dbname1 = *args.begin();
                args.pop_front();
            }
            
            CSeqDB db(dbname1, CSeqDB::eProtein);
            
            cout << "Num oids: " << db.GetNumSeqs() << endl;
            
            int pig = 2201, oid(0), pg2(0), len(0);
            bool b1, b2;
            
            b1  = db.PigToOid(pig, oid);
            b2  = db.OidToPig(oid, pg2);
            len = db.GetSeqLength(oid);
            
            cout << "PIG: " << pig
                 << ", OID: " << oid
                 << ", length " << len
                 << ", pig2 = " << pg2 << endl;
            
            cout << "Dumping deflines:" << endl;
            {
                auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText, cout));
                CRef<CBlast_def_line_set> obj(db.GetHdr(oid));
                *os << *obj;
            }
            cout << "Done dumping deflines:" << endl;
            
            assert(b1);
            assert(b2);
            assert(pig == pg2);
            //assert(db.GetSeqLength(oid) == 87);
            
            cout << "PIG translations worked, trying bulk mode:" << endl;
            
            //int numseqs(db.GetNumSeqs());
            
            int i = 0;
            
            for(i = 0; i<10000; i++) {
                int OID = 0;
                
                if (db.PigToOid(i, OID)) {
                    cout << "pig " << i << " is oid " << OID << endl;
                } else {
                    cout << "For pig " << i << ", translation failed." << endl;
                }
            }
            
//             int shout_at = 0;
            
//             for(; i<numseqs; i++) {
//                 int pig = 0;
                
//                 if (i > shout_at) {
//                     shout_at = ((i * 4) / 3);
                    
//                     cout << "Computed translation for " << i << " sequences." << endl;
//                 }
                
//                 if (db.OidToPig(i, pig)) {
//                     cout << "OID " << i << " is pig " << pig << endl;
//                 } else {
//                     cout << "For oid " << i << " translation failed." << endl;
//                 }
//             }
            
            cout << "Translations tested up to " << i << endl;
            
            return 0;
        } else desc += " [-xlate]";
        
        if (s == "-atlas") {
            {
                cerr << "Going to construct?" << endl;
                
                CSeqDB db("nr", CSeqDB::eProtein);
                
                cerr << "Constructor ran?" << endl;
            }
            cerr << "Destructor ran?" << endl;
            
            {
                cerr << "Going to construct?" << endl;
                
                CSeqDB db("nr", CSeqDB::eProtein);
                
                cerr << "Constructor ran?" << endl;
                
                const char * sq = 0;
                int sqlen = 0;
                
                sqlen = db.GetSequence(10, & sq);
                
                db.RetSequence(& sq);
                
                cout << "sqlen = " << sqlen << endl;
                
                cerr << "Finished simple test.. should suffer now? (no)" << endl;
            }
            cerr << "Destructor ran?" << endl;
            
            // Do nothing..
            
//             cout << "Num oids: " << db.GetNumSeqs() << endl;
            
//             char * buf1 = 0;
            
//             Int4 len = db.GetAmbigSeqAlloc(10,
//                                            & buf1,
//                                            kSeqDBNuclBlastNA8,
//                                            eNew);
            
//             cout << "Length (10): " << len << endl;
            
//             delete[] buf1;
            
            return 0;
        } else desc += " [-atlas]";
        
        if (s == "-atlas2") {
            CSeqDB db("nr", CSeqDB::eProtein);
            
            for(int index = 311073; index < 700000; index++) {
                const char * sq = 0;
                /*int sqlen =*/ db.GetSequence(10, & sq);
                db.RetSequence(& sq);
            }
            
            return 0;
        } else desc += " [-atlas2]";
        
        if (s == "-limit") {
            {
                CSeqDB db("/home/bealer/seqdb/tenth", CSeqDB::eProtein, 10, 20, true);
                
                cout << "Num oids: " << db.GetNumSeqs() << endl;
                
                CSeqDBIter i = db.Begin();
                
                while(i) {
                    CRef<CBioseq> bs = db.GetBioseq(i.GetOID());
                    
                    cout << "-----seq "
                         << i.GetOID() << " length "
                         << i.GetLength() << "-------" << endl;
                    
                    ++i;
                }
            }
            {
                CSeqDB db("swissprot", CSeqDB::eProtein, 135, 175, true);
                
                cout << "Num oids: " << db.GetNumSeqs() << endl;
                
                CSeqDBIter i = db.Begin();
                
                while(i) {
                    CRef<CBioseq> bs = db.GetBioseq(i.GetOID());
                    
                    cout << "-----seq "
                         << i.GetOID() << " length "
                         << i.GetLength() << "-------" << endl;
                    
                    ++i;
                }
            }
            
            return 0;
        } else desc += " [-local]";
        
        if (s == "-local") {
            CSeqDB nr("/home/bealer/seqdb/tenth", CSeqDB::eProtein);
            
            for(int i = 0; i<100; i++) {
                CRef<CBioseq> bs = nr.GetBioseq(i);
                
                cout << "-----seq " << i << "------------" << endl;
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                
                *outpstr << *bs;
            }
            
            return 0;
        } else desc += " [-local]";
        
        if (s == "-get-seqids") {
            // get all seqids, given a gi.
            
            CSeqDB db(dbname, seqtype);
            
            int gi(129295);
            int oid(0);
            
            if (! args.empty()) {
                gi = atoi(args.front().c_str());
                args.pop_front();
            }
            
            db.GiToOid(gi, oid);
            
            list< CRef<CSeq_id> > ids = db.GetSeqIDs(oid);
            
            cout << "-----\n-----Seqids for GI " << gi << " (oid=" << oid << ")\n-----" << endl;
            
            auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText, cout));
            
            ITERATE(list< CRef< CSeq_id > >, iter, ids) {
                *os << **iter;
            }
            cout << "-----" << endl;
            
            return 0;
        } else desc += " [-get-seqids <gi>]";
        
        if (s == "-seqids") {
            CSeqDB nr(/*dbpath,*/ "nr", CSeqDB::eProtein);
            
            for(int i = 0; i<100; i++) {
                list< CRef<CSeq_id> > seqids =
                    nr.GetSeqIDs(i);
                
                cout << "-----seq " << i << "------------" << endl;
                
                for(list< CRef<CSeq_id> >::iterator j = seqids.begin();
                    j != seqids.end();
                    j++) {
                    
                    cout << "SEQID----*:" << endl;
                    
                    auto_ptr<CObjectOStream>
                        outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    
                    CRef<CSeq_id> ident = *j;
                    
                    *outpstr << *ident;
                }
            }
            
            return 0;
        } else desc += " [-seqids]";
        
        if (s == "-memtest") {
            CSeqDB nt("nt", CSeqDB::eNucleotide, 0, 0, false);
            
            int oid = 0;
            
            for(int i = 0; i<100; i++) {
                const char * buf(0);
                
                if (! nt.CheckOrFindOID(oid))
                    break;
                
                cout << "-----------------" << endl;
                
                if (1) {
                    int length = nt.GetSequence(oid, & buf);
                    
                    cout << "NT OID = " << oid << ", length is " << length << endl;
                    
                    int y = (length > 16) ? 16 : length;
                    
                    for(int x = 0; x < y; x++) {
                        if ((x & 3) == 0)
                            cout << " ";
                        
                        if (unsigned(buf[x]) < 0x10)
                            cout << "0";
                        
                        cout << hex << (unsigned(buf[x]) & 0xFF) << " ";
                    }
                    
                    cout << dec << "\n";

                    nt.RetSequence(& buf);
                }
                
                if (1) {
                    int length = nt.GetAmbigSeq(oid, & buf, kSeqDBNuclNcbiNA8);
                    
                    int y = (length > 16) ? 16 : length;
                
                    for(int x = 0; x < y; x++) {
                        if ((x & 3) == 0)
                            cout << " ";
                    
                        if (unsigned(buf[x]) < 0x10)
                            cout << "0";
                    
                        cout << hex << (unsigned(buf[x]) & 0xFF) << " ";
                    }
                    
                    cout << dec << "\n";
                    
                    nt.RetSequence(& buf);
                }
                
                if (1) {
                    int length = nt.GetAmbigSeq(oid, & buf, kSeqDBNuclBlastNA8);
                
                    int y = (length > 16) ? 16 : length;
                
                    for(int x = 0; x < y; x++) {
                        if ((x & 3) == 0)
                            cout << " ";
                    
                        if (unsigned(buf[x]) < 0x10)
                            cout << "0";
                    
                        cout << hex << (unsigned(buf[x]) & 0xFF) << " ";
                    }
                    
                    cout << dec << "\n\n";
                    
                    nt.RetSequence(& buf);
                }
                
                oid++;
            }
            
            return 0;
        } else desc += " [-memtest]";
        
        if (s == "-getambig") {
            CSeqDB nt(/*dbpath,*/ "nt", CSeqDB::eNucleotide, 0, 0, false);
            
            int oid = 0;
            
            for(int i = 0; i<100; i++) {
                const char * buf(0);
                
                if (! nt.CheckOrFindOID(oid))
                    break;
                
                cout << "-----------------" << endl;
                
                {
                    int length = nt.GetSequence(oid, & buf);
                    
                    cout << "NT OID = " << oid << ", length is " << length << endl;
                    
                    int y = (length > 16) ? 16 : length;
                    
                    for(int x = 0; x < y; x++) {
                        if ((x & 3) == 0)
                            cout << " ";
                        
                        if (unsigned(buf[x]) < 0x10)
                            cout << "0";
                        
                        cout << hex << (unsigned(buf[x]) & 0xFF) << " ";
                    }
                    
                    cout << dec << "\n";

                    nt.RetSequence(& buf);
                }
                
                {
                    int length = nt.GetAmbigSeq(oid, & buf, kSeqDBNuclNcbiNA8);
                
                    int y = (length > 16) ? 16 : length;
                
                    for(int x = 0; x < y; x++) {
                        if ((x & 3) == 0)
                            cout << " ";
                    
                        if (unsigned(buf[x]) < 0x10)
                            cout << "0";
                    
                        cout << hex << (unsigned(buf[x]) & 0xFF) << " ";
                    }
                    
                    cout << dec << "\n";
                    
                    nt.RetSequence(& buf);
                }
                
                {
                    int length = nt.GetAmbigSeq(oid, & buf, kSeqDBNuclBlastNA8);
                
                    int y = (length > 16) ? 16 : length;
                
                    for(int x = 0; x < y; x++) {
                        if ((x & 3) == 0)
                            cout << " ";
                    
                        if (unsigned(buf[x]) < 0x10)
                            cout << "0";
                    
                        cout << hex << (unsigned(buf[x]) & 0xFF) << " ";
                    }
                    
                    cout << dec << "\n\n";
                    
                    nt.RetSequence(& buf);
                }
                
                oid++;
            }
            
            return 0;
        } else desc += " [-getambig]";
        
        if (s == "-iter2") {
            {
                CSeqDB phil(/*dbpath,*/ "swissprot pataa", CSeqDB::eProtein);
            
                {
                    CSeqDBIter skywalk = phil.Begin();

                    for(int i = 0; i<20; i++) {
                        cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                        ++skywalk;
                    }
                }
            
                {
                    CSeqDBIter skywalk = phil.Begin();

                    for(int i = 0; i<20; i++) {
                        cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                        CRef<CBioseq> bioseq = phil.GetBioseq(skywalk.GetOID());
                        auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                        *outpstr << *bioseq;
                        ++skywalk;
                    }
                }
            }
            {
                CSeqDB phil(/*dbpath,*/ "pataa swissprot", CSeqDB::eProtein);
            
                {
                    CSeqDBIter skywalk = phil.Begin();

                    for(int i = 0; i<20; i++) {
                        cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                        ++skywalk;
                    }
                }
            
                {
                    CSeqDBIter skywalk = phil.Begin();

                    for(int i = 0; i<20; i++) {
                        cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                        CRef<CBioseq> bioseq = phil.GetBioseq(skywalk.GetOID());
                        auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                        *outpstr << *bioseq;
                        ++skywalk;
                    }
                }
            }
            
            return 0;
        } else desc += " [-iter2]";
        
        if (s == "-iter") {
            CSeqDB phil(/*dbpath,*/ "swissprot pdb", CSeqDB::eProtein);
            
            {
                CSeqDBIter skywalk = phil.Begin();

                for(int i = 0; i<200; i++) {
                    cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                    ++skywalk;
                }
            }
            
            if (0) {
                for(int i = 0; i<200; i++) {
                    cout << "\n### Seq [" << i << "] length = " << phil.GetSeqLength(i) << "\n" << endl;
                    CRef<CBioseq> bioseq = phil.GetBioseq(i);

                    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    *outpstr << *bioseq;
                }
            }
            
            {
                CSeqDBIter skywalk = phil.Begin();

                for(int i = 0; i<200; i++) {
                    cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                    CRef<CBioseq> bioseq = phil.GetBioseq(skywalk.GetOID());
                    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    *outpstr << *bioseq;
                    ++skywalk;
                }
            }
            
            if (0) {
                for(int i = 0; i<200; i++) {
                    cout << "\n### Seq [" << i << "] length = " << phil.GetSeqLength(i) << "\n" << endl;
                }
            }
            
            return 0;
        } else desc += " [-iter]";
        
        if (s == "-iterpdb") {
            CSeqDB phil(/*dbpath,*/ "pdb", CSeqDB::eProtein);
            
            {
                CSeqDBIter skywalk = phil.Begin();

                for(int i = 0; i<200; i++) {
                    cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                    ++skywalk;
                }
            }
            
            if (0) {
                for(int i = 0; i<100; i++) {
                    cout << "\n### Seq [" << i << "] length = " << phil.GetSeqLength(i) << "\n" << endl;
                    CRef<CBioseq> bioseq = phil.GetBioseq(i);

                    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    *outpstr << *bioseq;
                }
            }
            
            return 0;
        } else desc += " [-iterpdb]";
        
        if (s == "-itersp") {
            CSeqDB phil(/*dbpath,*/ "swissprot", CSeqDB::eProtein);
            
            {
                CSeqDBIter skywalk = phil.Begin();

                for(int i = 0; i<200; i++) {
                    cout << "### Seq [" << skywalk.GetOID() << "] length = " << skywalk.GetLength() << endl;
                    ++skywalk;
                }
            }
            
            if (0) {
                for(int i = 0; i<100; i++) {
                    cout << "\n### Seq [" << i << "] length = " << phil.GetSeqLength(i) << "\n" << endl;
                    CRef<CBioseq> bioseq = phil.GetBioseq(i);

                    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    *outpstr << *bioseq;
                }
            }
            
            return 0;
        } else desc += " [-itersp]";
        
        if (s == "-spcount") {
            CSeqDB phil(/*dbpath,*/ "swissprot", CSeqDB::eProtein);
            
            double besht = 100.0;
            double woist = 0.0;
            double totul = 0.0;
            
            CStopWatch sw(true);
            
            for(int i = 0; i<10; i++) {
                CSeqDBIter skywalk = phil.Begin();
                
                double spt1 = sw.Elapsed();
                Uint8 mylen = 0;
                
                while(skywalk) {
                    int this_oid = 0;
                    //int this_len = 0;
                    
                    //mylen += (this_len = phil.GetSeqLength( this_oid = skywalk.GetOID() ));
                    this_oid = skywalk.GetOID();
                    ++ skywalk;
                    mylen ++;
                    
                    //cout << this_oid << " is length " << this_len << endl;
                }
                
                double spt2  = sw.Elapsed();
                double rezzy = spt2 - spt1;
                
                cout << "mylen " << mylen
                     << " spt2 - spt1 = " << rezzy << endl;
                
                if (rezzy > woist)
                    woist = rezzy;
                if (rezzy < besht)
                    besht = rezzy;
                
                totul += rezzy;
            }
            
            totul -= (besht + woist);
            
            cout << "Average = " << (totul/8.0) << endl;
            
            return 0;
        } else desc += " [-spcount]";
        
        if (s == "-swiss") {
            CSeqDB phil(/*dbpath,*/ "swissprot", CSeqDB::eProtein);
             
            {
                Uint8 tlen = 0;
                int numb = 0;
                
                CSeqDBIter skywalk = phil.Begin();
                
                {
                    int i = 276;
                    cout << "this_oid = " << i << " length = " << 0 << endl;
                    cout << "\n### Seq [" << i << "] length = " << phil.GetSeqLength(i) << "\n" << endl;
                    CRef<CBioseq> bioseq = phil.GetBioseq(i);
                    
                    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    *outpstr << *bioseq;
                }

                while(skywalk) {
                    int this_oid = 0;
                    int this_len = 0;
                    
                    numb ++;
                    tlen += (this_len = phil.GetSeqLength( this_oid = skywalk.GetOID() ));
                    ++ skywalk;
                    
                    cout << this_oid << endl;
                    
                    if (numb > 145680) {
                        int i = this_oid;
                        cout << "this_oid = " << this_oid << " length = " << this_len << endl;
                        cout << "\n### Seq [" << i << "] length = " << phil.GetSeqLength(i) << "\n" << endl;
                        CRef<CBioseq> bioseq = phil.GetBioseq(i);

                        auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                        *outpstr << *bioseq;
                    }
                }
                
                cout << "\n### Total swissprot length [" << tlen << "] numb = " << numb << "\n" << endl;
            }
             
            return 0;
        } else desc += " [-swiss]";
        
        if (s == "-bigchunk") {
            CSeqDB sp("month.wgs", CSeqDB::eNucleotide);
            
            cout << " num oids " << sp.GetNumSeqs() << endl;
            
            vector<int> chunky;
            chunky.resize(1000000);
            
            int b(0), e(0);
            
            CSeqDB::EOidListType et = sp.GetNextOIDChunk(b,e,chunky);
            
            cout << "et=" << et << ", chunky size=" << chunky.size() << endl;
            
            for(unsigned i = 0; i<chunky.size(); i++) {
                cout << chunky[i] << endl;
            }
            
            return 0;
            
        } else desc += " [-bigchunk]";
        
        if (s == "-chunk") {
            cout << "enter db name:" << endl;
            
            string dbname1;
            cin >> dbname1;
            
            cout << "entered db name: [" << dbname1 << "]" << endl;
            
            //CSeqDB phil(dbname1, 'p', 1000000, 1 << 30, true);
            CSeqDB phil(dbname1, CSeqDB::eProtein, 1880000, 1 << 30, true);
            
            const int max_oids = 100;
            
            int begin(0), end(0);
            
            //int oid_list[max_oids];
            vector<int> oid_list;
            oid_list.resize(max_oids);
            
            bool have_any = true;
            
            int chunk_index = 0;
            
            while (have_any) {
                cout << "\nChunk #" << chunk_index++ << "\n    ";
                
                if (CSeqDB::eOidList == phil.GetNextOIDChunk(begin, end, oid_list)) {
                    have_any = ! oid_list.empty();
                    
                    int begin2 = 0;
                    
                    for(int i = 0; i < (int)oid_list.size(); i++) {
                        cout << i << "/" << (i-begin2) << ": got oid(L) " << oid_list[i] << "\n    ";
                    }
                } else {
                    have_any = (begin != end);
                    
                    for(int i = begin; i<end; i++) {
                        cout << i << "/" << (i-begin) << ": got oid(R) " << i << "\n    ";
                    }
                }
                
                if (! have_any) {
                    cout << "\nNo more oids to get." << endl;
                }
            }
            
            return 0;
        } else desc += " [-chunk]";
        
        if (s == "-myriad") {
            
            // The microbial genomes as of Jan 3rd, 2005
            
            int dbs[] = {
                1063, 1108, 1352, 155919, 155920, 156889, 2021, 59919,
                63737, 171101, 242619, 184922, 100226, 74547, 84588, 97393,
                985, 1336, 160488, 160491, 107806, 115711, 93061, 115713,
                93062, 122586, 98360, 122587, 1148, 138677, 158878, 155864,
                158879, 160492, 5691, 169963, 170187, 5811, 64091, 71421,
                83331, 160490, 4896, 4932, 5141, 228405, 262724, 5874, 6238,
                6239, 6279, 238030, 83332, 83333, 83334, 83560, 85962,
                85963, 99287, 195102, 160489, 163164, 180454, 164513,
                257310, 187410, 36329, 226230, 40325, 197221, 34054, 36826,
                5085, 226231, 5759, 5807, 284591, 6035, 103690, 189423,
                32049, 186497, 186103, 190304, 188937, 167879, 195099,
                226301, 190192, 119072, 257313, 190485, 190486, 190650,
                192952, 203275, 205919, 205920, 205921, 220664, 44294,
                198466, 1085, 226302, 198628, 3702, 1596, 191218, 194439,
                196620, 198804, 203119, 203120, 203122, 203123, 203124,
                279010, 354, 205913, 205914, 205918, 207559, 189518, 212042,
                212045, 209841, 214684, 214688, 204722, 288681, 196164,
                199310, 208435, 210007, 211110, 216591, 216592, 216593,
                216594, 216595, 216596, 216597, 216598, 216599, 216600,
                85569, 211586, 218493, 218494, 218495, 218497, 5823, 218496,
                272564, 226125, 176279, 222891, 222929, 296591, 176280,
                192222, 193567, 196627, 203267, 208964, 212717, 214092,
                216895, 220668, 223283, 223926, 224308, 224324, 224326,
                224325, 224911, 224914, 224915, 222523, 183190, 226126,
                226127, 227321, 7227, 73239, 198215, 209261, 226185, 226186,
                227377, 229533, 227882, 31033, 5693, 226900, 7719, 96477,
                234826, 195103, 205922, 198094, 228410, 233150, 233413,
                178306, 206672, 221109, 36870, 208963, 228399, 228400,
                235279, 227941, 187420, 198214, 233412, 167539, 235443,
                237631, 240176, 182082, 295358, 203907, 243090, 243230,
                264199, 299768, 243365, 243159, 243160, 243164, 209882,
                243233, 243243, 243272, 243275, 246194, 246195, 246196,
                246197, 246198, 246199, 246200, 246201, 246202, 29390,
                59374, 882, 300852, 243231, 243276, 243277, 242507, 51511,
                286604, 196600, 228908, 243265, 254945, 7237, 7460, 258594,
                259536, 262316, 272627, 257314, 177439, 257363, 292414,
                159087, 262727, 262728, 783, 257311, 264462, 259564, 240016,
                264730, 264731, 272569, 176299, 9031, 1140, 165597, 229193,
                240292, 262543, 262981, 264198, 264732, 265072, 265311,
                266117, 266834, 266835, 246410, 267377, 267671, 266779,
                266940, 243232, 251221, 267608, 269482, 269483, 269484,
                33169, 264201, 242231, 272556, 272559, 272560, 272563,
                272567, 272620, 272624, 272831, 272989, 272994, 273035,
                237561, 272623, 257309, 272557, 272626, 272843, 7091,
                267747, 285217, 295405, 296543, 220341, 243273, 243274,
                262768, 265669, 272558, 272561, 272562, 272631, 272632,
                272633, 272634, 272635, 272844, 272944, 272947, 273057,
                273063, 273075, 273116, 273119, 273121, 272951, 247156,
                237895, 5664, 70601, 273507, 271848, 267748, 267409, 267410,
                99883, 261594, 269801, 279263, 261591, 280354, 280355,
                221988, 280477, 263820, 266264, 266265, 269797, 269799,
                278197, 279238, 281310, 260799, 34305, 281309, 282458,
                282459, 283166, 7245, 283643, 283165, 284590, 284593,
                284592, 290434, 281090, 286636, 273123, 292459, 292415,
                293614, 272943, 297245, 297246, 298386, 235909, 295319,
                283942, 294381, 76114, 264203, 5821, 119856, 5825, 273068,
                269084, 218491, 62977, 294934, 177416
            };
            
            string db_list;
            
            int num_dbs = int(sizeof(dbs)/sizeof(dbs[0]));
            
            cout << "\nDatabase will include data from:\n";
            
            for(int i = 0; i < num_dbs; i++) {
                string next_db = string("Microbial/");
                next_db.append(NStr::UIntToString(dbs[i]));
                
                cout << next_db;
                
                if (i != (num_dbs-1)) {
                    cout << ",";
                }
                
                if ((i % 5) == 4) {
                    cout << "\n    ";
                } else {
                    cout << " ";
                }
                
                if (i) {
                    db_list.append(" ");
                }
                db_list.append(next_db);
            }
            
            cout << "\nConstructing..." << endl;
            
            CSeqDB db(db_list, CSeqDB::eNucleotide);
            
            cout << "\nDone constructing; Getting first OID..." << endl;
            
            int oid = 0;
            int oid_cnt = 0;
            
            if (db.CheckOrFindOID(oid)) {
                cout << "\nFound first; Iterating over remaining OIDs..." << endl;
                
                oid_cnt = 1;
                
                for(oid_cnt = 1; db.CheckOrFindOID(oid); oid++) {
                    oid_cnt ++;
                }
            } else {
                cout << "\nNo first OID found;" << endl;
            }
            
            cout << "\nDone; " << oid_cnt << " OIDs were found." << endl;
            
            return 0;
        } else desc += " [-myriad]";
        
        if (s == "-lib") {
            CSeqDB phil(dbname, seqtype);
            phil.GetSeqLength(123);
            phil.GetSeqLengthApprox(123);
            phil.GetHdr(123);
            phil.GetBioseq(123);
            
            const char * buffer = 0;
            phil.GetSequence(123, & buffer);
            phil.RetSequence(& buffer);

            string db_type("unknown");
            switch (phil.GetSequenceType()) {
            case CSeqDB::eProtein: db_type = "protein"; break;
            case CSeqDB::eNucleotide: db_type = "nucleotide"; break;
            default: abort();
            }

            cout << "\nSeq type:     " << db_type;
            cout << "\nTitle:        " << phil.GetTitle();
            cout << "\nDate:         " << phil.GetDate();
            cout << "\nNumSeqs:      " << phil.GetNumSeqs();
            cout << "\nNumOIDs:      " << phil.GetNumOIDs();
            cout << "\nVolumeLength: " << phil.GetVolumeLength();
            cout << "\nTotalLength:  " << phil.GetTotalLength();
            cout << "\nMax Length:   " << phil.GetMaxLength() << endl;
            cout << endl;
            
            return 0;
        } else desc += " [-lib]";
        
        if (s == "-ugl-timing") {
            int STEP = 10000;
            
            if (! args.empty()) {
                string step_value = args.front();
                args.pop_front(); 
                
                int sv = atoi(step_value.c_str());
                
                STEP = (sv > 1) ? sv : 1;
            }
            
            cout << "STEP at " << STEP << endl;
            
            vector<int> gi_vec;
            vector<int> oid_vec;
            CRef<CSeqDBGiList> gi_list;
            
            {
//                 CTimedTask t1("build first seqdb object");
//                 CSeqDB db1(dbname, seqtype, 0, 0, use_mm, gi_list);
//                 t1.Mark();
                
//                 CTimedTask t2("build gi vector");
//                 int num_oids = db1.GetNumOIDs();
                
//                 // four on, four off.
//                 int pieces_of_eight = 0;
                
//                 for(int i = 0; i < num_oids; i += STEP) {
//                     list< CRef<CSeq_id> > ids = db1.GetSeqIDs(i);
                    
//                     ITERATE(list< CRef<CSeq_id> >, seqid, ids) {
//                         if ((**seqid).IsGi()) {
//                             pieces_of_eight++;
                            
//                             if (pieces_of_eight & 4) {
//                                 gi_vec.push_back((**seqid).GetGi());
//                             }
//                         }
//                     }
//                 }
//                 t2.Mark();
                
                
                CTimedTask t2b("build gi vector");
                string seqletter((seqtype == CSeqDB::eProtein) ? "p" : "n");
                SeqDB_ReadBinaryGiList("/net/fridge/vol/export/blast/db/blast/Eukaryota." + seqletter + ".gil", gi_vec);
                t2b.Mark();
                
                cout << "Total accumulated GIs: " << gi_vec.size() << endl << endl;
            }
            
            {
                CTimedTask t3("build gi list object");
                gi_list.Reset(new CVectorGiList(gi_vec));
                t3.Mark();
            }
            
            {
                CTimedTask t4("build seqdb with gilist");
                CSeqDB db2(dbname, seqtype, 0, 0, use_mm, gi_list);
                t4.Mark();
                
                int oid(0);
                
                CTimedTask t5("find one id");
                if (db2.CheckOrFindOID(oid)) {
                    oid_vec.push_back(oid++);
                }
                t5.Mark();
                
                CTimedTask t6("rest of iteration");
                if (oid) {
                    while(db2.CheckOrFindOID(oid)) {
                        oid_vec.push_back(oid++);
                    }
                }
                t6.Mark();
                cout << "Total accumulated OIDs: " << oid_vec.size() << endl;
            }
            
            return 0;
        } else desc += " [-ugl-timing]";
        
        if (s == "-euk-nt-timing") {
            CTimedTask t1("build euk gi list");
            CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("/net/fridge/vol/export/blast/db/blast/Eukaryota.n.gil"));
            t1.Mark();
            
            CTimedTask t2("build nucl_dbs seqdb");
            string dbn = "nucl_dbs";
            CSeqDB db(dbn, CSeqDB::eNucleotide, 0, 0, use_mm, gi_list);
            t2.Mark();
            
            int count = 0;
            
            CTimedTask t3("find first oid");
            int oid = 0;
            db.CheckOrFindOID(oid);
            t3.Mark();
            
            CTimedTask t4("find rest of oids");
            for(oid = 0; db.CheckOrFindOID(oid); oid++) {
                count ++;
            }
            t4.Mark();
            
            return 0;
        } else desc += " [-euk-nt-timing]";
        
        if (s == "-euk2") {
            CTimedTask t1("build euk gi list");
            CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("/net/fridge/vol/export/blast/db/blast/Mammalia.n.gil"));
            t1.Mark();
            
            CTimedTask t2("build euk2 seqdb");
            CSeqDB db("euk2", CSeqDB::eNucleotide, 0, 0, use_mm, gi_list);
            t2.Mark();
            
            int count = 0;
            
            CTimedTask t3("find first oid");
            int oid = 0;
            db.CheckOrFindOID(oid);
            t3.Mark();
            
            CTimedTask t4("find rest of oids");
            for(oid = 0; db.CheckOrFindOID(oid); oid++) {
                count ++;
            }
            t4.Mark();
            
            return 0;
        } else desc += " [-euk2]";
        
        if (s == "-euknt") {
            CTimedTask t1("build euk gi list");
            CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("/net/fridge/vol/export/blast/db/blast/Eukaryota.n.gil"));
            t1.Mark();
            
            CTimedTask t2("build nt seqdb");
            string dbn = "nt";
            CSeqDB db(dbn, CSeqDB::eNucleotide, 0, 0, use_mm, gi_list);
            t2.Mark();
            
            int count = 0;
            
            CTimedTask t3("find first oid");
            int oid = 0;
            db.CheckOrFindOID(oid);
            t3.Mark();
            
            CTimedTask t4("find rest of oids");
            for(oid = 0; db.CheckOrFindOID(oid); oid++) {
                count ++;
            }
            t4.Mark();
            
            cout << "Total number of oids found: " << count << endl;
            return 0;
        } else desc += " [-euknt]";
        
        if (s == "-translate-gis") {
            string listfile;
            if (! args.empty()) {
                listfile = args.front();
                args.pop_front(); 
            } else {
                cout << "Usage: test_pin -translate-gis <filename>" << endl;
                return 0;
            }
            
            string blastdb_path("/net/fridge/vol/export/blast/db/blast/Eukaryota");
            blastdb_path.append((seqtype == CSeqDB::eProtein) ? ".p.gil" : ".n.gil");
            
            CTimedTask t1("build gi list (" + blastdb_path + ")");
            CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList(blastdb_path));
            t1.Mark();
            
            CTimedTask t2(string("build seqdb (") + dbname + ")");
            CSeqDB db(dbname, seqtype, 0, 0, use_mm, gi_list);
            t2.Mark();
            
            int count = 0;
            
            CTimedTask t3("find first oid");
            int oid = 0;
            db.CheckOrFindOID(oid);
            t3.Mark();
            
            CTimedTask t4("find rest of oids");
            for(oid = 0; db.CheckOrFindOID(oid); oid++) {
                count ++;
            }
            cout << "Total number of oids found: " << count << endl;
            t4.Mark();
            
            CTimedTask t5("read gis from file");
            vector<int> gis;
            SeqDB_ReadGiList(listfile, gis);
            cout << "  Total number of gis read from file: " << gis.size() << endl;
            t5.Mark();
            
            CTimedTask t5a("sort gis read from file");
            sort(gis.begin(), gis.end());
            cout << "  Gis after sorting: " << gis.size() << endl;
            t5a.Mark();
            
            CTimedTask t5b("uniquify sorted gis");
            {
                int prev_gi = -1;
                vector<int> unique;
                ITERATE(vector<int>, gi_iter, gis) {
                    if (prev_gi != *gi_iter) {
                        unique.push_back(prev_gi = *gi_iter);
                    }
                }
                unique.swap(gis);
            }
            cout << "  Total number of gis after uniquification: " << gis.size() << endl;
            t5b.Mark();
            
            CTimedTask t6("convert to oids");
            vector<int> oids;
            ITERATE(vector<int>, gi_iter, gis) {
                int oid(0);
                if (db.GiToOid(*gi_iter, oid)) {
                    oids.push_back(oid);
                }
            }
            double e6sec = t6.Mark();
            cout << "    Total number of oids corresponding to GIs: " << oids.size() << endl;
            cout << "    Time: " << e6sec << ", oids found per second: " << (oids.size()/(e6sec)) << "\n\n";
            
            CTimedTask t7("convert oids (back) to gis");
            vector<int> gis2;
            gis2.reserve(oids.size());
            
            ITERATE(vector<int>, oid_iter, oids) {
                db.GetGis(*oid_iter, gis2, true);
            }
            double e7sec = t7.Mark();
            
            cout << "    Total number of gis corresponding to OIDs: " << gis2.size() << endl;
            cout << "    Time: " << e7sec << ", gis found per second: " << (gis2.size()/(e7sec)) << endl;
            cout << "    Time: " << e7sec << ", oids translated per second: " << (oids.size()/e7sec) << endl;
            
            cout << "    Total number of oids found: " << count << endl;
            return 0;
        } else desc += " [-translate-gis <filename>]";
        
        if (s == "-refrna") {
            string gil_name("Danio_rerio.n");
            //string gil_name("Homo_sapiens.n");
            CTimedTask t1(string("build ") + gil_name + " list");
            string gilist_fname(string("/net/fridge/vol/export/blast/db/blast/") + gil_name + ".gil");
            CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList(gilist_fname));
            
            //CTimedTask t1("build euk gi list");
            //CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("/net/fridge/vol/export/blast/db/blast/Eukaryota.n.gil"));
            t1.Mark();
            
            CTimedTask t2("build refseq_rna seqdb");
            string dbn = "refseq_rna";
            CSeqDB db(dbn, CSeqDB::eNucleotide, 0, 0, use_mm, gi_list);
            t2.Mark();
            
            int count = 0;
            
            CTimedTask t3("find first oid");
            int oid = 0;
            db.CheckOrFindOID(oid);
            t3.Mark();
            
            CTimedTask t4("find rest of oids");
            vector<int> gis;
            for(oid = 0; db.CheckOrFindOID(oid); oid++) {
                db.GetGis(oid, gis, true);
                count ++;
            }
            t4.Mark();
            
//             cout << "Dumping genomic ids:" << endl;
//             for(int q = 0; q<gis.size(); q++) {
//                 cout << "xgi:" << gis[q] << "\n";
//             }
            cout << "Total number of oids found: " << count << endl;
            return 0;
        } else desc += " [-refrna]";
        
        if (s == "-user-gi-list") {
            int gi = 129295;
            
            CRef<CSeqDBGiList> gi_list(new CGiOidList);
            
            CSeqDB db(dbname, seqtype, 0, 0, use_mm, gi_list);
            
            cout << "Phase 1: Check filtering of Bioseq deflines.\n" << endl;
            
            CRef<CBioseq> bioseq = db.GiToBioseq(gi);
            
            auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
            
            cout << "--- Seq #" << gi << "---" << endl;
            *outpstr << *bioseq;
            
            cout << "\nPhase 2: Check filtering of OID list.\n" << endl;
            
            int num_found = 0;
            
            for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
                cout << "\n--- Oid # " << oid << " ---" << endl;
                bioseq = db.GetBioseq(oid);
                
                _ASSERT(bioseq.NotEmpty());
                
                *outpstr << *bioseq;
                
                if (++num_found > 10) {
                    cout << "Found too many.\n" << endl;
                    break;
                }
            }
            
            return 0;
        } else desc += " [-user-gi-list]";
        
        if (s == "-seqid2oids") {
            CSeqDB db(dbname, seqtype);
            
            if (args.empty()) {
                cout << "Usage: test_pin -seqid2oids <seqid>" << endl;
                return 0;
            }
            
            CSeq_id seqid(args.front());
            args.pop_front();
            
            vector<int> oids;
            db.SeqidToOids(seqid, oids);
            
            for(int i = 0; i < (int)oids.size(); i++) {
                cout << " oids[" << i << "]= " << oids[i] << ", length= " << db.GetSeqLength(oids[i]) << endl;
            }
            
            return 0;
        } else desc += " [-acc2bioseq]";
        
        if (s == "-file-gi-list2") {
            string seqchar = (seqtype == CSeqDB::eProtein) ? "p" : "n";
            
            string fn1("/net/fridge/vol/export/blast/db/blast/Bos_taurus." +
                       seqchar + ".gil");
            string fn2("/net/fridge/vol/export/blast/db/blast/Eukaryota." +
                       seqchar + ".gil");
            string fn3("/net/fridge/vol/export/blast/db/blast/hivcg.gil");
            
            CTimedTask t1a("bt build list");
            CRef<CSeqDBFileGiList> gi_list1x(new CSeqDBFileGiList(fn1));
            CRef<CSeqDBGiList> gi_list1(& (*gi_list1x));
            t1a.Mark();
            
            CTimedTask t1b("euk build list");
            CRef<CSeqDBFileGiList> gi_list2x(new CSeqDBFileGiList(fn2));
            CRef<CSeqDBGiList> gi_list2(& (*gi_list2x));
            t1b.Mark();
            
            CTimedTask t1c("hiv build list");
            CRef<CSeqDBFileGiList> gi_list3x(new CSeqDBFileGiList(fn3));
            CRef<CSeqDBGiList> gi_list3(& (*gi_list3x));
            t1c.Mark();
            
            {
                string nm = "bt";
                
                CTimedTask t2a(nm + " build seqdb");
                CSeqDB db(dbname, seqtype, 0, 0, use_mm, gi_list1);
                t2a.Mark();
                
                CTimedTask t3a(nm + " find oid");
                int cnt(0);
                for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
                    cnt ++;
                }
                t3a.Mark();
                cout << "Found " << cnt << " OIDs in gi list " << fn1 << endl;
            }
            
            {
                string nm = "euk";
                
                CTimedTask t2a(nm + " build seqdb");
                CSeqDB db(dbname, seqtype, 0, 0, use_mm, gi_list2);
                t2a.Mark();
                
                CTimedTask t3a(nm + " find oid");
                int cnt(0);
                for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
                    cnt ++;
                }
                t3a.Mark();
                cout << "Found " << cnt << " OIDs in gi list " << fn2 << endl;
            }
            
            {
                string nm = "hiv";
                
                CTimedTask t2a(nm + " build seqdb");
                CSeqDB db(dbname, seqtype, 0, 0, use_mm, gi_list3);
                t2a.Mark();
                
                CTimedTask t3a(nm + " find oid");
                int cnt(0);
                for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
                    cnt ++;
                }
                t3a.Mark();
                cout << "Found " << cnt << " OIDs in gi list " << fn3 << endl;
            }
            
//             gi_list1x->Report();
//             gi_list2x->Report();
//             gi_list3x->Report();
            
            return 0;
        } else desc += " [-file-gi-list2]";
        
        if (s == "-file-gi-list") {
            CStopWatch sw(true);
            double e1, e2, e3, e4, e5;
            e1 = sw.Elapsed();
            
            string fn("/net/fridge/vol/export/blast/db/blast/Bos_taurus.p.gil");
            
            {
                cout << "build1..." << endl;
                CRef<CSeqDBGiList> file_gi_list(new CSeqDBFileGiList(fn));
                
                e2 = sw.Elapsed();
                
                cout << "build2..." << endl;
                CSeqDB db(dbname, seqtype, 0, 0, use_mm, file_gi_list);
                
                e3 = sw.Elapsed();
                
                cout << "get oid..." << endl;
                int oid = 0;
                db.CheckOrFindOID(oid);
                cout << "First oid = " << oid << endl;
                
                e4 = sw.Elapsed();
                cout << "destruct..." << endl;
            }
            e5 = sw.Elapsed();
            
            cout << "Time for task (build gi list):  " << (e2-e1) << endl;
            cout << "Time for task (build seqdb):    " << (e3-e2) << endl;
            cout << "Time for task (find first oid): " << (e4-e3) << endl;
            cout << "Time for task (destruct):       " << (e5-e4) << endl;
            
            return 0;
        } else desc += " [-file-gi-list]";
        
        if (s == "-summary") {
            CSeqDB phil(dbname, seqtype);
            cout << "dbpath: " << dbpath            << endl;
            cout << "title:  " << phil.GetTitle()   << endl;
            cout << "nseqs:  " << phil.GetNumSeqs() << endl;
            cout << "noids:  " << phil.GetNumOIDs() << endl;
            cout << "tleng:  " << phil.GetTotalLength() << endl;
            cout << "vleng:  " << phil.GetVolumeLength() << endl;
            return 0;
        } else desc += " [-summary]";
        
        if (s == "-dogs") {
            CSeqDB al("cfa_genome/cra_dog_assembly", CSeqDB::eNucleotide);
            return 0;
        } else desc += " [-dogs]";
        
        if (s == "-simple-iteration") {
            CTimedTask t1("iterate over db");
            CSeqDB db(dbname, seqtype);
            vector<int> gis;
            int count =0;
            for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
                count++;
                db.GetGis(oid, gis, true);
            }
            double spent = t1.Mark();
            cout << "OIDs found: " << count << " per second: " << (count / spent) << endl;
            cout << "GIs found:  " << gis.size() << " per second: " << (gis.size() / spent) << endl;
            
            int num_show = 10;
            
            for(int i = 0; (i<num_show) && (i<(int)gis.size()); i++) {
                if (! i) { // said the duck
                    cout << "\nFirst few GIs: " << gis[0];
                } else {
                    cout << ", " << gis[i];
                }
            }
            
            if (gis.size() > num_show) {
                cout << " ...";
            }
            cout << "\n" << endl;
            
            return 0;
        } else desc += " [-simple-iteration]";
        
        if (s == "-megabarley") {
            CStopWatch sw(true);
            
            double e1 = sw.Elapsed();
            double e2 = e1;
            
            int i(0);
            while((e2 - e1) < 10.0) {
                CSeqDB cob("genomes/barley", CSeqDB::eNucleotide);
                i++;
                e2 = sw.Elapsed();
            }
            
            cout << "Iterations of barley in " << (e2-e1) << " seconds: " << i << endl;
            return 0;
        } else desc += " [-megabarley]";
        
        if (s == "-one") {
            int xt_iter = 0;
            const int Asize = 100;
            double xt[Asize];
            const char * tag[Asize];
            
            CStopWatch sw(true);
            
            xt[xt_iter++] = sw.Elapsed();
            CSeqDB al("nr", CSeqDB::eProtein);
            xt[xt_iter++] = sw.Elapsed();
            cerr << "hmmmmmm" << endl;
            xt[xt_iter++] = sw.Elapsed();
            
            const char * buf = 0;
            
            for (int j = 0; j<10; j++) {
                xt[xt_iter++] = sw.Elapsed();
                al.GetSequence(1001, & buf);
            }
            int k01 = xt_iter;
            xt[xt_iter++] = sw.Elapsed();
            
            for(int i = 0; i<100000; i++) {
                al.GetSequence(1001, & buf);
            }
            xt[xt_iter++] = sw.Elapsed();
            
            for(int i = 0; i < Asize; i++) {
                tag[i] = 0;
            }
            
            tag[0] = "initial";
            tag[1] = "getting nr";
            tag[2] = "<nothing>";
            tag[3] = "loop start...";
            tag[k01] = "post loop";
            tag[k01+1] = "post big loop (end)";
            
            double lastly = 0.0;
            
            for(int k = 0; k<xt_iter; k++) {
                cerr << "xt[" << k
                     << "]=" << xt[k]
                     << "    diff=" << xt[k]-lastly
                     << "  tag " << (tag[k] ? tag[k] : "-")
                     << endl;
                
                lastly = xt[k];
            }
            
            return 0;
        } else desc += " [-one]";
        
        if (s == "-overlap") {
            CSeqDB sp("hoola/Test/pataa_emb", CSeqDB::eProtein);
            
            cout << " num oids " << sp.GetNumSeqs() << endl;
            
            vector<int> chunky;
            chunky.resize(1000000);
            
            int b(0), e(0);
            
            CSeqDB::EOidListType et = sp.GetNextOIDChunk(b,e,chunky);
            
            if (b != e) {
                cout << " oh boy. " << endl;
                return 0;
            }
            
            cout << "et=" << et << ", chunky size=" << chunky.size() << endl;
            
            for(unsigned i = 0; i<chunky.size(); i++) {
                cout << chunky[i] << endl;
            }
            
            return 0;
        } else desc += " [-overlap]";
        
//         if (s == "-alias") {
//             string dbname = "pdb";
            
//             if (! args.empty()) {
//                 dbname = args.front();
//                 args.pop_front(); 
//             }

//             string ending = "pal";
//             if (! args.empty()) {
//                 ending = args.front();
//                 args.pop_front(); 
//             }
            
//             ncbi::CSeqDBAliasNode phil(/*dbpath,*/ dbname, ending[0]);
            
//             vector<string> vols;
            
//             phil.GetVolumeNames(vols);
            
//             for(Uint4 i = 0; i<vols.size(); i++) {
//                 cout << "[" << i << "] "
//                      << vols[i] << endl;
//             }
            
//             return 0;
//         } else desc += " [-alias]";
        
        if (s == "-len3") {
            string dbname1("nr");
            string dbname2("pataa");
            string dbname3(dbname1 + " " + dbname2);
            
            CSeqDB dbi1(/*dbpath,*/ dbname1, CSeqDB::eNucleotide, 0, 0, use_mm);
            CSeqDB dbi2(/*dbpath,*/ dbname2, CSeqDB::eNucleotide, 0, 0, use_mm);
            CSeqDB dbi3(/*dbpath,*/ dbname3, CSeqDB::eNucleotide, 0, 0, use_mm);
            
            cout << "1 " << dbi1.GetTotalLength() << endl;
            cout << "2 " << dbi2.GetTotalLength() << endl;
            cout << "3 " << dbi3.GetTotalLength() << endl;

            cout << "---------------" << endl;
            
            Uint4 len1 = dbi1.GetNumSeqs();
            Uint4 len2 = dbi2.GetNumSeqs();
            Uint4 len3 = dbi3.GetNumSeqs();
            
            cout << "1 " << len1 << endl;
            cout << "2 " << len2 << endl;
            cout << "3 " << len3 << endl;
            
            Uint8 len_tot = 0;
            
            len_tot = 0;
            for(Uint4 i = 0; i<len1; i++) {
                len_tot += dbi1.GetSeqLength(i);
            }
            cout << "total1 " << len_tot << endl;
            

            len_tot = 0;
            for(Uint4 i = 0; i<len2; i++) {
                len_tot += dbi2.GetSeqLength(i);
            }
            cout << "total2 " << len_tot << endl;
            
            len_tot = 0;
            for(Uint4 i = 0; i<len3; i++) {
                len_tot += dbi3.GetSeqLength(i);
            }
            cout << "total3 " << len_tot << endl;
            
            return 0;
        } else desc += " [-len3]";
        
        if ((s == "-nt3") || (s == "-nt3a")) {
            approx = false;
            
            if (s == "-nt3a") {
                approx = true;
            }
            
            string dbname1 = "month";
            string dbname2 = "est";
            
            string dbname3(dbname1 + " " + dbname2);
            
            CSeqDB dbi1(/*dbpath,*/ dbname1, CSeqDB::eNucleotide, 0, 0, use_mm);
            CSeqDB dbi2(/*dbpath,*/ dbname2, CSeqDB::eNucleotide, 0, 0, use_mm);
            CSeqDB dbi3(/*dbpath,*/ dbname3, CSeqDB::eNucleotide, 0, 0, use_mm);
            
            cout << "1 " << dbi1.GetTotalLength() << endl;
            cout << "2 " << dbi2.GetTotalLength() << endl;
            cout << "3 " << dbi3.GetTotalLength() << endl;

            cout << "---------------" << endl;
            
            Uint4 len1 = dbi1.GetNumSeqs();
            Uint4 len2 = dbi2.GetNumSeqs();
            Uint4 len3 = dbi3.GetNumSeqs();

            len1 /= 10;
            len2 /= 10;
            len3 = len1 + len2;
            
            cout << "1 " << len1 << endl;
            cout << "2 " << len2 << endl;
            cout << "3 " << len3 << endl;
            
            Uint8 len_tot = 0;
            
            len_tot = 0;
            
            if (approx) {
                for(Uint4 i = 0; i<len1; i++) {
                    len_tot += dbi1.GetSeqLengthApprox(i);
                }
            } else {
                for(Uint4 i = 0; i<len1; i++) {
                    len_tot += dbi1.GetSeqLength(i);
                }
            }
            cout << "total1 " << len_tot << endl;
            

            len_tot = 0;
            if (approx) {
                for(Uint4 i = 0; i<len2; i++) {
                    len_tot += dbi2.GetSeqLengthApprox(i);
                }
            } else {
                for(Uint4 i = 0; i<len2; i++) {
                    len_tot += dbi2.GetSeqLength(i);
                }
            }
            cout << "total2 " << len_tot << endl;
            
            len_tot = 0;
            if (approx) {
                for(Uint4 i = 0; i<len3; i++) {
                    len_tot += dbi3.GetSeqLengthApprox(i);
                }
            } else {
                for(Uint4 i = 0; i<len3; i++) {
                    len_tot += dbi3.GetSeqLength(i);
                }
            }
            cout << "total3 " << len_tot << endl;
            
            return 0;
        } else desc += " [-nt3 | -nt3a]";
        
        if (s == "-mm") {
            use_mm = true;
            continue;
        } else desc += " [-mm]";
        
        if (s == "-defer-ret") {
            defer_ret = true;
            continue;
        } else desc += " [-defer-ret]";
        
        if (s == "-nt") {
            dbname = "nt";
            seqtype = CSeqDB::eNucleotide;
            use_mm = false;
            continue;
        } else desc += " [-nt]";
        
        if (s == "-db") {
            if (args.empty()) {
                cerr << "Option -db needs an argument." << endl;
                return 0;
            } else {
                dbname = *args.begin();
                args.pop_front();
            }
            continue;
        } else desc += " [-db]";
        
        if (s == "-nucl") {
            seqtype = CSeqDB::eNucleotide;
            continue;
        } else desc += " [-nucl]";
        
        if (s == "-prot") {
            seqtype = CSeqDB::eProtein;
            continue;
        } else desc += " [-prot]";

        if (s == "-show-ambig") {
            CSeqDB db(dbname.c_str(), seqtype);
            
            const char * datap = 0;
            Uint4 len0 = db.GetAmbigSeq(0, & datap, kSeqDBNuclBlastNA8);
            
            int j = 0;
            
            for(Uint4 i = 0; i<(len0+2); i++) {
                //cout << "number " << int(datap[i]) << endl;
                
                j++;
                
                switch(int(datap[i])) {
                case 15: cout << "!\n"; j=0; break;
                case 0:  cout << "A"; break;
                case 1:  cout << "C"; break;
                case 2:  cout << "G"; break;
                case 3:  cout << "T"; break;
                default:
                    cout << "<" << int(datap[i] & 0xFF) << ">";
                }
                if (j >= 80) {
                    j = 0;
                    cout << "\n";
                }
            }
            cout << endl;
            
            db.RetSequence(& datap);
            
            return 0;
        } else desc += " [-show-ambig]";
        
        if ((s == "-nt,est,gss") || (s == "-nt,gss,est") || (s == "-est,gss,nt")) {
            dbname = "nt est gss";
            seqtype = CSeqDB::eNucleotide;
            continue;
        } else desc += " [-nt,est,gss]";
        
        if (s == "-lump") {
            dbname = "../tdata/lump";
            seqtype = CSeqDB::eNucleotide;
            use_mm = false;
            continue;
        } else desc += " [-lump]";
        
        if (s == "-super_size") {
            CSeqDB db("nt est", CSeqDB::eNucleotide, 0, 0, use_mm);
            
            cout << "total length: " << db.GetTotalLength() << endl;
            cout << "number seqs : " << db.GetNumSeqs() << endl;
            
            Uint4 nseq = db.GetNumSeqs();
            
            const char * seqdata = 0;
            
            for(int i = 0; i<1000; i++) { 
                const char * s2 = 0;
                db.GetSequence(i, & s2);
                
                cout << "Got seq " << i << " first char " << (0xFF & s2[0]) << endl;
                
                if (seqdata) {
                    db.RetSequence(& seqdata);
                    //assert(seqdata == 0);
                }
                
                seqdata = s2;
            }
            
            unsigned iters = 10000;
            
            for(unsigned j = 0; j<iters; j++) {
                unsigned j2 = j * (nseq/iters);
                
                const char * s2 = 0;
                
                db.GetSequence(j, & s2);
                
                cout << "Got seq " << j2 << " first char " << (0xFF & s2[0]) << endl;
                
                if (seqdata) {
                    db.RetSequence(& seqdata);
                    assert(seqdata == 0);
                }
                
                seqdata = s2;
            }
            
            if (seqdata) {
                db.RetSequence(& seqdata);
                seqdata = 0;
            }
            
        } else desc += " [-lump]";
        
        if (s == "-file") {
            use_mm = false;
            continue;
        } else desc += " [-mm | -file]";
        
        if (s == "-db2") {
            dbname = "nr pataa";
            continue;
        } else desc += " [-db2]";
        
        if (s == "-aliasn") {
            dbname = "month htgs";
            seqtype = CSeqDB::eNucleotide;
            continue;
        } else desc += " [-aliasn]";
        
        if (s == "-approx") {
            approx = true;
            continue;
        } else desc += " [-approx]";
        
//         if (s == "-ambig") {
//             dbname = "/home/bealer/seqdb/dbs/ambig";
//             continue;
//         } else desc += " [-ambig]";
        
//         if (s == "-kevinx") {
//             dbname = "/home/bealer/seqdb/dbs/kevinx";
//             continue;
//         } else desc += " [-kevinx]";
        
        if (s == "-no-del") {
            deletions = false;
            continue;
        } else desc += " [-no-del]";
        
        if (s == "-look") {
            look_seq = true;
            continue;
        } else desc += " [-look]";
        
        if (s == "-get-bio") {
            get_bioseq = true;
            continue;
        } else desc += " [-get-bio]";
        
        if (s == "-no-progress") {
            show_progress = false;
            continue;
        } else desc += " [-no-progress]";
        
        if (s == "-show-bio") {
            show_bioseq = true;
            get_bioseq  = true;
            continue;
        } else desc += " [-show-bio]";
        
        if (s == "-show-fasta") {
            show_fasta = true;
            get_bioseq = true;
            continue;
        } else desc += " [-show-fasta]";
        
        if (s == "-membound") {
            if (args.size() < 2) {
                cerr << "Error: -membound requires two arguments." << endl;
                failed = true;
            }
            
            string s2 = *args.begin();
            args.pop_front();
            
            string s3 = *args.begin();
            args.pop_front();
            
            int s2_num = atoi(s2.c_str());
            int s3_num = atoi(s3.c_str());
            
            if (s2_num > 0) {
                membound = s2_num;
            }
            
            if (s3_num > 0) {
                slicesize = s3_num;
            }
            
            continue;
        } else desc += " [-membound]";
        
        if (s == "-num") {
            if (args.empty()) {
                cerr << "Error: -num requires an argument." << endl;
                failed = true;
            }
            
            string s2 = *args.begin();
            args.pop_front();
            
            int s2_num = atoi(s2.c_str());
            
            if (s2_num > 0) {
                num_display = s2_num;
            }
            
            continue;
        } else desc += " [-num <seqs to get>]";
        
        if (s == "-oid-at-offset") {
            CSeqDB db(dbname, CSeqDB::eUnknown);
            
            while(1) {
                string indx_str;
                Uint8 indx(0);
                cout << "\nWhat index (0 to end): " << flush;
                cin >> indx_str;
                indx = NStr::StringToUInt8(indx_str);
                
                if (cin && (indx != 0)) {
                    int oid = db.GetOidAtOffset(0, indx);
                    cout << "get at " << indx << " is " << oid << endl;
                } else {
                    break;
                }
            }
            
            return 0;
        } else desc += " [-oid-at-offset]";
        
        if ((s == "-splitp") || (s == "-splitn")) {
            string db;
            CSeqDB::ESeqType st;
            
            if (s == "-splitp") {
                db = "nr";
                st = CSeqDB::eProtein;
            } else {
                db = "nt";
                st = CSeqDB::eNucleotide;
            }
            
            CSeqDB nr(db.c_str(), st);
            
            Uint8 total_bases = nr.GetTotalLength();
            Uint8 num_seqs    = nr.GetNumSeqs();
            
            for(Uint4 i = 0; i<10; i++) {
                Uint8 offset = (i*total_bases) / 10;
                
                Uint4 split_oid = nr.GetOidAtOffset(0, offset);
                
                cout << "\n\niteration #" << i << "\n\n";
                cout << "req. offset = " << offset      << "\n";
                cout << "total_bases = " << total_bases << "\n";
                cout << "num_seqs    = " << num_seqs    << "\n";
                cout << "split_oid   = " << split_oid   << endl;
                cout << "base %      = " << (100.0*offset)/total_bases << endl;
                cout << "oids %      = " << (100.0*split_oid)/num_seqs << endl;
            }
            
            Uint8 start  = 0; 
            Uint8 offset = start + (total_bases - start) / 2;
           
            int oid1 = 1;
            int oid2 = 0;
            
            int oid_same_cnt = 0;
            
            while((oid_same_cnt < 10) && ((total_bases - start) > 5)) {
                oid1 = oid2;
                oid2 = nr.GetOidAtOffset(0, offset);
                
                if (oid1 == oid2) {
                    oid_same_cnt ++;
                } else {
                    oid_same_cnt = 0;
                }
                
                cout << "Convergence?  split_oid = " << oid2
                     << " (of " << num_seqs
                     << ")  given residue " << offset
                     << " of total " << total_bases;
                
                if (oid_same_cnt) {
                    cout << "  !! SAME";
                }
                cout << endl;
                
                start = offset;
                offset = start + (total_bases - start) / 2;
            }
            
            return 0;
        } else desc += " [-splitp | -splitn]";
        
        if (s == "-loop") {
            if (args.empty()) {
                cerr << "Error: -loop requires an argument." << endl;
                failed = true;
            }
            
            string s2 = *args.begin();
            args.pop_front();
            
            int s2_num = atoi(s2.c_str());
            
            if (s2_num > 0) {
                num_itera = s2_num;
            }
            
            continue;
        } else desc += " [-loop <iterations>]";
        
        if (s == "-") {
            cout << "\nUsage:\n\n" << argv[0];
            
            while(desc.length() > 70) {
                int loc = 70;
                
                for(int i = 20; i<70; i++) {
                    if (desc[i] == '[') {
                        loc = i;
                    }
                }
                
                cout << string(desc, 0, loc-1) << "\n"
                     << string(string(argv[0]).length() + 1, ' ');
                
                desc.erase(0, loc);
            }
            
            cout << desc << endl;
            
            return 0;
        }
        
        cerr << "Unknown option: " << s << endl;
        failed = true;
    }
    
    if (failed) {
        
    }
    
    if (failed)
        return 1;
    
    cout << "Using [" << (use_mm ? "mm" : "file") << "] mode." << endl;
    
    if (num_display != -1) {
        cout << "Displaying [" << (Int4)(num_display) << "]." << endl;
    }
    
    if (num_itera != 1) {
        cout << "Iterating [" << (num_itera) << "] times." << endl;
    }
    
    if (! deletions) {
        cout << "Omitting deletions." << endl;
    }
    
    cout << "------- starting -------" << endl;
    
    //thr_test();
    //return 0;
    
    
    CStopWatch sw(true);
    
    for(int k = 0; k<num_itera; k++) {
        try {
            double dstart = sw.Elapsed();
            
            CSeqDB dbi(dbname, seqtype, 0, 0, use_mm);
            
            if (membound) {
                cout << "Setting memory bound at " << membound << endl;
                dbi.SetMemoryBound(membound, slicesize);
            }
            
            if (show_progress)
                cout << "at line " << __LINE__ << endl;
            
            Uint4 nseqs = dbi.GetNumSeqs();
            Uint4 noids = dbi.GetNumOIDs();
            Uint8 tleng = dbi.GetTotalLength();
            Uint8 vleng = dbi.GetVolumeLength();
            
            if ((num_display <= 0) || (Uint4(num_display) > nseqs)) {
                num_display = noids;
            }
            
            if (show_progress)
                cout << "at line " << __LINE__ << endl;

            double dend = sw.Elapsed();
            
            if (show_progress) {
                cout << "DB seq count: " << (Int4)nseqs     << endl;
                cout << "DB oid count: " << (Int4)noids     << endl;
                cout << "Total length: " << tleng           << endl;
                cout << "Volume len  : " << vleng           << endl;
                cout << "Compute time: " << (dend - dstart) << endl;
            }
            
            double cstart = sw.Elapsed();
        
            Uint8 cleng = 0;
            
            Uint4 report_at = 0;
            
            Uint8 sampling = 0;
            Uint8 numsamp  = 0;
            
            if (show_progress)
                cout << "at line " << __LINE__ << endl;
            
            
            // These will get the sequences - these pointers will never be
            // nulled out after the loop, but the CSeqDB destructor will
            // reclaim the storage -- unless we are in mmap() mode, in
            // which case all of the memory get and set operations are
            // effectively noops.
        
            const char * buffer1[10];
            {
                for(int i = 0; i<10; i++) {
                    buffer1[i] = 0;
                }
            }
            
            Uint8 qsum = 0;
            
            for(Uint4 i = 0; i < Uint4(num_display); i++) {
                Int4 thislength = 0;
                
                cleng += (thislength = (approx ? dbi.GetSeqLengthApprox(i) : dbi.GetSeqLength(i)));
                
                if (get_bioseq) {
                    CRef<CBioseq> bioseq = dbi.GetBioseq(i);
                    
                    if (show_bioseq || show_fasta) {
                        if (show_bioseq) {
                            auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                    
                            cout << "--- Seq #" << i << "---" << endl;
                            *outpstr << *bioseq;
                        }
                    
                        if (show_bioseq && show_fasta) {
                            cout << "--- Fasta ---" << endl;
                        }
                    
                        if (show_fasta) {
                            CFastaOstream fost(cout);
                            fost.SetWidth(80);
                            fost.Write(*bioseq);
                        }
                    
                        if (show_bioseq && show_fasta) {
                            cout << "--- Seq done ---" << endl;
                        }
                    }
                }
                
                int ii = i % 10;
                
                if (deletions && buffer1[ii]) {
                    dbi.RetSequence(& buffer1[ii]);
                    buffer1[ii] = 0;
                }
                
                //cout << "[" << i << "]" << endl;
                
                const char * bufdata = 0;
                const char * seqbuf = 0;
                
                Int4 seqlen = 0;
                
                if (defer_ret) {
                    seqlen = dbi.GetSequence(i, & buffer1[ii]);
                    bufdata = buffer1[ii];
                } else {
                    seqlen = dbi.GetSequence(i, & seqbuf);
                    bufdata = seqbuf;
                }
                
                if (look_seq) {
                    int qstride = 100;
                    for(int q = 0; q < seqlen; q += qstride) {
                        qsum += Int8(bufdata[q]) & 0xFF;
                    }
                }
                
                if (! defer_ret) {
                    dbi.RetSequence(& seqbuf);
		}
                
                if (show_progress) {
                    if (i >= report_at) {
                        double t = sw.Elapsed() - cstart;
                        double s_per_t = i / (t ? t : 0.00001);
                        
                        cout << "t[" << t << "] s/t[" << s_per_t << "] REPORTING: i=" << i
                             << ", accumulated length = " << cleng
                             << ", (this length = " << thislength << "), qsum=" << qsum << "\n";
                        
                        report_at = Uint4(i * 1.5);
                        
                        if (report_at > Uint4(num_display)) {
                            report_at = Uint4(num_display - 1);
                        }
                        
                        sampling += thislength;
                        numsamp ++;
                    }
                }
            }
            
            cout << "Dumping remaining sequences:" << endl;
            
            {
                for(int i = 0; i<10; i++) {
                    if (buffer1[i] != 0) {
                        dbi.RetSequence(& buffer1[i]);
                        cout << "Deleted.  Is zero? " << ((buffer1[i]==0)?("yes"):("no")) << endl;
                        buffer1[i] = 0;
                    }
                }
            }
            cout << "Done dumping remaining sequences." << endl;
            
            double cend = sw.Elapsed();
            
            if (show_progress) {
                cout << "\nNR seq count:  " << (Int4)nseqs   << "\n";
                cout << "Total clength: "   << cleng   << "\n";
                cout << "Sampling est:  "   << ((sampling / double(numsamp)) * nseqs) << "\n";
                cout << "Compute ctime: "   << (cend - cstart) << endl;
            }
        }
        catch(CSeqDBException & ex) {
            cout << "Message follows: " << ex.GetErrCodeString() << endl;
            cout << "Actual Message : " << ex.GetMsg() << endl;
        }
        catch(string ee) {
            cout << "Caught me an " << ee << endl;
        }
        catch(std::exception ex) {
            cout << "Or maybe " << ex.what() << endl;
        }
    }
    
    return 0;
}

int test2(void)
{
    cout << "whoop" << endl;
    
    int done_count = 0;
    
    try {
        CMemoryFile a("/net/fridge/vol/export/blast/db/blast/nt.00.nsq");
        done_count ++;
        CMemoryFile b("/net/fridge/vol/export/blast/db/blast/nt.00.nsq");
        done_count ++;
        CMemoryFile c("/net/fridge/vol/export/blast/db/blast/nt.00.nsq");
        done_count ++;
    }
    catch(...) {
        cerr << "Shinola.." << endl;
    }
    
    cout << "Doneness: " << done_count << endl;
    
    return 3;
}

int main(int argc, char ** argv)
{
    int rc = 0;
    
    try {
        cout << "--one--" << endl;
        rc = test1(argc, argv);
        cout << "--two--" << endl;
    }
    catch(CSeqDBException e) {
        cout << "--three--" << endl;
        cout << "Caught a SeqDB exception: {" << e.GetErrCodeString() << "}" << endl;
        cout << "Actual Message : " << e.GetMsg() << endl;
        rc = 1;
        cout << "--four--" << endl;
    }
    catch(CException & e) {
        cout << "--three--" << endl;
        cout << "Caught an NCBI exception: {" << e.GetErrCodeString() << "}" << endl;
        cout << "Report All: {" << e.ReportAll() << "}" << endl;
        rc = 1;
        cout << "--four--" << endl;
    }
    catch(exception e) {
        cout << "--three--" << endl;
        cout << "Caught an exception: {" << e.what() << "}" << endl;
        rc = 1;
        cout << "--four--" << endl;
    }
    
    return rc;
}

