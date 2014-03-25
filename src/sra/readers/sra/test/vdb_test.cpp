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
 *   Sample test application for cSRA reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/sra/csraread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <align/align-access.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CCSRATestApp::


class CCSRATestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

//#define DEFAULT_FILE NCBI_TRACES01_PATH "/compress/DATA/ASW/NA19909.mapped.illumina.mosaik.ASW.exome.20110411/s-quantized.csra"
#define DEFAULT_FILE NCBI_TRACES01_PATH "/compress/1KG/ASW/NA19909/exome.ILLUMINA.MOSAIK.csra"

void CCSRATestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "vdb_test");

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

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

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
/*
string GetShortSeqData(const CBamAlignIterator& it)
{
    string ret;
    const CBamString& src = it.GetShortSequence();
    //bool minus = it.IsSetStrand() && IsReverse(it.GetStrand());
    TSeqPos src_pos = it.GetCIGARPos();

    const CBamString& CIGAR = it.GetCIGAR();
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
*/

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

#if 0
void CheckRc(const char* call, rc_t rc)
{
    if ( rc ) {
        char buffer[1024];
        size_t error_len;
        RCExplain(rc, buffer, sizeof(buffer), &error_len);
        cerr << call << ": SDK Error: 0x" << hex << rc << dec << ": " << buffer << endl;
        exit(1);
    }
}
#define CheckRc(call) CheckRc(#call, call)

#include <vdb/vdb-priv.h>
#include <sra/sradb-priv.h>
#include <sra/xf.h>
#include <sra/sraschema.h>

int LowLevelTest(const string& path)
{
    RegisterSRASchemaMake(SRASchemaMake);
    register_sraxf_functions();
    register_vxf_functions();
    register_axf_functions();

    for ( int i = 0; i < 10; ++i ) {
        const VDBManager* mgr = 0;
        const VDatabase* db = 0;
        const VTable* table = 0;
        const VCursor* cursor = 0;

        CheckRc(VDBManagerMakeRead(&mgr, 0));
        CheckRc(VDBManagerOpenDBRead(mgr, &db, 0, path.c_str()));
        CheckRc(VDatabaseOpenTableRead(db, &table, "PRIMARY_ALIGNMENT"));
        CheckRc(VTableCreateCursorRead(table, &cursor));

        uint32_t col_index;
        CheckRc(VCursorAddColumn(cursor, &col_index, "(bool)HAS_REF_OFFSET"));
        CheckRc(VCursorOpen(cursor));
        for ( uint64_t row_id = 1; row_id <= 1; ++row_id ) {
            const void* data;
            uint32_t bit_off, bit_len, cnt;
            CheckRc(VCursorCellDataDirect(cursor, row_id, col_index,
                                          &bit_len, &data, &bit_off, &cnt));
        }
            
        CheckRc(VCursorOpen(cursor));

        CheckRc(VCursorRelease(cursor));
        CheckRc(VTableRelease(table));
        if ( db ) CheckRc(VDatabaseRelease(db));
        CheckRc(VDBManagerRelease(mgr));
    }
    cout << "Done" << endl;
    return 0;
}
#endif

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

int CCSRATestApp::Run(void)
{
    const CArgs& args = GetArgs();

    bool verbose = args["verbose"];
    bool fasta = args["fasta"];

    CVDBMgr mgr;

    string acc_or_path = DEFAULT_FILE;
    if ( args["file"] ) {
        acc_or_path = args["file"].AsString();
    }
    else if ( args["acc"] ) {
        acc_or_path = args["acc"].AsString();
        if ( verbose ) {
            try {
                string path = mgr.FindAccPath(acc_or_path);
                cout << "Resolved "<<acc_or_path<<" -> "<<path<<endl;
            }
            catch ( CSraException& exc ) {
                ERR_POST("FindAccPath failed: "<<exc);
            }
        }
    }
    
    //return LowLevelTest(path);

    string query_id;
    CRange<TSeqPos> query_range = CRange<TSeqPos>::GetWhole();
    
    if ( args["refseq"] ) {
        query_id = args["refseq"].AsString();
        query_range.SetFrom(args["refpos"].AsInteger());
        if ( args["refwindow"] ) {
            TSeqPos window = args["refwindow"].AsInteger();
            if ( window != 0 ) {
                query_range.SetLength(window);
            }
            else {
                query_range.SetTo(kInvalidSeqPos);
            }
        }
        if ( args["refend"] ) {
            query_range.SetTo(args["refend"].AsInteger());
        }
    }
    if ( args["q"] ) {
        string q = args["q"].AsString();
        SIZE_TYPE colon_pos = q.find(':');
        if ( colon_pos == NPOS || colon_pos == 0 ) {
            ERR_POST(Fatal << "Invalid query format: " << q);
        }
        query_id = q.substr(0, colon_pos);
        SIZE_TYPE dash_pos = q.find('-', colon_pos+1);
        if ( dash_pos == NPOS ) {
            ERR_POST(Fatal << "Invalid query format: " << q);
        }
        TSeqPos from =
            NStr::StringToNumeric<TSeqPos>(q.substr(colon_pos+1,
                                                    dash_pos-colon_pos-1));
        TSeqPos to = NStr::StringToNumeric<TSeqPos>(q.substr(dash_pos+1));
        query_range.SetFrom(from).SetTo(to);
    }
    int min_quality = args["min_quality"].AsInteger();
    bool make_ref_seq = args["ref_seq"];
    bool make_short_seq = args["short_seq"];
    bool make_seq_annot = args["seq_annot"];
    bool make_seq_entry = args["seq_entry"];
    bool make_seq_iter = args["seq_iter"];
    bool make_quality_graph = args["quality_graph"];
    bool make_stat_graph = args["stat_graph"];
    bool print_objects = args["print_objects"];

    CNcbiOstream& out = cout;

    const bool kUseVDB = false;
    if ( kUseVDB ) {
        CVDB db(mgr, acc_or_path);
        if ( args["refseq_table"] ) {
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
                else if ( slot_count ) {
                    out << ref_name << " " << ref_seq_id
                        << " " << slot_count*slot_size
                        << " " << aln_count
                        << NcbiEndl;
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
        else {
        }
    }
    else {
        CStopWatch sw;
        
        sw.Restart();
        CCSraDb csra_db;
        if ( args["no_acc"] ) {
            try {
                csra_db = CCSraDb(mgr, acc_or_path);
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
                out << "Correctly detected absent SRA acc: "<<acc_or_path
                    << NcbiEndl;
                return 0;
            }
        }
        else {
            csra_db = CCSraDb(mgr, acc_or_path);
        }
        out << "Opened CSRA in "<<sw.Elapsed()
            << NcbiEndl;
        
        if ( args["spot_groups"] ) {
            CCSraDb::TSpotGroups spot_groups;
            csra_db.GetSpotGroups(spot_groups);
            ITERATE ( CCSraDb::TSpotGroups, it, spot_groups ) {
                out << *it << "\n";
            }
            out << "Total "<<spot_groups.size()<<" spot groups."<<endl;
        }

        if ( args["refseq_table"] ) {
            sw.Restart();
            for ( CCSraRefSeqIterator it(csra_db); it; ++it ) {
                out << it->m_Name << " " << it->m_SeqId
                    << " len: "<<it.GetSeqLength()
                    << " @(" << it->m_RowFirst << "," << it->m_RowLast << ")"
                    << NcbiEndl;
            }
            out << "Scanned reftable in "<<sw.Elapsed()
                << NcbiEndl;
            sw.Restart();
        }

        if ( args["make_graphs"] ) {
            sw.Restart();
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
                if ( 0 ) {
                    out << MSerial_AsnText << *graph;
                }
                out << it.GetRefSeqId()
                    << ": total_b = "<<total_count_b
                    << " max_b = "<<max_b
                    << NcbiEndl;
                Uint8 total_count_a = 0, max_a = 0;
                double k_min = 0, k_max = 1e9;
                for ( uint64_t row = it.GetInfo().m_RowFirst; row <= it.GetInfo().m_RowLast; ++row ) {
                    Uint1 b = graph->GetGraph().GetByte().GetValues()[row-it.GetInfo().m_RowFirst];
                    size_t a = it.GetRowAlignCount(row);
                    total_count_a += a;
                    if ( a > max_a ) {
                        max_a = a;
                    }
                    if ( a ) {
                        k_min = max(k_min, (b-.5)/a);
                        k_max = min(k_max, (b+.5)/a);
                        if ( 0 ) {
                            out << row<<": " << a << " " << b+0
                                << " " << (b-.5)/a << " - " << (b+.5)/a
                                << "\n";
                        }
                    }
                }
                out << it.GetRefSeqId()
                    << ": total_a = "<<total_count_a
                    << " max_a = "<<max_a
                    << NcbiEndl;
                out << "k: " << k_min << " - " << k_max
                    << NcbiEndl;
            }
            out << "Scanned graph in "<<sw.Elapsed()
                << NcbiEndl;
        }

        CSeq_id_Handle query_idh;
        if ( !query_id.empty() ) {
            query_idh = CSeq_id_Handle::GetHandle(query_id);
            sw.Restart();
            if ( make_seq_entry || make_seq_annot || make_quality_graph ||
                 make_ref_seq || make_short_seq || !make_stat_graph ) {
                if ( make_ref_seq ) {
                    CRef<CBioseq> obj =
                        CCSraRefSeqIterator(csra_db, query_idh).GetRefBioseq();
                    if ( print_objects )
                        out << MSerial_AsnText << *obj;
                }
                size_t count = 0, skipped = 0;
                size_t limit_count = args["limit_count"].AsInteger();
                for ( CCSraAlignIterator it(csra_db, query_idh,
                                            query_range.GetFrom(),
                                            query_range.GetLength());
                      it; ++it ) {
                    if ( it.GetMapQuality() < min_quality ) {
                        ++skipped;
                        continue;
                    }
                    if ( 0 ) {
                        CRef<CBioseq> seq = it.GetShortBioseq();
                        seq->SetAnnot().push_back(it.GetMatchAnnot());
                        CRef<CBioseq> seq2(SerialClone(*seq));
                        for ( CTypeIterator<CSeq_id> it(Begin(*seq2)); it; ++it ) {
                            CSeq_id& id = *it;
                            if ( id.IsLocal() ) {
                                id.Set("gnl|SRA|SRR389414.136780466.1");
                                break;
                            }
                        }
                        NcbiCout << "With local:"
                                 << MSerial_AsnText << *seq << endl;
                        char buf[1024];
                        for ( int t = 0; t < 10; ++t ) {
                            size_t mem0, rss0, shr0;
                            GetMemoryUsage(&mem0, &rss0, &shr0);
                            vector< CRef<CBioseq> > bb;
                            vector<CSeq_id_Handle> hh;
                            for ( int i = 0; i < 100000; ++i ) {
                                bb.push_back(Ref(SerialClone(*seq)));
                                sprintf(buf, "lcl|%d", 136780466+i);
                                hh.push_back(CSeq_id_Handle::GetHandle(buf));
                            }
                            size_t mem1, rss1, shr1;
                            GetMemoryUsage(&mem1, &rss1, &shr1);
                            NcbiCout << "mem: " << mem0 << " -> " << mem1
                                     << " = " << mem1-mem0 << endl;
                            NcbiCout << "rss: " << rss0 << " -> " << rss1
                                     << " = " << rss1-rss0 << endl;
                            NcbiCout << "shr: " << shr0 << " -> " << shr1
                                     << " = " << shr1-shr0 << endl;
                        }
                        NcbiCout << "With general:"
                                 << MSerial_AsnText << *seq2 << endl;
                        for ( int t = 0; t < 10; ++t ) {
                            size_t mem0, rss0, shr0;
                            GetMemoryUsage(&mem0, &rss0, &shr0);
                            vector< CRef<CBioseq> > bb;
                            vector<CSeq_id_Handle> hh;
                            for ( int i = 0; i < 100000; ++i ) {
                                bb.push_back(Ref(SerialClone(*seq2)));
                                sprintf(buf, "gnl|SRA|SRR389414.%d.1", 136780466+i);
                                hh.push_back(CSeq_id_Handle::GetHandle(buf));
                            }
                            size_t mem1, rss1, shr1;
                            GetMemoryUsage(&mem1, &rss1, &shr1);
                            NcbiCout << "mem: " << mem0 << " -> " << mem1
                                     << " = " << mem1-mem0 << endl;
                            NcbiCout << "rss: " << rss0 << " -> " << rss1
                                     << " = " << rss1-rss0 << endl;
                            NcbiCout << "shr: " << shr0 << " -> " << shr1
                                     << " = " << shr1-shr0 << endl;
                        }
                        break;
                    }
                    ++count;
                    if ( verbose ) {
                        out << it.GetAlignmentId() << ": "
                            << it.GetShortId1()<<"."<<it.GetShortId2() << ": "
                            << it.GetRefSeqId() << "@" << it.GetRefSeqPos()
                            << " vs " << it.GetShortSeq_id()->AsFastaString();
                        if ( args["spot_groups"] ) {
                            out << "  GROUP: "<<it.GetSpotGroup();
                        }
                        out << "\n    CIGAR: "<<it.GetCIGARLong();
                        out << "\n mismatch: "<<it.GetMismatchRaw();
                        out << NcbiEndl;
                    }
                    if ( make_seq_annot ) {
                        CRef<CSeq_annot> obj = it.GetMatchAnnot();
                        if ( print_objects )
                            out << MSerial_AsnText << *obj;
                    }
                    if ( make_short_seq ) {
                        CRef<CBioseq> obj = it.GetShortBioseq();
                        if ( print_objects )
                            out << MSerial_AsnText << *obj;
                    }
                    if ( make_seq_entry ) {
                        CRef<CSeq_entry> obj = it.GetMatchEntry();
                        if ( print_objects )
                            out << MSerial_AsnText << *obj;
                    }
                    if ( make_quality_graph ) {
                        CRef<CSeq_annot> obj = it.GetQualityGraphAnnot("q");
                        if ( print_objects )
                            out << MSerial_AsnText << *obj;
                    }
                    if ( make_seq_iter ) {
                        CCSraShortReadIterator it2(csra_db,
                                                   it.GetShortId1(),
                                                   it.GetShortId2());
                        CRef<CBioseq> obj = it2.GetShortBioseq();
                        if ( print_objects )
                            out << MSerial_AsnText << *obj;
                    }
                    if ( limit_count && count >= limit_count ) {
                        break;
                    }
                }
                out << "Found "<<count<<" alignments." << NcbiEndl;
                if ( skipped )
                    out << "Skipped "<<skipped<<" alignments." << NcbiEndl;
            }
            if ( make_stat_graph ) {
                size_t count = 0, skipped = 0;
                vector<SBaseStat> ss(query_range.GetLength());
                CRef<CSeq_id> ref_id =
                    CCSraRefSeqIterator(csra_db, query_idh).GetRefSeq_id();
                for ( CCSraAlignIterator it(csra_db, query_idh,
                                            query_range.GetFrom(),
                                            query_range.GetLength());
                      it; ++it ) {
                    if ( it.GetMapQuality() < min_quality ) {
                        ++skipped;
                        continue;
                    }
                    ++count;
                    TSeqPos ref_pos = it.GetRefSeqPos()-query_range.GetFrom();
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
                        else if ( type == 'D' || type == 'N' ) {
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
                out << "Found "<<count<<" alignments." << NcbiEndl;
                if ( skipped )
                    out << "Skipped "<<skipped<<" alignments." << NcbiEndl;

                CRef<CSeq_annot> obj(new CSeq_annot);
                obj->SetData().SetGraph();
                
                int c_min[SBaseStat::kNumStat], c_max[SBaseStat::kNumStat];
                for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
                    c_min[i] = kMax_Int;
                    c_max[i] = 0;
                }
                for ( size_t j = 0; j < ss.size(); ++j ) {
                    const SBaseStat& s = ss[j];
                    if ( verbose && s.total() ) {
                        out << j+query_range.GetFrom()<<": ";
                        for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
                            int c = s.cnts[i];
                            for ( int k = 0; k < c; ++k )
                                out << "ACGT-="[i];
                        }
                        out << '\n';
                    }
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
                    loc.SetFrom(query_range.GetFrom());
                    loc.SetTo(query_range.GetTo());
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

                if ( print_objects )
                    out << MSerial_AsnText << *obj;
            }
            out << "Scanned aligns in "<<sw.Elapsed()
                << NcbiEndl;
            sw.Restart();
        }

        if ( 0 ) {
            typedef size_t TShortId;
            typedef map<TShortId, TSeqPos> TShortId2Pos;
            TShortId2Pos id2pos;
            for ( CCSraAlignIterator it(csra_db, query_idh,
                                        query_range.GetFrom(),
                                        query_range.GetLength());
                  it; ++it ) {
                CTempString ref_seq_id = it.GetRefSeqId();
                TSeqPos ref_pos = it.GetRefSeqPos();
                TSeqPos ref_len = it.GetRefSeqLen();
                bool ref_minus = it.GetRefMinusStrand();
                CTempString cigar = it.GetCIGAR();
                TShortId id(it.GetShortId1());//, it.GetShortId2());
                pair<TShortId2Pos::iterator, bool> ins =
                    id2pos.insert(make_pair(id, ref_pos));
                if ( ref_pos < ins.first->second ) {
                    ins.first->second = ref_pos;
                }
                if ( 1 ) {
                    out << ref_seq_id << " "
                        << ref_pos << " " << ref_len << " " << ref_minus
                        << " " << cigar << " "
//                     << *spot_group << " " << *spot_id << "." << *read_id
                        << MSerial_AsnText << *it.GetMatchEntry()
//                     << MSerial_AsnText << *it.GetShortBioseq()
                        << NcbiEndl;
                }
            }
            ITERATE ( TShortId2Pos, it, id2pos ) {
                out << it->second << ": "
                    << it->first
                    << NcbiEndl;
            }
        }
        if ( args["scan_reads"] ) {
            int count = args["limit_count"].AsInteger();
            Uint8 scanned = 0, clipped = 0;
            Uint8 start_id = args["start_id"]? args["start_id"].AsInteger(): 1;
            Uint8 spot_id = 0;
            Uint4 read_id = 0;
            CCSraShortReadIterator::TBioseqFlags flags = make_quality_graph?
                CCSraShortReadIterator::fQualityGraph:
                CCSraShortReadIterator::fDefaultBioseqFlags;
            for ( CCSraShortReadIterator it(csra_db, start_id); it; ++it ) {
                if ( !spot_id ) {
                    spot_id = it.GetShortId1();
                    read_id = it.GetShortId2();
                }
                ++scanned;
                if ( it.HasClippingInfo() ) {
                    ++clipped;
                }
                CRef<CBioseq> seq = it.GetShortBioseq(flags);
                if ( print_objects ) {
                    out << MSerial_AsnText << *seq;
                }
                if ( fasta ) {
                    out << '>' << seq->GetId().front()->AsFastaString() << " length="<<seq->GetInst().GetLength() << '\n';
                    out << seq->GetInst().GetSeq_data().GetIupacna().Get() << '\n';
                }
                if ( !--count ) break;
            }
            CCSraShortReadIterator it(csra_db, spot_id, read_id);
            CRef<CBioseq> seq = it.GetShortBioseq(flags);
            out << "First: " << MSerial_AsnText << *seq;
            out << "Clipped: "<<clipped<<"/"<<scanned<<" = "<<100.*clipped/scanned<<"%" << endl;
        }
    }

    out << "Success." << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CCSRATestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CCSRATestApp().AppMain(argc, argv);
}
