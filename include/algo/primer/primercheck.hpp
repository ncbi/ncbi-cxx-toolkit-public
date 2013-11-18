#ifndef Oligo_SPECIFICITY_CHECK_HPP
#define Oligo_SPECIFICITY_CHECK_HPP

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
 * Author:  Jian Ye
 */

/** @file primercheck.hpp
 *  primer specificity checking tool
 *
 * @{
 */

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objtools/blast/gene_info_reader/gene_info_reader.hpp>
#include <util/itree.hpp>

//forward declarations
class CPrimercheckTest;  //For internal test only

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

struct SHspInfo {
    CConstRef<CSeq_align> hsp;
    CRange<TSeqPos> master_range;
    CRange<TSeqPos> slave_range;
    double bit_score;
};
    
struct SHspIndex : CObject {
    int index;
};

typedef pair<vector<SHspInfo*>, vector<SHspInfo*> > TSortedHsp; 

class COligoSpecificityCheck; // forward declaration

class NCBI_XPRIMER_EXPORT COligoSpecificityTemplate : public CObject {
friend class COligoSpecificityCheck; 
private:

    ///bioseq handle for input bioseq
    const CBioseq_Handle& m_TemplateHandle;

    ///seqid
    CConstRef<CSeq_id> m_Id;

    ///the processed sorted hit list corresponding to the input seqalign
    vector<TSortedHsp> m_SortHit;      
  
    ///sort the hit
    void x_SortHit(const CSeq_align_set& input);

    ///the requested length margin  for non-specific template
    TSeqPos m_ProductLenMargin;

    ///minimal continuous match required
    int m_WordSize;

    ///total mismatches allowed by user
    TSeqPos m_AllowedTotalMismatch;  

    ///3' end mismatches
    TSeqPos m_Allowed3EndMismatch;   
    
    ///total max allowed mismatch
    TSeqPos m_MaxMismatch;

    ///range on the input template
    CRange<TSeqPos> m_TemplateRange;

    //set up interval tree
    vector<CIntervalTree*> m_RangeTreeListPlusStrand;
    vector<CIntervalTree*> m_RangeTreeListMinusStrand;

    ///count splice variants as non-specific?
    bool m_AllowTranscriptVariants;

    ///user specified hits that can be disregarded for specificity checking
    vector<TSeqPos> m_AllowedSeqidIndex;

    bool m_UseITree;

    ///the length or region at the 3' end for checking mismatches
    TSeqPos m_MismatchRegionLength3End;

    int m_MaxHSPSize;

    ///the number non-specific targets to return
    int m_NumNonSpecificTarget;

    CSeq_id::EAccessionInfo m_TemplateType;


    ///constructor
    ///@template_handle: bioseq represents the pcr template
    ///@param input_seqalign: the alignment represening the PCR template hits from
    ///the specificity checking database 
    ///@param scope: scope to fetch the sequences in alignment
    ///@word_size: target must have at least this long consecutive match to primer
    ///to be detected.
    ///@param allowed_total_mismatch: the total mismatch threshhold below which a 
    ///primer hit pair representing a PCR product will be reported.  
    ///Note that this number is inclusive 
    ///and applies for both the left and right primer hit. Gaps are counted a mismatches.
    ///@param allowed_3end_mismatch: Same as allowed_total_mismatch except that the 
    ///mismatches are counted only if they are in the 3' end region as specified in 
    ///SetMismatchRegionLength3End().  Note that both allowed_total_mismatch and 
    ///allowed_3end_mismatch must be satisfied for a primer hit pair to be reported.
    ///@param allow_transcript_variants: a transcript variant from same gene 
    ///as the input template will not be reported as non-specific primer hit.
    COligoSpecificityTemplate(const CBioseq_Handle& template_handle,
                           const CSeq_align_set& input_seqalign,
                           CScope& scope,
                           int word_size,
                           TSeqPos allowed_total_mismatch = 1,
                           TSeqPos allowed_3end_mismatch = 1,
                           TSeqPos max_mismatch = 7,
                           bool allow_transcript_variants = false);

    //destructor
    ~COligoSpecificityTemplate();
    
    ///Set start and end position of the input PCR template
    ///@param range: the range
    void SetTemplateRange(CRange<TSeqPos> range) {
        m_TemplateRange = range;
    }

    ///Set the allowed pcr product length margin for the non-specific template
    ///@param length: the length margin
    void SetPcrLengthMargin(TSeqPos length) {
        m_ProductLenMargin = length;
    }

    ///Set mismatch threshhold below which a 
    ///primer hit pair representing a PCR product will be reported.
    ///@param allowed_total_mismatch: the total mismatch threshhold below which a 
    ///primer hit pair representing a PCR product will be reported.  
    ///Note that this number is inclusive 
    ///and applies for both the left and right primer hit. Gaps are counted a mismatches.
    ///@param allowed_3end_mismatch: Same as allowed_total_mismatch except that the 
    ///mismatches are counted only if they are in the 3' end region as specified in 
    ///SetMismatchRegionLength3End().  Note that both allowed_total_mismatch and 
    ///allowed_3end_mismatch must be satisfied for a primer hit pair to be reported.
    void SetAllowedMismatch(TSeqPos allowed_total_mismatch, 
                            TSeqPos allowed_3end_mismatch) {
        m_AllowedTotalMismatch = allowed_total_mismatch;
        m_Allowed3EndMismatch = allowed_3end_mismatch;
    }

    //hit that have equal to or more than this mismatch number will be ignored
    void SetMaxMismatchDetection(TSeqPos max_mismatch) {
        m_MaxMismatch = max_mismatch;
    }


    ///whether to count transcript of the same gene as non-specific
    ///@param allow_transcript_variants: a transcript variant from same gene 
    ///as the input template will not be reported as non-specific primer hit.
    ///
    void SetAllowTranscriptVariants(bool allow_transcript_variants) {
        m_AllowTranscriptVariants = allow_transcript_variants;
    }

    ///Allowed seqid will not be counted as non-specific hits
    ///@param allowed_seqid_index:  the position index for allowed seqid
    ///
    void SetAllowedSeqid(const vector<TSeqPos>& allowed_seqid_index) {
        m_AllowedSeqidIndex = allowed_seqid_index;
    }

    ///The maximal number of non-specific targets to return.  
    ///Default is 20 if not specified
    ///@param number: the number
    ///
    void SetNumNonSpecificTargets(int number) {
        m_NumNonSpecificTarget = number;
    }

    ///the length of region at the 3' end of primer during which the mismatched are
    ///counted.  The idea is that these mismatches affect the PCR amplification 
    ///much more than those at the 5' end.
    ///@param length: the length. Default is 10 if not specified
    void SetMismatchRegionLength3End(TSeqPos length) {
        m_MismatchRegionLength3End = length;
    }
};

class NCBI_XPRIMER_EXPORT COligoSpecificityCheck : public CObject {
public:

    ///the input primer pair information
    ///only the left and right range information is needed by this API
    ///
    struct SPrimerInfo {
        CRange<TSeqPos> left;                     //left primer range 
        CRange<TSeqPos> right;                    //right primer range
        CRange<TSeqPos> internal_probe;           //internal probe range
        double left_tm;                           //left primer melting temprature
        double right_tm;                          //right primer melting temprature
        double internal_probe_tm;                 //internal probe melting temprature
        double left_gc_percent;                   //left primer GC content
        double right_gc_percent;                  //right primer GC content
        double internal_probe_gc_percent;         //internal probe GC content
        string left_seq;                          //left primer sequence
        string right_seq;                         //right primer sequence
        string internal_probe_seq;                //internal probe sequence
        double right_self_complementarity_any;
        double right_self_complementarity_3end;
        double left_self_complementarity_any;
        double left_self_complementarity_3end;
        double pair_complementarity_any;
        double pair_complementarity_3end;
        double product_tm; 
        double product_tm_oligo_tm_diff;
        int left_primer_splice_site_pos;
        int right_primer_splice_site_pos;
        int left_primer_genomic_exon_start;
        int left_primer_genomic_exon_end;
        int right_primer_genomic_exon_start;
        int right_primer_genomic_exon_end;
        int total_intron_size;
        string genomic_dna_acc;
        int genomic_dna_gi;
    };


    struct SHspIndexInfo {
        int index;
        double bit_score;
    };
    

    typedef pair<CRef<CSeq_align>, CRef<CSeq_align> > TAlignmentPair;

    ///primer hit to the blast dababase
    struct SPrimerHitInfo {
        bool self_forward_primer;              //Is the pcr product by forward_primer alone?
        bool self_reverse_primer;              //Is the pcr product by reverse_primer alone?
        TSeqPos left_total_mismatch;           //total mismatch between left primer and hit
        TSeqPos left_3end_mismatch;            //mismatch at the 3' end
        TSeqPos left_total_gap;                //total gaps between left primer and hit
        TSeqPos left_3end_gap;                 //gaps at the 3' end
        TSeqPos right_total_mismatch;          
        TSeqPos right_3end_mismatch;         
        TSeqPos right_total_gap;                  
        TSeqPos right_3end_gap;
        int product_len;                       //pcr product length on the hit
        TAlignmentPair aln;                    //the alignments for left-hit and right-hit
        TSeqPos index;                         //the position index in m_SortHit
    };
    
   
    //constructor
    COligoSpecificityCheck(const COligoSpecificityTemplate* temp,
                           CScope& scope);

    //destructor
    ~COligoSpecificityCheck();
    

    
    ///check the specificity of the primer pairs. Note that this funtion can
    ///can be called repeatedly with different primer_info after constructor call, 
    ///assuming the input seqalign is the same. Subsequent calls will be very fast
    ///if the primer window has been checked before as the primer match info is
    ///cached
    ///@param primer_info: the primer to be checked for specificity
    ///@param from: the starting index of primer to be checked for specificity
    ///@param to: the ending index + 1 of primer to be checked for specificity
    ///
    void CheckSpecificity(const vector<SPrimerInfo>& primer_info_list, 
                          int from = 0, int to = -1);

    ///return the non-specific hits that the primer pairs can amplify
    ///
    const vector<vector<SPrimerHitInfo> >& GetNonSpecificTargetList() const {
        return m_PrimerHit;
    }

    ///return the hits the primer pair can amplify but users want to ignore for specificity
    //checking
    ///
    const vector<vector<SPrimerHitInfo> >& GetAllowedTargetList() const {
        return m_AllowedHit;
    }

    ///retrun the hit whose seqid is the same as the template hit itself.  
    ///
    const vector<vector<SPrimerHitInfo> >& GetSelfTargetList() const {
        return m_SelfHit;
    }

    ///return the hits that represent the transcript variant from the same gene
    ///as the input pcr template.
    ///
    const vector<vector<SPrimerHitInfo> >& GetTranscriptVariantTargetList() const {
        return m_VariantHit;
    }

 
private:
    ///the information about the blast results
    const COligoSpecificityTemplate* m_Hits;

    ///scope to fetch sequence
    CRef<CScope> m_Scope;

    ///the information about primer to be checked
    vector<const SPrimerInfo *> m_PrimerInfoList;
    
    //current primer
    const SPrimerInfo* m_PrimerInfo;

    ///the non-specific hit for the primer pair
    vector<vector<SPrimerHitInfo> > m_PrimerHit;

    ///the hit represent the input template
    vector<vector<SPrimerHitInfo> > m_SelfHit;

    ///the hit that user choose to ingnore for specificity
    vector<vector<SPrimerHitInfo> > m_AllowedHit;

    ///the hits represent the transcript variants from the same gene as the input template
    vector<vector<SPrimerHitInfo> >m_VariantHit;

    ///Analyze the primer pair specificity
    void x_AnalyzePrimerSpecificity();

    ///Analyze the the primer pair specificity usign both left and right primer at ends 
    ///@param sorted_hsp: contains seqalign from the same slave sequence
    ///@param hit_strand: the strand to use as the template
    ///@param index:  the array index of sorted_hsp in m_SortHit list.
    void x_AnalyzeTwoPrimers(const TSortedHsp& sorted_hsp,
                             TSeqPos index);
   
    ///analyze the case where the left primer itself can serve as both 
    ///left and right primer
    ///@param left_primer_hit_align: the alignment covering the left primer window
    ///@param left_total_mismatch: total mismatches in left primer alignment
    ///@param left_3end_mismatch: mismatches at 3' end in left primer alignment
    ///@param left_total_gap: total gaps in alignment
    ///@param left_3end_gap: gaps at 3' end region
    ///@param hit_list: the template hit list from the specificity checking database
    ///@param primer_order_reversed: set to true for the case where
    ///right primer can serve as both left and right primer
    void x_AnalyzeOnePrimer(const vector<SHspInfo*>& plus_strand_hsp_list,
                            const vector<SHspInfo*>& minus_strand_hsp_list,
                            int HspOverlappingWithLeftPrimer_size,
                            int HspOverlappingWithRightPrimer_size,
                            int HspOverlappingWithLeftPrimerMinusStrand_size,
                            int HspOverlappingWithRightPrimerMinusStrand_size,
                            TSeqPos hit_index);
    
    ///return alignment for the full primer window.  Parital alignment will be extended
    ///with NW global alignment.
    ///@param desired_align_range: the desired primer window
    ///@param input_hit: seqalign contains input template and the hits
    ///@param num_total_mismatch: total mismatches to be filled
    ///@param num_3end_mismatch: 3' end mismatches to be filled
    ///@param num_total_gap: total gaps to be filled
    ///@param num_3end_gap: 3' end gaps to be filled
    ///@param is_left_primer: Is the the left primer?
    ///@return: the alignment representing the desired primer window
    CRef<CSeq_align> 
    x_FillGlobalAlignInfo(const CRange<TSeqPos>& desired_align_range,
                          SHspInfo* input_hsp_info,
                          TSeqPos& num_total_mismatch,
                          TSeqPos& num_3end_mismatch, 
                          TSeqPos& num_total_gap, 
                          TSeqPos& num_3end_gap,
                          bool is_left_primer,
                          TSeqPos index,
                          ENa_strand hit_strand);

    ///Test if the primer pair generates the pcr product in specified length range 
    ///and fill the actual length
    ///@param left_primer_hit_align: alignment for the left primer
    ///@param right_primer_hit_align: alignment for the right primer
    ///@param product_len: length to be filled.
    bool x_IsPcrLengthInRange(const CSeq_align& left_primer_hit_align, 
                              const CSeq_align& right_primer_hit_align,
                              bool primers_on_different_strand,
                              ENa_strand hit_strand,
                              int& product_len); 

    ///save the primer informaton
    ///@param left_align: alignment for the left primer
    ///@param right_align: alignment for the right primer
    ///@param left_total_mismatch: total mismatches in left primer alignment
    ///@param left_3end_mismatch: mismatches at 3' end in left primer alignment
    ///@param left_total_gap: total gaps in alignment
    ///@param left_3end_gap: gaps at 3' end region
    ///@param right_total_mismatch: total mismatches in left primer alignment
    ///@param right_3end_mismatch: mismatches at 3' end in left primer alignment
    ///@param right_total_gap: total gaps in alignment
    ///@param right_3end_gap: gaps at 3' end region
    ///@param product_len: product length
    ///@param index: the position index is seqalign
    ///@param one_primer_case: is this the one primer case?
    void x_SavePrimerInfo(CSeq_align& left_align,
                          CSeq_align& right_align,
                          TSeqPos left_total_mismatch,
                          TSeqPos left_3end_mismatch,
                          TSeqPos left_total_gap,
                          TSeqPos left_3end_gap,
                          TSeqPos right_total_mismatch,
                          TSeqPos right_3end_mismatch,
                          TSeqPos right_total_gap,
                          TSeqPos right_3end_gap,
                          int product_len,
                          TSeqPos index,
                          bool is_self_forward_primer,
                          bool is_self_reverse_primer); 

    void x_AnalyzeLeftAndRightPrimer(const vector<SHspInfo*>& hsp_list, 
                                     ENa_strand hit_strand,
                                     int HspOverlappingWithLeftPrimer_size,
                                     int HspOverlappingWithRightPrimer_size,
                                     TSeqPos hit_index);
    
    
    void x_GetCachedAlnRange(CRange<TSeqPos>& master_range, 
                             CRange<TSeqPos>& hit_range,
                             bool get_master_range,
                             bool get_hit_range,
                             const CSeq_align& input_hit);

    CRange<TSeqPos> x_GetSlaveRangeGivenMasterRange(const CSeq_align& input_align,
                                                    CRange<TSeqPos>& master_range,
                                                    int index); 

    void x_FindOverlappingHSP(SHspIndexInfo* left_window_index_list,
                              int& left_window_index_list_size,
                              SHspIndexInfo* right_window_index_list,
                              int& right_window_index_list_size,
                              const CRange<TSeqPos>& left_window_desired_range,
                              const CRange<TSeqPos>& right_window_desired_range,
                              ENa_strand hit_strand,
                              TSeqPos hit_index,
                              const vector<SHspInfo*>& hsp_list);
    
    
    void x_SortPrimerHit(vector<vector<SPrimerHitInfo> >& primer_hit_list_list);

    bool x_SequencesMappedToSameTarget(CSeq_id::EAccessionInfo hit_type,
                                       const CSeq_align& left_align,
                                       const CSeq_align& right_align);



    void x_FindMatchInfoForAlignment(CDense_seg& primer_denseg,
                                     bool& end_gap,
                                     TSeqPos& num_total_mismatch, 
                                     TSeqPos& num_3end_mismatch, 
                                     TSeqPos& num_total_gap,
                                     TSeqPos& num_3end_gap,
                                     bool is_left_primer,
                                     int& max_num_continuous_match,
                                     CRange<TSignedSeqPos>& aln_range);

    CRef<CDense_seg> x_NW_alignment (const CRange<TSeqPos>& desired_align_range,
                                     const CSeq_align& input_hit,
                                     TSeqPos& num_total_mismatch, 
                                     TSeqPos& num_3end_mismatch, 
                                     TSeqPos& num_total_gap,
                                     TSeqPos& num_3end_gap,
                                     bool is_left_primer,
                                     int& max_num_continuous_match,
                                     int& align_length,
                                     TSeqPos master_local_start,
                                     TSeqPos master_local_stop,
                                     ENa_strand hit_strand,
                                     bool& nw_align_modified);

  
    ///the requested pcr length for non-specific template
    TSeqPos m_SpecifiedProductLen;
    
    int m_CurrentPrimerIndex;

    ///the actual length of the pcr product on the input template
    TSeqPos m_ActualProductLen;

    //scope associated with server for fetching feature info
    CRef<CObjectManager> m_FeatureOM;
    CRef<CScope> m_FeatureScope;

    /// value for coordinate-alignment cache
    struct SPrimerMatch {
        TSeqPos num_total_mismatch;       ///total mismatchs
        TSeqPos num_3end_mismatch;        ///3' end mismatches
        TSeqPos num_total_gap;            ///total gaps
        TSeqPos num_3end_gap;             ///3' end gaps
        CRef<CSeq_align> aln;             ///alignment for this primer and hit
    };

    
    /// key for coordinate-alignment cache 
    struct SAlnCache {
        int hit_id;
        int primer_start;
        int primer_stop;
        int master_start;
        int hit_start;
        bool is_positive_strand;
        bool is_left_primer;
    };

    /// strict weak ordering functor
    struct sort_order {
        bool operator()(const SAlnCache s1, const SAlnCache s2) const
        {
         
            if (s1.primer_start != s2.primer_start)
                return s1.primer_start < s2.primer_start ? true :  
                    false;
            
            if (s1.primer_stop != s2.primer_stop)
                return s1.primer_stop < s2.primer_stop ? true : false;
            
            if (s1.master_start != s2.master_start)
                return s1.master_start < s2.master_start ? true :  
                    false;
            
            if (s1.hit_start != s2.hit_start)
                return s1.hit_start < s2.hit_start ? true : false;
            
            if (s1.is_positive_strand != s2.is_positive_strand)
                return s1.is_positive_strand < s2.is_positive_strand ? true : false;
            
            if (s1.is_left_primer != s2.is_left_primer)
                return s1.is_left_primer < s2.is_left_primer ?  
                    true : false;
            
            if (s1.hit_id != s2.hit_id)
                return s1.hit_id < s2.hit_id ? true : false;
            
            return false;
        }
    };
    
    /// cache coordinate-alignment mapping
    map <SAlnCache, SPrimerMatch, sort_order> m_Cache;


    struct SSlaveRange {
        unsigned align_index;
        int master_start;
        int master_stop;
    };
    
    struct slave_range_sort_order {
        bool operator()(const SSlaveRange s1, const SSlaveRange s2) const
        {
         
            if (s1.align_index != s2.align_index)
                return s1.align_index < s2.align_index ? true : false;
            
            if (s1.master_start != s2.master_start)
                return s1.master_start < s2.master_start ? true : false;

            if (s1.master_stop != s2.master_stop)
                return s1.master_stop < s2.master_stop ? true : false;
            
            return false;
        }
    };
    
    vector<map <SSlaveRange, CRange<TSeqPos>, slave_range_sort_order> >m_SlaveRangeCache;

    ///tool to get gene id
    CGeneInfoFileReader m_FileReader;

    SHspIndexInfo* m_HspOverlappingWithLeftPrimer;
    SHspIndexInfo* m_HspOverlappingWithRightPrimer;
    SHspIndexInfo* m_HspOverlappingWithLeftPrimerMinusStrand;
    SHspIndexInfo* m_HspOverlappingWithRightPrimerMinusStrand;

    //For internal test 
    friend class ::CPrimercheckTest;

};
    
END_objects_SCOPE
END_NCBI_SCOPE


/* @} */
#endif
