#ifndef SRA__READER__BAM__BAMGRAPH__HPP
#define SRA__READER__BAM__BAMGRAPH__HPP
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

#include <corelib/ncbistd.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_graph;
class CSeq_inst;
class CBamMgr;
class CBamDb;

/////////////////////////////////////////////////////////////////////////////
///  CBam2Seq_graph
///
///  Generate Seq-graph object with sequence coverage in BAM file.
///  The graph is generated in Seq-annot with descriptors,
///  or Seq-entry with virtual sequence and the Seq-annot.
///
///  The graph has 2 special values:
///  0 - no alignments in the bin at all.
///  max (255 for byte graph, and kMax_Int = 0x7fffffff = 2147483647 for int) -
///      values exceding limit (see also SetOutlierMax() and GetOutlierMax()).
///  All remainging values will be scaled to the range [1..max-1].
///  The outliers values (if any) will be encoded in Seq-annot descriptors,
///  field "Outliers", data fields for each outlier as an User-field with
///  the outlier bin number in its 'label id', and value in its 'data real'.
/////////////////////////////////////////////////////////////////////////////

class NCBI_BAMREAD_EXPORT CBam2Seq_graph
{
public:
    CBam2Seq_graph(void);
    ~CBam2Seq_graph(void);
    
    /// Label of the reference sequence in the BAM file
    const string& GetRefLabel(void) const;
    void SetRefLabel(const string& ref_label);

    /// Seq-id for the reference sequence in generated entry
    const CSeq_id& GetRefId(void) const;
    void SetRefId(const CSeq_id& ref_id);

    /// Title of generated Seq-graph
    const string& GetGraphTitle(void) const;
    void SetGraphTitle(const string& title);

    /// Annot name of generated Seq-graph
    const string& GetAnnotName(void) const;
    void SetAnnotName(const string& name);

    /// Use specified Seq-inst object for the virtual sequence
    void SetSeq_inst(CRef<CSeq_inst> inst);

    /// Minimal map quality of alignments to include in graph
    int GetMinMapQuality(void) const;
    void SetMinMapQuality(int qual);

    /// Type of graph coverage axis - linear or logarithmic
    enum EGraphType {
        eGraphType_linear, // default
        eGraphType_logarithmic
    };
    EGraphType GetGraphType(void) const;
    void SetGraphType(EGraphType type);
    
    /// Type of graph values - byte (0-255) or int
    enum EGraphValueType {
        eGraphValueType_byte, // default
        eGraphValueTyps_int
    };
    EGraphValueType GetGraphValueType(void) const;
    void SetGraphValueType(EGraphValueType type);
    
    /// Size of sequence bins in bases corresponding to one graph value
    enum {
        kDefaultGraphBinSize = 1000,
        kEstimatedGraphBinSize = 1<<14
    };
    TSeqPos GetGraphBinSize(void) const;
    void SetGraphBinSize(TSeqPos bin_size);
    
    /// Limit too big graph values by a multiple of their average
    double GetOutlierMax(void) const;
    void SetOutlierMax(double x);
    bool GetOutlierDetails(void) const;
    void SetOutlierDetails(bool details = true);

    /// try to use raw BAM file access for efficiency
    bool GetRawAccess(void) const;
    void SetRawAccess(bool raw_access = true);
    
    /// make estimated graph using BAM index only
    /// the bin size will be always kEstimatedGraphBinSize
    bool GetEstimated(void) const;
    void SetEstimated(bool estimated = true);
    
    /// Generate raw align coverage for BAM file using BAM file index
    vector<Uint8> CollectCoverage(CBamMgr& mgr,
                                  const string& bam_file,
                                  const string& bam_index);
    vector<Uint8> CollectCoverage(CBamDb& db);
    vector<Uint8> CollectEstimatedCoverage(CBamDb& db);
    vector<Uint8> CollectEstimatedCoverage(const string& bam_file,
                                           const string& bam_index);
    
    /// Generate Seq-annot for BAM file using BAM file index
    CRef<CSeq_annot> MakeSeq_annot(CBamMgr& mgr,
                                   const string& bam_file,
                                   const string& bam_index);
    /// Generate Seq-annot for BAM file using default BAM file index (.bai)
    CRef<CSeq_annot> MakeSeq_annot(CBamMgr& mgr,
                                   const string& bam_file);
    CRef<CSeq_annot> MakeSeq_annot(CBamDb& db,
                                   const string& bam_file);
    CRef<CSeq_annot> MakeSeq_annot(const vector<Uint8>& cov,
                                   const string& bam_file);
    /// Generate Seq-entry for BAM file
    CRef<CSeq_entry> MakeSeq_entry(CBamMgr& mgr,
                                   const string& bam_file,
                                   const string& bam_index);
    /// Generate Seq-entry for BAM file using default BAM file index (.bai)
    CRef<CSeq_entry> MakeSeq_entry(CBamMgr& mgr,
                                   const string& bam_file);
    CRef<CSeq_entry> MakeSeq_entry(CBamDb& db,
                                   const string& bam_file);
    CRef<CSeq_entry> MakeSeq_entry(CRef<CSeq_annot> annot);

private:
    // parameters
    string          m_RefLabel;
    CRef<CSeq_id>   m_RefId;
    string          m_GraphTitle;
    string          m_AnnotName;
    CRef<CSeq_inst> m_Seq_inst;
    int             m_MinMapQuality;
    EGraphType      m_GraphType;
    EGraphValueType m_GraphValueType;
    TSeqPos         m_GraphBinSize;
    double          m_OutlierMax;
    bool            m_OutlierDetails;
    bool            m_RawAccess;
    bool            m_Estimated;

    // statistics
    CRange<TSeqPos> m_TotalRange;
    Uint8           m_AlignCount;
    TSeqPos         m_MaxAlignSpan;
};


/////////////////////////////////////////////////////////////////////////////
// Inline methods for class CBam2Seq_graph

inline const string& CBam2Seq_graph::GetRefLabel(void) const
{
    return m_RefLabel;
}


inline const CSeq_id& CBam2Seq_graph::GetRefId(void) const
{
    return *m_RefId;
}


inline const string& CBam2Seq_graph::GetGraphTitle(void) const
{
    return m_GraphTitle;
}


inline const string& CBam2Seq_graph::GetAnnotName(void) const
{
    return m_AnnotName;
}


inline CBam2Seq_graph::EGraphType CBam2Seq_graph::GetGraphType(void) const
{
    return m_GraphType;
}


inline CBam2Seq_graph::EGraphValueType CBam2Seq_graph::GetGraphValueType(void) const
{
    return m_GraphValueType;
}


inline TSeqPos CBam2Seq_graph::GetGraphBinSize(void) const
{
    return m_GraphBinSize;
}


inline bool CBam2Seq_graph::GetOutlierDetails(void) const
{
    return m_OutlierDetails;
}


inline bool CBam2Seq_graph::GetRawAccess(void) const
{
    return m_RawAccess;
}


inline bool CBam2Seq_graph::GetEstimated(void) const
{
    return m_Estimated;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAMGRAPH__HPP
