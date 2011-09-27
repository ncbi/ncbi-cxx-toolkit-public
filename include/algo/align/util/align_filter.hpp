#ifndef GPIPE_COMMON___ALIGN_FILTER__HPP
#define GPIPE_COMMON___ALIGN_FILTER__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <util/qparse/query_parse.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <algo/align/util/score_lookup.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_annot;
    class CScope;
END_SCOPE(objects)


///
/// CAlignFilter exposes a query language for inspecting properties and scores
/// placed on Seq-align objects.  The query language supports a wide variety of
/// parameters and language structures.
///
/// Basic syntax
/// ------------
///
///  - Queries can contain any mix of balanced parentheses
///  - Queries can use standard boolean operators (AND / OR / NOT; operators
///    are not case sensitive)
///  - Queries consist of tokens expressing a conditional phrase, of the forms:
///      -# a = b
///      -# a != b
///      -# a < b
///      -# a > b
///  - Tokens may be a numeric or a text string.  Text strings are evaluated in
///    a dictionary against a list of known computable values.  If a text
///    string is not found in the computed dictionary, the text string is
///    looked up as a score in Seq-align.score.
///  - CAlignFilter supports a set of functions as well.  Functions express
///    additional parameters or mathematical operations.  The current list of
///    functions is:
///     -# MUL(a, b) = a * b; a and b are tokens as defined above
///     -# ADD(a, b) = a + b; a and b are tokens as defined above
///     -# IS_SEG_TYPE(a) = 1 if the Seq-align is of segment type a (where a is
///       one of 'disc', 'denseg', 'std', 'spliced', 'packed', 'dendiag')
///     -# COALESCE(a,b,...) = first of (a, b, ...) that evaluates to a
///       supported value.  In order to avoid problems when querying against a
///       missing value, COALESCE() allows the specification of alternate score
///       names or of alternate values.  Thus, COALESCE(score, 0) will return 0
///       if 'score' is not present.
///
/// Current Accepted Tokens
/// -----------------------
///
/// - Any named score.  CSeq_align enforces through the use of enums specific
///   score names; some standard score names are described in CSeq_align and
///   include:
///     -# align_length
///     -# bit_score
///     -# comp_adjustment_method
///     -# e_value
///     -# longest_gap
///     -# num_ident
///     -# num_mismatch
///     -# num_negatives
///     -# num_positives
///     -# pct_coverage
///     -# pct_identity_gap
///     -# pct_identity_gapopen_only
///     -# pct_identity_ungap
///     -# score
///     -# sum_e
///   NOTE: There is no requirement that an alignment contain any of the above
///   scores.
///
/// - Any number.  All numbers are interpreted as doubles.
///
/// - One of a fixed set of computable characteristics locally defined in
///   CAlignFilter.  These include:
///     -# 3prime_unaligned - Length of 3' unaligned sequence
///     -# 5prime_unaligned - Length of 5' unaligned sequence (same as
///       query_start)
///     -# align_length - Length of aligned query span
///     -# align_length_ratio - Length of aligned subject span / length of
///       aligned query span
///     -# align_length_ungap - Sum of lengths of aligned query segments
///     -# cds_internal_stops - For Spliced-segs, returns the count of the
///       number of internal stops present in the mapped CDS (mapped =
///       CGeneModel::CreateGeneModel() mapped)
///     -# internal_unaligned - Length of unaligned sequence between 5'-most and
///       3'-most ends
///     -# min_exon_len - Length of shortest exon
///     -# product_length - Same as query_length
///     -# query_end - End pos (0-based) of query sequence
///     -# query_length - Length of query sequence
///     -# query_start - Start pos (0-based) of query sequence
///     -# subject_end - Ending pos (0-based) of subject span
///     -# subject_length - Length of subject length
///     -# subject_start - Starting pos (0-based) of subject span
///
/// - A specific sequence identifier.  The special tokens 'query' and 'subject'
///   can be used to specify individual sequences using any of the sequence's
///   seq-id synonyms
///
///
/// Example queries:
/// ----------------
///
/// - pct_coverage > 99.5
///     - finds alignments with the score pct_coverage > 99.5
///
/// - (pct_identity_gap > 99.9 AND pct_coverage > 98) OR (pct_identity_gap > 99.0 AND pct_coverage > 99.5)
///     - Evaluates two simultaneous logical conditions, returning the
///       inclusive OR set
///
/// - query = NM_012345.1
///     - returns all alignments for the query sequence
///
/// - MUL(align_length, 0.8) > num_positives
///     - evaluates for all alignments for which num_positives covers 80% of
///      the aligned length
///

class NCBI_XALGOALIGN_EXPORT CAlignFilter : public CObject
{
public:
    CAlignFilter();
    CAlignFilter(const string& filter_string);

    /// Set the query to be used
    void SetFilter(const string& filter_string);

    /// CAlignFilter uses a scope internally.  You can set a scope yourself;
    /// alternatively, the scope used internally will be a default scope
    void SetScope(objects::CScope& scope);
    objects::CScope& SetScope();

    /// Remove duplicate alignments when filtering
    /// NOTE: this may be expensive for a large number of alignments, as it
    /// forces the algorithm to maintain a list of hash keys for each alignment
    CAlignFilter& SetRemoveDuplicates(bool b = true);

    /// Add a sequence to a blacklist.
    /// Blacklisted sequences are excluded always; if an alignment contains a
    /// query or subject that matches a blacklisted alignment, then that
    /// alignment will be excluded.
    ///
    /// NOTE: this is only triggered if the alignments are pairwise!
    ///
    void AddBlacklistQueryId(const objects::CSeq_id_Handle& idh);
    void AddBlacklistSubjectId(const objects::CSeq_id_Handle& idh);

    /// Add a sequence to the white list.
    /// If an alignment matches a whitelisted ID as appropriate, it will always
    /// be returned.
    ///
    /// NOTE: this is only triggered if the alignments are pairwise!
    ///
    void AddWhitelistQueryId(const objects::CSeq_id_Handle& idh);
    void AddWhitelistSubjectId(const objects::CSeq_id_Handle& idh);

    /// Match a single alignment
    bool Match(const objects::CSeq_align& align);

    /// Filter a set of alignments, iteratively applying Match() to each
    /// alignment and emitting all matched alignments in the output set.
    void Filter(const list< CRef<objects::CSeq_align> >& aligns_in,
                list< CRef<objects::CSeq_align> >& aligns_out);

    /// Filter a set of alignments, iteratively applying Match() to each
    /// alignment and emitting all matched alignments in the output set.
    void Filter(const objects::CSeq_align_set& aligns_in,
                objects::CSeq_align_set&       aligns_out);

    /// Filter a set of alignments, iteratively applying Match() to each
    /// alignment and emitting all matched alignments in the output seq-annot.
    void Filter(const objects::CSeq_annot& aligns_in,
                objects::CSeq_annot&       aligns_out);

    /// Print out the dictionary of score generators
    void PrintDictionary(CNcbiOstream&);

    /// Do a dry run of the filter, printing out the parse tree and
    /// looking up all strings
    void DryRun(CNcbiOstream&);

private:
    bool x_Match(const CQueryParseTree::TNode& node,
                 const objects::CSeq_align& align);

    bool x_IsUnique(const objects::CSeq_align& align);

    double x_GetAlignmentScore(const string& score_name,
                               const objects::CSeq_align& align,
                               bool throw_if_not_found = false);

    bool x_Query_Op(const CQueryParseTree::TNode& key_node,
                    CQueryParseNode::EType type,
                    bool is_not,
                    const CQueryParseTree::TNode& val_node,
                    const objects::CSeq_align& align);

    double x_FuncCall(const CQueryParseTree::TNode& func_node,
                      const objects::CSeq_align& align);
    double x_TermValue(const CQueryParseTree::TNode& term_node,
                       const objects::CSeq_align& align,
                       bool throw_if_not_found = false);

    bool x_Query_Range(const CQueryParseTree::TNode& key_node,
                       bool is_not,
                       const CQueryParseTree::TNode& val1_node,
                       const CQueryParseTree::TNode& val2_node,
                       const objects::CSeq_align& align);

private:
    bool m_RemoveDuplicates;
    string m_Query;
    auto_ptr<CQueryParseTree> m_ParseTree;

    /// Flag indicating whether this is a dry run of the filter. If so we are not
    /// matching an alignment, but instead walking the parse tree and printing
    /// information about each score name
    bool m_IsDryRun;
    CNcbiOstream *m_DryRunOutput;

    CRef<objects::CScope> m_Scope;

    set<objects::CSeq_id_Handle> m_QueryBlacklist;
    set<objects::CSeq_id_Handle> m_QueryWhitelist;
    set<objects::CSeq_id_Handle> m_SubjectBlacklist;
    set<objects::CSeq_id_Handle> m_SubjectWhitelist;

    typedef set<string> TUniqueAligns;
    TUniqueAligns m_UniqueAligns;

    objects::CScoreLookup m_ScoreLookup;
};



END_NCBI_SCOPE


#endif  // GPIPE_COMMON___ALIGN_FILTER__HPP
