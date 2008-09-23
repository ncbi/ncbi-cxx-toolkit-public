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
 * Authors:  Josh Cherry
 *
 * File Description:
 *
 */

#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Na_strand_.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE
class CSeq_id;
class CSeq_align;
class CSeq_align_set;
class CDense_seg;
class CScope;
class CAlnVec;
END_objects_SCOPE


/// This class provides alignment-related functions intended
/// for finding overlaps for contig assembly.
class NCBI_XALGOCONTIG_ASSEMBLY_EXPORT CContigAssembly
{
public:
    class NCBI_XALGOCONTIG_ASSEMBLY_EXPORT CAlnStats : public CObject
    {
    public:
        CAlnStats(unsigned int adjusted_len,
                  unsigned int mm,
                  unsigned int gaps) :
            m_AdjustedLen(adjusted_len), m_MM(mm), m_Gaps(gaps) {}
        CAlnStats(const objects::CDense_seg& ds, objects::CScope& scope);

        unsigned int GetAdjustedLength() const {return m_AdjustedLen;}
        /// Returns a fraction between 0 and 1, not a percentage
        double       GetFracIdentity() const;
        unsigned int GetNumMismatches() const {return m_MM;}
        unsigned int GetNumGaps() const {return m_Gaps;}
    private:
        unsigned int m_AdjustedLen;
        unsigned int m_MM;
        unsigned int m_Gaps;
    };

    /// Most users of the class need only to call this function.
    /// It runs blastn and, if the results are not satisfactory,
    /// tries a banded dynamic-programming alignment, using a band
    /// chosen based on the blast results.
    static vector<CRef<objects::CSeq_align> >
    Align(const objects::CSeq_id& id0, const objects::CSeq_id& id1,
          const string& blast_params, double min_ident,
          unsigned int max_end_slop, objects::CScope& scope,
          CNcbiOstream* ostr = 0,
          const vector<unsigned int>& band_halfwidths
              = vector<unsigned int>(1, 200),
          unsigned int diag_finding_window = 200,
          unsigned int min_align_length = 50,
          objects::ENa_strand strand0 = objects::eNa_strand_unknown,
          objects::ENa_strand strand1 = objects::eNa_strand_unknown);

    /// Utility for running blastn.
    // It accepts a blast parameter string such as
    // "-W 28 -r 1 -q -3 -e 1e-5 -Z 200 -F 'm L; R -d rodents.lib'"
    // (single quotes are respected)
    static CRef<objects::CSeq_align_set>
    Blastn(const objects::CSeq_id& query_id,
           const objects::CSeq_id& subject_id,
           const string& param_string, objects::CScope& scope);

    static CRef<objects::CSeq_align_set>
    Blastn(const objects::CSeq_loc& query_loc,
           const objects::CSeq_loc& subject_loc,
           const string& param_string, objects::CScope& scope);

    /// Given a set of alignments, pick out a diagonal to use as
    /// the center of a band in a banded alignment.
    static void FindDiagFromAlignSet(const objects::CSeq_align_set& align_set,
                                     objects::CScope& scope,
                                     unsigned int window_size,
                                     objects::ENa_strand& strand,
                                     unsigned int& diag);

    /// Do a banded global alignment using an arbitrary band.
    static CRef<objects::CDense_seg>
    BandedGlobalAlignment(const objects::CSeq_id& id0,
                          const objects::CSeq_id& id1,
                          objects::ENa_strand strand,
                          unsigned int diag,
                          unsigned int half_width,
                          objects::CScope& scope);

    /// Find the highest-scoring local subalignment.
    /// This function is necessary only because we don't have
    /// a banded local alignment algorithm.
    static CRef<objects::CDense_seg>
    BestLocalSubAlignment(const objects::CDense_seg& ds_in,
                          objects::CScope& scope);

    /// Count the cells with "ink" along each diagonal in a
    /// dot-matrix-type plot of some set of alignments (e.g., blast results)
    static void DiagCounts(const objects::CSeq_align_set& align_set,
                           objects::CScope& scope,
                           vector<unsigned int>& plus_vec,
                           vector<unsigned int>& minus_vec);

    /// Find the range (or more than one tied range) containing
    /// the maximal diagonal count, summed over a window.
    static void FindMaxRange(const vector<unsigned int>& vec,
                             unsigned int window,
                             unsigned int& max,
                             vector<unsigned int>& max_range);

    static bool IsDovetail(const objects::CDense_seg& ds,
                           unsigned int slop, objects::CScope& scope);
    static bool IsAtLeastHalfDovetail(const objects::CDense_seg& ds,
                                      unsigned int slop,
                                      objects::CScope& scope);
    static bool IsContained(const objects::CDense_seg& ds,
                            unsigned int slop, objects::CScope& scope);
    static double FracIdent(const objects::CDense_seg& ds,
                            objects::CScope& scope);


    /// Alignment characterization

    struct SAlignStats {

        // unaligned tails
        struct STails {
            TSeqPos left;
            TSeqPos right;
        };

        // constructor
        SAlignStats()
                : total_length(0),
                aligned_length(0),
                gap_count(0),
                mismatches(0),
                pct_identity(0)
        {
        }

        /// total covered length of the alignment, including gaps
        TSeqPos total_length;

        /// total number of bases included in the alignment
        TSeqPos aligned_length;

        /// count of total number of gaps
        TSeqPos gap_count;

        /// number of mismatched bases
        TSeqPos mismatches;

        /// % identity (varies from 0 to 100)
        double pct_identity;

        /// unaligned tails
        vector<STails> tails;

        /// the set of gap lengths for this alignment
        vector<TSeqPos> gaps;

        /// for each gap, whether is consists of "simple sequence"
        vector<bool> is_simple;
    };
    static void GatherAlignStats(const objects::CAlnVec& vec,
                                 SAlignStats& align_stats);
    static void GatherAlignStats(const objects::CDense_seg& ds,
                                 objects::CScope& scope,
                                 SAlignStats& align_stats);
    static void GatherAlignStats(const objects::CSeq_align& aln,
                                 objects::CScope& scope,
                                 SAlignStats& align_stats);

    private:
        static void x_OrientAlign(objects::CDense_seg& ds, objects::CScope& scope);
        static bool x_IsAllowedStrands(const objects::CDense_seg& ds,
                                       objects::ENa_strand strand0,
                                       objects::ENa_strand strand1);
        static TSeqPos x_DensegLength(const objects::CDense_seg& ds);
};


inline
double CContigAssembly::CAlnStats::GetFracIdentity() const
{
    return 1.0 - double(m_MM + m_Gaps) / m_AdjustedLen;
}


END_NCBI_SCOPE
