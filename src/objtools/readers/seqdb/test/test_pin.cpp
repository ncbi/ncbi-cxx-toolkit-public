
#include <objtools/readers/seqdb/seqdb.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <iostream>
#include <string>
#include <corelib/ncbimtx.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <sys/types.h>
#include <sys/wait.h>

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

class CAccelThread {
public:
    CAccelThread(CSeqDB & db)
        : _db(db)
    {
    }
    
    void RunInline(void)
    {
//         _pid = fork();
        
//         if (! _pid) {
            _Iterate();
            exit(0);
//         }
    }
    
    void Run(void)
    {
        _pid = fork();
        
        if (! _pid) {
            _Iterate();
            exit(0);
        }
    }
    
    void Wait(void)
    {
        if (_pid) {
            waitpid(_pid, 0, 0);
            _pid = 0;
        }
    }
    
private:
    void _Iterate(void)
    {
        Uint4 oidx = 0;
        
        try {
            Uint4 oid = 0;
            const char * buf = 0;
        
            Uint4 hashesque = 0;
        
            Uint4 part = _db.GetNumSeqs() / 40;
            Uint4 parts = part;
        
            cout << "ITER:" << flush;
        
            while(_db.CheckOrFindOID(oid)) {
                if (oid > parts) {
                    parts += part;
                    cout << "+" << flush;
                }
                
                /*Uint4 oid_len =*/ _db.GetSequence(oid, & buf);
                
//                 for(Uint4 ii = 0; ii < oid_len; ii += 511) {
//                     hashesque += buf[ii];
//                 }
            
                _db.RetSequence(& buf);
            
                oid ++;
                oidx = oid;
                
                if (oid > 2763324) {
                    x_PossiblyStopMaybe();
                }
            }
            _hash = hashesque;
            cout << "!" << endl;
        }
        catch(...) {
            cout << "***" << oidx << "-" << _db.GetNumSeqs() << endl;
        }
    }
    
    void x_PossiblyStopMaybe(void)
    {
        //cout << " It is not widely known that I once leveled Rome. " << endl;
    }
    
    CSeqDB & _db;
    Uint4    _hash;
    Uint4    _pid;
};

int test1(int argc, char ** argv)
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
    bool accel_thread  = false;
    bool accel_inline  = false;
    
    string dbname("nr");
    char seqtype = kSeqTypeProt;
    
    bool failed      = false;
    
    while(! args.empty()) {
        string desc;
        
        string s = args.front();
        args.pop_front();
        
        if (s == "-accel1") {
            s = "-accel2";
            accel_thread = true;
        } else desc += " [-accel1]";
        
        if (s == "-accel3") {
            s = "-accel2";
            accel_thread = true;
            accel_inline = true;
        } else desc += " [-accel1]";
        
        if (s == "-accel2") {
            CSeqDB nt("nr", 'p');
            
            CAccelThread accel(nt);
            
            vector<Uint4> V;
            
            Uint4 numseq = nt.GetNumSeqs();
            
            for(Uint4 i = 0; i < numseq; i+=2) {
                // Pick a random location in the array; push the
                // value at that location onto the end.
                
                // Put i into the old location for that value.
                
                V.push_back(0);
                
                Uint4 loc = (rand() + (rand() << 16)) % V.size();
                
                V[i/2] = V[loc];
                V[loc] = i/2;
            }
            
            double t_s(dbl_time());
            
            if (accel_thread) {
                if (accel_inline) {
                    accel.RunInline();
                } else {
                    accel.Run();
                }
            }
            
            Uint4 part = V.size() / 40;
            Uint4 parts = part;
            
            cout << "iter:" << flush;
            
            for(Uint4 i = 0; i < V.size(); i++) {
                if (i > parts) {
                    parts += part;
                    cout << "-" << flush;
                }
                
                Uint4 oid = V[i];
                
                const char * buf = 0;
                nt.GetSequence(oid, & buf);
                nt.RetSequence(& buf);
            }
            
            double t_e(dbl_time());
            
            cout << "#" << endl;
            cout << "time elapsed: " << (t_e - t_s) << endl;
            
            
            double w_s(dbl_time());
            
            accel.Wait();
            
            double w_e(dbl_time());
            
            cout << "time elapsed: " << (w_e - w_s) << endl;
            
            return 0;
        } else desc += " [-accel2]";
        
        if (s == "-fromcpp") {
            const char * ge = getenv("BLASTDB");
            string pe("BLASTDB=/home/bealer/code/ut/internal/blast/unit_test/data:" + string(ge ? ge : ""));
            putenv((char*)pe.c_str());
            
            CSeqDB local1("seqp", 'p');
            CSeqDB local2("seqp", 'p', 0, 0, false);
            
            Int4 num1 = local1.GetNumSeqs();
            Int4 num2 = local1.GetNumSeqs();
        
            assert(num1 >= 1);
            assert(num1 == num2);
            
            cout << "Test worked." << endl;
            return 0;
        } else desc += " [-fromcpp]";
        
        if (s == "-alphabeta") {
            
            // Note: this test is NOT EXPECTED to work, unless the
            // user has databases named "alpha" and "beta" somewhere
            // in their path.
            
            CSeqDB ab("alpha beta", 'p');
            
            for(Uint4 i = 0; i < ab.GetNumSeqs(); i++) {
                cout << "-----seq " << i << " length " << ab.GetSeqLength(i) << " ------------" << endl;
            }
            
            return 0;
        } else desc += " [-alphabeta]";
        
        if (s == "-here") {
            CSeqDB nr("tenth", 'p');
            
            for(int i = 0; i<100; i++) {
                CRef<CBioseq> bs = nr.GetBioseq(i);
                
                cout << "-----seq " << i << "------------" << endl;
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                
                *outpstr << *bs;
            }
            
            return 0;
        } else desc += " [-here]";
        
        if (s == "-dyn") {
            CSeqDB db("nr", 'p');
            
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
        
        if (s == "-limit") {
            {
                CSeqDB db("/home/bealer/seqdb/tenth", 'p', 10, 20, true);
                
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
                CSeqDB db("swissprot", 'p', 135, 175, true);
                
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
            CSeqDB nr("/home/bealer/seqdb/tenth", 'p');
            
            for(int i = 0; i<100; i++) {
                CRef<CBioseq> bs = nr.GetBioseq(i);
                
                cout << "-----seq " << i << "------------" << endl;
                
                auto_ptr<CObjectOStream>
                    outpstr(CObjectOStream::Open(eSerial_AsnText, cout));
                
                *outpstr << *bs;
            }
            
            return 0;
        } else desc += " [-local]";
        
        if (s == "-seqids") {
            CSeqDB nr(/*dbpath,*/ "nr", 'p');
            
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
            CSeqDB nt("nt", 'n', 0, 0, false);
            
            Uint4 oid = 0;
            
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
            CSeqDB nt(/*dbpath,*/ "nt", 'n', 0, 0, false);
            
            Uint4 oid = 0;
            
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
                CSeqDB phil(/*dbpath,*/ "swissprot pataa", 'p');
            
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
                CSeqDB phil(/*dbpath,*/ "pataa swissprot", 'p');
            
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
            CSeqDB phil(/*dbpath,*/ "swissprot pdb", 'p');
            
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
            CSeqDB phil(/*dbpath,*/ "pdb", 'p');
            
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
            CSeqDB phil(/*dbpath,*/ "swissprot", 'p');
            
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
            CSeqDB phil(/*dbpath,*/ "swissprot", 'p');
            
            double besht = 100.0;
            double woist = 0.0;
            double totul = 0.0;
            
            for(int i = 0; i<10; i++) {
                CSeqDBIter skywalk = phil.Begin();
                
                double spt1 = dbl_time();
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
                
                double spt2  = dbl_time();
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
            CSeqDB phil(/*dbpath,*/ "swissprot", 'p');
             
            {
                Uint8 tlen = 0;
                Uint4 numb = 0;
                
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
        
        
        if (s == "-lib") {
            CSeqDB phil(/*dbpath,*/ "nt nt month est", 'n');
            phil.GetSeqLength(123);
            phil.GetSeqLengthApprox(123);
            phil.GetHdr(123);
            phil.GetBioseq(123);
            
            const char * buffer = 0;
            phil.GetSequence(123, & buffer);
            phil.RetSequence(& buffer);

            cout << "\nSeq type:    " << phil.GetSeqType();
            cout << "\nTitle:       " << phil.GetTitle();
            cout << "\nDate:        " << phil.GetDate();
            cout << "\nNumSeqs:     " << phil.GetNumSeqs();
            cout << "\nTotalLength: " << phil.GetTotalLength();
            cout << "\nMax Length:  " << phil.GetMaxLength() << endl;
            cout << endl;
            
            return 0;
        } else desc += " [-lib]";
        
        if (s == "-summary") {
            CSeqDB phil(/*dbpath,*/ "month", 'n');
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
            
            CSeqDB dbi1(/*dbpath,*/ dbname1, kSeqTypeProt, 0, 0, use_mm);
            CSeqDB dbi2(/*dbpath,*/ dbname2, kSeqTypeProt, 0, 0, use_mm);
            CSeqDB dbi3(/*dbpath,*/ dbname3, kSeqTypeProt, 0, 0, use_mm);
            
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
            approx = false;
            
            if (s == "-nt3a") {
                approx = true;
            }
            
            string dbname1 = "month";
            string dbname2 = "est";
            
            string dbname3(dbname1 + " " + dbname2);
            
            CSeqDB dbi1(/*dbpath,*/ dbname1, kSeqTypeNucl, 0, 0, use_mm);
            CSeqDB dbi2(/*dbpath,*/ dbname2, kSeqTypeNucl, 0, 0, use_mm);
            CSeqDB dbi3(/*dbpath,*/ dbname3, kSeqTypeNucl, 0, 0, use_mm);
            
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
            
            CSeqDB dbi(dbname, seqtype, 0, 0, use_mm);
            
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
                            report_at = Uint4(num_display - 1);
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

int main(int argc, char ** argv)
{
    int rc = 0;
    
    try {
        cout << "--one--" << endl;
        rc = test1(argc, argv);
        cout << "--two--" << endl;
    }
    catch(exception e) {
        cout << "--three--" << endl;
        cout << "Caught an exception: {" << e.what() << "}" << endl;
        rc = 1;
        cout << "--four--" << endl;
    }
    catch(...) {
        cout << "--five--" << endl;
    }
    
    return rc;
}


