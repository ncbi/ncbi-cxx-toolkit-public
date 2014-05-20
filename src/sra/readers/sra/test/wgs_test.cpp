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
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

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

    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100");
    arg_desc->AddFlag("verbose", "Print info about found data");

    arg_desc->AddFlag("withdrawn", "Include withdrawn sequences");

    arg_desc->AddFlag("print_seq", "Print loader Bioseq objects");

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
    if ( 1 ) {
        cout << "LowLevelTest for MT cursor read..." << endl;
        const VDBManager* mgr = 0;
        CALL(VDBManagerMakeRead(&mgr, 0));
        
        const VDatabase* db = 0;
        CALL(VDBManagerOpenDBRead(mgr, &db, 0, "GAMP01"));
        
        const VTable* table = 0;
        CALL(VDatabaseOpenTableRead(db, &table, "SEQUENCE"));

        const size_t kNumCursors = 8;
        const size_t kNumReads = 5000;
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
    cout << "LowLevelTest done" << endl;
    return 0;
}
#endif

int CWGSTestApp::Run(void)
{
#ifdef CALL
    return LowLevelTest();
#endif

    const CArgs& args = GetArgs();

    string path = args["file"].AsString();
    bool verbose = args["verbose"];
    bool print_seq = args["print_seq"];
    size_t limit_count = args["limit_count"].AsInteger();

    CNcbiOstream& out = cout;

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

    CWGSDb wgs_db(mgr, path);
    if ( verbose ) {
        out << "Opened WGS in "<<sw.Restart()
            << NcbiEndl;
    }
    
    bool is_scaffold;
    uint64_t row = wgs_db.ParseRow(path, &is_scaffold);

    if ( 1 ) {
        CWGSSeqIterator::EWithdrawn withdrawn;
        if ( args["withdrawn"] ) {
            withdrawn = CWGSSeqIterator::eIncludeWithdrawn;
        }
        else {
            withdrawn = CWGSSeqIterator::eExcludeWithdrawn;
        }

        CWGSSeqIterator it;
        // try accession
        if ( row ) {
            // print only one accession
            limit_count = 1;
            if ( !is_scaffold ) {
                it = CWGSSeqIterator(wgs_db, row, withdrawn);
                if ( !it ) {
                    out << "No such row: "<<path
                        << NcbiEndl;
                }
            }
        }
        else {
            // otherwise scan all sequences
            it = CWGSSeqIterator(wgs_db, withdrawn);
        }
        size_t count = 0;
        for ( ; it; ++it ) {
            out << it.GetAccession()<<'.'<<it.GetAccVersion();
            if ( it.HasGi() ) {
                out << " gi: "<<it.GetGi();
            }
            out << " len: "<<it.GetSeqLength();
            if ( it.GetGBState() ) {
                out << " gbstate: "<<it.GetGBState();
            }
            out << '\n';
            CRef<CBioseq> seq1 = it.GetBioseq();
            CRef<CBioseq> seq2 = it.GetBioseq(it.fDefaultIds|it.fInst_ncbi4na);
            if ( print_seq ) {
                out << MSerial_AsnText << *seq1;
                //out << MSerial_AsnText << *seq2;
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
            if ( ++count >= limit_count ) {
                break;
            }
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
        size_t count = 0;
        for ( ; it; ++it ) {
            out << it.GetScaffoldName() << '\n';
            if ( print_seq ) {
                out << MSerial_AsnText << *it.GetBioseq();
            }
            if ( ++count >= limit_count ) {
                break;
            }
        }
    }

    out << "Success." << NcbiEndl;
    return 0;
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
