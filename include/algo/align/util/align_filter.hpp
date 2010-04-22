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

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_annot;
    class CScope;
END_SCOPE(objects)


class CAlignFilter : public CObject
{
public:
    class IScore : public CObject
    {
    public:
        virtual ~IScore() {}
        virtual double Get(const objects::CSeq_align& align,
                           objects::CScope* scope) const = 0;
    };

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

private:
    bool x_Match(const CQueryParseTree::TNode& node,
                 const objects::CSeq_align& align);

    bool x_IsUnique(const objects::CSeq_align& align);

    double x_GetAlignmentScore(const string& score_name,
                               const objects::CSeq_align& align);

    bool x_Query_Op(const CQueryParseTree::TNode& key_node,
                    CQueryParseNode::EType type,
                    bool is_not,
                    const CQueryParseTree::TNode& val_node,
                    const objects::CSeq_align& align);

    double x_FuncCall(const CQueryParseTree::TNode& func_node,
                      const objects::CSeq_align& align);
    double x_TermValue(const CQueryParseTree::TNode& term_node,
                       const objects::CSeq_align& align);

    bool x_Query_Range(const CQueryParseTree::TNode& key_node,
                       bool is_not,
                       const CQueryParseTree::TNode& val1_node,
                       const CQueryParseTree::TNode& val2_node,
                       const objects::CSeq_align& align);

private:
    bool m_RemoveDuplicates;
    string m_Query;
    auto_ptr<CQueryParseTree> m_ParseTree;

    CRef<objects::CScope> m_Scope;

    set<objects::CSeq_id_Handle> m_QueryBlacklist;
    set<objects::CSeq_id_Handle> m_QueryWhitelist;
    set<objects::CSeq_id_Handle> m_SubjectBlacklist;
    set<objects::CSeq_id_Handle> m_SubjectWhitelist;

    typedef set<string> TUniqueAligns;
    TUniqueAligns m_UniqueAligns;

    typedef map<string, CIRef<IScore> > TScoreDictionary;
    TScoreDictionary m_Scores;
};



END_NCBI_SCOPE


#endif  // GPIPE_COMMON___ALIGN_FILTER__HPP
