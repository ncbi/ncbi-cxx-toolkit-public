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
 *   Make alignment density graphs from BAM files.
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/bam/bamgraph.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/bam/bamindex.hpp>
#include <sra/error_codes.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <serial/serial.hpp>
#include <serial/typeinfo.hpp>
#include <cmath>
#include <numeric>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   BAM2Graph
NCBI_DEFINE_ERR_SUBCODE_X(6);

BEGIN_SCOPE(objects)

class CSeq_entry;

static const int kDefaultOutlierMax_Byte = 50;
static const int kDefaultOutlierMax_Int = 1000000;
static const Uint8 kDefaultMinMapQuality = 4;

CBam2Seq_graph::CBam2Seq_graph(void)
    : m_MinMapQuality(-1),
      m_GraphType(eGraphType_linear),
      m_GraphValueType(eGraphValueType_byte),
      m_GraphBinSize(kDefaultGraphBinSize),
      m_OutlierMax(0),
      m_OutlierDetails(false),
      m_RawAccess(false),
      m_Estimated(false)
{
}


CBam2Seq_graph::~CBam2Seq_graph(void)
{
}


void CBam2Seq_graph::SetRefLabel(const string& label)
{
    m_RefLabel = label;
}


void CBam2Seq_graph::SetRefId(const CSeq_id& id)
{
    m_RefId = SerialClone(id);
}


void CBam2Seq_graph::SetGraphTitle(const string& title)
{
    m_GraphTitle = title;
}


void CBam2Seq_graph::SetAnnotName(const string& name)
{
    m_AnnotName = name;
}


void CBam2Seq_graph::SetSeq_inst(CRef<CSeq_inst> inst)
{
    m_Seq_inst = inst;
}


void CBam2Seq_graph::SetGraphType(EGraphType type)
{
    m_GraphType = type;
}


void CBam2Seq_graph::SetGraphValueType(EGraphValueType type)
{
    m_GraphValueType = type;
}


int CBam2Seq_graph::GetMinMapQuality(void) const
{
    return m_MinMapQuality != -1? m_MinMapQuality: kDefaultMinMapQuality;
}


void CBam2Seq_graph::SetMinMapQuality(int qual)
{
    m_MinMapQuality = qual;
}


void CBam2Seq_graph::SetGraphBinSize(TSeqPos bin_size)
{
    m_GraphBinSize = bin_size;
}


void CBam2Seq_graph::SetOutlierMax(double x)
{
    m_OutlierMax = x;
}


double CBam2Seq_graph::GetOutlierMax(void) const
{
    if ( m_OutlierMax > 1 ) {
        return m_OutlierMax;
    }
    if ( GetGraphValueType() == eGraphValueType_byte ) {
        return kDefaultOutlierMax_Byte;
    }
    else {
        return kDefaultOutlierMax_Int;
    }
}


void CBam2Seq_graph::SetOutlierDetails(bool details)
{
    m_OutlierDetails = details;
}


void CBam2Seq_graph::SetRawAccess(bool raw_access)
{
    m_RawAccess = raw_access;
}


void CBam2Seq_graph::SetEstimated(bool estimated)
{
    m_Estimated = estimated;
}


vector<Uint8> CBam2Seq_graph::CollectCoverage(CBamMgr& mgr,
                                              const string& bam_file,
                                              const string& bam_index)
{
    CBamDb db(mgr, bam_file, bam_index);
    return CollectCoverage(db);
}


vector<Uint8> CBam2Seq_graph::CollectCoverage(CBamDb& db)
{
    if ( GetEstimated() ) {
        return CollectEstimatedCoverage(db);
    }
    vector<Uint8> ret;
    TSeqPos bin_cnt = 0;
    int align_cnt = 0;
    double align_cov = 0;
    ret.reserve(1024);

    TSeqPos bin_size = GetGraphBinSize();
    TSeqPos min_pos = kInvalidSeqPos, max_pos = 0;
    TSeqPos max_align_span = 0;
    int min_qual = GetMinMapQuality();

    TSeqPos ref_length = db.GetRefSeqLength(GetRefLabel());

    typedef map<TSeqPos, Int8> TCovLevelChangeMap;
    TCovLevelChangeMap cov_level_change_map;
    const TSeqPos kMapBinCountThreshold = 20;

    const TSeqPos kWarnLongAlignThreshold = 10000;
    const size_t kWarnLongAlignCount = 10;
    size_t invalid_align_count = 0;
    size_t long_align_count = 0;
    if ( GetRawAccess() ) {
        
    }
    else {
        for ( CBamAlignIterator ait(db, GetRefLabel(), 0); ait; ++ait ) {
            if ( min_qual > 0 && ait.GetMapQuality() < min_qual ) {
                continue;
            }
            ++align_cnt;
            TSeqPos size = ait.GetCIGARRefSize();
            if ( size == 0 ) {
                continue;
            }
            TSeqPos pos = ait.GetRefSeqPos();
            //x_AddCoverage(pos, size);

            TSeqPos end = pos + size;
            if ( end > ref_length ) {
                if ( ++invalid_align_count <= kWarnLongAlignCount ) {
                    ERR_POST_X(5, Warning << "CBam2Seq_graph: "
                               "alignment is out of refseq bounds " <<
                               GetRefLabel() << " @ " << pos << ": " << size
                               << ", CIGAR: "<< ait.GetCIGAR());
                }
                else if ( invalid_align_count == kWarnLongAlignCount+1 ) {
                    ERR_POST_X(6, Warning << "CBam2Seq_graph: "
                               "there are more alignments out of refseq bounds...");
                }
                --align_cnt;
                continue;
            }
            if ( pos < min_pos ) {
                min_pos = pos;
            }
            if ( end > max_pos ) {
                max_pos = end;
            }
            if ( size > max_align_span ) {
                max_align_span = size;
            }
            align_cov += size;
            if ( size > kWarnLongAlignThreshold ) {
                if ( ++long_align_count <= kWarnLongAlignCount ) {
                    ERR_POST_X(3, Warning << "CBam2Seq_graph: "
                               "alignment is too long at " <<
                               GetRefLabel() << " @ " << pos << ": " << size
                               << ", CIGAR: "<< ait.GetCIGAR());
                }
                else if ( long_align_count == kWarnLongAlignCount+1 ) {
                    ERR_POST_X(4, Warning << "CBam2Seq_graph: "
                               "there are more very long alignments...");
                }
            }
            _ASSERT(end > pos);
            TSeqPos end_bin = (end - 1) / bin_size;
            if ( end_bin >= bin_cnt ) {
                bin_cnt = end_bin + 1;
                size_t cap = ret.capacity();
                while ( bin_cnt > cap ) {
                    LOG_POST_X(1, Info<<"CBam2Seq_graph: "
                               "Cap "<<cap<<" at "<<align_cnt<<" aligns ");
                    cap *= 2;
                }
                ret.reserve(cap);
                ret.resize(bin_cnt);
            }
            TSeqPos begin_bin = pos / bin_size;
            if ( begin_bin == end_bin ) {
                ret[begin_bin] += size;
            }
            else {
                TSeqPos begin_bin_coverage = (begin_bin + 1) * bin_size - pos;
                ret[begin_bin] += begin_bin_coverage;
                ++begin_bin;
                TSeqPos end_bin_coverage = end - end_bin * bin_size;
                ret[end_bin] += end_bin_coverage;
                // all intermediate bins are fully covered
                if ( end_bin > begin_bin + kMapBinCountThreshold ) {
                    // keep long alignment spans in a separate map
                    cov_level_change_map[begin_bin] += bin_size;
                    cov_level_change_map[end_bin] -= bin_size;
                }
                else {
                    for ( TSeqPos bin = begin_bin; bin < end_bin; ++bin ) {
                        ret[bin] += bin_size;
                    }
                }
            }
        }
    }
    if ( !cov_level_change_map.empty() ) {
        TSeqPos bin = cov_level_change_map.begin()->first;
        Uint8 level = 0;
        ITERATE ( TCovLevelChangeMap, it, cov_level_change_map ) {
            TSeqPos next_bin = it->first;
            for ( ; bin < next_bin; ++bin ) {
                ret[bin] += level;
            }
            level += it->second;
        }
    }

    m_TotalRange.SetFrom(min_pos).SetToOpen(max_pos);
    m_AlignCount = align_cnt;
    m_MaxAlignSpan = max_align_span;
    LOG_POST_X(2, Info<<"CBam2Seq_graph: "
               "Total aligns: "<<align_cnt<<
               " total size: "<<align_cov<<" "<<
               " max align span: "<<max_align_span);
    return ret;
}


vector<Uint8> CBam2Seq_graph::CollectEstimatedCoverage(CBamDb& db)
{
    if ( db.GetIndexName().empty() ) {
        NCBI_THROW(CBamException, eInvalidArg,
                   "BAM index file name is empty");
    }
    m_GraphBinSize = kEstimatedGraphBinSize;
    CBamIndex bam_index(db.GetIndexName());
    size_t ref_index = 0;
    TSeqPos length = kInvalidSeqPos;
    for ( CBamRefSeqIterator it(db); it; ++it, ++ref_index ) {
        if ( it.GetRefSeqId() == GetRefLabel() ) {
            length = it.GetLength();
            break;
        }
    }
    
    vector<uint64_t> ret = bam_index.CollectEstimatedCoverage(ref_index);
    if ( length == 0 || length == kInvalidSeqPos ) {
        length = TSeqPos(ret.size())*kEstimatedGraphBinSize;
    }
    m_TotalRange.SetFrom(0).SetToOpen(length);
    m_AlignCount = 0;
    m_MaxAlignSpan = 0;
    return ret;
}


static void sx_SetTitle(CSeq_graph& graph, CSeq_annot& annot,
                        string title, string name)
{
    if ( name.empty() ) {
        name = "BAM coverage";
    }
    if ( title.empty() ) {
        title = name;
    }
    graph.SetTitle(title);
    CRef<CAnnotdesc> desc(new CAnnotdesc);
    desc->SetName(name);
    annot.SetDesc().Set().push_back(desc);
}


CRef<CSeq_annot> CBam2Seq_graph::MakeSeq_annot(CBamMgr& mgr,
                                               const string& bam_file,
                                               const string& bam_index)
{
    CBamDb db(mgr, bam_file, bam_index);
    return MakeSeq_annot(db, bam_file);
}


CRef<CSeq_annot> CBam2Seq_graph::MakeSeq_annot(CBamDb& db,
                                               const string& bam_file)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_graph> graph(new CSeq_graph);
    annot->SetData().SetGraph().push_back(graph);
    sx_SetTitle(*graph, *annot, GetGraphTitle(), GetAnnotName());

    CRef<CAnnotdesc> desc(new CAnnotdesc);
    CUser_object& user_desc = desc->SetUser();
    user_desc.SetType().SetStr("BAM coverage");
    annot->SetDesc().Set().push_back(desc);
    user_desc.AddField("MinMapQuality", GetMinMapQuality());

    vector<Uint8> cov = CollectCoverage(db);
    if ( cov.empty() ) cov.push_back(0);
    Uint8 min_cov = kMax_UI8, max_cov = 0, sum_cov = 0;
    size_t val_count = 0;
    ITERATE ( vector<Uint8>, it, cov ) {
        Uint8 c = *it;
        if ( c != 0 ) {
            ++val_count;
            sum_cov += c;
            if ( c < min_cov ) {
                min_cov = c;
            }
            if ( c > max_cov ) {
                max_cov = c;
            }
        }
    }
    double avg_cov;
    if ( val_count == 0 ) {
        // avoid division by zero
        min_cov = 1;
        max_cov = 2;
        avg_cov = 0;
    }
    else {
        avg_cov = double(sum_cov)/val_count;
    }
    double cov_mul = 1./GetGraphBinSize();
    user_desc.AddField("SourceFile", bam_file);
    if ( m_AlignCount ) {
        if ( m_AlignCount < kMax_Int ) {
            user_desc.AddField("AlignCount", int(m_AlignCount));
        }
        else {
            user_desc.AddField("AlignCount", double(m_AlignCount));
        }
    }
    if ( m_MaxAlignSpan ) {
        user_desc.AddField("MaxAlignSpan", int(m_MaxAlignSpan));
    }
    user_desc.AddField("MinCoverage", min_cov*cov_mul);
    user_desc.AddField("MaxCoverage", max_cov*cov_mul);
    user_desc.AddField("AvgCoverage", avg_cov*cov_mul);

    CUser_field::TData::TFields* outliers = 0;
    if ( max_cov > avg_cov*GetOutlierMax() ) {
        max_cov = Uint8(avg_cov*GetOutlierMax());
        user_desc.AddField("LimitCoverage", max_cov*cov_mul);
        if ( GetOutlierDetails() ) {
            outliers =
                &user_desc.SetFieldRef("Outliers")->SetData().SetFields();
        }
    }

    graph->SetLoc().SetWhole(*SerialClone(*m_RefId));
    graph->SetComp(GetGraphBinSize());
    graph->SetNumval(TSeqPos(cov.size()));
    vector<char>* vvb = 0;
    vector<int>* vvi = 0;
    int MAX = 0;
    if ( GetGraphValueType() == eGraphValueType_byte ) {
        MAX = 254;
        CByte_graph& bytes = graph->SetGraph().SetByte();
        bytes.SetAxis(0);
        bytes.SetMin(1);
        bytes.SetMax(MAX);
        vvb = &bytes.SetValues();
        vvb->reserve(cov.size());
    }
    else {
        MAX = kMax_Int-1;
        CInt_graph& ints = graph->SetGraph().SetInt();
        ints.SetAxis(0);
        ints.SetMin(1);
        ints.SetMax(MAX);
        vvi = &ints.SetValues();
        vvi->reserve(cov.size());
    }
    
    if ( GetGraphType() == eGraphType_logarithmic ) {
        user_desc.AddField("Logarithmic", true);

        // logarithmic:
        // value = log(bin_cov/bin_size) = log(bin_cov) - log(bin_size)
        //   1 -> log(min_cov) - log(bin_size)
        // MAX -> log(max_cov) - log(bin_size)
        // v = 1 + (log(cov)-log(min_cov))*(253/(log(max_cov)-log(min_cov))) = 
        //     1 + (log(cov)-log(min_cov))*byte_mul;
        // x = log(min_cov) + (v-1)*(log(max_cov)-log(min_cov))/253 - log(bin)=
        //     v / byte_mul + log(min_cov) - 1/byte_mul - log(bin_size);
        double base = log(double(min_cov));
        double byte_mul = double(MAX-1)/(log(double(max_cov))-base);
        graph->SetA(1./byte_mul);
        graph->SetB(log(double(min_cov))-log(double(GetGraphBinSize()))-1./byte_mul);
        ITERATE ( vector<Uint8>, it, cov ) {
            Uint8 c = *it;
            int b;
            if ( c < min_cov ) {
                b = 0;
            }
            else if ( c > max_cov ) {
                b = MAX+1;
            }
            else {
                b = 1 + int((log(double(c))-base)*byte_mul+.5);
            }
            if ( vvb )
                vvb->push_back(b);
            else
                vvi->push_back(b);
        }
    }
    else {
        // linear:
        // value = average bin coverage = bin_cov / bin_size
        //   1 -> min_cov / bin_size
        // MAX -> max_cov / bin_size
        // v = 1 + (cov-min_cov) * ((MAX-1)/(max_cov-min_cov)) = 
        //     1 + (cov-min_cov) * byte_mul;
        // x*bin_size = min_cov + (v-1)*((max_cov-min_cov)/(max-1)) =
        //       v / byte_mul + min_cov - 1 / byte_mul;
        // x = v / (byte_mul*bin_size) + (min_cov - 1 / byte_mul)/bin_size;
        double byte_mul = double(MAX-1)/(max_cov-min_cov);
        graph->SetA(cov_mul/byte_mul);
        graph->SetB(cov_mul*(min_cov - 1./byte_mul));
        ITERATE ( vector<Uint8>, it, cov ) {
            Uint8 c = *it;
            int b;
            if ( c < min_cov ) {
                b = 0;
            }
            else if ( c > max_cov ) {
                b = MAX+1;
                if ( outliers ) {
                    CRef<CUser_field> field(new CUser_field);
                    field->SetLabel().SetId(int(it-cov.begin()));
                    field->SetData().SetReal(double(c));
                    outliers->push_back(field);
                }
            }
            else {
                b = 1 + int((c-min_cov)*byte_mul+.5);
            }
            if ( vvb )
                vvb->push_back(b);
            else
                vvi->push_back(b);
        }
    }
    return annot;
}


CRef<CSeq_annot> CBam2Seq_graph::MakeSeq_annot(CBamMgr& mgr,
                                               const string& bam_file)
{
    return MakeSeq_annot(mgr, bam_file, bam_file+".bai");
}


CRef<CSeq_entry> CBam2Seq_graph::MakeSeq_entry(CBamMgr& mgr,
                                               const string& bam_file,
                                               const string& bam_index)
{
    CBamDb db(mgr, bam_file, bam_index);
    return MakeSeq_entry(db, bam_file);
}


CRef<CSeq_entry> CBam2Seq_graph::MakeSeq_entry(CBamDb& db,
                                               const string& bam_file)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& seq = entry->SetSeq();
    seq.SetAnnot().push_back(MakeSeq_annot(db, bam_file));
    if ( m_RefId ) {
        CRef<CSeq_id> id(SerialClone(GetRefId()));
        seq.SetId().push_back(id);
    }
    if ( 1 ) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local, GetRefLabel()));
        id->SetLocal().SetStr(GetRefLabel());
        if ( !m_RefId || !GetRefId().Equals(*id) ) {
            seq.SetId().push_back(id);
        }
    }
    if ( m_Seq_inst ) {
        seq.SetInst(*m_Seq_inst);
    }
    else {
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_virtual);
        inst.SetMol(CSeq_inst::eMol_na);
        inst.SetLength(m_TotalRange.GetToOpen());
    }
    return entry;
}


CRef<CSeq_entry> CBam2Seq_graph::MakeSeq_entry(CBamMgr& mgr,
                                               const string& bam_file)
{
    return MakeSeq_entry(mgr, bam_file, bam_file+".bai");
}


END_SCOPE(objects)
END_NCBI_SCOPE
