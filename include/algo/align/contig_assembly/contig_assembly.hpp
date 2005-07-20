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

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE
class CSeq_id;
class CSeq_align;
class CSeq_align_set;
class CDense_seg;
class CScope;
END_objects_SCOPE


/// This class provides alignment-related functions intended
/// for finding overlaps for contig assembly.
class NCBI_XALGOCONTIG_ASSEMBLY_EXPORT CContigAssembly
{
public:
    /// Most users of the class need only to call this function.
    /// It runs blastn and, if the results are not satisfactory,
    /// tries a banded dynamic-programming alignment, using a band
    /// chosen based on the blast results.
    static vector<CRef<objects::CSeq_align> >
    Align(const objects::CSeq_id& id0, const objects::CSeq_id& id1,
          const string& blast_params, double min_ident,
          unsigned int max_end_slop, objects::CScope& scope,
          CNcbiOstream* ostr = 0, unsigned int band_halfwidth = 200,
          unsigned int diag_finding_window = 200);

    /// Utility for running blastn.
    // It accepts a blast parameter string such as
    // "-W 28 -r 1 -q -3 -e 1e-5 -Z 200 -F 'm L; R -d rodents.lib'"
    // (single quotes are respected)
    static CRef<objects::CSeq_align_set>
    Blastn(const objects::CSeq_id& query_id,
           const objects::CSeq_id& subject_id,
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
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/07/20 15:28:55  jcherry
 * Added export specifier for CContigAssembly
 *
 * Revision 1.1  2005/06/16 17:30:23  jcherry
 * Initial version
 *
 * ===========================================================================
 */

