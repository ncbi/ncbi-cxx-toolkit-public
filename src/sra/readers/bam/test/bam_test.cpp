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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Sample test application for BAM reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <objtools/readers/idmapper.hpp>
#include <objtools/simple/simple_om.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#include <align/align-access.h>

#include <connect/ncbi_conn_stream.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <corelib/ncbisys.hpp> // for NcbiSys_write
#include <stdio.h> // for perror

#include "bam_test_common.hpp"

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBAMTestApp::


class CBAMTestApp : public CBAMTestCommon
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


void CBAMTestApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CBAMTestApp test program");

    InitCommonArgs(*arg_desc);
    arg_desc->AddFlag("header", "Dump BAM header text");
    arg_desc->AddFlag("no_short", "Do not collect short ids");
    arg_desc->AddFlag("range_only", "Collect only the range on RefSeq");
    arg_desc->AddFlag("no_ref_size", "Assume zero ref_size");

    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of BAM entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100000");
    arg_desc->AddOptionalKey("check_id", "CheckId",
                             "Compare alignments with the specified sequence",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("min_quality", "MinQuality",
                            "Minimal quality of alignments to check",
                            CArgDescriptions::eInteger,
                            "1");
    arg_desc->AddFlag("verbose_weak_matches", "Print light mismatches");
    arg_desc->AddFlag("no_spot_id_detector",
                      "Do not use SpotId detector to resolve short id conflicts");
    arg_desc->AddFlag("no_conflict_detector",
                      "Do not run expensive conflict detection");
    arg_desc->AddFlag("make_seq_entry", "Generated Seq-entries");
    arg_desc->AddFlag("print_seq_entry", "Print generated Seq-entry");
    arg_desc->AddOptionalKey("include_tags", "IncludeTags",
                             "Include comma-separated list of alignmnet tags",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("ignore_errors", "Ignore errors in individual entries");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

inline bool Matches(char ac, char rc)
{
    static int bits[26] = {
        0x1, 0xE, 0x2, 0xD,
        0x0, 0x0, 0x4, 0xB,
        0x0, 0x0, 0xC, 0x0,
        0x3, 0xF, 0x0, 0x0,
        0x0, 0x5, 0x6, 0x8,
        0x0, 0x7, 0x9, 0x0,
        0xA, 0x0
    };
    
    //if ( ac == rc || ac == 'N' || rc == 'N' ) return true;
    if ( ac < 'A' || ac > 'Z' || rc < 'A' || rc > 'Z' ) return false;
    if ( bits[ac-'A'] & bits[rc-'A'] ) return true;
    return false;
}

string GetShortSeqData(const CBamAlignIterator& it)
{
    string ret;
    string src = it.GetShortSequence();
    //bool minus = it.IsSetStrand() && IsReverse(it.GetStrand());
    TSeqPos src_pos = it.GetCIGARPos();

    string CIGAR = it.GetCIGAR();
    const char* ptr = CIGAR.data();
    const char* end = ptr + CIGAR.size();
    char type;
    TSeqPos len;
    while ( ptr != end ) {
        type = *ptr;
        for ( len = 0; ++ptr != end; ) {
            char c = *ptr;
            if ( c >= '0' && c <= '9' ) {
                len = len*10+(c-'0');
            }
            else {
                break;
            }
        }
        if ( type == 'M' || type == '=' || type == 'X' ) {
            // match
            for ( TSeqPos i = 0; i < len; ++i ) {
                ret += src[src_pos++];
            }
        }
        else if ( type == 'I' || type == 'S' ) {
            // insert
            src_pos += len;
        }
        else if ( type == 'D' || type == 'N' ) {
            ret.append(len, 'N');
        }
    }
    return ret;
}


char Complement(char c)
{
    switch ( c ) {
    case 'N': return 'N';
    case 'A': return 'T';
    case 'T': return 'A';
    case 'C': return 'G';
    case 'G': return 'C';
    case 'B': return 'V';
    case 'V': return 'B';
    case 'D': return 'H';
    case 'H': return 'D';
    case 'K': return 'M';
    case 'M': return 'K';
    case 'R': return 'Y';
    case 'Y': return 'R';
    case 'S': return 'S';
    case 'W': return 'W';
    default: Abort();
    }
}


string Reverse(const string& s)
{
    size_t size = s.size();
    string r(size, ' ');
    for ( size_t i = 0; i < size; ++i ) {
        r[i] = Complement(s[size-1-i]);
    }
    return r;
}

class CSimpleSpotIdDetector : public CObject,
                              public CBamAlignIterator::ISpotIdDetector
{
public:
    CSimpleSpotIdDetector(void) : m_ResolveCount(0) {}

    size_t m_ResolveCount;

    void AddSpotId(string& short_id, const CBamAlignIterator* iter) {
        string seq = iter->GetShortSequence();
        if ( iter->IsSetStrand() && IsReverse(iter->GetStrand()) ) {
            seq = Reverse(seq);
        }
        SShortSeqInfo& info = m_ShortSeqs[short_id];
        if ( info.spot1.empty() ) {
            info.spot1 = seq;
            short_id += ".1";
            return;
        }
        if ( info.spot1 == seq ) {
            short_id += ".1";
            return;
        }
        if ( info.spot2.empty() ) {
            ++m_ResolveCount;
            info.spot2 = seq;
            short_id += ".2";
            return;
        }
        if ( info.spot2 == seq ) {
            short_id += ".2";
            return;
        }
        short_id += ".?";
    }

private:
    struct SShortSeqInfo {
        string spot1, spot2;
    };
    map<string, SShortSeqInfo> m_ShortSeqs;
};

#if 0 // LowLevelTest

#ifdef _MSC_VER
# include <io.h>
CRITICAL_SECTION sdk_mutex;
# define SDKLock() EnterCriticalSection(&sdk_mutex)
# define SDKUnlock() LeaveCriticalSection(&sdk_mutex)
#else
# include <unistd.h>
# define SDKLock() do{}while(0)
# define SDKUnlock() do{}while(0)
#endif

#define CALL(call) CheckRc((call), #call, __FILE__, __LINE__)

void CheckRc(rc_t rc, const char* code, const char* file, int line)
{
    if ( rc ) {
        char buffer1[4096];
        size_t error_len;
        RCExplain(rc, buffer1, sizeof(buffer1), &error_len);
        char buffer2[8192];
        unsigned len = sprintf(buffer2, "%s:%d: %s failed: %#x: %s\n",
                             file, line, code, rc, buffer1);
        const char* ptr = buffer2;
        while ( len ) {
            int written = NcbiSys_write(2, ptr, len);
            if ( written == -1 ) {
                perror("stderr write failed");
                exit(1);
            }
            len -= written;
            ptr += written;
        }
        exit(1);
    }
}

DEFINE_STATIC_MUTEX(s_BamMutex);

struct SThreadInfo
{
#ifdef _MSC_VER
    HANDLE thread_id;
#else
    pthread_t thread_id;
#endif

    int t;
    const AlignAccessDB* bam;
    const char* ref;
    int count;

    void init(int t, const AlignAccessDB* bam, const char* ref)
        {
            this->t = t;
            this->bam = bam;
            this->ref = ref;
            count = 0;
        }

#define GUARD() CMutexGuard guard(s_BamMutex)

    void run()
    {
        GUARD();
        AlignAccessAlignmentEnumerator* iter = 0;
        rc_t rc;
        {
            GUARD();
            rc = AlignAccessDBWindowedAlignments(bam, &iter, ref, 0, 0);
        }
        while ( rc == 0 ) {
            ++count;
            GUARD();
            rc = AlignAccessAlignmentEnumeratorNext(iter);
        }
        if ( !AlignAccessAlignmentEnumeratorIsEOF(rc) ) {
            CALL(rc);
        }
        {
            GUARD();
            AlignAccessAlignmentEnumeratorRelease(iter);
        }
    }

#undef GUARD

};

#ifdef _MSC_VER
DWORD
#else
void*
#endif
read_thread_func(void* arg)
{
    ((SThreadInfo*)arg)->run();
    return 0;
}

int LowLevelTest()
{
    cout << "Running LowLevelTest()." << endl;
    const AlignAccessMgr* mgr = 0;
    CALL(AlignAccessMgrMake(&mgr));
    VFSManager* vfs_mgr;
    CALL(VFSManagerMake(&vfs_mgr));
    VPath* bam_path = 0;
#ifdef _MSC_VER
# define BAM_FILE "//traces04/1kg_pilot_data/ftp/pilot_data/data/NA10851/alignment/NA10851.SLX.maq.SRP000031.2009_08.bam"
#else
//# define BAM_FILE "/netmnt/traces04/1kg_pilot_data/ftp/pilot_data/data/NA10851/alignment/NA10851.SLX.maq.SRP000031.2009_08.bam"
# define BAM_FILE "/am/ncbiapdata/test_data//traces04//1000genomes3/ftp/data/NA10851/alignment/NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam"
#endif
    cout << "Testing BAM file: "<<BAM_FILE<<endl;
    CALL(VFSManagerMakeSysPath(vfs_mgr, &bam_path, BAM_FILE));
    VPath* bai_path = 0;
    CALL(VFSManagerMakeSysPath(vfs_mgr, &bai_path, BAM_FILE ".bai"));
    
    const AlignAccessDB* bam = 0;
    CALL(AlignAccessMgrMakeIndexBAMDB(mgr, &bam, bam_path, bai_path));

#ifdef _MSC_VER
    InitializeCriticalSection(&sdk_mutex);
#endif

    if ( 1 ) {
        AlignAccessAlignmentEnumerator* iter = 0;
#if 1
        CALL(AlignAccessDBEnumerateAlignments(bam, &iter));
#endif
#if 0
        CALL(AlignAccessDBWindowedAlignments(bam, &iter, "1", 0, 0));
        for ( int i = 0; i < 1000; ++i ) {
            CALL(AlignAccessAlignmentEnumeratorNext(iter));
        }
#endif
#if 0
        CALL(AlignAccessDBWindowedAlignments(bam, &iter, "1", 24725086, 0));
        for ( int i = 0; i < 10; ++i ) {
            uint64_t refpos = 0, readpos = 0;
            CALL(AlignAccessAlignmentEnumeratorGetRefSeqPos(iter, &refpos));
            char cigar[999];
            size_t cigarlen = 0;
            CALL(AlignAccessAlignmentEnumeratorGetCIGAR(iter, &readpos, cigar, sizeof(cigar), &cigarlen));
            cout << "ref @ "<<refpos<<" read @ "<<readpos<<" CIGAR: "<<cigar<<endl;
            CALL(AlignAccessAlignmentEnumeratorNext(iter));
        }
#endif
        CALL(AlignAccessAlignmentEnumeratorRelease(iter));
    }
    else {
        const size_t kNumCursors = 8;
        const size_t kNumThreads = 3;
        SThreadInfo tinfo[kNumCursors];
        const char* ids[kNumCursors] = {
            "NT_113960",
            "NT_113945",
            "NT_113880",
            "NT_113960",
            "NT_113960",
            "NT_113960",
            "NT_113945",
            "NT_113880"
        };
        for ( size_t i = 0; i < kNumThreads; ++i ) {
            tinfo[i].init(i, bam, ids[i]);
        }
        for ( size_t i = 0; i < kNumThreads; ++i ) {
            cout << "Starting thread " << i << " for " << ids[i] << endl;
#ifdef _MSC_VER
            tinfo[i].thread_id = CreateThread(NULL, 0, read_thread_func,
                                              &tinfo[i], 0, NULL);
#else
            pthread_create(&tinfo[i].thread_id, 0, read_thread_func, &tinfo[i]);
#endif
        }
        for ( size_t i = 0; i < kNumThreads; ++i ) {
            cout << "Waiting for thread " << i << endl;
            void* ret = 0;
#ifdef _MSC_VER
            WaitForSingleObject(tinfo[i].thread_id, INFINITE);
            CloseHandle(tinfo[i].thread_id);
#else
            pthread_join(tinfo[i].thread_id, &ret);
#endif
            cout << "Align count: " << tinfo[i].count << endl;
        }
    }

    CALL(AlignAccessDBRelease(bam));
        
    CALL(VPathRelease(bam_path));
    CALL(VPathRelease(bai_path));
    CALL(AlignAccessMgrRelease(mgr));
    CALL(VFSManagerRelease(vfs_mgr));
    cout << "Success." << endl;
    return 0;
}


int LowLevelTestFile()
{
    const char* url = "https://ftp-trace.ncbi.nlm.nih.gov/giab/ftp/data/AshkenazimTrio/HG002_NA24385_son/NIST_HiSeq_HG002_Homogeneity-10953946/NHGRI_Illumina300X_AJtrio_novoalign_bams/HG002.GRCh38.300x.bam.bai";
    //const char* url = "http://ftp.ensembl.org/pub/current_data_files/homo_sapiens/GRCh38/rnaseq/GRCh38.illumina.ovary.1.bam.bai";
    cout << "Running LowLevelTestFile()." << endl
         << "URL: " << url << endl;
    VFSManager* mgr;
    CALL(VFSManagerMake(&mgr));
    VPath* path;
    CALL(VFSManagerMakePath(mgr, &path, url));
    const KFile* file;
    CALL(VFSManagerOpenFileRead(mgr, &file, path));
    uint64_t size;
    CALL(KFileSize(file, &size));
    char* buffer = new char[size];
    CStopWatch sw(CStopWatch::eStart);
    CALL(KFileReadExactly(file, 0, buffer, size-(0<<20)));
    cout << "Loaded in "<<sw.Elapsed()<<" seconds"<<endl;
    delete[] buffer;
    CALL(KFileRelease(file));
    CALL(VPathRelease(path));
    CALL(VFSManagerRelease(mgr));
    cout << "Success." << endl;
    return 0;
}
#endif

int CBAMTestApp::Run(void)
{
#ifdef CALL
    return LowLevelTest();
    //return LowLevelTestFile();
#endif

    // Get arguments
    const CArgs& args = GetArgs();

    if ( !ParseCommonArgs(args) ) {
        return 1;
    }

    bool verbose_weak_matches = args["verbose_weak_matches"];
    CSeqVector sv;
    if ( args["check_id"] ) {
        sv = CSimpleOM::GetSeqVector(args["check_id"].AsString());
        if ( sv.empty() ) {
            ERR_POST(Fatal<<"Cannot get check sequence: "<<args["check_id"].AsString());
        }
        sv.SetCoding(CSeq_data::e_Iupacna);
    }

    bool no_spot_id_detector = args["no_spot_id_detector"];
    bool no_conflict_detector = args["no_conflict_detector"];
    
    for ( int cycle = 0; cycle < 1; ++cycle ) {
        if ( cycle ) path = "/panfs/traces03.be-md.ncbi.nlm.nih.gov/1kg_pilot_data/data/NA10851/alignment/NA10851.SLX.maq.SRP000031.2009_08.bam";
        out << "File: " << path << NcbiEndl;
        CBamMgr mgr;
        CBamDb bam_db;
        
        if ( index_path.empty() ) {
            bam_db = CBamDb(mgr, path);
        }
        else {
            bam_db = CBamDb(mgr, path, index_path);
        }

        if ( id_mapper.get() ) {
            bam_db.SetIdMapper(id_mapper.get(), eNoOwnership);
        }

        if ( args["header"] ) {
            out << bam_db.GetHeaderText() << endl;
        }
        if ( refseq_table ) {
            out << "RefSeq table:\n";
            for ( CBamRefSeqIterator it(bam_db); it; ++it ) {
                out << "RefSeq: " << it.GetRefSeqId() << " " << it.GetLength()
                    << endl;
            }
        }
        int limit_count = args["limit_count"].AsInteger();
        bool ignore_errors = args["ignore_errors"];
        bool make_seq_entry = args["make_seq_entry"];
        if ( args["include_tags"] ) {
            vector<string> tags;
            NStr::Split(args["include_tags"].AsString(), ",", tags);
            for ( auto& tag : tags ) {
                bam_db.IncludeAlignTag(tag);
            }
        }
        bool print_seq_entry = args["print_seq_entry"];
        int min_quality = args["min_quality"].AsInteger();
        vector<CRef<CSeq_entry> > entries;
        typedef map<string, int> TSeqDupMap;
        typedef map<string, TSeqDupMap> TConflicts;
        TConflicts conflicts;
        
        string seqdata_str;
        string conflict_asnb;
        CConn_MemoryStream conflict_memstr;
        CObjectOStreamAsnBinary conflict_objstr(conflict_memstr);

        vector<string> refseqs;
        if ( args["refseq"] ) {
            NStr::Split(args["refseq"].AsString(), ",", refseqs);
        }
        else {
            refseqs.push_back(string());
        }

        if ( queries.empty() ) {
            queries.push_back(SQuery());
        }
        for ( auto& q : queries ) {
            int count = 0;
            CBamAlignIterator it;
            bool collect_short = false;
            bool range_only = args["range_only"];
            bool no_ref_size = args["no_ref_size"];
            if ( !q.refseq_id.empty() ) {
                collect_short = !args["no_short"];
                it = CBamAlignIterator(bam_db, q.refseq_id,
                                       q.refseq_range.GetFrom(),
                                       q.refseq_range.GetLength(),
                                       CBamAlignIterator::ESearchMode(by_start));
            }
            else {
                it = CBamAlignIterator(bam_db);
            }
            CRef<CSimpleSpotIdDetector> detector(new CSimpleSpotIdDetector);
            if ( !no_spot_id_detector ) {
                it.SetSpotIdDetector(detector);
            }
            string p_ref_seq_id, p_short_seq_id, p_short_seq_acc, p_data, p_cigar;
            TSeqPos p_ref_pos = 0, p_ref_size = 0;
            TSeqPos p_short_pos = 0, p_short_size = 0;
            Uint1 p_qual = 0;
            int p_strand = -1;
            typedef map<string, vector<COpenRange<TSeqPos> > > TRefIds;
            typedef vector<pair<string, TSeqPos> > TShortIds;
            TRefIds ref_ids;
            TSeqPos ref_min = 0, ref_max = 0;
            if ( range_only ) {
                ref_min = it.GetRefSeqPos();
            }
            TShortIds short_ids;

            size_t skipped_by_quality_count = 0;
            size_t perfect_match_count = 0, weak_match_count = 0;
            size_t mismatch_unmapped_count = 0, mismatch_mapped_count = 0;
            for ( ; it; ++it ) {
                if ( limit_count > 0 && count == limit_count ) break;
                if ( min_quality && it.GetMapQuality() < min_quality ) {
                    skipped_by_quality_count += 1;
                    continue;
                }
                try {
                    ++count;
                    TSeqPos ref_pos;
                    try {
                        ref_pos = it.GetRefSeqPos();
                    }
                    catch ( CBamException& exc ) {
                        if ( exc.GetErrCode() != exc.eNoData ) {
                            throw;
                        }
                        ref_pos = kInvalidSeqPos;
                    }
                    if ( range_only ) {
                        TSeqPos ref_end = ref_pos;
                        if ( !no_ref_size ) {
                            ref_end += it.GetCIGARRefSize();
                        }
                        if ( ref_end > ref_max ) {
                            ref_max = ref_end;
                        }
                        if ( count == limit_count ) {
                            break;
                        }
                        continue;
                    }
                    TSeqPos ref_size = it.GetCIGARRefSize();
                    int qual = it.GetMapQuality();
                    string ref_seq_id;
                    if ( ref_pos == kInvalidSeqPos ) {
                        _ASSERT(ref_size == 0 || ref_size == it.GetShortSequence().size());
                        _ASSERT(qual == 0);
                        if ( verbose ) {
                            out << count << ": Unaligned\n";
                        }
                    }
                    else {
                        ref_seq_id = it.GetRefSeqId();
                        if ( verbose ) {
                            out << count << ": Ref: " << ref_seq_id
                                << " at [" << ref_pos
                                << " - " << (ref_pos+ref_size-1)
                                << "] = " << ref_size
                                << " Qual = " << qual
                                << '\n';
                        }
                        _ASSERT(!ref_seq_id.empty());
                    }
                    string short_seq_id = it.GetShortSeqId();
                    string short_seq_acc = it.GetShortSeqAcc();
                    TSeqPos short_pos = it.GetCIGARPos();
                    TSeqPos short_size = it.GetCIGARShortSize();
                    int strand = -1;
                    const char* strand_str = "";
                    bool minus = false;
                    if ( it.IsSetStrand() ) {
                        strand = it.GetStrand();
                        if ( strand == eNa_strand_plus ) {
                            strand_str = " plus";
                        }
                        else if ( strand == eNa_strand_minus ) {
                            strand_str = " minus";
                            //minus = true;
                        }
                        else {
                            strand_str = " unknown";
                        }
                    }
                    Uint2 flags = it.GetFlags();
                    bool is_paired = it.IsPaired();
                    bool is_first = it.IsFirstInPair();
                    bool is_second = it.IsSecondInPair();
                    if ( verbose ) {
                        out << "Seq: " << short_seq_id << " " << short_seq_acc
                            << " at [" << short_pos
                            << " - " << (short_pos+short_size-1)
                            << "] = " << short_size
                            << " 0x" << hex << flags << dec
                            << strand_str << " "
                            << (is_paired? "P": "")
                            << (is_first? "1": "")
                            << (is_second? "2": "")
                            << '\n';
                    }
                    if ( !(ref_seq_id != p_ref_seq_id || 
                           ref_pos != p_ref_pos ||
                           ref_size != p_ref_size ||
                           short_seq_id != p_short_seq_id ||
                           short_pos != p_short_pos ||
                           short_size != p_short_size ||
                           strand != p_strand ||
                           qual != p_qual ) ) {
                        if ( verbose ) {
                            out << "Error: match info is the same"
                                << endl;
                        }
                    }
                    if ( verbose ) {
                        string data = it.GetShortSequence();
                        if ( verbose ) {
                            out << "Sequence[" << data.size() << "]"
                                << ": " << data
                                << '\n';
                        }
                        string cigar = it.GetCIGAR();
                        if ( verbose ) {
                            out << "CIGAR: " << cigar
                                << '\n';
                        }
                        if ( cigar.empty() ) {
                            out << "Error: Empty CIGAR"
                                << endl;
                        }
                        else if ( !cigar[cigar.size()-1] ) {
                            out << "Error: Bad CIGAR: "
                                << NStr::PrintableString(cigar)
                                << endl;
                        }
                        if ( ref_pos == p_ref_pos && strand == p_strand &&
                             cigar == p_cigar ) {
                            if ( data != p_data ) {
                                out << "Error: match data at the same place "
                                    << "is not the same: "
                                    << data << " vs " << p_data
                                    << endl;
                            }
                        }
                        else {
                            if ( !(data != p_data ||
                                   cigar != p_cigar) ) {
                                out << "Error: match data is the same"
                                    << endl;
                            }
                        }
                        p_data = data;
                        p_cigar = cigar;
                    }
                    if ( verbose && it.UsesRawIndex() ) {
                        // dump tags
                        out << "Tags:";
                        for ( auto it2 = it.GetAuxIterator(); it2; ++it2 ) {
                            out << ' ' << it2->GetTag() << ':';
                            if ( it2->IsArray() ) {
                                out << "B:" << it2->GetDataType();
                                for ( size_t i = 0; i < it2->size(); ++it ) {
                                    out << ',';
                                    if ( it2->IsFloat() ) {
                                        out << it2->GetFloat(i);
                                    }
                                    else {
                                        out << it2->GetInt(i);
                                    }
                                }
                            }
                            else {
                                if ( it2->IsChar() ) {
                                    out << "A:" << it2->GetChar();
                                }
                                else if ( it2->IsString() ) {
                                    out << "Z:" << it2->GetString();
                                }
                                else if ( it2->IsFloat() ) {
                                    out << "f:" << it2->GetFloat();
                                }
                                else {
                                    out << "i:" << it2->GetInt();
                                }
                            }
                        }
                        out << endl;
                    }
                    if ( print_seq_entry || make_seq_entry ) {
                        CRef<CSeq_entry> entry = it.GetMatchEntry();
                        if ( make_seq_entry ) {
                            entries.push_back(entry);
                        }
                        if ( print_seq_entry ) {
                            out << MSerial_AsnText << *entry << '\n';
                        }
                    }
                    if ( !sv.empty() && qual < min_quality ) {
                        skipped_by_quality_count += 1;
                    }
                    if ( !sv.empty() && qual >= min_quality ) {
                        const string& s = GetShortSeqData(it);
                        string av = minus? Reverse(s): s;
                        size_t match = 0, error = 0;
                        for ( TSeqPos i = 0; i < s.size(); ++i ) {
                            char ac = av[i];
                            char sc = sv[ref_pos+i];
                            if ( Matches(ac, sc) ) {
                                ++match;
                            }
                            else if ( ac != 'N' ) {
                                ++error;
                            }
                        }
                        bool print_error = false;
                        if ( error ) {
                            if ( error > match ) {
                                if ( qual == 0 ) {
                                    mismatch_unmapped_count += 1;
                                    if ( verbose ) {
                                        print_error = true;
                                        out << "Strand:" << strand_str << " ";
                                        out << "Mismatch: bad" << endl;
                                    }
                                }
                                else {
                                    mismatch_mapped_count += 1;
                                    print_error = true;
                                    out << "Strand:" << strand_str << " ";
                                    out << "Mismatch: bad, qual: " << qual
                                        << endl;
                                }
                            }
                            else {
                                weak_match_count += 1;
                                if ( verbose_weak_matches ) {
                                    print_error = true;
                                    out << "Strand:" << strand_str << " ";
                                    out << "Mismatch: weak" << endl;
                                }
                            }
                        }
                        else {
                            perfect_match_count += 1;
                            if ( verbose ) {
                                print_error = true;
                                out << "Strand:" << strand_str << " ";
                                out << "Matches perfectly" << endl;
                            }
                        }
                        if ( print_error ) {
                            if ( !verbose ) {
                                out << count << ": Ref: " << ref_seq_id
                                    << " at [" << ref_pos
                                    << " - " << (ref_pos+ref_size-1)
                                    << "] = " << ref_size
                                    << " Qual = " << qual
                                    << '\n';
                                out << "Seq: " << short_seq_id << " " << short_seq_acc
                                    << " at [" << short_pos
                                    << " - " << (short_pos+short_size-1)
                                    << "] = " << short_size
                                    << " 0x" << hex << flags << dec
                                    << strand_str << " "
                                    << (is_paired? "P": "")
                                    << (is_first? "1": "")
                                    << (is_second? "2": "")
                                    << endl;
                            }
                            out << "Matched: "<<match<<" error: "<<error<<endl;
                            sv.GetSeqData(ref_pos, ref_pos+ref_size,
                                          seqdata_str);
                            out << "Short data: " << s << endl;
                            if ( minus )
                                out << "Align data: " << av << endl;
                            out << "Check data: " << seqdata_str << endl;
                        }
                    }
                    out << flush;
                    if ( !no_conflict_detector && qual >= min_quality ) {
                        if ( CRef<CBioseq> seq = it.GetShortBioseq() ) {
                            conflict_objstr << *seq;
                            conflict_objstr.FlushBuffer();
                            conflict_memstr.ToString(&conflict_asnb);
                            conflicts[it.GetShortSeq_id()->AsFastaString()]
                                [conflict_asnb] += 1;
                        }
                    }
                    p_ref_seq_id = ref_seq_id;
                    p_ref_pos = ref_pos;
                    p_ref_size = ref_size;
                    p_short_seq_id = short_seq_id;
                    p_short_pos = short_pos;
                    p_short_size = short_size;
                    p_qual = qual;
                    p_strand = strand;
                    ref_ids[ref_seq_id]
                        .push_back(COpenRange<TSeqPos>(ref_pos, ref_pos+ref_size));
                    if ( collect_short ) {
                        short_ids.push_back(make_pair(short_seq_id, ref_pos));
                    }
                }
                catch ( CException& exc ) {
                    ERR_POST("Error: "<<exc.what());
                    if ( !ignore_errors ) {
                        throw;
                    }
                }
                if ( limit_count > 0 && count == limit_count ) break;
            }
            out << "Loaded: " << count << " alignments." << NcbiEndl;
            if ( range_only ) {
                out << "Range: " << ref_min << "-" << ref_max-1 << NcbiEndl;
            }
            else {
                NON_CONST_ITERATE ( TRefIds, it, ref_ids ) {
                    if ( it->first.empty() ) {
                        out << "Unmapped alignments: " << it->second.size() << NcbiEndl;
                    }
                    else {
                        out << "Ref " << it->first << ": " << it->second.size() << NcbiFlush;
                        sort(it->second.begin(), it->second.end());
                        out << "    " << it->second[0].GetFrom() << "-"
                            << it->second.back().GetToOpen()-1 << NcbiEndl;
                    }
                }
                if ( collect_short && !short_ids.empty() ) {
                    sort(short_ids.begin(), short_ids.end());
                    out << "Short: " << short_ids[0].first << " - "
                        << short_ids.back().first << NcbiEndl;
                }
                out << "Sorted." << NcbiEndl;
            }
            if ( skipped_by_quality_count ) {
                out << "Skipped low quality: " << skipped_by_quality_count
                    << endl;
            }
            if ( !sv.empty() ) {
                if ( perfect_match_count ) {
                    out << "Perfect matches: " << perfect_match_count << endl;
                }
                if ( weak_match_count ) {
                    out << "Weak matches: " << weak_match_count << endl;
                }
                if ( mismatch_unmapped_count ) {
                    out << "Unmapped mismatches: " << mismatch_unmapped_count
                        << endl;
                }
                if ( mismatch_mapped_count ) {
                    out << "Mapped mismatches: " << mismatch_mapped_count
                        << endl;
                }
            }
            //SleepMilliSec(3000);
            int conflict_count = 0, dup_count = 0;
            ITERATE ( TConflicts, it, conflicts ) {
                if ( it->second.size() > 1 ) {
                    out << "Conflict id: " << it->first << NcbiEndl;
                    ++conflict_count;
                    if ( verbose ) {
                        ITERATE ( TSeqDupMap, it2, it->second ) {
                            CObjectIStreamAsnBinary str(it2->first.data(),
                                                        it2->first.size());
                            CRef<CBioseq> seq(new CBioseq);
                            str >> *seq;
                            NcbiCout << MSerial_AsnText << *seq << NcbiEndl;
                        }
                    }
                }
                ITERATE ( TSeqDupMap, it2, it->second ) {
                    dup_count += it2->second - 1;
                }
            }
            if ( conflict_count ) {
                out << "Conflicts: " << conflict_count << NcbiEndl;
            }
            if ( dup_count ) {
                out << "Short seq dups: " << dup_count << NcbiEndl;
            }
            if ( detector->m_ResolveCount ) {
                out << "SpotId detector resolve count: " <<
                    detector->m_ResolveCount << NcbiEndl;
            }
        }
    }
    out << "Exiting" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBAMTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBAMTestApp().AppMain(argc, argv);
}
