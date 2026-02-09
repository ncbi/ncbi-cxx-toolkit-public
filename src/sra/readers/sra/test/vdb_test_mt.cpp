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
 *   Sample multithreaded test application for cSRA reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/request_ctx.hpp>
#include <sra/readers/sra/csraread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <insdc/sra.h>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <thread>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CCSRATestMTApp::


class CCSRATestMTApp : public CNcbiApplication
{
public:
    CCSRATestMTApp();

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

private:
    bool m_UseVDB = false;
    string m_Acc;

    bool m_Verbose = false;
    bool m_Fasta = false;
    string m_CachePath;
    string m_Path;
    string m_Query_id;
    CSeq_id_Handle m_Query_idh;
    CRange<TSeqPos> m_Query_range;
    CCSraAlignIterator::ESearchMode m_Mode = CCSraAlignIterator::eSearchByOverlap;
    int m_Min_quality;
    bool m_Make_ref_seq;
    bool m_Make_short_seq;
    bool m_Make_seq_annot;
    bool m_Make_seq_entry;
    bool m_Make_seq_iter;
    bool m_Make_quality_graph;
    bool m_Make_stat_graph;
    bool m_Print_objects;
    bool m_Count_unique;
    bool m_Refseq_table = false;
    bool m_No_acc = false;
    bool m_Spot_groups = false;
    bool m_Over_start = false;
    bool m_Over_start_verify = false;
    bool m_Primary = false;
    bool m_Secondary = false;
    bool m_Count_ids = false;
    bool m_Make_graphs = false;
    int m_Limit_count = 0;
    bool m_Scan_reads = false;
    Uint8 m_Start_id = 0;
    int m_Repeats = 0;

    void RunTestThread();
    void RunTest();
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

CCSRATestMTApp::CCSRATestMTApp()
{
    SetVersion( CVersionInfo(1,23,45) );
}

void CCSRATestMTApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "vdb_test_mt");

    arg_desc->AddOptionalKey("vol_path", "VolPath",
                             "Search path for volumes",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("file", "File",
                             "cSRA file name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("acc", "SRZAccession",
                             "SRZ accession to the cSRA file",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("no_acc", "SRZ accession is supposed to be absent");

    arg_desc->AddOptionalKey("mapfile", "MapFile",
                             "IdMapper config filename",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("genome", "Genome",
                            "UCSC build number",
                            CArgDescriptions::eString, "");

    arg_desc->AddFlag("refseq_table", "Dump RefSeq table");
    arg_desc->AddFlag("spot_groups", "List spot groups");
    arg_desc->AddFlag("make_graphs", "Generate coverage graphs");

    arg_desc->AddOptionalKey("q", "Query",
                             "Query coordinates in form chr1:100-10000",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("refseq", "RefSeqId",
                             "RefSeq id",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refpos", "RefSeqPos",
                            "RefSeq position",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddOptionalKey("refposs", "RefSeqPoss",
                            "RefSeq positions",
                            CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refwindow", "RefSeqWindow",
                            "RefSeq window",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddOptionalKey("refend", "RefSeqEnd",
                            "RefSeq end position",
                            CArgDescriptions::eInteger);
    arg_desc->AddFlag("by-start", "Search range by start position");
    arg_desc->AddFlag("primary", "Only primary alignments");
    arg_desc->AddFlag("secondary", "Only secondary alignments");
    arg_desc->AddFlag("count-unique", "Count number of unique reads");
    
    arg_desc->AddFlag("over-start", "Dump overlap start positions");
    arg_desc->AddFlag("over-start-verify", "Verify overlap start positions");
    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of cSRA entries to read (0 - unlimited)",
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
    arg_desc->AddFlag("verbose", "Print info about found alignments");
    arg_desc->AddFlag("ref_seq", "Make reference Bioseq");
    arg_desc->AddFlag("short_seq", "Make Bioseq with short read sequence");
    arg_desc->AddFlag("seq_annot", "Make Seq-annot with match alignment");
    arg_desc->AddFlag("seq_entry",
                      "Make Seq-entry with both read and alignment");
    arg_desc->AddFlag("quality_graph", "Make read quality Seq-graph");
    arg_desc->AddFlag("stat_graph", "Make base statistics Seq-graph");
    arg_desc->AddFlag("seq_iter", "Make Bioseq from short read iterator");
    arg_desc->AddFlag("print_objects", "Print generated objects");
    arg_desc->AddFlag("ignore_errors", "Ignore errors in individual entries");

    arg_desc->AddFlag("scan_reads", "Scan reads");
    arg_desc->AddOptionalKey("start_id", "StartId",
                             "Start short read id",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("fasta", "Print reads in fasta format");
    arg_desc->AddFlag("count_ids", "Count alignments ids");

    arg_desc->AddFlag("get-cache", "Get cache root");
    arg_desc->AddOptionalKey("set-cache", "SetCache",
                             "Set cache root", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddOptionalKey("session-id", "SessionID",
                             "request session ID",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("client-ip", "ClientIP",
                             "request client IP",
                             CArgDescriptions::eString);
    
    arg_desc->AddFlag("use_vdb", "Use CVDB instead of CCSraDb");
    arg_desc->AddDefaultKey("repeats", "Repeats",
                            "Number of repeats per thread",
                            CArgDescriptions::eInteger,
                            "1");
    arg_desc->AddDefaultKey("threads", "Threads",
                            "Number of threads",
                            CArgDescriptions::eInteger,
                            "30");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

template<class Call>
static typename std::invoke_result<Call>::type
CallWithRetry(Call&& call,
              const char* name,
              unsigned retry_count = 0)
{
    static const unsigned kDefaultRetryCount = 4;
    if ( retry_count == 0 ) {
        retry_count = kDefaultRetryCount;
    }
    for ( unsigned t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<"VDBTest: "<<name<<"() try "<<t<<" exception: "<<exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<"VDBTest: "<<name<<"() try "<<t<<" exception: "<<exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<"VDBTest: "<<name<<"() try "<<t<<" exception");
        }
        if ( t >= 2 ) {
            //double wait_sec = m_WaitTime.GetTime(t-2);
            double wait_sec = 1;
            LOG_POST(Warning<<"VDBTest: waiting "<<wait_sec<<"s before retry");
            SleepMilliSec(Uint4(wait_sec*1000));
        }
    }
    return call();
}
#define RETRY(expr) CallWithRetry([&]()->auto{return (expr);}, #expr)


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

#include <klib/rc.h>
#include <klib/writer.h>
#include <vdb/manager.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>
#include <kdb/kdb-priv.h>
#include <vfs/manager.h>
#include <vfs/path.h>
#ifdef _MSC_VER
# include <io.h>
#else
# include <unistd.h>
#endif

// low level SRA SDK test
void CheckRc(rc_t rc, const char* code, const char* file, int line)
{
    if ( rc ) {
        char buffer1[4096];
        size_t error_len;
        RCExplain(rc, buffer1, sizeof(buffer1), &error_len);
        char buffer2[8192];
        int len = snprintf(buffer2, sizeof(buffer2), "%s:%d: %s failed: %#x: %s\n",
                           file, line, code, rc, buffer1);
        len = min(len, int(sizeof(buffer2)));
        int exit_code = 1;
        if ( NcbiSys_write(2, buffer2, len) != len ) {
            ++exit_code;
        }
        exit(exit_code);
    }
}


struct SBaseStat
{
    enum {
        kStat_A = 0,
        kStat_C = 1,
        kStat_G = 2,
        kStat_T = 3,
        kStat_Insert = 4,
        kStat_Match = 5,
        kNumStat = 6
    };
    SBaseStat(void) {
        for ( int i = 0; i < kNumStat; ++i ) {
            cnts[i] = 0;
        }
    }

    unsigned total() const {
        unsigned ret = 0;
        for ( int i = 0; i < kNumStat; ++i ) {
            ret += cnts[i];
        }
        return ret;
    }

    void add_base(char b) {
        switch ( b ) {
        case 'A': cnts[kStat_A] += 1; break;
        case 'C': cnts[kStat_C] += 1; break;
        case 'G': cnts[kStat_G] += 1; break;
        case 'T': cnts[kStat_T] += 1; break;
        case '=': cnts[kStat_Match] += 1; break;
        }
    }
    void add_match() {
        cnts[kStat_Match] += 1;
    }
    void add_gap() {
        cnts[kStat_Insert] += 1;
    }

    unsigned cnts[kNumStat];
};

int CCSRATestMTApp::Run(void)
{
    const CArgs& args = GetArgs();

    if ( args["session-id"] ) {
        CDiagContext::GetRequestContext().SetSessionID(args["session-id"].AsString());
    }
    if ( args["client-ip"] ) {
        CDiagContext::GetRequestContext().SetClientIP(args["client-ip"].AsString());
    }

    m_UseVDB = args["use_vdb"];
    m_Verbose = args["verbose"];
    m_Fasta = args["fasta"];
    if (args["set-cache"] ) m_CachePath = args["set-cache"].AsString();

    m_Acc = "SRR413273";
    if ( args["file"] ) {
        m_Path = args["file"].AsString();
    }
    else if ( args["acc"] ) {
        m_Acc = args["acc"].AsString();
    }

    m_Query_range = CRange<TSeqPos>::GetWhole();
    if ( args["refseq"] ) {
        m_Query_id = args["refseq"].AsString();
        m_Query_range.SetFrom(args["refpos"].AsInteger());
        if ( args["refwindow"] ) {
            TSeqPos window = args["refwindow"].AsInteger();
            if ( window != 0 ) {
                m_Query_range.SetLength(window);
            }
            else {
                m_Query_range.SetTo(kInvalidSeqPos);
            }
        }
        if ( args["refend"] ) {
            m_Query_range.SetTo(args["refend"].AsInteger());
        }
    }
    if ( args["q"] ) {
        string q = args["q"].AsString();
        SIZE_TYPE colon_pos = q.find(':');
        if ( colon_pos == 0 ) {
            ERR_POST(Fatal << "Invalid query format: " << q);
        }
        if ( colon_pos == NPOS ) {
            m_Query_id = q;
            m_Query_range = m_Query_range.GetWhole();
        }
        else {
            m_Query_id = q.substr(0, colon_pos);
            SIZE_TYPE dash_pos = q.find('-', colon_pos+1);
            if ( dash_pos == NPOS ) {
                ERR_POST(Fatal << "Invalid query format: " << q);
            }
            string q_from = q.substr(colon_pos+1, dash_pos-colon_pos-1);
            string q_to = q.substr(dash_pos+1);
            TSeqPos from, to;
            if ( q_from.empty() ) {
                from = 0;
            }
            else {
                from = NStr::StringToNumeric<TSeqPos>(q_from);
            }
            if ( q_to.empty() ) {
                to = kInvalidSeqPos;
            }
            else {
                to = NStr::StringToNumeric<TSeqPos>(q_to);
            }
            m_Query_range.SetFrom(from).SetTo(to);
        }
    }
    
    if ( args["by-start"] ) {
        m_Mode = CCSraAlignIterator::eSearchByStart;
    }

    m_Min_quality = args["min_quality"].AsInteger();
    m_Make_ref_seq = args["ref_seq"];
    m_Make_short_seq = args["short_seq"];
    m_Make_seq_annot = args["seq_annot"];
    m_Make_seq_entry = args["seq_entry"];
    m_Make_seq_iter = args["seq_iter"];
    m_Make_quality_graph = args["quality_graph"];
    m_Make_stat_graph = args["stat_graph"];
    m_Print_objects = args["print_objects"];
    m_Count_unique = args["count-unique"];
    m_Refseq_table = args["refseq_table"];
    m_No_acc = args["no_acc"];
    m_Spot_groups = args["spot_groups"];
    m_Over_start = args["over-start"];
    m_Over_start_verify = args["over-start-verify"];
    m_Primary = args["primary"];
    m_Secondary = args["secondary"];
    m_Count_ids = args["count_ids"];
    m_Make_graphs = args["make_graphs"];
    m_Limit_count = args["limit_count"].AsInteger();
    m_Scan_reads = args["scan_reads"];
    if (args["start_id"]) m_Start_id = args["start_id"].AsInteger();

    m_Repeats = args["repeats"].AsInteger();
    int thr_count = args["threads"].AsInteger();

    cout << "Running " << thr_count << " threads, " << m_Repeats
         << " repeats, accession " << m_Acc << ", using " << (m_UseVDB ? "VDB" : "CSraDb") << endl;
    cout << "Starting threads";
    vector<thread> threads;
    for (int i = 0; i < thr_count; ++i) {
        cout << ".";
        threads.push_back(thread(&CCSRATestMTApp::RunTestThread, this));
    }
    cout << endl;
    cout << "Waiting for threads";
    for (int i = 0; i < thr_count; ++i) {
        cout << ".";
        threads[i].join();
    }
    cout << endl;

    return 0;
}

void CCSRATestMTApp::RunTestThread(void)
{
    for (int i = 0; i < m_Repeats; ++i) {
        RunTest();
    }
}

void CCSRATestMTApp::RunTest(void)
{
    CVDBMgr mgr;

    if ( !m_CachePath.empty() ) {
        mgr.SetCacheRoot(m_CachePath);
        mgr.CommitConfig();
    }

    if (m_Verbose && !m_Acc.empty()) {
        try {
            string path = mgr.FindAccPath(m_Acc);
        }
        catch ( CSraException& exc ) {
            ERR_POST("FindAccPath failed: "<<exc);
        }
    }

    CSeq_id_Handle query_idh;
    if ( !m_Query_id.empty() ) {
        query_idh = CSeq_id_Handle::GetHandle(m_Query_id);
    }
    
    map<string, Uint8> unique_counts;

    string acc_or_path = !m_Acc.empty() ? m_Acc : m_Path;
    if ( m_UseVDB ) {
        CVDB db(mgr, acc_or_path);
        if ( m_Refseq_table ) {
            CVDBTable ref_tbl(db, "REFERENCE");
            CVDBCursor ref_curs(ref_tbl);
            CVDBColumn name_col(ref_curs, "NAME");
            CVDBColumn name_range_col(ref_curs, "NAME_RANGE");
            CVDBColumn seq_id_col(ref_curs, "SEQ_ID");
            CVDBColumn aln_ids_col(ref_curs, "PRIMARY_ALIGNMENT_IDS");
            CVDBColumn seq_len_col(ref_curs, "SEQ_LEN");

            string ref_name, ref_seq_id;
            uint64_t slot_size = 0, slot_count = 0, aln_count = 0;
            for ( uint64_t row = 1, max_row = ref_curs.GetMaxRowId();
                  true; ++row ) {
                bool more = row <= max_row && ref_curs.TryOpenRow(row);
                if ( more &&
                     *CVDBStringValue(ref_curs, name_col) == ref_name ) {
                    CVDBValueFor<uint64_t> aln_ids(ref_curs, aln_ids_col);
                    slot_count += 1;
                    aln_count += aln_ids.size();
                    continue;
                }
                if ( !more ) {
                    break;
                }
                CVDBStringValue name(ref_curs, name_col);
                CVDBStringValue seq_id(ref_curs, seq_id_col);
                CVDBValueFor<int32_t> seq_len(ref_curs, seq_len_col);
                ref_name = *name;
                ref_seq_id = *seq_id;
                slot_size = *seq_len;
                
                CVDBValueFor<uint64_t> aln_ids(ref_curs, aln_ids_col);
                slot_count = 1;
                aln_count = aln_ids.size();
            }
        }
    }
    else {
        CCSraDb csra_db;
        if ( m_No_acc ) {
            try {
                csra_db = RETRY(CCSraDb(mgr, acc_or_path));
                ERR_POST(Fatal<<
                         "CCSraDb succeeded for an absent SRA acc: "<<
                         acc_or_path);
            }
            catch ( CSraException& exc ) {
                if ( exc.GetErrCode() != CSraException::eNotFoundDb ) {
                    ERR_POST(Fatal<<
                             "CSraException is wrong for an absent SRA acc: "<<
                             acc_or_path<<": "<<exc);
                }
                return;
            }
        }
        else {
            csra_db = RETRY(CCSraDb(mgr, acc_or_path));
        }
        
        if ( m_Spot_groups ) {
            CCSraDb::TSpotGroups spot_groups;
            csra_db.GetSpotGroups(spot_groups);
            ITERATE ( CCSraDb::TSpotGroups, it, spot_groups ) {
            }
        }

        if ( m_Refseq_table ) {
            for ( CCSraRefSeqIterator it(csra_db); it; ++it ) {
            }
        }
        
        if ( query_idh && (m_Over_start || m_Over_start_verify) ) {
            CCSraRefSeqIterator ref_it(csra_db, query_idh);
            TSeqPos step = csra_db.GetRowSize();
            TSeqPos ref_len = ref_it.GetSeqLength();
            vector<TSeqPos> vv = ref_it.GetAlnOverStarts();
            vector<TSeqPos> vv_exp;
            if ( m_Over_start_verify ) {
                for ( TSeqPos i = 0; i*step < ref_len; ++i ) {
                    vv_exp.push_back(i*step);
                }
                for ( CCSraAlignIterator it =
                          RETRY(CCSraAlignIterator(csra_db, query_idh, 0, ref_len,
                                                   CCSraAlignIterator::eSearchByStart));
                      it; ++it ) {
                    TSeqPos aln_beg = it.GetRefSeqPos();
                    TSeqPos aln_len = it.GetRefSeqLen();
                    _ASSERT(aln_beg+aln_len <= ref_len);
                    TSeqPos slot_beg = aln_beg/step;
                    TSeqPos slot_end = (aln_beg+aln_len-1)/step;
                    for ( TSeqPos i = slot_beg; i <= slot_end; ++i ) {
                        vv_exp[i] = min(vv_exp[i], aln_beg/*-aln_beg%step*/);
                    }
                }
            }
        }

        CCSraRefSeqIterator::TAlignType align_type;
        if ( m_Primary ) {
            align_type = CCSraRefSeqIterator::fPrimaryAlign;
        }
        else if ( m_Secondary ) {
            align_type = CCSraRefSeqIterator::fSecondaryAlign;
        }
        else {
            align_type = CCSraRefSeqIterator::fAnyAlign;
        }
        if ( query_idh && m_Count_ids ) {
            CCSraRefSeqIterator ref_it(csra_db, query_idh);
            TSeqPos step = csra_db.GetRowSize();
            uint64_t count = 0;
            TSeqPos end = min(m_Query_range.GetToOpen(), ref_it.GetSeqLength());
            for ( TSeqPos p = m_Query_range.GetFrom(); p < end; p += step ) {
                count += ref_it.GetAlignCountAtPos(p, align_type);
            }
        }

        if ( m_Make_graphs ) {
            for ( CCSraRefSeqIterator it(csra_db); it; ++it ) {
                CRef<CSeq_graph> graph = it.GetCoverageGraph();
                Uint8 total_count_b = 0, max_b = 0;
                ITERATE ( CByte_graph::TValues, it2, graph->GetGraph().GetByte().GetValues() ) {
                    Uint1 b = *it2;
                    total_count_b += b;
                    if ( b > max_b ) {
                        max_b = b;
                    }
                }
                Uint8 total_count_a = 0, max_a = 0;
                double k_min = 0, k_max = 1e9;
                for ( TVDBRowId row = it.GetInfo().m_RowFirst; row <= it.GetInfo().m_RowLast; ++row ) {
                    Uint1 b = graph->GetGraph().GetByte().GetValues()[size_t(row-it.GetInfo().m_RowFirst)];
                    size_t a = it.GetAlignCountAtPos(TSeqPos(row*csra_db.GetRowSize()));
                    total_count_a += a;
                    if ( a > max_a ) {
                        max_a = a;
                    }
                    if ( a ) {
                        k_min = max(k_min, (b-.5)/a);
                        k_max = min(k_max, (b+.5)/a);
                    }
                }
            }
        }

        if ( query_idh ) {
            if ( m_Make_seq_entry || m_Make_seq_annot || m_Make_quality_graph ||
                 m_Make_ref_seq || m_Make_short_seq || !m_Make_stat_graph ) {
                if ( m_Make_ref_seq ) {
                    CRef<CBioseq> obj =
                        CCSraRefSeqIterator(csra_db, query_idh).GetRefBioseq();
                }
                size_t count = 0, skipped = 0;
                for ( CCSraAlignIterator it =
                          RETRY(CCSraAlignIterator(csra_db, query_idh,
                                                   m_Query_range.GetFrom(),
                                                   m_Query_range.GetLength(),
                                                   m_Mode, align_type));
                      it; ++it ) {
                    if ( it.GetMapQuality() < m_Min_quality ) {
                        ++skipped;
                        continue;
                    }
                    ++count;
                    if ( m_Count_unique ) {
                        string key = NStr::NumericToString(it.GetRefSeqPos())+'/'+it.GetCIGARLong()+'/'+it.GetMismatchRaw();
                        unique_counts[key] += 1;
                    }
                    if ( m_Make_seq_annot ) {
                        CRef<CSeq_annot> obj = it.GetMatchAnnot();
                    }
                    if ( m_Make_short_seq ) {
                        CRef<CBioseq> obj = it.GetShortBioseq();
                    }
                    if ( m_Make_seq_entry ) {
                        CRef<CSeq_entry> obj = it.GetMatchEntry();
                    }
                    if ( m_Make_quality_graph ) {
                        CRef<CSeq_annot> obj = it.GetQualityGraphAnnot("q");
                    }
                    if ( m_Make_seq_iter ) {
                        CCSraShortReadIterator it2(csra_db,
                                                   it.GetShortId1(),
                                                   it.GetShortId2());
                        CRef<CBioseq> obj = it2.GetShortBioseq();
                    }
                    if ( m_Limit_count && count >= m_Limit_count ) {
                        break;
                    }
                }
            }
            if ( m_Make_stat_graph ) {
                size_t count = 0, skipped = 0;
                vector<SBaseStat> ss(m_Query_range.GetLength());
                CRef<CSeq_id> ref_id =
                    CCSraRefSeqIterator(csra_db, query_idh).GetRefSeq_id();
                for ( CCSraAlignIterator it(csra_db, query_idh,
                                            m_Query_range.GetFrom(),
                                            m_Query_range.GetLength(),
                                            m_Mode, align_type);
                      it; ++it ) {
                    if ( it.GetMapQuality() < m_Min_quality ) {
                        ++skipped;
                        continue;
                    }
                    ++count;
                    TSeqPos ref_pos = it.GetRefSeqPos()-m_Query_range.GetFrom();
                    TSeqPos read_pos = it.GetShortPos();
                    CTempString read = it.GetMismatchRead();
                    CTempString cigar = it.GetCIGARLong();
                    const char* ptr = cigar.data();
                    const char* end = cigar.end();
                    while ( ptr != end ) {
                        char type = 0;
                        int seglen = 0;
                        for ( ; ptr != end; ) {
                            char c = *ptr++;
                            if ( c >= '0' && c <= '9' ) {
                                seglen = seglen*10+(c-'0');
                            }
                            else {
                                type = c;
                                break;
                            }
                        }
                        if ( seglen == 0 ) {
                            NCBI_THROW_FMT(CSraException, eDataError,
                                           "Bad CIGAR length: " << type <<
                                           "0 in " << cigar);
                        }
                        if ( type == 'M' || type == 'X' ) {
                            // match
                            for ( int i = 0; i < seglen; ++i ) {
                                if ( ref_pos < ss.size() ) {
                                    ss[ref_pos].add_base(read[read_pos]);
                                }
                                ++ref_pos;
                                ++read_pos;
                            }
                        }
                        else if ( type == '=' ) {
                            // match
                            for ( int i = 0; i < seglen; ++i ) {
                                if ( ref_pos < ss.size() ) {
                                    ss[ref_pos].add_match();
                                }
                                ++ref_pos;
                                ++read_pos;
                            }
                        }
                        else if ( type == 'I' || type == 'S' ) {
                            if ( type == 'S' ) {
                                // soft clipping already accounted in seqpos
                                continue;
                            }
                            read_pos += seglen;
                        }
                        else if ( type == 'N' ) {
                            // intron
                            ref_pos += seglen;
                        }
                        else if ( type == 'D' ) {
                            // delete
                            for ( int i = 0; i < seglen; ++i ) {
                                if ( ref_pos < ss.size() ) {
                                    ss[ref_pos].add_gap();
                                }
                                ++ref_pos;
                            }
                        }
                        else if ( type != 'P' ) {
                            NCBI_THROW_FMT(CSraException, eDataError,
                                           "Bad CIGAR char: " <<type<< " in " <<cigar);
                        }
                    }
                }

                CRef<CSeq_annot> obj(new CSeq_annot);
                obj->SetData().SetGraph();
                
                int c_min[SBaseStat::kNumStat], c_max[SBaseStat::kNumStat];
                for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
                    c_min[i] = kMax_Int;
                    c_max[i] = 0;
                }
                for ( size_t j = 0; j < ss.size(); ++j ) {
                    const SBaseStat& s = ss[j];
                    for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
                        int c = s.cnts[i];
                        c_min[i] = min(c_min[i], c);
                        c_max[i] = max(c_max[i], c);
                    }
                }
                for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
                    CRef<CSeq_graph> graph(new CSeq_graph);
                    static const char* const titles[6] = {
                        "Number of A bases",
                        "Number of C bases",
                        "Number of G bases",
                        "Number of T bases",
                        "Number of inserts",
                        "Number of matches"
                    };
                    graph->SetTitle(titles[i]);
                    CSeq_interval& loc = graph->SetLoc().SetInt();
                    loc.SetId(*ref_id);
                    loc.SetFrom(m_Query_range.GetFrom());
                    loc.SetTo(m_Query_range.GetTo());
                    graph->SetNumval(TSeqPos(ss.size()));

                    if ( c_max[i] < 256 ) {
                        CByte_graph& data = graph->SetGraph().SetByte();
                        CByte_graph::TValues& vv = data.SetValues();
                        vv.reserve(ss.size());
                        for ( size_t j = 0; j < ss.size(); ++j ) {
                            vv.push_back(ss[j].cnts[i]);
                        }
                        data.SetMin(c_min[i]);
                        data.SetMax(c_max[i]);
                        data.SetAxis(0);
                    }
                    else {
                        CInt_graph& data = graph->SetGraph().SetInt();
                        CInt_graph::TValues& vv = data.SetValues();
                        vv.reserve(ss.size());
                        for ( size_t j = 0; j < ss.size(); ++j ) {
                            vv.push_back(ss[j].cnts[i]);
                        }
                        data.SetMin(c_min[i]);
                        data.SetMax(c_max[i]);
                        data.SetAxis(0);
                    }
                    obj->SetData().SetGraph().push_back(graph);
                }
            }
        }

        if ( m_Scan_reads ) {
            int count = m_Limit_count;
            Uint8 scanned = 0, clipped = 0;
            Uint8 rejected = 0, duplicate = 0, hidden = 0;
            Uint8 spot_id = 0;
            Uint4 read_id = 0;
            CCSraShortReadIterator::TBioseqFlags flags = m_Make_quality_graph?
                CCSraShortReadIterator::fQualityGraph:
                CCSraShortReadIterator::fDefaultBioseqFlags;
            CCSraShortReadIterator it;
            if ( m_Start_id ) {
                it = CCSraShortReadIterator(csra_db, m_Start_id);
            }
            else {
                it = CCSraShortReadIterator(csra_db);
            }
            for ( ; it; ++it ) {
                if ( !spot_id ) {
                    spot_id = it.GetShortId1();
                    read_id = it.GetShortId2();
                }
                ++scanned;
                if ( it.HasClippingInfo() ) {
                    ++clipped;
                }
                switch ( it.GetReadFilter() ) {
                case SRA_READ_FILTER_REJECT: ++rejected; break;
                case SRA_READ_FILTER_CRITERIA: ++duplicate; break;
                case SRA_READ_FILTER_REDACTED: ++hidden; break;
                default: break;
                }
                CRef<CBioseq> seq = it.GetShortBioseq(flags);
                if ( m_Fasta ) {
                    seq->GetInst().GetSeq_data().GetIupacna().Get();
                }
                if ( !--count ) break;
            }
            if ( spot_id ) {
                CCSraShortReadIterator it2(csra_db, spot_id, read_id);
                CRef<CBioseq> seq = it2.GetShortBioseq(flags);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CCSRATestMTApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CCSRATestMTApp().AppMain(argc, argv);
}
