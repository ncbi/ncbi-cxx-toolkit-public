
#include <objtools/readers/seqdb/seqdb.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <iostream>
#include <string>
#include <corelib/ncbimtx.hpp>
#include <objmgr/util/sequence.hpp>

USING_NCBI_SCOPE;


#include <sys/time.h>
#include <unistd.h>

inline double dbl_time(void)
{
    struct timeval tv;
    gettimeofday(& tv, 0);
    
    return tv.tv_sec + double(tv.tv_usec) / 1000000.0;
}


struct charbox {
    char xyz[10234];
};

//#include "thr_test.cpp"

int hang10()
{
    if (long(& dbl_time) == 1102000L) {
        cout << "inconceivable!" << endl;
    }
    return 0;
}

int main(int argc, char ** argv)
{
    string dbpath = "/net/fridge/vol/export/blast/db/blast";
    
    list<string> args;
    
    while(argc > 1) {
        args.push_front(string(argv[--argc]));
    }
    
    bool use_mm        = true;
    bool deletions     = true;
    Int8 num_display   = -1;
    Int4 num_itera     = 1;
    bool look_seq      = false;
    bool show_bioseq   = false;
    bool show_fasta    = false;
    bool get_bioseq    = false;
    bool show_progress = true;
    bool approx        = true;
    
    string dbname("nr");
    char seqtype = kSeqTypeProt;
    
    bool failed      = false;
    
    while(! args.empty()) {
        string desc;
        
        string s = args.front();
        args.pop_front();
        
        if (s == "-lib") {
            CSeqDB phil(dbpath, "month", 'n', true);
            phil.GetSeqLength(123);
            phil.GetSeqLengthApprox(123);
            phil.GetHdr(123);
            phil.GetBioseq(123);
            
            const char * buffer = 0;
            phil.GetSequence(123, & buffer);
            phil.RetSequence(& buffer);

            phil.GetSeqType();

            phil.GetTitle();
            phil.GetDate();
            phil.GetNumSeqs();
            phil.GetTotalLength();
            phil.GetMaxLength();
            
            return 0;
        } else desc += " [-lib]";
        
        if (s == "-summary") {
            CSeqDB phil(dbpath, "month", 'n', true);
            cout << "dbpath: " << dbpath            << endl;
            cout << "title:  " << phil.GetTitle()   << endl;
            cout << "nseqs:  " << phil.GetNumSeqs() << endl;
            cout << "tleng:  " << phil.GetTotalLength() << endl;
            return 0;
        } else desc += " [-summary]";
        
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
            
//             ncbi::CSeqDBAliasNode phil(dbpath, dbname, ending[0], true);
            
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
            
            CSeqDB dbi1(dbpath, dbname1, kSeqTypeProt, use_mm);
            CSeqDB dbi2(dbpath, dbname2, kSeqTypeProt, use_mm);
            CSeqDB dbi3(dbpath, dbname3, kSeqTypeProt, use_mm);
            
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
            
            Uint8 x = len1 - 5;
            
            len_tot = 0;
            for(Uint4 i = 0; i<len3; i++) {
                if (i > x)
                    hang10();
                len_tot += dbi3.GetSeqLength(i);
            }
            cout << "total3 " << len_tot << endl;
            
            return 0;
        } else desc += " [-len3]";
        
        if ((s == "-nt3") || (s == "-nt3a")) {
            bool approx = false;
            
            if (s == "-nt3a") {
                approx = true;
            }
            
            string dbname1 = "month";
            string dbname2 = "est";
            
            string dbname3(dbname1 + " " + dbname2);
            
            CSeqDB dbi1(dbpath, dbname1, kSeqTypeNucl, use_mm);
            CSeqDB dbi2(dbpath, dbname2, kSeqTypeNucl, use_mm);
            CSeqDB dbi3(dbpath, dbname3, kSeqTypeNucl, use_mm);
            
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
        }
        
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
            seqtype = kSeqTypeNucl;
            continue;
        } else desc += " [-aliasn]";
        
        if (s == "-approx") {
            approx = true;
            continue;
        } else desc += " [-approx]";
        
        if (s == "-ambig") {
            dbname = "/home/bealer/seqdb/dbs/ambig";
            continue;
        } else desc += " [-ambig]";
        
        if (s == "-kevinx") {
            dbname = "/home/bealer/seqdb/dbs/kevinx";
            continue;
        } else desc += " [-kevinx]";
        
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
        } else desc += " [-num <seqs to get]";
        
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
            cout << "Usage:\n"
                 << argv[0]
                 << desc
                 << endl;
            
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
        cout << "Displaying [" << (num_display) << "]." << endl;
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
    
    
    for(int k = 0; k<num_itera; k++) {
        try {
            double dstart = dbl_time();
            
            CSeqDB dbi(dbpath, dbname, seqtype, use_mm);
            
            if (show_progress)
                cout << "at line " << __LINE__ << endl;
            
            Int8 nseqs  = (Int8) dbi.GetNumSeqs();
            Uint8 tleng = dbi.GetTotalLength();
            
            if ((num_display <= 0) || (num_display > nseqs)) {
                num_display = nseqs;
            }
            
            if (show_progress)
                cout << "at line " << __LINE__ << endl;

            double dend = dbl_time();
            
            if (show_progress) {
            cout << "NR seq count: " << nseqs   << endl;
            cout << "Total length: " << tleng   << endl;
            cout << "Compute time: " << (dend - dstart) << endl;
            }
            
            double cstart = dbl_time();
        
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
            
            for(Uint4 i = 0; i < num_display; i++) {
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
                }
                
                Int4 seqlen = dbi.GetSequence(i, & buffer1[ii]);
                const char * bufdata = buffer1[ii];
                
                if (look_seq) {
                    int qstride = 100;
                    for(int q = 0; q < seqlen; q += qstride) {
                        qsum += Int8(bufdata[q]) & 0xFF;
                    }
                }
                
                if (show_progress) {
                    if (i >= report_at) {
                        double t = dbl_time() - cstart;
                        double s_per_t = i / (t ? t : 0.00001);
                
                        cout << "t[" << t << "] s/t[" << s_per_t << "] REPORTING: i=" << i
                             << ", accumulated length = " << cleng
                             << ", (this length = " << thislength << "), qsum=" << qsum << "\n";
                    
                        report_at = Uint4(i * 1.5);
                    
                        if (report_at > num_display) {
                            report_at = num_display - 1;
                        }
                        
                        sampling += thislength;
                        numsamp ++;
                    }
                }
            }
            
            double cend = dbl_time();
            
            if (show_progress) {
                cout << "\nNR seq count:  " << nseqs   << "\n";
                cout << "Total clength: "   << cleng   << "\n";
                cout << "Sampling est:  "   << ((sampling / double(numsamp)) * nseqs) << "\n";
                cout << "Compute ctime: "   << (cend - cstart) << endl;
            }
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


#if 0
{
//     {
//         string dbname("/home/bealer/seqdb/dbs/nr.test");
        
//         CSeqDBIndexFile DBIndex(dbname, kSeqTypeUnkn, false);
        
//         Int4 seq_length5 = DBIndex.GetSeqLengthBytes(5);
        
//         cout << "Sequence length for NR sequence 5 == " << seq_length5 << endl;
//     }
    
//     {
//         string dbname("/home/bealer/seqdb/dbs/nt.test");
        
//         CSeqDBIndexFile DBIndex(dbname, kSeqTypeUnkn, false);
        
//         Int4 seq_length5 = DBIndex.GetSeqLengthBytes(5);
        
//         cout << "Sequence length for NT sequence 5 == " << seq_length5 << endl;
//     }
    
    
//     {
//         string dbname("/home/bealer/seqdb/dbs/nr.test");
        
//         CSeqDB DBIndex(dbname, kSeqTypeUnkn, false);
        
//         Int4 seq_length5 = DBIndex.GetSeqLength(5);
        
//         cout << "Sequence length for NR sequence 5 == " << seq_length5 << endl;
//     }
    
    
//     {
//         string dbname("/home/bealer/seqdb/dbs/nt.test");
        
//         CSeqDB DBIndex(dbname, kSeqTypeUnkn, false);
        
//         for (int scuba = 1; scuba < 10; scuba ++) {
//             Int4 seq_length5 = DBIndex.GetSeqLength(scuba);
            
//             cout << "Sequence length for NT sequence ("
//                  << scuba
//                  << ") == "
//                  << seq_length5 << endl;
//         }
//     }
    
    string dbname("/home/bealer/seqdb/dbs/nr.test");
    CSeqDB DBIndex(dbpath, dbname, kSeqTypeUnkn, false);
    
    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
    
    CRef<CBlast_def_line_set> phil;
    
    for(int i = 0; i<1; i++) {
        phil = DBIndex.GetHdr(i);

        // Blast-def-line-set ::= {
        //   {
        //     title "gi|16857|gb|M6485.1|HTOPATP Hitoplasma capslatum plama mebrane H+
        //  ATase gene",
        //     seqid {
        //       general {
        //         db "BL_ORD_ID",
        //         tag id 0
        //       }
        //     },
        //     taxid 0
        //   }
        // }
        
        CRef<CBlast_def_line> phil1 = phil->Get().front();
        
        const CDbtag & seq1 = phil1->GetSeqid().front()->GetGeneral();
        
        cout << "------------------------------\n";
        cout << "Fetched oid       =" << i << "\n";
        cout << "Fetched title     =<<* " << phil1->GetTitle()       << " *>> \n";
        cout << "Fetched taxid     =<<* " << phil1->GetTaxid()       << " *>>\n";
        cout << "Fetched seqid.db  =<<* " << seq1.GetDb()            << " *>>\n";
        
        if (seq1.GetTag().IsId()) {
            cout << "Fetched seqid.tag(id)  =" << seq1.GetTag().GetId() << "\n";
        } else if (seq1.GetTag().IsStr()) {
            cout << "Fetched seqid.tag(str) =<<* " << seq1.GetTag().GetStr() << " *>>\n";
        }
        
        cout << "---" << endl;
        
        if (phil.NotEmpty()) {
            *outpstr << *phil;
        }
    }
    
    cout << endl;
    //cout << "Got sequence for oid = 4:\n[" << DBIndex.GetProtSeq(4) << endl;
    
    {
        string dbname("/home/bealer/seqdb/dbs/nr.test");
        CSeqDB dbi(dbpath, dbname, kSeqTypeUnkn, false);
        
        const char * buffy = 0;
        int lenny1 = dbi.GetSequence(3, & buffy);
        
        cout << "dont vote for (strlen=" << strlen(buffy) <<") lenny = [" << lenny1 << "]" << endl;
        cout << "Seq 3 nr = [" << 0 << "]" << endl;
    }

    {
        string dbname("/home/bealer/seqdb/dbs/nt.test");
        CSeqDB dbi(dbpath, dbname, kSeqTypeUnkn, false);
        
        const char * buffy = 0;
        int lenny1 = dbi.GetSequence(3, & buffy);
        
        cout << "dont vote for (strlen=" << strlen(buffy) <<") lenny = [" << lenny1 << "]" << endl;
        cout << "Seq 3 nt = [" << 0 << "]" << endl;
    }
    
    {
        auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
        
        string dbname("/home/bealer/seqdb/dbs/nr.test");
        CSeqDB dbi(dbpath, dbname, kSeqTypeUnkn, false);
        
        //string descriptor = dbi.GetDescriptor();
        
        CRef<CBioseq> bseq = dbi.GetBioseq(4);
        
        cout << "----" << endl;
        
        *outpstr << *bseq;
        
        cout << "----" << endl;
    }
}
#endif
