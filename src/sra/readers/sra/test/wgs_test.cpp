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
 *   Sample test application for WGS reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/readers/sra/impl/wgsresolver_impl.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CWGSTestApp::


class CWGSTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

void CWGSTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "wgs_test");

    arg_desc->AddOptionalKey("vol_path", "VolPath",
                             "Search path for volumes",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("file", "File",
                            "WGS file name, accession, or prefix",
                            CArgDescriptions::eString,
                            "AAAA");

    arg_desc->AddOptionalKey("limit_count", "LimitCount",
                             "Number of entries to read (-1 : unlimited)",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("verbose", "Print info about found data");

    arg_desc->AddFlag("withdrawn", "Include withdrawn sequences");
    arg_desc->AddFlag("replaced", "Include replaced sequences");
    arg_desc->AddFlag("suppressed", "Include suppressed sequences");
    arg_desc->AddFlag("unverified", "Include unverified sequences");
    arg_desc->AddFlag("master", "Include master descriptors if any");
    arg_desc->AddFlag("print-master", "Print master entry");
    arg_desc->AddFlag("master-no-filter", "Do not filter master descriptors");

    //arg_desc->AddFlag("resolve-gi", "Resolve gi list");

    arg_desc->AddFlag("gi-check", "Check GI index");
    arg_desc->AddFlag("gi-range", "Print GI range if any");
    arg_desc->AddDefaultKey("max-gap", "MaxGap",
                            "max gap in a single gi range",
                            CArgDescriptions::eInteger, "10000");

    arg_desc->AddFlag("print_seq", "Print loaded Bioseq objects");
    arg_desc->AddFlag("print_entry", "Print loaded Seq-entry objects");
    arg_desc->AddFlag("print_split", "Print loaded Split info object");

    arg_desc->AddFlag("keep_sequences", "Keep all sequences in memory");

    arg_desc->AddOptionalKey("contig_row", "ContigRow",
                             "contig row to fetch",
                             CArgDescriptions::eInt8);
    arg_desc->AddOptionalKey("contig_version", "ContigVersion",
                             "contig version to fetch",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("scaffold_row", "ScaffoldRow",
                             "scaffold row to fetch",
                             CArgDescriptions::eInt8);
    arg_desc->AddOptionalKey("protein_row", "ProteinRow",
                             "protein row to fetch",
                             CArgDescriptions::eInt8);

    arg_desc->AddFlag("check_non_empty_lookup",
                      "Check that lookup produce non-empty result");
    arg_desc->AddFlag("check_empty_lookup",
                      "Check that lookup produce empty result");
    arg_desc->AddOptionalKey("gi", "GI",
                             "lookup by GI",
                             CArgDescriptions::eIntId);
    arg_desc->AddOptionalKey("contig_name", "ContigName",
                             "lookup by contig name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("scaffold_name", "ScaffoldName",
                             "lookup by scaffold name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("protein_name", "ProteinName",
                             "lookup by protein name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("protein_acc", "ProteinAcc",
                             "lookup by protein accession",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("seed", "RandomSeed",
                            "Seed for random number generator",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("seq_acc_count", "SequentialAccCount",
                            "size of spans of sequential accessions",
                            CArgDescriptions::eInteger, "10");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


string sx_GetSeqData(const CBioseq& seq)
{
    CScope scope(*CObjectManager::GetInstance());
    CBioseq_Handle bh = scope.AddBioseq(seq);
    string ret;
    bh.GetSeqVector().GetSeqData(0, kInvalidSeqPos, ret);
    NON_CONST_ITERATE ( string, i, ret ) {
        if ( *i == char(0xff) || *i == char(0) )
            *i = char(0xf);
    }
    if ( 0 ) {
        size_t w = 0;
        ITERATE ( string, i, ret ) {
            if ( w == 78 ) {
                cout << '\n';
                w = 0;
            }
            cout << "0123456789ABCDEF"[*i&0xff];
            ++w;
        }
        cout << endl;
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

#if 0 // enable low-level SRA SDK test
# include <unistd.h>
# include <klib/rc.h>
# include <klib/writer.h>
# include <align/align-access.h>
# include <vdb/manager.h>
# include <vdb/database.h>
# include <vdb/table.h>
# include <vdb/cursor.h>
# include <vdb/vdb-priv.h>

// low level SRA SDK test
void CheckRc(rc_t rc, const char* code, const char* file, int line)
{
    if ( rc ) {
        char buffer1[4096];
        size_t error_len;
        RCExplain(rc, buffer1, sizeof(buffer1), &error_len);
        char buffer2[8192];
        size_t len = sprintf(buffer2, "%s:%d: %s failed: %#x: %s\n",
                             file, line, code, rc, buffer1);
        write(2, buffer2, len);
        exit(1);
    }
}
#define CALL(call) CheckRc((call), #call, __FILE__, __LINE__)

struct SThreadInfo
{
    pthread_t thread_id;
    const VCursor* cursor;
    vector<uint32_t> columns;
    uint64_t row_start;
    uint64_t row_end;
};

void* read_thread_func(void* arg)
{
    SThreadInfo& info = *(SThreadInfo*)arg;
    for ( int pass = 0; pass < 100; ++pass ) {
        for ( uint64_t row = info.row_start; row < info.row_end; ++row ) {
            for ( size_t i = 0; i < info.columns.size(); ++i ) {
                const void* data;
                uint32_t bit_offset, bit_length;
                uint32_t elem_count;
                CALL(VCursorCellDataDirect(info.cursor, row, info.columns[i],
                                           &bit_length, &data, &bit_offset,
                                           &elem_count));
            }
        }
    }
    return 0;
}

int LowLevelTest(void)
{
    if ( 1 ) {
        cout << "LowLevelTest for CSHW01000012.1 gap info..." << endl;
        const VDBManager* mgr = 0;
        CALL(VDBManagerMakeRead(&mgr, 0));
        
        const VDatabase* db = 0;
        CALL(VDBManagerOpenDBRead(mgr, &db, 0, "CSHW01"));
        
        const VTable* table = 0;
        CALL(VDatabaseOpenTableRead(db, &table, "SEQUENCE"));
        
        const VCursor* cursor = 0;
        CALL(VTableCreateCursorRead(table, &cursor));
        CALL(VCursorPermitPostOpenAdd(cursor));
        CALL(VCursorOpen(cursor));
        
        uint32_t col_GAP_PROPS;
        CALL(VCursorAddColumn(cursor, &col_GAP_PROPS, "GAP_PROPS"));

        const void* data;
        uint32_t bit_offset, bit_length;
        uint32_t elem_count;
        CALL(VCursorCellDataDirect(cursor, 12, col_GAP_PROPS,
                                   &bit_length, &data, &bit_offset,
                                   &elem_count));
        cout << "GAP_PROPS:";
        for ( uint32_t i = 0; i < elem_count; ++i ) {
            cout << ' ' << ((int16_t*)data)[i];
        }
        cout << endl;
        
        CALL(VCursorRelease(cursor));
        CALL(VTableRelease(table));
        CALL(VDatabaseRelease(db));
        CALL(VDBManagerRelease(mgr));
        return 0;
    }

    if ( 1 ) {
        cout << "LowLevelTest for AAAD01 opening..." << endl;
        const VDBManager* mgr = 0;
        CALL(VDBManagerMakeRead(&mgr, 0));
        
        const VDatabase* db = 0;
        CALL(VDBManagerOpenDBRead(mgr, &db, 0, "AAAD01"));
        
        CALL(VDatabaseRelease(db));
        CALL(VDBManagerRelease(mgr));
        return 0;
    }

    if ( 0 ) {
        cout << "LowLevelTest for multiple cursor opening..." << endl;
        for ( int i = 0; i < 10; ++i ) {
            cout << "Iteration " << i << endl;
            const VDBManager* mgr = 0;
            CALL(VDBManagerMakeRead(&mgr, 0));

            const VDatabase* db = 0;
            CALL(VDBManagerOpenDBRead(mgr, &db, 0, "GAMP01"));

            const VTable* table = 0;
            CALL(VDatabaseOpenTableRead(db, &table, "SEQUENCE"));

            const VCursor* cursor = 0;
            CALL(VTableCreateCursorRead(table, &cursor));
            CALL(VCursorPermitPostOpenAdd(cursor));
            CALL(VCursorOpen(cursor));

            uint32_t col_SEQ_ID, col_ACC_VERSION;
            CALL(VCursorAddColumn(cursor, &col_SEQ_ID, "SEQ_ID"));
            CALL(VCursorAddColumn(cursor, &col_ACC_VERSION, "ACC_VERSION"));
            
            CALL(VCursorRelease(cursor));
            CALL(VTableRelease(table));
            CALL(VDatabaseRelease(db));
            CALL(VDBManagerRelease(mgr));
        }
    }
    if ( 0 ) {
        cout << "LowLevelTest for MT cursor read..." << endl;
        const VDBManager* mgr = 0;
        CALL(VDBManagerMakeRead(&mgr, 0));
        
        const VDatabase* db = 0;
        //CALL(VDBManagerOpenDBRead(mgr, &db, 0, "GAMP01"));
        CALL(VDBManagerOpenDBRead(mgr, &db, 0, "JRAS01"));
        const size_t kNumCursors = 8;
        const size_t kNumReads = 250/8;
        
        const VTable* table = 0;
        CALL(VDatabaseOpenTableRead(db, &table, "SEQUENCE"));

        SThreadInfo tinfo[kNumCursors];
        for ( size_t i = 0; i < kNumCursors; ++i ) {
            cout << "Create cursor " << i << endl;
            CALL(VTableCreateCursorRead(table, &tinfo[i].cursor));
            CALL(VCursorPermitPostOpenAdd(tinfo[i].cursor));
            CALL(VCursorOpen(tinfo[i].cursor));

#define ADD_COLUMN(name)                                                \
            do {                                                        \
                uint32_t column;                                        \
                CALL(VCursorAddColumn(tinfo[i].cursor, &column, name)); \
                tinfo[i].columns.push_back(column);                     \
            } while(0)
            
            ADD_COLUMN("GI");
            ADD_COLUMN("ACCESSION");
            ADD_COLUMN("ACC_VERSION");
            ADD_COLUMN("CONTIG_NAME");
            ADD_COLUMN("NAME");
            ADD_COLUMN("TITLE");
            ADD_COLUMN("LABEL");
            ADD_COLUMN("READ_START");
            ADD_COLUMN("READ_LEN");
            ADD_COLUMN("READ");
            //ADD_COLUMN("SEQ_ID");
            ADD_COLUMN("TAXID");
            ADD_COLUMN("DESCR");
            ADD_COLUMN("ANNOT");
            ADD_COLUMN("GB_STATE");
            ADD_COLUMN("GAP_START");
            ADD_COLUMN("GAP_LEN");
            ADD_COLUMN("GAP_PROPS");
            ADD_COLUMN("GAP_LINKAGE");

#undef ADD_COLUMN
        }
        for ( size_t i = 0; i < kNumCursors; ++i ) {
            cout << "Starting thread " << i << endl;
            tinfo[i].row_start = 1+i*kNumReads;
            tinfo[i].row_end = tinfo[i].row_start + kNumReads;
            pthread_create(&tinfo[i].thread_id, 0, read_thread_func, &tinfo[i]);
        }
        for ( size_t i = 0; i < kNumCursors; ++i ) {
            cout << "Waiting for thread " << i << endl;
            void* ret = 0;
            pthread_join(tinfo[i].thread_id, &ret);
        }
        for ( size_t i = 0; i < kNumCursors; ++i ) {
            CALL(VCursorRelease(tinfo[i].cursor));
        }
        CALL(VTableRelease(table));
        CALL(VDatabaseRelease(db));
        CALL(VDBManagerRelease(mgr));
    }
    if ( 1 ) {
        cout << "LowLevelTest sequence reading..." << endl;
        for ( int i = 0; i < 1; ++i ) {
            const VDBManager* mgr = 0;
            CALL(VDBManagerMakeRead(&mgr, 0));

            const VDatabase* db = 0;
            CALL(VDBManagerOpenDBRead(mgr, &db, 0, "JELW01"));

            const VTable* table = 0;
            CALL(VDatabaseOpenTableRead(db, &table, "SEQUENCE"));

            const VCursor* cursor = 0;
            CALL(VTableCreateCursorRead(table, &cursor));
            CALL(VCursorPermitPostOpenAdd(cursor));
            CALL(VCursorOpen(cursor));

            char data0[10];
            uint32_t total = 0;
            const char* type = 0;
            uint32_t bit_size = 0;
            const size_t kBases = 1<<17;
            char* buffer = new char[kBases];
            uint32_t col;

            uint64_t row0 = 1, row1 = 300;
            CStopWatch sw(CStopWatch::eStart);
            if ( 1 ) {
                type = "packed 2na";
                CALL(VCursorAddColumn(cursor, &col,
                                      "(INSDC:2na:packed)READ"));
                bit_size = 2;
            }
            if ( 0 ) {
                type = "packed 4na";
                CALL(VCursorAddColumn(cursor, &col,
                                      "(INSDC:4na:packed)READ"));
                bit_size = 4;
            }
            if ( 0 ) {
                type = "byte 4na";
                CALL(VCursorAddColumn(cursor, &col,
                                      "(INSDC:4na:bin)READ"));
                bit_size = 8;
            }
            const char* method = 0;
            if ( 1 ) {
                method = "in chunks";
                for ( uint64_t row = row0; row <= row1; ++row ) {
                    uint32_t pos = 0, elem_read, elem_rem = 1;
                    for ( ; elem_rem; pos += elem_read ) {
                        CALL(VCursorReadBitsDirect(cursor, row, col,
                                                   bit_size, pos,
                                                   buffer, 0, kBases,
                                                   &elem_read, &elem_rem));
                        if ( row == 1 && pos == 0 ) memcpy(data0, buffer, 10);
                    }
                    total += pos;
                }
            }
            else {
                method = "in whole";
                for ( uint64_t row = row0; row <= row1; ++row ) {
                    uint32_t bit_offset, bit_length, elem_count;
                    const void* data;
                    CALL(VCursorCellDataDirect(cursor, row, col,
                                               &bit_length, &data, &bit_offset,
                                               &elem_count));
                    assert(bit_length = bit_size);
                    if ( row == 1 ) memcpy(data0, data, 10);
                    total += elem_count;
                }
            }
            double time = sw.Elapsed();
            delete[] buffer;
            cout << "read "<<type<<" "<<method
                 <<" time: "<<time<<" bases="<<total << endl;
            cout << "data:" << hex;
            for ( size_t i = 0; i < 10; ++i ) {
                cout << ' ' << (data0[i]&0xff);
            }
            cout << dec << endl;

            CALL(VCursorRelease(cursor));
            CALL(VTableRelease(table));
            CALL(VDatabaseRelease(db));
            CALL(VDBManagerRelease(mgr));
        }
    }
    cout << "LowLevelTest done" << endl;
    return 0;
}
#endif

int CWGSTestApp::Run(void)
{
#ifdef CALL
    return LowLevelTest();
#endif

    uint64_t error_count = 0;
    const CArgs& args = GetArgs();

    string path = args["file"].AsString();
    bool verbose = args["verbose"];
    bool print_seq = args["print_seq"];
    bool print_entry = args["print_entry"];
    bool print_split = args["print_split"];
    size_t limit_count = 100;
    if ( args["limit_count"] ) {
        limit_count = size_t(args["limit_count"].AsInteger());
    }

    CNcbiOstream& out = args["o"].AsOutputFile();

    CVDBMgr mgr;
    CStopWatch sw;
    
    sw.Restart();

    if ( verbose ) {
        try {
            string acc = CWGSDb_Impl::NormalizePathOrAccession(path);
            string resolved = mgr.FindAccPath(acc);
            out << "Resolved "<<path<<" -> "<<acc<<" -> "<<resolved
                << NcbiEndl;
        }
        catch ( CException& exc ) {
            ERR_POST("FindAccPath failed: "<<exc);
        }
    }

    bool is_component = false, is_scaffold = false, is_protein = false;
    uint64_t row = 0;
    int contig_version = -1;
    if ( args["contig_version"] ) {
        contig_version = args["contig_version"].AsInteger();
    }
    else if ( path.size() > 12 && path.find('/') == NPOS ) {
        SIZE_TYPE dot_pos = path.rfind('.');
        if ( dot_pos != NPOS &&
             (contig_version = NStr::StringToNonNegativeInt(path.substr(dot_pos+1))) >= 0 ) {
            path.resize(dot_pos);
        }
    }
    if ( args["contig_row"] || args["scaffold_row"] || args["protein_row"] ) {
        if ( args["contig_row"] ) {
            row = args["contig_row"].AsInt8();
            is_component = true;
        }
        else if ( args["scaffold_row"] ) {
            row = args["scaffold_row"].AsInt8();
            is_scaffold = true;
        }
        else if ( args["protein_row"] ) {
            row = args["protein_row"].AsInt8();
            is_protein = true;
        }
    }
    else {
        if ( !row && (row = CWGSDb::ParseContigRow(path)) ) {
            is_component = true;
            path = path.substr(0, 6);
        }
        if ( !row && (row = CWGSDb::ParseScaffoldRow(path)) ) {
            is_scaffold = true;
            path = path.substr(0, 6);
        }
        if ( !row && (row = CWGSDb::ParseProteinRow(path)) ) {
            is_protein = true;
            path = path.substr(0, 6);
        }
    }
    if ( row && !args["limit_count"] ) {
        limit_count = 1;
    }

    CWGSDb wgs_db(mgr, path);
    if ( verbose ) {
        out << "Opened WGS in "<<sw.Restart()
            << NcbiEndl;
    }
    if ( wgs_db->GetProjectGBState() ) {
        out << "WGS Project GB State: "<< wgs_db->GetProjectGBState() << NcbiEndl;
    }
    if ( wgs_db->IsReplaced() ) {
        out << "WGS Project is replaced by "<< wgs_db->GetReplacedBy() << NcbiEndl;
    }
    if ( args["master"] ) {
        CWGSDb::EDescrFilter filter = CWGSDb::eDescrDefaultFilter;
        if ( args["master-no-filter"] ) {
            filter = CWGSDb::eDescrNoFilter;
        }
        if ( !wgs_db.LoadMasterDescr(filter) ) {
            ERR_POST("No master descriptors found");
        }
    }
    if ( args["print-master"] ) {
        if ( CRef<CSeq_entry> master = wgs_db->GetMasterSeq_entry() ) {
            out << "Master " << MSerial_AsnText << *master;
        }
        else {
            out << "No master entry" << NcbiEndl;
        }
    }

    vector< CConstRef<CBioseq> > all_seqs;
    bool keep_seqs = args["keep_sequences"];
    
    CWGSSeqIterator::TIncludeFlags include_flags = CWGSSeqIterator::fIncludeLive;
    if ( args["withdrawn"] ) {
        include_flags |= CWGSSeqIterator::fIncludeWithdrawn;
    }
    if ( args["replaced"] ) {
        include_flags |= CWGSSeqIterator::fIncludeReplaced;
    }
    if ( args["suppressed"] ) {
        include_flags |= CWGSSeqIterator::fIncludeSuppressed;
    }
    if ( args["unverified"] ) {
        include_flags |= CWGSSeqIterator::fIncludeUnverified;
    }
    
    if ( 1 ) {
        CWGSSeqIterator it;
        // try accession
        if ( row ) {
            // print only one accession
            if ( is_component ) {
                if ( !args["limit_count"] ) {
                    it = CWGSSeqIterator(wgs_db, row, include_flags);
                }
                else if ( row + limit_count < row ) {
                    it = CWGSSeqIterator(wgs_db, row, kMax_UI8, include_flags);
                }
                else {
                    it = CWGSSeqIterator(wgs_db, row, row+limit_count-1, include_flags);
                }
                if ( !it ) {
                    out << "No such row: "<<path
                        << NcbiEndl;
                }
            }
        }
        else {
            // otherwise scan all sequences
            it = CWGSSeqIterator(wgs_db, include_flags);
        }
        for ( size_t count = 0; it && count < limit_count; ++it, ++count ) {
            if ( contig_version ) {
                it.SelectAccVersion(contig_version);
            }
            out << it.GetAccession()<<'.'<<it.GetAccVersion();
            if ( it.HasGi() ) {
                out << " gi: "<<it.GetGi();
            }
            out << " len: "<<it.GetSeqLength();
            if ( it.GetGBState() ) {
                out << " gbstate: "<<it.GetGBState();
            }
            if ( it.HasSeqHash() ) {
                out << " hash: 0x"<<hex<<it.GetSeqHash()<<dec;
            }
            if ( it.HasPublicComment() ) {
                out << " comment: \""<<it.GetPublicComment()<<"\"";
            }
            out << '\n';
            CRef<CBioseq> seq1 = it.GetBioseq();
            CRef<CBioseq> seq2 = it.GetBioseq(it.fDefaultIds|it.fInst_ncbi4na);
            if ( print_seq ) {
                out << MSerial_AsnText << *seq1;
            }
            if ( print_entry ) {
                out << MSerial_AsnText << *it.GetSeq_entry();
            }
            if ( print_split ) {
                out << MSerial_AsnText << *it.GetSplitInfo();
            }
            string data1 = sx_GetSeqData(*seq1);
            string data2 = sx_GetSeqData(*seq2);
            if ( data1 != data2 ) {
                size_t pos = 0;
                while ( data1[pos] == data2[pos] ) {
                    ++pos;
                }
                ERR_POST(Fatal<<"Different Seq-data at " << pos << ": " <<
                         MSerial_AsnText << *seq1 << MSerial_AsnText << *seq2);
            }
            if ( keep_seqs ) {
                all_seqs.push_back(seq1);
            }
        }
    }

    /*
      if ( args["resolve-gi"] ) {
      vector<string> gis_str;
      NStr::Split(args["resolve-gi"].AsString(), ",", gis_str);
      ITERATE ( vector<string>, it, gis_str ) {
      TGi gi = NStr::StringToNumeric<TIntId>(*it);
            
      }
      }
    */

    if ( args["gi-check"] ) {
        typedef map<TGi, uint64_t> TGiIdx;
        TGiIdx nuc_idx;
        for ( CWGSGiIterator it(wgs_db, CWGSGiIterator::eNuc); it; ++it ) {
            nuc_idx[it.GetGi()] = it.GetRowId();
        }
        for ( CWGSSeqIterator it(wgs_db, include_flags); it; ++it ) {
            if ( !it.HasGi() ) {
                continue;
            }
            TGi gi = it.GetGi();
            uint64_t row = it.GetCurrentRowId();
            TGiIdx::iterator idx_it = nuc_idx.find(gi);
            if ( idx_it == nuc_idx.end() ) {
                ERR_POST("GI "<<gi<<" row="<<row<<" idx=none");
                ++error_count;
            }
            else {
                if ( idx_it->second != row ) {
                    ERR_POST("GI "<<gi<<" row="<<row<<" idx="<<idx_it->second);
                    ++error_count;
                }
                nuc_idx.erase(idx_it);
            }
        }
        ITERATE ( TGiIdx, it, nuc_idx ) {
            ERR_POST("GI "<<it->first<<" row=none"<<" idx="<<it->second);
            ++error_count;
        }
    }
    if ( args["gi-range"] ) {
        pair<TGi, TGi> gi_range = wgs_db.GetNucGiRange();
        if ( gi_range.second != ZERO_GI ) {
            out << "Nucleotide GI range: "
                << gi_range.first << " - " << gi_range.second
                << NcbiEndl;
        }
        else {
            out << "Nucleotide GI range is empty" << NcbiEndl;
        }
        gi_range = wgs_db.GetProtGiRange();
        if ( gi_range.second != ZERO_GI ) {
            out << "Protein GI range: "
                << gi_range.first << " - " << gi_range.second
                << NcbiEndl;
        }
        else {
            out << "Protein GI range is empty" << NcbiEndl;
        }
    }
    bool check_non_empty_lookup = args["check_non_empty_lookup"];
    bool check_empty_lookup = args["check_empty_lookup"];
    if ( args["gi"] ) {
        TGi gi = GI_FROM(TIntId, args["gi"].AsIntId());
        CRef<CWGSResolver> resolver = CWGSResolver_VDB::CreateResolver(mgr);
        if ( resolver ) {
            CWGSResolver::TWGSPrefixes prefixes = resolver->GetPrefixes(gi);
            if ( prefixes.empty() ) {
                out << "No WGS accessions with gi "<<gi<<NcbiEndl;
            }
            else {
                ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
                    out << "GI "<<gi<<" is found in WGS " << *it << NcbiEndl;
                }
            }
        }
        CWGSGiIterator gi_it(wgs_db, gi);
        if ( !gi_it ) {
            out << "GI "<<gi<<" not found" << NcbiEndl;
            if ( check_non_empty_lookup ) {
                ++error_count;
            }
        }
        else if ( gi_it.GetSeqType() == gi_it.eNuc ) {
            out << "GI "<<gi<<" Nucleotide row: "<<gi_it.GetRowId()
                << NcbiEndl;
            if ( check_empty_lookup ) {
                ++error_count;
            }
            CWGSSeqIterator it(wgs_db, gi_it.GetRowId(), include_flags);
            if ( !it ) {
                out << "No such row: "<< gi_it.GetRowId() << NcbiEndl;
                ++error_count;
            }
            else {
                out << "GI "<<gi<<" len: "<<it.GetSeqLength() << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
                if ( print_entry ) {
                    out << MSerial_AsnText << *it.GetSeq_entry();
                }
            }
        }
        else {
            out << "GI "<<gi<<" Protein row: "<<gi_it.GetRowId() << NcbiEndl;
            if ( check_empty_lookup ) {
                ++error_count;
            }
            CWGSProteinIterator it(wgs_db, gi_it.GetRowId());
            if ( !it ) {
                out << "No such row: "<< gi_it.GetRowId() << NcbiEndl;
                ++error_count;
            }
            else {
                out << "GI "<<gi<<" len: "<<it.GetSeqLength();
                if ( it.HasPublicComment() ) {
                    out << " comment: \""<<it.GetPublicComment()<<"\"";
                }
                out << NcbiEndl;
                
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
                if ( print_entry ) {
                    out << MSerial_AsnText << *it.GetSeq_entry();
                }
            }
        }
    }
    if ( args["contig_name"] ) {
        string name = args["contig_name"].AsString();
        uint64_t row_id = wgs_db.GetContigNameRowId(name);
        out << "Contig name "<<name<<" is in CONTIG table row " << row_id
            << NcbiEndl;
        if ( !row_id ) {
            if ( check_non_empty_lookup ) {
                ++error_count;
            }
        }
        else {
            if ( check_empty_lookup ) {
                ++error_count;
            }
            CWGSSeqIterator it(wgs_db, row_id, include_flags);
            if ( !it ) {
                out << "CONTIG: No such row: "<< row_id << NcbiEndl;
                ++error_count;
            }
            else {
                out << "CONTIG["<<row_id<<"] len: "<<it.GetSeqLength()
                    << " name: " << it.GetContigName()
                    << NcbiEndl;
                if ( !NStr::EqualNocase(it.GetContigName(), name) ) {
                    out << "Name is different!" << NcbiEndl;
                    ++error_count;
                }
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
                if ( print_entry ) {
                    out << MSerial_AsnText << *it.GetSeq_entry();
                }
            }
        }
    }
    if ( args["scaffold_name"] ) {
        string name = args["scaffold_name"].AsString();
        uint64_t row_id = wgs_db.GetScaffoldNameRowId(name);
        out << "Scaffold name "<<name<<" is in SCAFFOLD table row " << row_id
            << NcbiEndl;
        if ( !row_id ) {
            if ( check_non_empty_lookup ) {
                ++error_count;
            }
        }
        else {
            if ( check_empty_lookup ) {
                ++error_count;
            }
            CWGSScaffoldIterator it(wgs_db, row_id);
            if ( !it ) {
                out << "SCAFFOLD: No such row: "<< row_id << NcbiEndl;
                ++error_count;
            }
            else {
                out << "SCAFFOLD["<<row_id<<"] len: "<<it.GetSeqLength()
                    << " name: " << it.GetScaffoldName()
                    << NcbiEndl;
                if ( !NStr::EqualNocase(it.GetScaffoldName(), name) ) {
                    out << "Name is different!" << NcbiEndl;
                    ++error_count;
                }
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
                if ( print_entry ) {
                    out << MSerial_AsnText << *it.GetSeq_entry();
                }
                if ( keep_seqs ) {
                    all_seqs.push_back(it.GetBioseq());
                }
            }
        }
    }
    if ( args["protein_name"] ) {
        string name = args["protein_name"].AsString();
        uint64_t row_id = wgs_db.GetProteinNameRowId(name);
        out << "Protein name "<<name<<" is in PROTEIN table row " << row_id
            << NcbiEndl;
        if ( !row_id ) {
            if ( check_non_empty_lookup ) {
                ++error_count;
            }
        }
        else {
            if ( check_empty_lookup ) {
                ++error_count;
            }
            CWGSProteinIterator it(wgs_db, row_id);
            if ( !it ) {
                out << "PROTEIN: No such row: "<< row_id << NcbiEndl;
                ++error_count;
            }
            else {
                out << "PROTEIN["<<row_id<<"] len: "<<it.GetSeqLength()
                    << " name: " << it.GetProteinName()
                    << NcbiEndl;
                if ( !NStr::EqualNocase(it.GetProteinName(), name) ) {
                    out << "Name is different!" << NcbiEndl;
                    ++error_count;
                }
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
                if ( print_entry ) {
                    out << MSerial_AsnText << *it.GetSeq_entry();
                }
            }
        }
    }
    if ( args["protein_acc"] ) {
        string param = args["protein_acc"].AsString();
        unsigned random_count = 0;
        try {
            random_count = NStr::StringToNumeric<unsigned>(param);
        }
        catch ( CStringException& /*ignored*/ ) {
        }
        vector<string> accs;
        if ( random_count ) {
            unsigned seq_count = args["seq_acc_count"].AsInteger();
            CRandom r(args["seed"].AsInteger());
            for ( unsigned i = 0; i < random_count; ++i ) {
                string prefix;
                for ( int j = 0; j < 3; ++j ) {
                    prefix += char(r.GetRand('A', 'Z'));
                }
                unsigned start = r.GetRand(0, 99999);
                unsigned count = min(min(100000-start, random_count-i), r.GetRand(1, seq_count));
                for ( unsigned j = 0; j < count; ++j ) {
                    string s = NStr::IntToString(start+j);
                    s = string(5-s.size(), '0') + s;
                    accs.push_back(prefix+s);
                }
                i += count-1;
            }
        }
        else {
            NStr::Split(param, ",", accs);
        }
        unsigned found_count = 0;
        CStopWatch sw(CStopWatch::eStart);
        CRef<CWGSResolver> resolver = CWGSResolver_VDB::CreateResolver(mgr);
        if ( resolver ) {
            for ( auto& acc_ver : accs ) {
                string acc, ver;
                NStr::SplitInTwo(acc_ver, ".", acc, ver);
                CWGSResolver::TWGSPrefixes prefixes = resolver->GetPrefixes(acc);
                if ( random_count ) {
                    found_count += (prefixes.size()!=0);
                }
                else if ( prefixes.empty() ) {
                    out << "No WGS accessions with protein acc "<<acc<<NcbiEndl;
                }
                else {
                    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
                        out << "Protein acc "<<acc<<" is found in WGS " << *it << NcbiEndl;
                    }
                }
            }
        }
        out << "Resolved "<<accs.size()<<" WGS accessions in " << sw.Restart() << "s" << endl;
        if ( random_count ) {
            out << "Found valid WGS accessions: "<<found_count<<endl;
        }
        if ( !random_count ) {
            for ( auto& acc_ver : accs ) {
                string acc, ver;
                NStr::SplitInTwo(acc_ver, ".", acc, ver);
                int version = ver.empty()? -1: NStr::StringToNumeric<int>(ver);
                uint64_t row_id = wgs_db.GetProtAccRowId(acc, version);
                out << "Protein acc "<<acc_ver<<" is in PROTEIN table row " << row_id
                    << NcbiEndl;
                if ( !row_id ) {
                    if ( check_non_empty_lookup ) {
                        ++error_count;
                    }
                }
                else {
                    if ( check_empty_lookup ) {
                        ++error_count;
                    }
                    CWGSProteinIterator it(wgs_db, row_id);
                    if ( !it ) {
                        out << "PROTEIN: No such row: "<< row_id << NcbiEndl;
                        ++error_count;
                    }
                    else {
                        out << "PROTEIN["<<row_id<<"] len: "<<it.GetSeqLength()
                            << " acc: " << it.GetAccession()<<"."<<it.GetAccVersion()
                            << NcbiEndl;
                        if ( !NStr::EqualNocase(it.GetAccession(), acc) ) {
                            out << "Accession is different!" << NcbiEndl;
                            ++error_count;
                        }
                        if ( version != -1 && it.GetAccVersion() != version ) {
                            out << "Version is different!" << NcbiEndl;
                            ++error_count;
                        }
                        if ( print_seq ) {
                            out << MSerial_AsnText << *it.GetBioseq();
                        }
                        if ( print_entry ) {
                            out << MSerial_AsnText << *it.GetSeq_entry();
                        }
                    }
                }
            }
            out << "Found "<<accs.size()<<" WGS accessions in " << sw.Restart() << "s" << endl;
        }
    }

    if ( 1 ) {
        CWGSScaffoldIterator it;
        if ( row ) {
            if ( is_scaffold ) {
                it = CWGSScaffoldIterator(wgs_db, row);
                if ( !it ) {
                    out << "No such scaffold row: "<<path
                        << NcbiEndl;
                }
            }
        }
        else {
            it = CWGSScaffoldIterator(wgs_db);
        }
        for ( size_t count = 0; it && count < limit_count; ++it, ++count ) {
            out << it.GetScaffoldName() << '\n';
            CRef<CBioseq> seq = it.GetBioseq();
            if ( print_seq ) {
                out << MSerial_AsnText << *seq;
            }
            if ( print_entry ) {
                out << MSerial_AsnText << *it.GetSeq_entry();
            }
        }
    }

    if ( 1 ) {
        CWGSProteinIterator it;
        if ( row ) {
            if ( is_protein ) {
                it = CWGSProteinIterator(wgs_db, row);
                if ( !it ) {
                    out << "No such protein row: "<<path
                        << NcbiEndl;
                }
            }
        }
        else {
            it = CWGSProteinIterator(wgs_db);
        }
        for ( size_t count = 0; it && count < limit_count; ++it, ++count ) {
            out << it.GetProteinName() << '\n';
            CRef<CBioseq> seq = it.GetBioseq();
            if ( print_seq ) {
                out << MSerial_AsnText << *seq;
            }
            if ( print_entry ) {
                out << MSerial_AsnText << *it.GetSeq_entry();
            }
        }
    }

    if ( error_count ) {
        out << "Failure. Error count: "<<error_count<< NcbiEndl;
        return 1;
    }
    else {
        out << "Success." << NcbiEndl;
        return 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CWGSTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CWGSTestApp().AppMain(argc, argv);
}
