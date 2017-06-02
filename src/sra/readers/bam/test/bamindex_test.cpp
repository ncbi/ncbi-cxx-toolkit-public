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

#include "bam_test_common.hpp"
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBamIndexTestApp::


class CBamIndexTestApp : public CBAMTestCommon
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


void CBamIndexTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    InitCommonArgs(*arg_desc);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CBam raw index access test program");

    arg_desc->AddOptionalKey("seq_id", "SeqId",
                             "RefSeq Seq-id",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("l", "IndexLevel",
                             "Level of BAM index to scan",
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("l", new CArgAllow_Integers(CBamIndex::kMinLevel, CBamIndex::kMaxLevel));

    arg_desc->AddOptionalKey("title", "Title",
                             "Title of generated Seq-graph",
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of BAM entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100000");
    
    arg_desc->AddFlag("bin", "Write binary ASN.1");
    
    arg_desc->AddFlag("file-range", "Print file range of alignements");
    arg_desc->AddFlag("dump", "Dump index");
    arg_desc->AddFlag("sra", "Use SRA toolkit");
    arg_desc->AddFlag("ST", "Single thread");
    arg_desc->AddFlag("overstart", "Verify overstart info");
    arg_desc->AddFlag("data-size", "Check data size info");

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
            cout << "ref["<<i<<"] linear["<<k<<" = "<<k*kBlockSize<<"] FP = "
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
        cout << "FP( " << v.first.first << " - " << v.first.second
             << " ) : ref[" << v.second.first << "] bin(" << v.second.second << ") = "
             << SBamIndexBinInfo::GetSeqRange(v.second.second)
             << '\n';
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

    int error_code = 0;
    
    if ( !ParseCommonArgs(args) ) {
        return 1;
    }

    Uint8 limit_count = args["limit_count"].AsInteger();
    
    CBamIndex::EIndexLevel level1 = CBamIndex::kMinLevel;
    CBamIndex::EIndexLevel level2 = CBamIndex::kMaxLevel;
    if ( args["l"] ) {
        level1 = level2 = CBamIndex::EIndexLevel(args["l"].AsInteger());
    }

    CStopWatch sw;
    sw.Restart();
    CBamRawDb bam_raw_db(path, index_path);
    cout << "Opened bam file in "<<sw.Elapsed()<<"s"<<endl;
    
    if ( refseq_table ) {
        cout << "RefSeq table:\n";
        for ( size_t i = 0; i < bam_raw_db.GetHeader().GetRefs().size(); ++i ) {
            cout << "RefSeq: " << bam_raw_db.GetRefName(i) << " " << bam_raw_db.GetRefSeqLength(i) << " @ "
                 << bam_raw_db.GetIndex().GetRef(i).GetFileRange()
                 << endl;
        }
    }
    
    for ( auto& q : queries ) {
        auto ref_index = bam_raw_db.GetRefIndex(q.refseq_id);
        if ( ref_index == size_t(-1) ) {
            ERR_POST(Fatal<<"Unknown reference sequence: "<<q.refseq_id);
        }
        if ( args["dump"] ) {
            s_DumpIndex(bam_raw_db.GetIndex(), ref_index);
        }
    }

    if ( args["o"].AsString() != "-" ) {
        for ( auto& q : queries ) {
            CSeq_id seq_id;
            if ( args["seq_id"] ) {
                seq_id.Set(args["seq_id"].AsString());
            }
            else {
                seq_id.Set(CSeq_id::e_Local, q.refseq_id);
            }
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSet().SetSeq_set();
            entry->SetSet().SetAnnot().push_back
                (bam_raw_db.GetIndex().MakeEstimatedCoverageAnnot(bam_raw_db.GetRefIndex(q.refseq_id), seq_id, "graph", level1, level2));

            if ( args["bin"] )
                out << MSerial_AsnBinary;
            else
                out << MSerial_AsnText;
            out << *entry;
        }
        out << flush;
    }
    
    if ( args["file-range"] ) {
        for ( auto& q : queries ) {
            CBamFileRangeSet rs;
            if ( level1 != CBamIndex::kMinLevel || level2 != CBamIndex::kMaxLevel ) {
                rs.AddRanges(bam_raw_db.GetIndex(),
                             bam_raw_db.GetRefIndex(q.refseq_id), q.refseq_range,
                             level1, level2);
            }
            else {
                rs = CBamFileRangeSet(bam_raw_db.GetIndex(),
                                      bam_raw_db.GetRefIndex(q.refseq_id), q.refseq_range);
            }
            for ( auto& c : rs ) {
                cout << "Ref["<<q.refseq_id<<"] @"<<q.refseq_range<<": "
                     << c.first<<" - "<<c.second
                     << endl;
            }
        }
    }

    if ( args["data-size"] ) {
        for ( auto& q : queries ) {
            auto ref_index = bam_raw_db.GetRefIndex(q.refseq_id);
            auto& ref = bam_raw_db.GetIndex().GetRef(ref_index);
            auto ref_len = bam_raw_db.GetRefSeqLength(ref_index);
            sw.Restart();
            const auto& data_size = ref.EstimateDataSizeByAlnStartPos(ref_len);
            cout <<"Collected data size data in "<<sw.Elapsed()<<"s"<<endl;
            cout <<"Total size: "<<accumulate(data_size.begin(), data_size.end(), Uint8(0))<<" bytes"<<endl;
        }
    }

    if ( args["overstart"] ) {
        for ( auto& q : queries ) {
            auto ref_index = bam_raw_db.GetRefIndex(q.refseq_id);
            auto& ref = bam_raw_db.GetIndex().GetRef(ref_index);
            auto ref_len = bam_raw_db.GetRefSeqLength(ref_index);
            sw.Restart();
            const auto& overstart = ref.GetAlnOverStarts();
            cout <<"Collected overstart data in "<<sw.Elapsed()<<"s"<<endl;
            cout <<"Verifying..."<<endl;
            for ( TSeqPos pos = (q.refseq_range.GetFrom()+16383)&-16384;
                  pos < q.refseq_range.GetToOpen() && pos < ref_len;
                  pos += 16384 ) {
                TSeqPos k = pos/16384;
                TSeqPos actual_start = pos;
                CBamRawAlignIterator it(bam_raw_db, q.refseq_id, pos, 1);
                if ( it ) {
                    actual_start = it.GetRefSeqPos()&-16384;
                }
                TSeqPos calculated_start = k < overstart.size()? overstart[k]: pos;
                if ( actual_start < calculated_start ) {
                    cout << "overstart["<<pos<<"] = "
                         << calculated_start << " != " << actual_start
                         << endl;
                    error_code = 1;
                }
                else if ( verbose && actual_start != calculated_start ) {
                    cout << "inexact overstart["<<pos<<"] = "
                         << calculated_start << " != " << actual_start
                         << " by " << (actual_start-calculated_start)
                         << endl;
                }
            }
            cout <<"Done verifying overstart data."<<endl;
        }
    }

    CBamMgr mgr;
    CBamDb bam_db;
    if ( args["sra"] ) {
        bam_db = CBamDb(mgr, path, index_path);
    }
    bool single_thread = args["ST"];

    Uint8 total_align_count = 0;
    Uint8 total_wrong_level_count = 0, total_wrong_range_count = 0;
    const size_t NQ = queries.size();
    vector<thread> tt(NQ);
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i] =
            thread([&]
                   (SQuery q)
                   {
                       CMutexGuard guard(eEmptyGuard);
                       if ( single_thread ) {
                           guard.Guard(s_Mutex);
                       }
                       Uint8 align_count = 0;
                       Uint8 wrong_level_count = 0, wrong_range_count = 0;
                       try {
                           if ( bam_db ) {
                               for ( CBamAlignIterator it(bam_db, q.refseq_id,
                                                          q.refseq_range.GetFrom(),
                                                          q.refseq_range.GetLength()); it; ++it ) {
                                   TSeqPos ref_pos = it.GetRefSeqPos();
                                   TSeqPos ref_size = it.GetCIGARRefSize();
                                   TSeqPos ref_end = ref_pos + ref_size;
                                   if ( verbose ||
                                        ref_pos > q.refseq_range.GetTo() || ref_end <= q.refseq_range.GetFrom() ) {
                                       cout << "Ref: " << q.refseq_id
                                            << " at [" << ref_pos
                                            << " - " << (ref_end-1)
                                            << "] = " << ref_size
                                            << '\n';
                                       if ( ref_pos > q.refseq_range.GetTo() || ref_end <= q.refseq_range.GetFrom() ) {
                                           cout << "Wrong range" << endl;
                                           ++wrong_range_count;
                                       }
                                   }
                                   ++align_count;
                                   if ( limit_count && align_count >= limit_count ) {
                                       break;
                                   }
                               }
                           }
                           else {
                               for ( CBamRawAlignIterator it(bam_raw_db, q.refseq_id,
                                                             q.refseq_range, level1, level2); it; ++it ) {
                                   TSeqPos ref_pos = it.GetRefSeqPos();
                                   TSeqPos ref_size = it.GetCIGARRefSize();
                                   TSeqPos ref_end = ref_pos + ref_size;
                                   uint32_t level = CBamIndex::kMinLevel;
                                   if ( ref_size && (level1 != CBamIndex::kMinLevel || level2 != CBamIndex::kMaxLevel) ) {
                                       TSeqPos pos1 = ref_pos >> CBamIndex::kLevel0BinShift;
                                       TSeqPos pos2 = (ref_end-1) >> CBamIndex::kLevel0BinShift;
                                       while ( pos1 != pos2 ) {
                                           ++level;
                                           pos1 >>= CBamIndex::kLevelStepBinShift;
                                           pos2 >>= CBamIndex::kLevelStepBinShift;
                                       }
                                   }
                                   if ( verbose ||
                                        level < level1 || level > level2 ||
                                        ref_pos > q.refseq_range.GetTo() || ref_end <= q.refseq_range.GetFrom() ) {
                                       cout << "Ref: " << q.refseq_id
                                            << " at [" << ref_pos
                                            << " - " << (ref_end-1)
                                            << "] = " << ref_size
                                            << '\n';
                                       if ( level < level1 || level > level2 ) {
                                           cout << "Wrong index level: " << level << endl;
                                           ++wrong_level_count;
                                       }
                                       if ( ref_pos > q.refseq_range.GetTo() || ref_end <= q.refseq_range.GetFrom() ) {
                                           cout << "Wrong range" << endl;
                                           ++wrong_range_count;
                                       }
                                   }
                                   ++align_count;
                                   if ( limit_count && align_count >= limit_count ) {
                                       break;
                                   }
                               }
                           }
                       }
                       catch ( exception& exc ) {
                           ERR_POST("Run("<<q.refseq_id<<"): Exception: "<<exc.what());
                       }
                       {
                           CMutexGuard guard(s_Mutex);
                           LOG_POST("Run("<<q.refseq_id<<"): count "<<align_count);
                           if ( wrong_level_count ) {
                               LOG_POST("Run("<<q.refseq_id<<"): wrong level count "<<wrong_level_count);
                           }
                           if ( wrong_range_count ) {
                               LOG_POST("Run("<<q.refseq_id<<"): wrong range count "<<wrong_range_count);
                           }
                           total_align_count += align_count;
                           total_wrong_level_count += wrong_level_count;
                           total_wrong_range_count += wrong_range_count;
                       }
                   }, queries[i]);
    }
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i].join();
    }
    cout << "Total good align count: "<<
        (total_align_count-total_wrong_range_count-total_wrong_level_count)<<endl;
    if ( total_wrong_level_count ) {
        cout << "Total wrong level count: "<<total_wrong_level_count<<endl;
        error_code = 1;
    }
    if ( total_wrong_range_count ) {
        cout << "Total wrong range count: "<<total_wrong_range_count<<endl;
        error_code = 1;
    }

    return error_code;
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
