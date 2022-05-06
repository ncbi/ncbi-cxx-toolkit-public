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
#include <numeric>
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

    bool check_file_range;
    int s_CheckOverlaps(CBamRawDb& bam);
    int s_CheckOverstart(CBamRawDb& bam);
    int s_CheckOverend(CBamRawDb& bam);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


void CBamIndexTestApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    InitCommonArgs(*arg_desc);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CBam raw index access test program");

    arg_desc->AddOptionalKey("seq_id", "SeqId",
                             "RefSeq Seq-id",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("l", "IndexLevel",
                             "Level(s) of BAM index to scan",
                             CArgDescriptions::eString);
    
    arg_desc->AddDefaultKey("min_quality", "MinQuality",
                            "Minimal quality of alignments to check",
                            CArgDescriptions::eInteger,
                            "1");

    arg_desc->AddOptionalKey("scan_for_long_alignments", "ScanForLongAlignments",
                            "Scan for alignments longer than the argument",
                            CArgDescriptions::eInteger);
    
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
    arg_desc->AddFlag("overlap", "Verify overlap info");
    arg_desc->AddFlag("overstart", "Verify overstart info");
    arg_desc->AddFlag("overend", "Verify overend info");
    arg_desc->AddFlag("data-size", "Check data size info");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


static inline Uint8 s_GetSize(CBGZFRange range)
{
    return CBamFileRangeSet::GetFileSize(range);
}


static inline TSeqPos s_BinIndex(TSeqPos pos, TSeqPos bin_size)
{
    return pos/bin_size;
}


static inline TSeqPos s_BinStart(TSeqPos pos, TSeqPos bin_size)
{
    return s_BinIndex(pos, bin_size)*bin_size;
}


static inline TSeqPos s_BinEnd(TSeqPos pos, TSeqPos bin_size)
{
    return s_BinStart(pos, bin_size)+bin_size-1;
}


void s_DumpIndex(const CBamIndex& index,
                 size_t ref_index)
{
    const unsigned kBlockSize = index.GetMinBinSize();
    vector<pair<CBGZFRange, pair<size_t, Uint4>>> cc;
    for ( size_t i = ref_index; i <= ref_index; ++i ) {
        const SBamIndexRefIndex& r = index.GetRef(i);
        for ( size_t k = 0; k < r.m_Overlaps.size(); ++k ) {
            cout << "ref["<<i<<"] linear["<<k<<" = "<<k*kBlockSize<<"] oFP = "
                 << r.m_Overlaps[k]
                 << endl;
        }
        for ( auto& b : r.m_Bins ) {
#ifdef BAM_SUPPORT_CSI
            if ( index.is_CSI && b.m_Overlap ) {
                cout << "ref["<<i<<"] bin["<<b.m_Bin<<" = "<<b.GetSeqRange(index)<<"] oFP = "
                     << b.m_Overlap
                     << endl;
            }
#endif
            size_t total_size = 0;
            for ( auto& c : b.m_Chunks ) {
                cc.push_back(make_pair(c, make_pair(i, b.m_Bin)));
                total_size += s_GetSize(c);
            }
            cout << "Bin("<<b.m_Bin<<") = "<<index.GetSeqRange(b.m_Bin)<<" size: "<<total_size<<endl;
        }
    }
    sort(cc.begin(), cc.end());
    CBGZFPos prev(0);
    for ( auto& v : cc ) {
        cout << "FP( " << v.first.first << " - " << v.first.second << " size="<<s_GetSize(v.first)
             << " ) : ref[" << v.second.first << "] bin(" << v.second.second << ") = "
             << index.GetSeqRange(v.second.second)
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


int CBamIndexTestApp::s_CheckOverlaps(CBamRawDb& bam_raw_db)
{
    int error_code = 0;
    const auto bin_shift = bam_raw_db.GetIndex().GetMinLevelBinShift();
    const TSeqPos bin_size = 1u << bin_shift;
    
    struct SBinOverlapInfo {
        CBGZFPos min_file_pos;
        TSeqPos min_ref_pos;
    };
    map<int32_t, vector<SBinOverlapInfo>> overlaps;
    
    CStopWatch sw(CStopWatch::eStart);
    int32_t current_seq = -1;
    vector<SBinOverlapInfo> current_seq_overlaps;
    size_t unmapped_count = 0;
    size_t nocigar_count = 0;
    size_t total_count = 0;
    for ( CBamRawAlignIterator it(bam_raw_db); it; ++it ) {
        ++total_count;
        auto seq = it.GetRefSeqIndex();
        if ( !it.IsMapped() ) {
            ++unmapped_count;
            continue;
        }
        if ( !it.GetCIGAROpsCount() ) {
            ++nocigar_count;
            continue;
        }
        if ( seq != current_seq ) {
            if ( current_seq >= 0 ) {
                overlaps[current_seq].swap(current_seq_overlaps);
            }
            cout <<"Start collecting overstart data for ref["<<seq<<"] = "
                 <<bam_raw_db.GetRefName(seq)<<endl;
            current_seq = seq;
            current_seq_overlaps.clear();
            TSeqPos seq_length = bam_raw_db.GetIndex().GetRef(seq).m_EstimatedLength;
            current_seq_overlaps.resize(seq_length >> bin_shift);
        }
        auto pos = it.GetRefSeqPos();
        auto len = it.GetCIGARRefSize();
        auto end = pos+len-1;
        _ASSERT(end >= pos);
        auto beg_bin = pos >> bin_shift;
        auto end_bin = end >> bin_shift;
        _ASSERT(end_bin >= beg_bin);
        if ( end_bin >= current_seq_overlaps.size() ) {
            current_seq_overlaps.resize(end_bin+1);
        }
        for ( auto bin = beg_bin; bin <= end_bin; ++bin ) {
            auto& overlap = current_seq_overlaps[bin];
            if ( !overlap.min_file_pos ) {
                overlap.min_file_pos = it.GetFilePos();
                overlap.min_ref_pos = pos;
            }
        }
    }
    if ( current_seq >= 0 ) {
        overlaps[current_seq].swap(current_seq_overlaps);
    }
    for ( auto& ref_it : overlaps ) {
        for ( size_t i = 0; i < ref_it.second.size(); ++i ) {
            if ( !ref_it.second[i].min_file_pos ) {
                ref_it.second[i].min_ref_pos = TSeqPos(i << bin_shift);
            }
        }
    }
    cout <<"Collected all real overlap data in "<<sw.Elapsed()<<"s"<<endl;
    cout <<"Total alignments: "<<total_count
         <<", unmapped: "<<unmapped_count
         <<", no CIGAR: "<<nocigar_count
         <<endl;

    for ( auto& ref_it : overlaps ) {
        auto ref_index = ref_it.first;
        auto& ref = bam_raw_db.GetIndex().GetRef(ref_index);
        sw.Restart();
        const auto& overstart = ref.GetAlnOverStarts();
        cout <<"Collected overstart data for ref["<<ref_index<<"] = "
             <<bam_raw_db.GetRefName(ref_index)<<" in "<<sw.Elapsed()<<"s"<<endl;
        auto bin_count = max(overstart.size(), ref_it.second.size());
        size_t wrong_start_count = 0;
        size_t decrease_start_count = 0;
        TSeqPos prev_calculated_start = 0;
        size_t excess_pages_count = 0;
        for ( size_t i = 0; i < bin_count; ++i ) {
            TSeqPos bin_pos = TSeqPos(i<<bin_shift);
            TSeqPos actual_start = (i<ref_it.second.size())? ref_it.second[i].min_ref_pos: bin_pos;
            TSeqPos calculated_start = (i<overstart.size())? overstart[i]: bin_pos;
            actual_start = (actual_start >> bin_shift) << bin_shift;
            calculated_start = (calculated_start >> bin_shift) << bin_shift;
            if ( actual_start < calculated_start ) {
                cout << "wrong overstart["<<bin_pos<<"] = "
                     << calculated_start << " > actual " << actual_start
                     << endl;
                ++wrong_start_count;
                error_code = 1;
            }
            else if ( actual_start > calculated_start ) {
                TSeqPos excess = (actual_start-calculated_start)/bin_size;
                excess_pages_count += excess;
                if ( verbose ) {
                    cout << "inexact overstart["<<bin_pos<<"] = "
                         << calculated_start << " < actual " << actual_start
                         << " by " << excess << " pages"
                         << endl;
                }
            }
            if ( calculated_start < prev_calculated_start ) {
                cout << "wrong decrease overstart["<<bin_pos<<"] = "
                     << calculated_start
                     << " < overstart["<<(bin_pos-bin_size)<<"] = "
                     << prev_calculated_start
                     << endl;
                ++decrease_start_count;
                error_code = 1;
            }
            prev_calculated_start = calculated_start;
        }
        cout <<"Done verifying overend data for ref["<<ref_index<<"] = "
             <<bam_raw_db.GetRefName(ref_index)<<" ("<<excess_pages_count<<" excess pages) - ";
        if ( wrong_start_count || decrease_start_count ) {
            cout <<"ERRORS:"<<endl;
            if ( wrong_start_count ) {
                cout << "  wrong start in "<<wrong_start_count<<" bins"<<endl;
            }
            if ( decrease_start_count ) {
                cout << "  decreased start in "<<decrease_start_count<<" bins"<<endl;
            }
        }
        else {
            cout <<"no errors."<<endl;
        }
    }
    return error_code;
}


int CBamIndexTestApp::s_CheckOverstart(CBamRawDb& bam_raw_db)
{
    int error_code = 0;
    Uint8 total_file_scan_size = 0;
    Uint8 total_chunks_file_scan_size = 0;
    for ( auto& q : queries ) {
        TSeqPos bin_size = bam_raw_db.GetIndex().GetMinBinSize();
        auto ref_index = bam_raw_db.GetRefIndex(q.refseq_id);
        auto& ref = bam_raw_db.GetIndex().GetRef(ref_index);
        auto ref_len = bam_raw_db.GetRefSeqLength(ref_index);
        CStopWatch sw(CStopWatch::eStart);
        const auto& overstart = ref.GetAlnOverStarts();
        cout <<"Collected overstart data in "<<sw.Restart()<<"s"<<endl;
        cout <<"Verifying..."<<endl;
        size_t wrong_start_count = 0;
        size_t decrease_start_count = 0;
        TSeqPos prev_calculated_start = 0;
        size_t total_alignment_count = 0;
        size_t excess_pages_count = 0;
        for ( TSeqPos pos = s_BinStart(q.refseq_range.GetFrom(), bin_size);
              pos < q.refseq_range.GetToOpen() && pos < ref_len;
              pos += bin_size ) {
            TSeqPos actual_start = s_BinStart(pos, bin_size);
            CStopWatch sw2;
            if ( verbose ) {
                sw2.Start();
            }
            if ( check_file_range ) {
                CBamFileRangeSet rs;
                rs.AddRanges(bam_raw_db.GetIndex(),
                             bam_raw_db.GetRefIndex(q.refseq_id),
                             COpenRange<TSeqPos>(pos, pos+bin_size),
                             CBamRawAlignIterator::eSearchByOverlap);
                if ( !rs.GetRanges().empty() ) {
                    auto chunks_size = rs.GetFileSize();
                    auto total_size = 
                        CBamFileRangeSet::GetFileSize(CBGZFRange(rs.begin()->first,
                                                                 prev(rs.end())->second));
                    total_chunks_file_scan_size += chunks_size;
                    total_file_scan_size += total_size;
                    if ( verbose ) {
                        cout << "File sizes @"<<pos<<": "<<total_size<<" "<<chunks_size<<endl;
                    }
                }
            }
            for ( CBamRawAlignIterator it(bam_raw_db, q.refseq_id, pos, bin_size, CBamRawAlignIterator::eSearchByOverlap); it; ++it ) {
                ++total_alignment_count;
                TSeqPos start = it.GetRefSeqPos();
                actual_start = min(actual_start, s_BinStart(start, bin_size));
            }
            if ( verbose ) {
                cout <<"Collected real overstart data @"<<pos<<" in "<<sw2.Elapsed()*1e3<<"ms"<<endl;
            }
            TSeqPos k = s_BinIndex(pos, bin_size);
            TSeqPos calculated_start = k < overstart.size()? overstart[k]: s_BinStart(pos, bin_size);
            if ( actual_start < calculated_start ) {
                cout << "wrong overstart["<<pos<<"] = "
                     << calculated_start << " > actual " << actual_start
                     << endl;
                ++wrong_start_count;
                error_code = 1;
            }
            else if ( actual_start > calculated_start ) {
                TSeqPos excess = (actual_start-calculated_start)/bin_size;
                excess_pages_count += excess;
                if ( verbose ) {
                    cout << "inexact overstart["<<pos<<"] = "
                         << calculated_start << " < actual " << actual_start
                         << " by " << excess << " pages"
                         << endl;
                }
            }
            if ( calculated_start < prev_calculated_start ) {
                cout << "wrong decrease overstart["<<pos<<"] = "
                     << calculated_start
                     << " < overstart["<<(pos-bin_size)<<"] = "
                     << prev_calculated_start
                     << endl;
                ++decrease_start_count;
                error_code = 1;
            }
            prev_calculated_start = calculated_start;
        }
        cout <<"Collected real overstart data in "<<sw.Elapsed()<<"s"<<endl;
        cout <<"Done verifying overend data ("<<total_alignment_count<<" alignments, "<<excess_pages_count<<" excess pages) - ";
        if ( wrong_start_count || decrease_start_count ) {
            cout <<"ERRORS:"<<endl;
            if ( wrong_start_count ) {
                cout << "  wrong start in "<<wrong_start_count<<" bins"<<endl;
            }
            if ( decrease_start_count ) {
                cout << "  decreased start in "<<decrease_start_count<<" bins"<<endl;
            }
        }
        else {
            cout <<"no errors."<<endl;
        }
    }
    if ( check_file_range ) {
        cout <<"Total file scan size: "<<total_file_scan_size
             <<" chunks: "<<total_chunks_file_scan_size
             <<endl;
    }
    return error_code;
}


int CBamIndexTestApp::s_CheckOverend(CBamRawDb& bam_raw_db)
{
    int error_code = 0;
    Uint8 total_file_scan_size = 0;
    Uint8 total_chunks_file_scan_size = 0;
    for ( auto& q : queries ) {
        TSeqPos bin_size = bam_raw_db.GetIndex().GetMinBinSize();
        auto ref_index = bam_raw_db.GetRefIndex(q.refseq_id);
        auto& ref = bam_raw_db.GetIndex().GetRef(ref_index);
        auto ref_len = bam_raw_db.GetRefSeqLength(ref_index);
        CStopWatch sw(CStopWatch::eStart);
        const auto& overend = ref.GetAlnOverEnds();
        cout <<"Collected overend data in "<<sw.Restart()<<"s"<<endl;
        cout <<"Verifying..."<<endl;
        size_t wrong_end_count = 0;
        size_t decrease_end_count = 0;
        TSeqPos prev_calculated_end = 0;
        size_t total_alignment_count = 0;
        size_t excess_pages_count = 0;
        for ( TSeqPos pos = s_BinStart(q.refseq_range.GetFrom(), bin_size);
              pos < q.refseq_range.GetToOpen() && pos < ref_len;
              pos += bin_size ) {
            TSeqPos actual_end = s_BinEnd(pos, bin_size);
            CStopWatch sw2;
            if ( verbose ) {
                sw2.Start();
            }
            if ( check_file_range ) {
                CBamFileRangeSet rs;
                rs.AddRanges(bam_raw_db.GetIndex(),
                             bam_raw_db.GetRefIndex(q.refseq_id),
                             COpenRange<TSeqPos>(pos, pos+bin_size),
                             CBamRawAlignIterator::eSearchByStart);
                if ( !rs.GetRanges().empty() ) {
                    auto chunks_size = rs.GetFileSize();
                    auto total_size = 
                        CBamFileRangeSet::GetFileSize(CBGZFRange(rs.begin()->first,
                                                                 prev(rs.end())->second));
                    total_chunks_file_scan_size += chunks_size;
                    total_file_scan_size += total_size;
                    if ( verbose ) {
                        cout << "File sizes @"<<pos<<": "<<total_size<<" "<<chunks_size<<endl;
                    }
                }
            }
            for ( CBamRawAlignIterator it(bam_raw_db, q.refseq_id, pos, bin_size, CBamRawAlignIterator::eSearchByStart); it; ++it ) {
                ++total_alignment_count;
                TSeqPos end = it.GetRefSeqPos()+max(1u, it.GetCIGARRefSize())-1;
                actual_end = max(actual_end, s_BinEnd(end, bin_size));
            }
            if ( verbose ) {
                cout <<"Collected real overend data @"<<pos<<" in "<<sw2.Elapsed()*1e3<<"ms"<<endl;
            }
            TSeqPos k = s_BinIndex(pos, bin_size);
            TSeqPos calculated_end = k < overend.size()? overend[k]: s_BinEnd(pos, bin_size);
            if ( actual_end > calculated_end ) {
                cout << "wrong overend["<<pos<<"] = "
                     << calculated_end << " < " << actual_end
                     << endl;
                ++wrong_end_count;
                error_code = 1;
            }
            else if ( actual_end < calculated_end ) {
                TSeqPos excess = (calculated_end-actual_end)/bin_size;
                excess_pages_count += excess;
                if ( verbose ) {
                    cout << "inexact overend["<<pos<<"] = "
                         << calculated_end << " > " << actual_end
                         << " by " << excess << " pages"
                         << endl;
                }
            }
            if ( calculated_end < prev_calculated_end ) {
                cout << "wrong decrease overend["<<pos<<"] = "
                     << calculated_end
                     << " < overend["<<(pos-bin_size)<<"] = "
                     << prev_calculated_end
                     << endl;
                ++decrease_end_count;
                error_code = 1;
            }
            prev_calculated_end = calculated_end;
        }
        cout <<"Collected real overend data in "<<sw.Elapsed()<<"s"<<endl;
        cout <<"Done verifying overend data ("<<total_alignment_count<<" alignments, "<<excess_pages_count<<" excess pages) - ";
        if ( wrong_end_count || decrease_end_count ) {
            cout << "ERRORS:"<<endl;
            if ( wrong_end_count ) {
                cout << "  wrong end in "<<wrong_end_count<<" bins"<<endl;
            }
            if ( decrease_end_count ) {
                cout << "  decreased end in "<<decrease_end_count<<" bins"<<endl;
            }
        }
        else {
            cout <<"no errors."<<endl;
        }
    }
    if ( check_file_range ) {
        cout <<"Total file scan size: "<<total_file_scan_size
             <<" chunks: "<<total_chunks_file_scan_size
             <<endl;
    }
    return error_code;
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
    check_file_range = args["file-range"];

    Uint8 limit_count = args["limit_count"].AsInteger();
    
    CStopWatch sw;
    sw.Restart();
    CBamRawDb bam_raw_db(path, index_path);
    cout << "Opened bam file in "<<sw.Elapsed()<<"s"<<endl;
    out << "File: " << path << NcbiEndl;
    out << "Index: " << bam_raw_db.GetIndexName() << NcbiEndl;
    
    CBamIndex::TIndexLevel level1 = 0;
    CBamIndex::TIndexLevel level2 = bam_raw_db.GetIndex().GetMaxIndexLevel();
    if ( args["l"] ) {
        string levels = args["l"].AsString();
        SIZE_TYPE dash = levels.find('-');
        if ( dash != NPOS ) {
            level1 = CBamIndex::TIndexLevel(NStr::StringToInt(levels.substr(0, dash)));
            level2 = CBamIndex::TIndexLevel(NStr::StringToInt(levels.substr(dash+1)));
        }
        else {
            level1 = level2 = CBamIndex::TIndexLevel(NStr::StringToInt(levels));
        }
    }

    if ( refseq_table ) {
        cout << "RefSeq table:\n";
        for ( size_t i = 0; i < bam_raw_db.GetHeader().GetRefs().size(); ++i ) {
            cout << "RefSeq: " << bam_raw_db.GetRefName(i) << " " << bam_raw_db.GetRefSeqLength(i) << " @ "
                 << bam_raw_db.GetIndex().GetRef(i).GetFileRange()
                 << endl;
        }
    }

    if ( args["overlap"] ) {
        if ( s_CheckOverlaps(bam_raw_db) != 0 ) {
            error_code = 1;
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

    CBamRawAlignIterator::ESearchMode search_mode = CBamRawAlignIterator::ESearchMode(by_start);
    
    if ( check_file_range ) {
        for ( auto& q : queries ) {
            CBamFileRangeSet rs;
            if ( level1 != 0 ||
                 level2 != bam_raw_db.GetIndex().GetMaxIndexLevel() ) {
                rs.AddRanges(bam_raw_db.GetIndex(),
                             bam_raw_db.GetRefIndex(q.refseq_id), q.refseq_range,
                             level1, level2, search_mode);
            }
            else {
                rs = CBamFileRangeSet(bam_raw_db.GetIndex(),
                                      bam_raw_db.GetRefIndex(q.refseq_id), q.refseq_range, search_mode);
            }
            for ( auto& c : rs ) {
                cout << "Ref["<<q.refseq_id<<"] @"<<q.refseq_range<<": "
                     << c.first<<" - "<<c.second << " " << (rs.GetFileSize(c)/(1024*1024.)) << " MB"
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
        if ( s_CheckOverstart(bam_raw_db) != 0 ) {
            error_code = 1;
        }
    }

    if ( args["overend"] ) {
        if ( s_CheckOverend(bam_raw_db) != 0 ) {
            error_code = 1;
        }
    }

    if ( args["scan_for_long_alignments"] ) {
        TSeqPos min_length = args["scan_for_long_alignments"].AsInteger();
        CBamIndex::TIndexLevel min_level = 0;
        CBamIndex::TIndexLevel max_level = bam_raw_db.GetIndex().GetMaxIndexLevel();
        while ( min_level < max_level &&
                bam_raw_db.GetIndex().GetBinSize(min_level) < min_length ) {
            ++min_level;
        }
        size_t total_long_count = 0;
        for ( size_t ref_index = 0; ref_index < bam_raw_db.GetRefCount(); ++ref_index ) {
            cout << "Scanning "<<bam_raw_db.GetRefName(ref_index)<<": "<<flush;
            size_t long_count = 0;
            for ( CBamRawAlignIterator it(bam_raw_db, bam_raw_db.GetRefName(ref_index),
                                          CRange<TSeqPos>::GetWhole(),
                                          min_level, max_level, CBamIndex::eSearchByStart);
                  it; ++it ) {
                if ( it.GetCIGARRefSize() < min_length ) {
                    continue;
                }
                ++long_count;
            }
            if ( long_count ) {
                cout << "found "<<long_count<<" long alignements"<<endl;
                total_long_count += long_count;
            }
            else {
                cout << "no long alignments found"<<endl;
            }
        }
        if ( total_long_count ) {
            cout << "found "<<total_long_count<<" long alignements in the whole file"<<endl;
        }
        else {
            cout << "no long alignements found in the whole file"<<endl;
        }
    }
    
    CBamMgr mgr;
    CBamDb bam_db;
    if ( args["sra"] ) {
        bam_db = CBamDb(mgr, path, index_path, CBamDb::eUseAlignAccess);
    }
    bool single_thread = args["ST"];
    int min_quality = args["min_quality"].AsInteger();

    Uint8 total_align_count = 0, total_skipped_min_quality = 0;
    Uint8 total_wrong_level_count = 0, total_wrong_indexed_level_count = 0;
    Uint8 total_wrong_range_count = 0;
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
                    Uint8 align_count = 0, skipped_min_quality = 0;
                    Uint8 wrong_level_count = 0, wrong_indexed_level_count = 0;
                    Uint8 wrong_range_count = 0;
                    try {
                        if ( bam_db ) {
                            for ( CBamAlignIterator it(bam_db, q.refseq_id,
                                                       q.refseq_range.GetFrom(),
                                                       q.refseq_range.GetLength(),
                                                       CBamAlignIterator::ESearchMode(search_mode)); it; ++it ) {
                                if ( min_quality && it.GetMapQuality() < min_quality ) {
                                    ++skipped_min_quality;
                                    continue;
                                }
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
                                                          q.refseq_range, level1, level2, search_mode); it; ++it ) {
                                if ( min_quality && it.GetMapQuality() < min_quality ) {
                                    ++skipped_min_quality;
                                    continue;
                                }
                                TSeqPos ref_pos = it.GetRefSeqPos();
                                TSeqPos ref_size = it.GetCIGARRefSize();
                                TSeqPos ref_end = ref_pos + ref_size;
                                unsigned pos_level = 0;
                                if ( ref_size ) {
                                    TSeqPos pos1 = ref_pos >> bam_raw_db.GetIndex().GetLevelBinShift(0);
                                    TSeqPos pos2 = (ref_end-1) >> bam_raw_db.GetIndex().GetLevelBinShift(0);
                                    while ( pos1 != pos2 ) {
                                        ++pos_level;
                                        pos1 >>= CBamIndex::kLevelStepBinShift;
                                        pos2 >>= CBamIndex::kLevelStepBinShift;
                                    }
                                }
                                unsigned got_level = it.GetIndexLevel();
                                if ( bam_raw_db.GetIndex().GetMinLevelBinShift() != SBamIndexDefs::kBAI_min_shift ||
                                     bam_raw_db.GetIndex().GetMaxIndexLevel() != SBamIndexDefs::kBAI_depth ) {
                                    // in-BAM bin level is valid for standard BAI parameters
                                    got_level = pos_level;
                                }
                                if ( verbose ||
                                     got_level < level1 || got_level > level2 ||
                                     got_level != pos_level ||
                                     ref_pos > q.refseq_range.GetTo() || ref_end <= q.refseq_range.GetFrom() ) {
                                    cout << "Ref: " << q.refseq_id
                                         << " at [" << ref_pos
                                         << " - " << (ref_end-1)
                                         << "] = " << ref_size
                                         << " " << it.GetCIGAR()
                                         << " Q: " << int(it.GetMapQuality())
                                         << " bin: " << it.GetIndexBin()
                                         << " @: " << it.GetFilePos()
                                         << '\n';
                                    if ( got_level < level1 || got_level > level2 ) {
                                        cout << "Wrong index level: " << got_level << endl;
                                        ++wrong_level_count;
                                    }
                                    if ( pos_level != got_level ) {
                                        cout << "Alignment indexed in wrong level: " << got_level << " vs " << pos_level << endl;
                                        ++wrong_indexed_level_count;
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
                        if ( wrong_indexed_level_count ) {
                            LOG_POST("Run("<<q.refseq_id<<"): wrong indexed level count "<<wrong_indexed_level_count);
                        }
                        if ( wrong_range_count ) {
                            LOG_POST("Run("<<q.refseq_id<<"): wrong range count "<<wrong_range_count);
                        }
                        if ( skipped_min_quality ) {
                            LOG_POST("Run("<<q.refseq_id<<"): skipped low quality count "<<skipped_min_quality);
                        }
                        total_align_count += align_count;
                        total_skipped_min_quality += skipped_min_quality;
                        total_wrong_indexed_level_count += wrong_indexed_level_count;
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
    if ( total_skipped_min_quality ) {
        cout << "Total skipped low quality count: "<<total_skipped_min_quality<<endl;
    }
    if ( total_wrong_indexed_level_count ) {
        cout << "Total wrong indexed level count: "<<total_wrong_indexed_level_count<<endl;
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
