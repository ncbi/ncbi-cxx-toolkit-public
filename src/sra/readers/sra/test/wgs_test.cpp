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

    arg_desc->AddOptionalKey("limit_count", "LimitCount",
                             "Number of entries to read (-1 : unlimited)",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("verbose", "Print info about found data");

    arg_desc->AddFlag("withdrawn", "Include withdrawn sequences");
    arg_desc->AddFlag("master", "Include master descriptors if any");
    arg_desc->AddFlag("master-no-filter", "Do not filter master descriptors");

    arg_desc->AddFlag("gi-check", "Check GI index");
    arg_desc->AddFlag("gi-range", "Print GI range if any");
    arg_desc->AddFlag("gi-ranges", "Print GI ranges");
    arg_desc->AddFlag("acc-ranges", "Print protein accession ranges");
    arg_desc->AddFlag("make-all-ranges", "Scan all WGS files and make two files with GI/acc ranges");
    arg_desc->AddDefaultKey("max-gap", "MaxGap",
                            "max gap in a single gi range",
                            CArgDescriptions::eInteger, "100000");

    arg_desc->AddFlag("print_seq", "Print loader Bioseq objects");

    arg_desc->AddOptionalKey("contig_row", "ContigRow",
                             "contig row to fetch",
                             CArgDescriptions::eInt8);
    arg_desc->AddOptionalKey("scaffold_row", "ScaffoldRow",
                             "scaffold row to fetch",
                             CArgDescriptions::eInt8);
    arg_desc->AddOptionalKey("protein_row", "ProteinRow",
                             "protein row to fetch",
                             CArgDescriptions::eInt8);

    arg_desc->AddFlag("check_non_empty_lookup",
                      "Check that lookup produce non-empty result");
    arg_desc->AddOptionalKey("gi", "GI",
                             "lookup by GI",
                             CArgDescriptions::eInt8);
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

static const size_t kMinAccessionLength = 6;
static const size_t kMaxAccessionLength = 8;
typedef pair<TIntId, TIntId> TGiRange;
typedef pair<string, string> TAccRange;
typedef vector<TGiRange> TGiRanges;
typedef vector<TAccRange> TAccRanges;
typedef map<string, TGiRanges> TGiRangeIndex;
typedef map<string, TAccRanges> TAccRangeIndex;

static TGiRanges PackRanges(const CWGSDb::TGiRanges& src, TIntId max_gap)
{
    TGiRanges dst;
    ITERATE ( CWGSDb::TGiRanges, it, src ) {
        TIntId gi_from = it->GetFrom();
        TIntId gi_to = it->GetTo();
        if ( !dst.empty() && gi_from <= dst.back().second + 1 + max_gap ) {
            dst.back().second = gi_to;
        }
        else {
            dst.push_back(TGiRange(gi_from, gi_to));
        }
    }
    return dst;
}

static TGiRanges GetNucGiRanges(CWGSDb wgs_db, TIntId max_gap)
{
    pair<TIntId, TIntId> range = wgs_db.GetNucGiRange();
    if ( !range.second ) {
        return TGiRanges();
    }

    size_t gi_count = range.second - range.first + 1;
    size_t seq_count = CWGSSeqIterator(wgs_db).GetLastRowId();
    if ( gi_count <= seq_count + max_gap ) {
        return TGiRanges(1, range);
    }
    
    return PackRanges(wgs_db.GetNucGiRanges(), max_gap);
}

static TGiRanges GetProtGiRanges(CWGSDb wgs_db, TIntId max_gap)
{
    pair<TIntId, TIntId> range = wgs_db.GetProtGiRange();
    if ( !range.second ) {
        return TGiRanges();
    }

    size_t gi_count = range.second - range.first + 1;
    size_t seq_count = CWGSProteinIterator(wgs_db).GetLastRowId();
    if ( gi_count <= seq_count + max_gap ) {
        return TGiRanges(1, range);
    }
    
    return PackRanges(wgs_db.GetProtGiRanges(), max_gap);
}

static TAccRanges GetProtAccRanges(CWGSDb wgs_db)
{
    TAccRanges ret;
    TAccRange range;
    CWGSDb::TAccRanges ranges = wgs_db.GetProtAccRanges();
    ITERATE ( CWGSDb::TAccRanges, it, ranges ) {
        CWGSProtAccResolver::SAccInfo info(it->first);
        info.m_Id = it->second.GetFrom();
        range.first = info.GetAcc();
        info.m_Id = it->second.GetTo();
        range.second = info.GetAcc();
        ret.push_back(range);
    }
    return ret;
}

static bool LoadGiIndex(TGiRangeIndex& index,
                        CTime& time,
                        const string& file_name)
{
    CNcbiIfstream in(file_name.c_str());
    if ( !in ) {
        // allow missing file
        time = CTime(CTime::eCurrent);
        return false;
    }
    if ( !CDirEntry(file_name).GetTime(&time) ) {
        time = CTime(CTime::eCurrent);
    }

    int line = 0;
    string wgs_acc;
    TGiRange range;
    char eol[128];
    for ( ;; ) {
        ++line;
        if ( !(in >> wgs_acc) ) {
            if ( in.eof() ) {
                // end of data
                return true;
            }
        }
        size_t length = wgs_acc.size();
        if ( length < kMinAccessionLength || length > kMaxAccessionLength ) {
            break; // error
        }
        in >> range.first >> range.second;
        if ( !in ) {
            break; // error
        }
        if ( !in.getline(eol, sizeof(eol)) ) {
            // incomplete line
            break; // error
        }
        if ( TIntId(range.first) <= 0 ||
             TIntId(range.first) > TIntId(range.second) ) {
            break; // error
        }
        index[wgs_acc].push_back(range);
    }
    ERR_POST("LoadGiIndex: bad index file format: "<<file_name<<":"<<line);
    return false;
}

static bool LoadAccIndex(TAccRangeIndex& index,
                         CTime& time,
                         const string& file_name)
{
    CNcbiIfstream in(file_name.c_str());
    if ( !in ) {
        // allow missing file
        time = CTime(CTime::eCurrent);
        return false;
    }
    if ( !CDirEntry(file_name).GetTime(&time) ) {
        time = CTime(CTime::eCurrent);
    }

    int line = 0;
    string wgs_acc;
    TAccRange range;
    char eol[128];
    for ( ;; ) {
        ++line;
        if ( !(in >> wgs_acc) ) {
            if ( in.eof() ) {
                // end of data
                return true;
            }
        }
        size_t length = wgs_acc.size();
        if ( length < kMinAccessionLength || length > kMaxAccessionLength ) {
            break; // error
        }
        in >> range.first >> range.second;
        if ( !in ) {
            break; // error
        }
        if ( !in.getline(eol, sizeof(eol)) ) {
            // incomplete line
            break; // error
        }
        if ( range.first > range.second ) {
            break; // error
        }
        index[wgs_acc].push_back(range);
    }
    ERR_POST("LoadAccIndex: bad index file format: "<<file_name<<":"<<line);
    return false;
}

static void SaveGiIndex(const TGiRangeIndex& index, const string& file_name)
{
    CNcbiOfstream out(file_name.c_str());
    ITERATE ( TGiRangeIndex, it, index ) {
        ITERATE ( TGiRanges, it2, it->second ) {
            out << it->first << ' ' << it2->first << ' ' << it2->second
                << '\n';
        }
    }
}

static void SaveAccIndex(const TAccRangeIndex& index, const string& file_name)
{
    CNcbiOfstream out(file_name.c_str());
    ITERATE ( TAccRangeIndex, it, index ) {
        ITERATE ( TAccRanges, it2, it->second ) {
            out << it->first << ' ' << it2->first << ' ' << it2->second
                << '\n';
        }
    }
}

static void MakeAllRanges(TIntId max_gap)
{
    CStopWatch sw;

    const string gi_file_name = "list.wgs_gi_ranges";
    const string acc_file_name = "list.wgs_acc_ranges";

    const string wgs_root = CDirEntry::MakePath(NCBI_TRACES04_PATH, "wgs01");
    const string vdb_root = CDirEntry::MakePath(wgs_root, "WGS");
    const string idx_dir = CDirEntry::MakePath(wgs_root, "wgs_aux");

    map<string, TGiRanges> gi_index;
    map<string, TAccRanges> acc_index;

    CTime gi_time, acc_time;
    // load old index files
    LoadGiIndex(gi_index, gi_time,
                CDirEntry::MakePath(idx_dir, gi_file_name));
    LoadAccIndex(acc_index, acc_time,
                 CDirEntry::MakePath(idx_dir, acc_file_name));

    // make the threshold modification time from files
    CTime rescan_time = gi_time < acc_time? gi_time: acc_time;
    rescan_time += CTimeSpan(-1, 0, 0, 0); // one day overlap

    // collecting VDB directories
    typedef pair<CDir, CTime> TVDBDir;
    typedef map<string, TVDBDir> TVDBDirs;
    TVDBDirs vdb_dirs;
    sw.Restart();
    {
        CDir dir0(vdb_root);
        NcbiCout << "Scanning "<<dir0.GetPath()<<NcbiEndl;
        CDir::TEntries dirs1 = dir0.GetEntries("??");
        ITERATE ( CDir::TEntries, it1, dirs1 ) {
            CDir dir1(**it1);
            if ( !isupper(Uint1(dir1.GetName()[0])) ||
                 !isupper(Uint1(dir1.GetName()[1])) ) {
                continue;
            }
            NcbiCout << "Scanning "<<dir1.GetPath()<<NcbiEndl;
            CDir::TEntries dirs2 = dir1.GetEntries("??");
            ITERATE ( CDir::TEntries, it2, dirs2 ) {
                CDir dir2(**it2);
                if ( !isupper(Uint1(dir2.GetName()[0])) ||
                     !isupper(Uint1(dir2.GetName()[1])) ) {
                    continue;
                }
                string prefix4 = dir1.GetName()+dir2.GetName();
                TVDBDir& slot = vdb_dirs[prefix4];
                slot.first = dir2;
                dir2.GetTime(&slot.second);
            }
        }
    }
    NcbiCout << "Found " << vdb_dirs.size() << " VDB directories in "
             << fixed << setprecision(3) << sw.Elapsed() << "s" << NcbiEndl;

    // collecting VDB directories
    typedef pair<CDirEntry, CTime> TVDBFile;
    typedef map<string, TVDBFile> TVDBFiles;
    TVDBFiles vdb_files;
    sw.Restart();
    size_t dir_count = 0;
    {
        ITERATE ( TVDBDirs, it, vdb_dirs ) {
            if ( it->second.second < rescan_time ) {
                continue;
            }
            ++dir_count;
            const string& prefix4 = it->first;
            const CDir dir2 = it->second.first;
            //NcbiCout << "Scanning "<<dir2.GetPath()<<NcbiEndl;
            CDir::TEntries files = dir2.GetEntries(prefix4+"??");
            ITERATE ( CDir::TEntries, it, files ) {
                const CDirEntry& file = **it;
                if ( !isdigit(Uint1(file.GetName()[4])) ||
                     !isdigit(Uint1(file.GetName()[5])) ) {
                    continue;
                }
                string wgs_acc = file.GetName();
                TVDBFile& slot = vdb_files[wgs_acc];
                slot.first = file;
                file.GetTime(&slot.second);
            }
        }
    }
    NcbiCout << "Found " << vdb_files.size() << " recent VDB files in "
             << fixed << setprecision(3) << sw.Elapsed() << "s" << NcbiEndl;

    // remove obsolete entries
    ERASE_ITERATE ( TGiRangeIndex, it, gi_index ) {
        const string& wgs_acc = it->first;
        string prefix4 = wgs_acc.substr(0, 4);
        TVDBDirs::const_iterator dir_it = vdb_dirs.find(prefix4);
        if ( dir_it == vdb_dirs.end() ) {
            NcbiCout << "Erasing old gi VDB dir: " << it->first
                     << NcbiEndl;
            gi_index.erase(it);
            continue;
        }
        if ( dir_it->second.second < rescan_time ) {
            // old directory
            continue;
        }
        TVDBFiles::const_iterator file_it = vdb_files.find(wgs_acc);
        if ( file_it == vdb_files.end() ) {
            NcbiCout << "Erasing old gi VDB file: " << it->first
                     << NcbiEndl;
            gi_index.erase(it);
            continue;
        }
    }
    ERASE_ITERATE ( TAccRangeIndex, it, acc_index ) {
        const string& wgs_acc = it->first;
        string prefix4 = wgs_acc.substr(0, 4);
        TVDBDirs::const_iterator dir_it = vdb_dirs.find(prefix4);
        if ( dir_it == vdb_dirs.end() ) {
            NcbiCout << "Erasing old prot acc VDB dir: " << it->first
                     << NcbiEndl;
            acc_index.erase(it);
            continue;
        }
        if ( dir_it->second.second < rescan_time ) {
            // old directory
            continue;
        }
        TVDBFiles::const_iterator file_it = vdb_files.find(wgs_acc);
        if ( file_it == vdb_files.end() ) {
            NcbiCout << "Erasing old prot acc VDB file: " << it->first
                     << NcbiEndl;
            acc_index.erase(it);
            continue;
        }
    }

    // update index for modified VDB files
    CVDBMgr mgr;
    ITERATE ( TVDBFiles, it, vdb_files ) {
        if ( it->second.second < rescan_time ) {
            continue;
        }
        const string& wgs_acc = it->first;
        const CDirEntry& file = it->second.first;
        NcbiCout << "Scanning "<<file.GetPath()<<NcbiEndl;
        gi_index.erase(wgs_acc);
        acc_index.erase(wgs_acc);
        try {
            CWGSDb wgs_db(mgr, file.GetPath());
            {
                TGiRanges ranges = GetNucGiRanges(wgs_db, max_gap);
                TGiRanges ranges2 = GetProtGiRanges(wgs_db, max_gap);
                ranges.insert(ranges.end(),
                              ranges2.begin(),
                              ranges2.end());
                if ( !ranges.empty() ) {
                    gi_index[wgs_acc] = ranges;
                }
            }
            {
                TAccRanges ranges = GetProtAccRanges(wgs_db);
                if ( !ranges.empty() ) {
                    acc_index[wgs_acc] = ranges;
                }
            }
        }
        catch ( CException& exc ) {
            ERR_POST("Exception while processing "<<file.GetPath()
                     <<": "<<exc);
        }
    }

    SaveGiIndex(gi_index, gi_file_name);
    SaveAccIndex(acc_index, acc_file_name);
}


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
    size_t limit_count = 100;
    if ( args["limit_count"] ) {
        limit_count = size_t(args["limit_count"].AsInteger());
    }

    CNcbiOstream& out = cout;

    if ( args["make-all-ranges"] ) {
        MakeAllRanges(args["max-gap"].AsInteger());
        return 0;
    }

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
    if ( args["master"] ) {
        CWGSDb::EDescrFilter filter = CWGSDb::eDescrDefaultFilter;
        if ( args["master-no-filter"] ) {
            filter = CWGSDb::eDescrNoFilter;
        }
        if ( !wgs_db.LoadMasterDescr(filter) ) {
            ERR_POST("No master descriptors found");
        }
    }

    bool is_component = false, is_scaffold = false, is_protein = false;
    uint64_t row = 0;
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
        row = wgs_db.ParseContigRow(path);
        if ( row ) {
            is_component = true;
        }
        if ( !row ) {
            if ( (row = wgs_db.ParseScaffoldRow(path)) ) {
                is_scaffold = true;
            }
        }
        if ( !row ) {
            if ( (row = wgs_db.ParseProteinRow(path)) ) {
                is_protein = true;
            }
        }
    }
    if ( row && !args["limit_count"] ) {
        limit_count = 1;
    }

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
            if ( is_component ) {
                if ( !args["limit_count"] ) {
                    it = CWGSSeqIterator(wgs_db, row, withdrawn);
                }
                else if ( row + limit_count < row ) {
                    it = CWGSSeqIterator(wgs_db, row, kMax_UI8, withdrawn);
                }
                else {
                    it = CWGSSeqIterator(wgs_db, row, row+limit_count-1, withdrawn);
                }
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
        for ( size_t count = 0; it && count < limit_count; ++it, ++count ) {
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
        }
    }

    if ( args["gi-check"] ) {
        typedef map<TGi, uint64_t> TGiIdx;
        TGiIdx nuc_idx;
        for ( CWGSGiIterator it(wgs_db, CWGSGiIterator::eNuc); it; ++it ) {
            nuc_idx[it.GetGi()] = it.GetRowId();
        }
        for ( CWGSSeqIterator it(wgs_db); it; ++it ) {
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
        if ( gi_range.second ) {
            out << "Nucleotide GI range: "
                << gi_range.first << " - " << gi_range.second
                << NcbiEndl;
        }
        else {
            out << "Nucleotide GI range is empty" << NcbiEndl;
        }
        gi_range = wgs_db.GetProtGiRange();
        if ( gi_range.second ) {
            out << "Protein GI range: "
                << gi_range.first << " - " << gi_range.second
                << NcbiEndl;
        }
        else {
            out << "Protein GI range is empty" << NcbiEndl;
        }
    }
    if ( args["gi-ranges"] ) {
        CWGSDb::TGiRanges ranges;
        sw.Restart();
        ranges = wgs_db.GetNucGiRanges();
        out << "Got "<<ranges.size()<<" nuc GI ranges in "<<sw.Elapsed()<<"s:"
            << NcbiEndl;
        ITERATE ( CWGSDb::TGiRanges, it, ranges ) {
            out << " nuc GIs: "<<it->GetFrom()<<"-"<<it->GetTo()
                << NcbiEndl;
        }
        sw.Restart();
        ranges = wgs_db.GetProtGiRanges();
        out << "Got "<<ranges.size()<<" prot GI ranges in "<<sw.Elapsed()<<"s:"
            << NcbiEndl;
        ITERATE ( CWGSDb::TGiRanges, it, ranges ) {
            out << " prot GIs: "<<it->GetFrom()<<"-"<<it->GetTo()
                << NcbiEndl;
        }
    }
    if ( args["acc-ranges"] ) {
        sw.Restart();
        CWGSDb::TAccRanges ranges = wgs_db.GetProtAccRanges();
        out << "Got "<<ranges.size()<<" protein accession ranges in "<<sw.Elapsed()<<"s:"
            << NcbiEndl;
        ITERATE ( CWGSDb::TAccRanges, it, ranges ) {
            out << "  Accs: "<<it->first<<": "<<it->second
                << NcbiEndl;
        }
    }

    bool check_non_empty_lookup = args["check_non_empty_lookup"];
    if ( args["gi"] ) {
        TGi gi = TIntId(args["gi"].AsInt8());
        CWGSGiResolver gi_resolver;
        CWGSGiResolver::TAccessionList accs = gi_resolver.FindAll(gi);
        if ( accs.empty() ) {
            out << "No WGS accessions with gi "<<gi<<NcbiEndl;
        }
        else {
            ITERATE ( CWGSGiResolver::TAccessionList, it, accs ) {
                out << "WGS accessions with gi: "<<*it<<NcbiEndl;
            }
        }
        CWGSGiIterator gi_it(wgs_db, gi);
        if ( !gi_it ) {
            out << "GI "<<gi<<" not found" << NcbiEndl;
            if ( check_non_empty_lookup ) {
                return 1;
            }
        }
        else if ( gi_it.GetSeqType() == gi_it.eNuc ) {
            out << "GI "<<gi<<" Nucleotide row: "<<gi_it.GetRowId()
                << NcbiEndl;
            CWGSSeqIterator it(wgs_db, gi_it.GetRowId());
            if ( !it ) {
                out << "No such row: "<< gi_it.GetRowId() << NcbiEndl;
                return 1;
            }
            else {
                out << "GI "<<gi<<" len: "<<it.GetSeqLength() << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
            }
        }
        else {
            out << "GI "<<gi<<" Protein row: "<<gi_it.GetRowId() << NcbiEndl;
            CWGSProteinIterator it(wgs_db, gi_it.GetRowId());
            if ( !it ) {
                out << "No such row: "<< gi_it.GetRowId() << NcbiEndl;
                return 1;
            }
            else {
                out << "GI "<<gi<<" len: "<<it.GetSeqLength() << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
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
                return 1;
            }
        }
        else {
            CWGSSeqIterator it(wgs_db, row_id);
            if ( !it ) {
                out << "CONTIG: No such row: "<< row_id << NcbiEndl;
                return 1;
            }
            else {
                out << "CONTIG["<<row_id<<"] len: "<<it.GetSeqLength()
                    << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
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
                return 1;
            }
        }
        else {
            CWGSScaffoldIterator it(wgs_db, row_id);
            if ( !it ) {
                out << "SCAFFOLD: No such row: "<< row_id << NcbiEndl;
                return 1;
            }
            else {
                out << "SCAFFOLD["<<row_id<<"] len: "<<it.GetSeqLength()
                    << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
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
                return 1;
            }
        }
        else {
            CWGSProteinIterator it(wgs_db, row_id);
            if ( !it ) {
                out << "PROTEIN: No such row: "<< row_id << NcbiEndl;
                return 1;
            }
            else {
                out << "PROTEIN["<<row_id<<"] len: "<<it.GetSeqLength()
                    << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
            }
        }
    }
    if ( args["protein_acc"] ) {
        string name = args["protein_acc"].AsString();
        uint64_t row_id = wgs_db.GetProtAccRowId(name);
        out << "Protein acc "<<name<<" is in PROTEIN table row " << row_id
            << NcbiEndl;
        if ( !row_id ) {
            if ( check_non_empty_lookup ) {
                return 1;
            }
        }
        else {
            CWGSProteinIterator it(wgs_db, row_id);
            if ( !it ) {
                out << "PROTEIN: No such row: "<< row_id << NcbiEndl;
                return 1;
            }
            else {
                out << "PROTEIN["<<row_id<<"] len: "<<it.GetSeqLength()
                    << NcbiEndl;
                if ( print_seq ) {
                    out << MSerial_AsnText << *it.GetBioseq();
                }
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
        for ( size_t count = 0; it && count < limit_count; ++it, ++count ) {
            out << it.GetScaffoldName() << '\n';
            if ( print_seq ) {
                out << MSerial_AsnText << *it.GetBioseq();
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
            if ( print_seq ) {
                out << MSerial_AsnText << *it.GetBioseq();
            }
        }
    }

    out << "Success." << NcbiEndl;
    return error_count? 1: 0;
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
