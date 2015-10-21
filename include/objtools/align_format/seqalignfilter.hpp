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
 * Authors:  Vahram Avagyan
 *
 */

/// @file seqalignfilter.hpp
/// Defines the alignment filtering class.
///

#ifndef OBJTOOLS_ALIGN_FORMAT___SEQALIGN_FILTER__HPP
#define OBJTOOLS_ALIGN_FORMAT___SEQALIGN_FILTER__HPP

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)

/// CSeqAlignFilter
///
/// Alignment filtering class.
///
/// This class provides functionality for filtering a set of sequence
/// alignments by a list of gi's. Supports different I/O options. Filtering can
/// take into account the equivalence of gi's in a database.

class NCBI_ALIGN_FORMAT_EXPORT CSeqAlignFilter
{
public:
    //--- Types ---//

    /// EResultsFormat - output options for filtered seqaligns.
    ///
    /// eMultipleSeqaligns - one seqalign in the original set is replaced with
    /// zero or more seqaligns in the filtered set, with updated gi's.
    ///
    /// eCombined - generates one new seqalign with multiple "use_this_gi" entries when more
    /// than one equivalent gi's are found.
    ///
    enum EResultsFormat {
        eMultipleSeqaligns,
        eCombined
    };

    //--- Construction/destruction ---//

    /// Constructor
    ///
    /// @param eFormat
    ///   EResultsFormat constant to specify how the filtered alignments should be generated.
    ///
    CSeqAlignFilter(EResultsFormat eFormat = eCombined);

    /// Destructor
    virtual ~CSeqAlignFilter();

    //--- File-based top-level interface ---//

    /// Filter Seqaligns - file-based version.
    ///
    /// Filters the given seqalign set by the given gi list and
    /// stores the new seqalign set in file.
    ///
    /// @param fname_in_seqaligns
    ///   Name of the input file containging the original seqalign set.
    /// @param fname_out_seqaligns
    ///   Name of the output file for the filtered seqalign set.
    /// @param fname_gis_to_filter
    ///   Name of the input file containing the gi list.
    /// @sa FilterSeqalignsExt(), FilterByGiListFromFile().
    ///
    void FilterSeqaligns(const string& fname_in_seqaligns,
                         const string& fname_out_seqaligns,
                         const string& fname_gis_to_filter);

    /// Filter Seqaligns - extended file-based version.
    ///
    /// Filters the given seqalign set using a CSeqDB database for advanced lookup of gi's
    /// and stores the new seqalign set in file. The function may replace a gi of an aligned
    /// sequence with one or more of its equivalent gi's.
    ///
    /// @param fname_in_seqaligns
    ///   Name of the input file containging the original seqalign set.
    /// @param fname_out_seqaligns
    ///   Name of the output file for the filtered seqalign set.
    /// @param db
    ///   A CSeqDB object that will be used for gi lookup and equivalence tests.
    /// @sa FilterBySeqDB().
    ///
    void FilterSeqalignsExt(const string& fname_in_seqaligns,
                            const string& fname_out_seqaligns,
                            CRef<CSeqDB> db);

    //--- Main seqalign filtering functions ---//

    /// Filter Seqaligns using a gi-list stored in file.
    ///
    /// @param full_aln
    ///   Original seqalign set.
    /// @param fname_gis_to_filter
    ///   Name of the input file containing the gi list.
    /// @param filtered_aln
    ///   Output: filtered set of alignments.
    ///
    void FilterByGiListFromFile(const objects::CSeq_align_set& full_aln,
                                const string& fname_gis_to_filter,
                                objects::CSeq_align_set& filtered_aln);

    /// Filter Seqaligns using a list of integers as the gi-list.
    ///
    /// @param full_aln
    ///   Original seqalign set.
    /// @param list_gis
    ///   List of gi's.
    /// @param filtered_aln
    ///   Output: filtered set of alignments.
    ///
    void FilterByGiList(const objects::CSeq_align_set& full_aln,
                        const list<TGi>& list_gis,
                        objects::CSeq_align_set& filtered_aln);

    /// Filter Seqaligns using a SeqDB object.
    ///
    /// @param full_aln
    ///   Original seqalign set.
    /// @param db
    ///   A CSeqDB object that will be used for gi lookup and equivalence tests.
    /// @param filtered_aln
    ///   Output: filtered set of alignments.
    ///
    void FilterBySeqDB(const objects::CSeq_align_set& full_aln,
                        CRef<CSeqDB> db,
                        objects::CSeq_align_set& filtered_aln);

    //--- Auxiliary methods used for seqalign filtering ---//

    /// Load a SeqDB database with the given gi-list.
    CRef<CSeqDB> PrepareSeqDB(const string& fname_db, bool is_prot,
                                const string& fname_gis_to_filter);

    /// Read a seqalign set from file.
    void ReadSeqalignSet(const string& fname, objects::CSeq_align_set& aln);

    /// Write a seqalign to a file.
    void WriteSeqalignSet(const string& fname, const objects::CSeq_align_set& aln);

    /// Read a gi list from a file and, optionally, sort it.
    void ReadGiList(const string& fname, list<TGi>& list_gis, bool sorted = false);

    /// Read a gi vector from a file and, optionally, sort it.
    void ReadGiVector(const string& fname, vector<TGi>& vec_gis, bool sorted = false);

private:
    //--- Internal methods ---//

    /// Create one or more seqalign objects for output, based on the given
    /// input seqalign and the list of gi's to be included in the output.
    void x_CreateOusputSeqaligns(CConstRef<objects::CSeq_align> in_aln, TGi in_gi,
                                objects::CSeq_align_set& out_aln, const vector<TGi>& out_gi_vec);

    /// Generate the list of gi's based on the old list and the newly available gi's.
    void x_GenerateNewGis(TGi main_old_gi,                        // in: main gi stored before filtering
                        const vector<TGi>& vec_old_extra_gis,    // in: extra gi's stored before filtering
                        const vector<TGi>& vec_out_gis,            // in: list of all gi's to remain after filtering
                        TGi& main_new_gi,                        // out: main gi after filtering
                        vector<TGi>& vec_new_extra_gis);        // out: extra gi's after filtering

    /// Change the gi of one of the sequences referenced in the seqalign object.
    CRef<objects::CSeq_align> x_UpdateGiInSeqalign(CConstRef<objects::CSeq_align> sa, unsigned int n_row,
                                        TGi old_gi, TGi new_gi, bool& success);

    /// Read the "use_this_gi" entries from a seqalign object.
    void x_ReadExtraGis(CConstRef<objects::CSeq_align> sa, vector<TGi>& vec_extra_gis);

    // Write new "use_this_gi" entries to a seqalign object.
    void x_WriteExtraGis(CRef<objects::CSeq_align> sa, const vector<TGi>& vec_extra_gis);

    // Remove all the "use_this_gi" entries from a seqalign object.
    void x_RemoveExtraGis(CRef<objects::CSeq_align> sa);

    // Add one new "use_this_gi" entry to a seqalign object.
    bool x_AddUseGiEntryInSeqalign(CRef<objects::CSeq_align> sa, TGi new_gi);

    //--- Internal data ---//

    EResultsFormat m_eFormat;
};

END_SCOPE(align_format)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_FORMAT___SEQALIGN_FILTER__HPP */
