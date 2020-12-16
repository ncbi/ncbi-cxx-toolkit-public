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
 *   Application to check span field of cSRA files
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
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

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CCSRATestApp::


class CSpanCheckApp : public CNcbiApplication
{
public:
    CSpanCheckApp();

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    CVDBMgr mgr;

    enum EVerbose {
        eVerbose_none = 0,
        eVerbose_error = 1,
        eVerbose_statistics = 2,
        eVerbose_overlap = 7,
        eVerbose_skip = 8,
        eVerbose_trace = 9
    };
    int verbose;
    
    int total_file_count;
    int single_table_file_count;
    int unaligned_file_count;
    int no_overlap_file_count;
    int bad_schema_file_count;
    int bad_overlap_file_count;
    int good_overlap_file_count;

    struct SDB {
        SDB(const CVDBMgr& mgr, const string& file);

        CVDB db;
        string file;

        bool is_aligned() const
            {
                return aln1.cursor;
            }

        bool has_overlap_info() const
            {
                return ref.col_overlap_pos;
            }
        
        struct SRef {
            SRef() {}
            SRef(const CVDB& db);

            CVDBTable table;
            CVDBCursor cursor;
            
            CVDBColumnBits<sizeof(INSDC_coord_len)*8> col_max_seq_len;
            CVDBColumnBits<sizeof(INSDC_coord_zero)*8> col_overlap_pos;
            CVDBColumnBits<sizeof(INSDC_coord_len)*8> col_overlap_len;
            CVDBColumnBits<8> col_name;
            typedef pair<TVDBRowId, TVDBRowId> TRowRange;
            CVDBColumnBits<sizeof(TRowRange)*8> col_name_range;
            CVDBColumnBits<8> col_seq_id;
            CVDBColumnBits<sizeof(TVDBRowId)*8> col_aln1;
            CVDBColumnBits<sizeof(TVDBRowId)*8> col_aln2;

            TSeqPos GetRowSize() const
                {
                    return *CVDBValueFor<INSDC_coord_len>(cursor, 1, col_max_seq_len);
                }
            
            CVDBStringValue NAME(TVDBRowId row) const
                {
                    return CVDBStringValue(cursor, row, col_name);
                }

            TRowRange NAME_RANGE(CTempString name) const
                {
                    cursor.SetParam("QUERY_SEQ_NAME", name);
                    return *CVDBValueFor<TRowRange>(cursor, 1, col_name_range);
                }

            CVDBStringValue SEQ_ID(TVDBRowId row) const
                {
                    return CVDBStringValue(cursor, row, col_seq_id);
                }

            CVDBValueFor<TVDBRowId> PRIMARY_ALIGNMENT_IDS(TVDBRowId row) const
                {
                    return CVDBValueFor<TVDBRowId>(cursor, row, col_aln1);
                }
            CVDBValueFor<TVDBRowId> SECONDARY_ALIGNMENT_IDS(TVDBRowId row) const
                {
                    return col_aln2? CVDBValueFor<TVDBRowId>(cursor, row, col_aln2): CVDBValueFor<TVDBRowId>();
                }
            CVDBValueFor<TVDBRowId> ALIGNMENT_IDS(int secondary, TVDBRowId row) const
                {
                    return secondary? SECONDARY_ALIGNMENT_IDS(row): PRIMARY_ALIGNMENT_IDS(row);
                }

            CVDBValueFor<INSDC_coord_zero> OVERLAP_REF_POS(TVDBRowId row) const
                {
                    return CVDBValueFor<INSDC_coord_zero>(cursor, row, col_overlap_pos);
                }
            CVDBValueFor<INSDC_coord_len> OVERLAP_REF_LEN(TVDBRowId row) const
                {
                    return CVDBValueFor<INSDC_coord_len>(cursor, row, col_overlap_len);
                }
        };
        SRef ref;

        struct SAln {
            SAln() {}
            SAln(const CVDB& db, const char* name);

            CVDBTable table;
            CVDBCursor cursor;
            
            CVDBColumnBits<sizeof(INSDC_coord_zero)*8> col_pos;
            CVDBColumnBits<sizeof(INSDC_coord_len)*8> col_len;

            INSDC_coord_zero REF_POS(TVDBRowId row) const
                {
                    return *CVDBValueFor<INSDC_coord_zero>(cursor, row, col_pos);
                }
            INSDC_coord_len REF_LEN(TVDBRowId row) const
                {
                    return *CVDBValueFor<INSDC_coord_len>(cursor, row, col_len);
                }
        };

        SAln aln1;
        mutable SAln aln2;

        const SAln& aln(int secondary) const
            {
                if ( secondary ) {
                    if ( !aln2.cursor ) {
                        aln2 = SAln(db, "SECONDARY_ALIGNMENT");
                    }
                    return aln2;
                }
                else {
                    return aln1;
                }
            }
        
        CVDBTable aln1_table;
        CVDBCursor aln1_cursor;
        
        CVDBTable aln2_table;
        CVDBCursor aln2_cursor;
    };
    
    void ProcessFile(const string& name);
    
    void CheckFile(const SDB& db);
    void RecalculateFile(const SDB& db);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

CSpanCheckApp::CSpanCheckApp()
{
    SetVersion(CVersionInfo(1,1,1));
}

void CSpanCheckApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "csra_span_check");

    arg_desc->AddOptionalKey("vol_path", "VolPath",
                             "Search path for volumes",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("file", "File",
                             "cSRA accession or file names",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("files", "Files",
                             "comma separated list of cSRA accessions or files names",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("filelist", "FileList",
                             "file with cSRA accessions or files names",
                             CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("verbose", "Verbose",
                            "level of details to output",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddFlag("recalculate",
                      "Recalculate OVERELAP_REF_POS && OVERLAP_REF_LEN columns");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

int CSpanCheckApp::Run(void)
{
    const CArgs& args = GetArgs();

    verbose = args["verbose"].AsInteger();
    total_file_count = 0;
    single_table_file_count = 0;
    unaligned_file_count = 0;
    no_overlap_file_count = 0;
    bad_schema_file_count = 0;
    bad_overlap_file_count = 0;
    good_overlap_file_count = 0;
    
    vector<string> files;
    if ( args["file"] ) {
        files.push_back(args["file"].AsString());
    }
    else if ( args["files"] ) {
        NStr::Split(args["files"].AsString(), ",", files);
    }
    else if ( args["filelist"] ) {
        CNcbiIstream& in = args["filelist"].AsInputFile();
        string file;
        while ( getline(in, file) ) {
            files.push_back(file);
        }
    }

    for ( auto& file : files ) {
        ProcessFile(file);
    }

    if ( verbose >= eVerbose_statistics ) {
        cout << "Processed "<<total_file_count<<" files.";
        if ( single_table_file_count ) {
            cout << " Single-table: " << single_table_file_count <<".";
        }
        if ( unaligned_file_count ) {
            cout << " Unaligned: " << unaligned_file_count <<".";
        }
        if ( no_overlap_file_count ) {
            cout << " No overlap: " << no_overlap_file_count <<".";
        }
        if ( bad_schema_file_count ) {
            cout << " Bad schema: " << bad_schema_file_count<<".";
        }
        if ( bad_overlap_file_count ) {
            cout << " Bad overlap: "<<bad_overlap_file_count<<".";
        }
        if ( 1 || good_overlap_file_count ) {
            cout << " Good overlap: "<<good_overlap_file_count<<".";
        }
        cout << endl;
    }
    if ( verbose >= eVerbose_trace ) {
        cout << "Success." << endl;
    }
    return (bad_schema_file_count || bad_overlap_file_count? 1: 0);
}


CSpanCheckApp::SDB::SDB(const CVDBMgr& mgr, const string& file)
    : file(file)
{
    // open VDB
    try {
        db = CVDB(mgr, file);
    }
    catch ( CSraException& exc ) {
        // We want to open old SRA schema files too.
        // In this case the DB object is not found, and the error is eDataError
        // any other error cannot allow to work with this accession
        if ( exc.GetErrCode() != CSraException::eDataError ) {
            // report all other exceptions as is
            throw;
        }
        // there must be a single short read table
        CVDBTable seq_table(mgr, file);
        return;
    }
    
    // open tables
    ref = SRef(db);

    try {
        aln1 = SAln(db, "PRIMARY_ALIGNMENT");
    }
    catch ( CSraException& exc ) {
        // check for unaligned run
        if ( exc.GetErrCode() != CSraException::eNotFoundTable ) {
            // report all other exceptions as is
            throw;
        }
        return;
    }
}


CSpanCheckApp::SDB::SRef::SRef(const CVDB& db)
{
    try {
        table = CVDBTable(db, "REFERENCE");
    }
    catch ( CSraException& exc ) {
        // allow missing REFERENCE table
        // any error except eNotFoundTable is not good
        if ( exc.GetErrCode() != CSraException::eNotFoundTable ) {
            throw;
        }
        return;
    }
    cursor = CVDBCursor(table);

    col_max_seq_len = CVDBColumnBits<sizeof(INSDC_coord_len)*8>(cursor, "MAX_SEQ_LEN");
    col_overlap_pos = CVDBColumnBits<sizeof(INSDC_coord_zero)*8>(cursor, "OVERLAP_REF_POS", 0, CVDBColumn::eMissing_Allow);
    if ( !col_overlap_pos ) {
        return;
    }
    col_overlap_len = CVDBColumnBits<sizeof(INSDC_coord_len)*8>(cursor, "OVERLAP_REF_LEN");
    col_name = CVDBColumnBits<8>(cursor, "NAME");
    col_name_range = CVDBColumnBits<sizeof(TRowRange)*8>(cursor, "NAME_RANGE");
    col_seq_id = CVDBColumnBits<8>(cursor, "SEQ_ID");
    col_aln1 = CVDBColumnBits<sizeof(TVDBRowId)*8>(cursor, "PRIMARY_ALIGNMENT_IDS");
    col_aln2 = CVDBColumnBits<sizeof(TVDBRowId)*8>(cursor, "SECONDARY_ALIGNMENT_IDS", 0, CVDBColumn::eMissing_Allow);
}


CSpanCheckApp::SDB::SAln::SAln(const CVDB& db, const char* name)
    : table(db, name),
      cursor(table),
      col_pos(cursor, "REF_POS"),
      col_len(cursor, "REF_LEN")
{
}

void CSpanCheckApp::ProcessFile(const string& file)
{
    if ( verbose >= eVerbose_trace ) {
        string path = mgr.FindAccPath(file);
        cout << file << " -> "<<path<<endl;
    }
    ++total_file_count;

    try {
        SDB db(mgr, file);
        if ( !db.db ) {
            // old single-table VDB file
            if ( verbose >= eVerbose_skip ) {
                cout << file << ": skipping old single-table run"<<endl;
            }
            ++single_table_file_count;
            return;
        }
        if ( !db.is_aligned() ) {
            // we don't scan it as there are no alignments
            if ( verbose >= eVerbose_skip ) {
                cout << file << ": skipping unaligned run"<<endl;
            }
            ++unaligned_file_count;
            return;
        }
        if ( !db.has_overlap_info() ) {
            // we don't scan runs with default overlap
            if ( verbose >= eVerbose_skip ) {
                cout << file << ": skipping run with default overlap"<<endl;
            }
            ++no_overlap_file_count;
            return;
        }
        
        // now scan
        if ( GetArgs()["recalculate"] ) {
            if ( verbose >= eVerbose_trace ) {
                cout << file << ": recalculating..."<<endl;
            }
            RecalculateFile(db);
        }
        else {
            if ( verbose >= eVerbose_trace ) {
                cout << file << ": scanning..."<<endl;
            }
            CheckFile(db);
        }
    }
    catch ( exception& exc ) {
        if ( verbose >= eVerbose_error ) {
            cout << file << ": exception: "<<exc.what()<<endl;
        }
        ++bad_schema_file_count;
    }
}


void CSpanCheckApp::CheckFile(const SDB& db)
{
    const TSeqPos kRefPageSize = db.ref.GetRowSize();
    bool bad_overlap = false;
    auto name_row_range = db.ref.col_name.GetRowIdRange(db.ref.cursor);
    TVDBRowId end_ref_row = name_row_range.first + name_row_range.second;
    for ( TVDBRowId ref_row = 1; ref_row < end_ref_row; ) {
        // read range and names
        CTempString ref_name = db.ref.NAME(ref_row);
        //cout << "ref "<<ref_name<<" @ "<<ref_row<<endl;
        
        auto ref_row_range = db.ref.NAME_RANGE(ref_name);

        CTempString ref_seq_id = db.ref.SEQ_ID(ref_row);
        
        int max_aln_secondary = 0;
        TVDBRowId max_aln_row_id = 0;
        TSeqPos max_aln_end = 0;
        
        for ( int secondary = 0; secondary < 2; ++secondary ) {
            CVDBValueFor<TVDBRowId> ids = db.ref.ALIGNMENT_IDS(secondary, ref_row);
            for ( auto row_id : ids ) {
                INSDC_coord_zero pos = db.aln(secondary).REF_POS(row_id);
                INSDC_coord_len len = db.aln(secondary).REF_LEN(row_id);
                if ( pos != 0 ) {
                    // check only alignments that start at the very beginning
                    break;
                }
                auto end = pos + len;
                if ( end > max_aln_end ) {
                    max_aln_secondary = secondary;
                    max_aln_row_id = row_id;
                    max_aln_end = end;
                }
            }
        }
        
        if ( max_aln_end > 2*kRefPageSize ) {
            if ( verbose >= eVerbose_overlap ) {
                cout << db.file << ": "<<ref_seq_id<<" @"<<ref_row<<" "
                     << (!max_aln_secondary? "primary": "secondary") << " @"<<max_aln_row_id
                     << " spans to "<<max_aln_end<<endl;
            }
            TVDBRowId last_ref_row = ref_row + (max_aln_end-1)/kRefPageSize;
            for ( TVDBRowId row_id = ref_row+1; row_id <= last_ref_row; ++row_id ) {
                auto overlap_pos = db.ref.OVERLAP_REF_POS(row_id);
                TSeqPos pos = TSeqPos(row_id-ref_row)*kRefPageSize;
                for ( int i = 0; i < 2; ++i ) {
                    TSeqPos p = overlap_pos[i];
                    if ( p && p < pos ) {
                        pos = p;
                    }
                }
                if ( pos >= kRefPageSize ) {
                    // effective overlap starts not at the first ref page
                    // this is an actual overlap info error
                    if ( verbose >= eVerbose_error ) {
                        cout << db.file << ": "<<ref_seq_id<<" @"<<ref_row<<" "
                             << (!max_aln_secondary? "primary": "secondary") << " @"<<max_aln_row_id
                             << " spans to "<<max_aln_end<<endl;
                    }
                    bad_overlap = true;
                    break;
                }
            }
        }
        
        ref_row = ref_row_range.second+1;
    }
    if ( bad_overlap ) {
        ++bad_overlap_file_count;
    }
    else {
        ++good_overlap_file_count;
    }
}


void CSpanCheckApp::RecalculateFile(const SDB& db)
{
    const TSeqPos kRefPageSize = db.ref.GetRowSize();
    auto name_row_range = db.ref.col_name.GetRowIdRange(db.ref.cursor);
    TVDBRowId end_ref_row = name_row_range.first + name_row_range.second;
    for ( TVDBRowId ref_row = 1; ref_row < end_ref_row; ) {
        // read range and names
        CTempString ref_name = db.ref.NAME(ref_row);
        //cout << "ref "<<ref_name<<" @ "<<ref_row<<endl;
        
        auto ref_row_range = db.ref.NAME_RANGE(ref_name);

        vector<TSeqPos> new_ref_pos[2];
        vector<TSeqPos> new_ref_len[2];
        vector<TSeqPos> old_ref_pos[2];
        vector<TSeqPos> old_ref_len[2];
        for ( int i = 0; i < 2; ++i ) {
            new_ref_pos[i].resize(1, kRefPageSize);
            new_ref_len[i].resize(1);
        }

        TSeqPos max_ref_page = 0;
        for ( TSeqPos ref_page = 0; ref_page <= max_ref_page; ++ref_page ) {
            auto overlap_pos = db.ref.OVERLAP_REF_POS(ref_row + ref_page);
            auto overlap_len = db.ref.OVERLAP_REF_LEN(ref_row + ref_page);
            for ( int secondary = 0; secondary < 2; ++secondary ) {
                old_ref_pos[secondary].push_back(overlap_pos[secondary]);
                old_ref_len[secondary].push_back(overlap_len[secondary]);
                CVDBValueFor<TVDBRowId> ids = db.ref.ALIGNMENT_IDS(secondary, ref_row+ref_page);
                for ( auto row_id : ids ) {
                    TSeqPos pos = db.aln(secondary).REF_POS(row_id);
                    TSeqPos len = db.aln(secondary).REF_LEN(row_id);
                    TSeqPos last_pos = pos + len - 1;
                    TSeqPos last_page = last_pos / kRefPageSize;
                    if ( last_page == ref_page ) {
                        //continue;
                    }
                    if ( ref_page == 0 ) {
                        if ( last_page > max_ref_page ) {
                            max_ref_page = last_page;
                            for ( int i = 0; i < 2; ++i ) {
                                new_ref_pos[i].resize(last_page+1, kRefPageSize);
                                new_ref_len[i].resize(last_page+1);
                            }
                        }
                    }
                    if ( last_page <= max_ref_page ) {
                        if ( pos != 0 ) {
                            new_ref_pos[secondary][last_page] =
                                min(new_ref_pos[secondary][last_page], pos);
                        }
                        if ( ref_page != last_page &&
                             last_pos % kRefPageSize != kRefPageSize - 1 ) {
                            new_ref_len[secondary][last_page] =
                                max(new_ref_len[secondary][last_page], last_pos % kRefPageSize + 1);
                        }
                    }
                }
            }
        }

        for ( TSeqPos ref_page = 0; ref_page <= max_ref_page; ++ref_page ) {
            for ( int secondary = 0; secondary < 2; ++secondary ) {
                if ( new_ref_pos[secondary][ref_page] == kRefPageSize ) {
                    new_ref_pos[secondary][ref_page] = 0;
                }
            }
        }

        assert(old_ref_pos[0].size() == new_ref_pos[0].size());
        assert(old_ref_pos[1].size() == new_ref_pos[1].size());
        if ( old_ref_pos[0] != new_ref_pos[0] ||
             old_ref_pos[1] != new_ref_pos[1] ) {
            for ( TSeqPos ref_page = 0; ref_page <= max_ref_page; ++ref_page ) {
                cout << db.file << ": "<<ref_name<<" @"<<ref_row<<" POS["<<ref_page<<"]: "
                     <<"("<<old_ref_pos[0][ref_page]<<", "<<old_ref_pos[1][ref_page]<<") vs "
                     <<"("<<new_ref_pos[0][ref_page]<<", "<<new_ref_pos[1][ref_page]<<")"<<endl;
            }
        }
        assert(old_ref_len[0].size() == new_ref_len[0].size());
        assert(old_ref_len[1].size() == new_ref_len[1].size());
        if ( old_ref_len[0] != new_ref_len[0] ||
             old_ref_len[1] != new_ref_len[1] ) {
            for ( TSeqPos ref_page = 0; ref_page <= max_ref_page; ++ref_page ) {
                cout << db.file << ": "<<ref_name<<" @"<<ref_row<<" LEN["<<ref_page<<"]: "
                     <<"("<<old_ref_len[0][ref_page]<<", "<<old_ref_len[1][ref_page]<<") vs "
                     <<"("<<new_ref_len[0][ref_page]<<", "<<new_ref_len[1][ref_page]<<")"<<endl;
            }
        }
        
        ref_row = ref_row_range.second+1;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSpanCheckApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSpanCheckApp().AppMain(argc, argv);
}
