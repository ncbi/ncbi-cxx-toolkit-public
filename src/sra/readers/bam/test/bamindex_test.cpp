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
 *   Sample test application for BAM raw index access
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <sra/readers/bam/bamgraph.hpp>
#include <sra/readers/bam/bamindex.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <util/simple_buffer.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <thread>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBamIndexTestApp::


class CBamIndexTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
                         //  Init test


#define BAM_DIR1 "/1000genomes/ftp/data"
#define BAM_DIR2 "/1kg_pilot_data/data"
#define BAM_DIR3 "/1000genomes2/ftp/data"
#define BAM_DIR4 "/1000genomes2/ftp/phase1/data"

#define BAM_FILE "HG00116.chrom20.ILLUMINA.bwa.GBR.low_coverage.20101123.bam"

void CBamIndexTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("dir", "Dir",
                             "BAM files files directory",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("file", "File",
                            "BAM file name",
                            CArgDescriptions::eString,
                            BAM_FILE);

    arg_desc->AddOptionalKey("ref_label", "RefLabel",
                             "RefSeq id in BAM file",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("seq_id", "SeqId",
                             "RefSeq Seq-id",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("q", "Query",
                             "Query variants: chr1, chr1:123-432",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("title", "Title",
                             "Title of generated Seq-graph",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("o", "OutputFile",
                             "Output file of ASN.1",
                             CArgDescriptions::eOutputFile);
    arg_desc->AddFlag("bin", "Write binary ASN.1");
    
    arg_desc->AddFlag("file-range", "Print file range of alignements");
    arg_desc->AddFlag("verbose", "Verbose output");
    arg_desc->AddFlag("dump", "Dump index");
    arg_desc->AddFlag("sra", "Use SRA toolkit");
    arg_desc->AddFlag("ST", "Single thread");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


void s_DumpIndex(const CBamIndex& index,
                 size_t ref_index)
{
    const unsigned kBlockSize = 1<<14;
    vector<pair<CBGZFRange, pair<size_t, Uint4>>> cc;
    for ( size_t i = ref_index; i <= ref_index; ++i ) {
        const SBamIndexRefIndex& r = index.GetRef(i);
        for ( size_t k = 0; k < r.m_Intervals.size(); ++k ) {
            cout << "ref["<<i<<"] int["<<k<<" = "<<k*kBlockSize<<"] = "
                 << r.m_Intervals[k]
                 << endl;
        }
        for ( auto& b : r.m_Bins ) {
            for ( auto& c : b.m_Chunks ) {
                cc.push_back(make_pair(c, make_pair(i, b.m_Bin)));
            }
        }
    }
    sort(cc.begin(), cc.end());
    CBGZFPos prev(0);
    for ( auto& v : cc ) {
        cout << v.first.first << '-' << v.first.second
             << " : ref[" << v.second.first << "] "
             << SBamIndexBinInfo::GetSeqRange(v.second.second)
             << endl;
        _ASSERT(v.first.first < v.first.second);
        if ( prev.GetVirtualPos() ) {
            if ( prev != v.first.first ) {
                cout << " overlap: "<<prev<<" vs "<<v.first.first<<endl;
            }
        }
        prev = v.first.second;
    }
}


DEFINE_STATIC_MUTEX(s_Mutex);


int CBamIndexTestApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);
    // Get arguments
    const CArgs& args = GetArgs();

    bool verbose = args["verbose"];

    vector<string> dirs;
    if ( args["dir"] ) {
        dirs.push_back(args["dir"].AsString());
    }
    else {
        vector<string> reps;
        NStr::Split(NCBI_TEST_BAM_FILE_PATH, ":", reps);
        ITERATE ( vector<string>, it, reps ) {
            dirs.push_back(CFile::MakePath(*it, BAM_DIR1));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR2));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR3));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR4));
        }
    }

    string file = args["file"].AsString();
    string path;
    
    if ( NStr::StartsWith(file, "http://") ||
         NStr::StartsWith(file, "https://") ||
         NStr::StartsWith(file, "ftp://") ||
         CFile(file).Exists() ) {
        path = file;
    }
    else {
        ITERATE ( vector<string>, it, dirs ) {
            string dir = *it;
            if ( !CDirEntry(dir).Exists() ) {
                continue;
            }
            path = CFile::MakePath(dir, file);
            if ( !CFile(path).Exists() ) {
                SIZE_TYPE p1 = file.rfind('/');
                if ( p1 == NPOS ) {
                    p1 = 0;
                }
                else {
                    p1 += 1;
                }
                SIZE_TYPE p2 = file.find('.', p1);
                if ( p2 != NPOS ) {
                    path = CFile::MakePath(dir, file.substr(p1, p2-p1) + "/alignment/" + file);
                }
            }
            if ( CFile(path).Exists() ) {
                break;
            }
            path.clear();
        }
        if ( path.empty() ) {
            path = file;
        }
        if ( !CFile(path).Exists() ) {
            ERR_POST("Data file "<<args["file"].AsString()<<" not found.");
            return 1;
        }
    }

    typedef CRange<TSeqPos> TRange;
    typedef pair<string, TRange> TQuery;
    vector<TQuery> queries;

    if ( args["ref_label"] ) {
        vector<string> ss;
        NStr::Split(args["ref_label"].AsString(), ",", ss);
        for ( auto& l : ss ) {
            queries.push_back(make_pair(l, TRange::GetWhole()));
        }
    }
    else if ( args["q"] ) {
        vector<string> ss;
        NStr::Split(args["q"].AsString(), ",", ss);
        for ( auto& q : ss ) {
            SIZE_TYPE colon = q.find(':');
            string ref_label;
            TRange ref_range;
            if ( colon == NPOS ) {
                ref_label = q;
                ref_range = TRange::GetWhole();
            }
            else {
                ref_label = q.substr(0, colon);
                q = q.substr(colon+1);
                SIZE_TYPE dash = q.find('-');
                if ( dash == NPOS ) {
                    ERR_POST(Fatal<<"Bad query format");
                }
                ref_range.SetFrom(NStr::StringToNumeric<TSeqPos>(q.substr(0, dash)));
                ref_range.SetTo(NStr::StringToNumeric<TSeqPos>(q.substr(dash+1)));
            }
            queries.push_back(make_pair(ref_label, ref_range));
        }
    }

    CBamRawDb bam_raw_db(path, path+".bai");

    for ( auto& q : queries ) {
        if ( bam_raw_db.GetRefIndex(q.first) == size_t(-1) ) {
            ERR_POST(Fatal<<"Unknown reference sequence: "<<q.first);
        }
        if ( args["dump"] ) {
            s_DumpIndex(bam_raw_db.GetIndex(), bam_raw_db.GetRefIndex(q.first));
        }
    }

    if ( args["o"] ) {
        for ( auto& q : queries ) {
            CSeq_id seq_id;
            if ( args["seq_id"] ) {
                seq_id.Set(args["seq_id"].AsString());
            }
            else {
                seq_id.Set(CSeq_id::e_Local, q.first);
            }
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSet().SetSeq_set();
            entry->SetSet().SetAnnot().push_back
                (bam_raw_db.GetIndex().MakeEstimatedCoverageAnnot(bam_raw_db.GetRefIndex(q.first), seq_id, "graph"));

            CNcbiOstream& out = args["o"].AsOutputFile();
            if ( args["bin"] )
                out << MSerial_AsnBinary;
            else
                out << MSerial_AsnText;
            out << *entry;
        }
    }
    
    if ( args["file-range"] ) {
        for ( auto& q : queries ) {
            CBamFileRangeSet rs(bam_raw_db.GetIndex(),
                                bam_raw_db.GetRefIndex(q.first), q.second);
            for ( auto& c : rs ) {
                cout << "Ref["<<q.first<<"] @"<<q.second<<": "
                     << c.first<<" - "<<c.second
                     << endl;
            }
        }
    }

    CBamMgr mgr;
    CBamDb bam_db;
    if ( args["sra"] ) {
        bam_db = CBamDb(mgr, path, path+".bai");
    }
    bool single_thread = args["ST"];
    
    Uint8 align_count = 0;
    const size_t NQ = queries.size();
    vector<thread> tt(NQ);
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i] =
            thread([&bam_raw_db, &bam_db, &align_count, verbose, single_thread]
                   (string ref_label, CRange<TSeqPos> ref_range)
                   {
                       CMutexGuard guard(eEmptyGuard);
                       if ( single_thread ) {
                           guard.Guard(s_Mutex);
                       }
                       Uint8 count = 0;
                       try {
                           if ( bam_db ) {
                               for ( CBamAlignIterator it(bam_db, ref_label, ref_range.GetFrom(), ref_range.GetLength()); it; ++it ) {
                                   if ( verbose ) {
                                       TSeqPos ref_pos = it.GetRefSeqPos();
                                       TSeqPos ref_size = it.GetCIGARRefSize();
                                       cout << "Ref: " << ref_label
                                            << " at [" << ref_pos
                                            << " - " << (ref_pos+ref_size-1)
                                            << "] = " << ref_size
                                            << '\n';
                                   }
                                   ++count;
                               }
                           }
                           else {
                               for ( CBamRawAlignIterator it(bam_raw_db, ref_label, ref_range); it; ++it ) {
                                   if ( verbose ) {
                                       TSeqPos ref_pos = it.GetRefSeqPos();
                                       TSeqPos ref_size = it.GetCIGARRefSize();
                                       cout << "Ref: " << ref_label
                                            << " at [" << ref_pos
                                            << " - " << (ref_pos+ref_size-1)
                                            << "] = " << ref_size
                                            << '\n';
                                   }
                                   ++count;
                               }
                           }
                       }
                       catch ( exception& exc ) {
                           ERR_POST("Run("<<ref_label<<"): Exception: "<<exc.what());
                       }
                       {
                           CMutexGuard guard(s_Mutex);
                           LOG_POST("Run("<<ref_label<<"): count "<<count);
                           align_count += count;
                       }
                   }, queries[i].first, queries[i].second);
    }
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i].join();
    }
    /*
      for ( CBamRawAlignIterator it(bam_raw_db, ref_label, ref_range); it; ++it ) {
      if ( verbose ) {
      TSeqPos ref_pos = it.GetRefSeqPos();
      TSeqPos ref_size = it.GetCIGARRefSize();
      cout << "Ref: " << ref_label
      << " at [" << ref_pos
      << " - " << (ref_pos+ref_size-1)
      << "] = " << ref_size
      << '\n';
      }
      ++align_count;
      }
    */
    cout << "Align count: "<<align_count<<endl;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBamIndexTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBamIndexTestApp().AppMain(argc, argv);
}
