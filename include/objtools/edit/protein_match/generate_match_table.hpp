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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Justin Foley
 */

#ifndef _GENERATE_MATCH_TABLE_HPP_
#define _GENERATE_MATCH_TABLE_HPP_

#include <corelib/ncbistd.hpp>
#include <objects/seq/Annotdesc.hpp>

class CSeq_annot;
class CSeq_feat;
class CSeq_align;
class CUser_object;
class CSeq_table;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CMatchTabulate {

public:
    CMatchTabulate(void);
    virtual ~CMatchTabulate(void);
    void AppendToMatchTable(const CSeq_align& alignment,
        const list<CRef<CSeq_annot>>& annots);

    void WriteTable(CNcbiOstream& out);

    typedef list<CRef<CSeq_annot>> TMatches;

    struct SNucMatchInfo {
        string accession;
        string status;
    };

private:

    void x_InitMatchTable(void);

    bool x_AppendToMatchTable(
        const SNucMatchInfo& nuc_match_info,
        const TMatches& matches,
        const list<string>& new_proteins,
        const list<string>& dead_proteins);


    bool x_TryProcessAlignment(const CSeq_align& alignment,
        SNucMatchInfo& nuc_match_info);

    bool x_IsPerfectAlignment(const CSeq_align& alignment) const;

    bool x_TryProcessAnnots(const list<CRef<CSeq_annot>>& annot_list,
        TMatches& matches,
        list<string>& new_prot_ids,
        list<string>& dead_prot_ids);

    bool x_IsProteinMatch(const CSeq_annot& annot) const;
    bool x_IsCdsComparison(const CSeq_annot& annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CSeq_annot& annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CUser_object& user_obj) const;
    bool x_HasNovelSubject(const CSeq_annot& annot) const;
    bool x_HasNovelQuery(const CSeq_annot& annot) const;
    CAnnotdesc::TName x_GetComparisonClass(const CSeq_annot& annot) const;
    const CSeq_feat& x_GetQuery(const CSeq_annot& compare_annot) const;
    const CSeq_feat& x_GetSubject(const CSeq_annot& compare_annot) const;
    bool x_FetchAccessionVersion(const CSeq_align& align,
        string& accver);
    void x_AddColumn(const string& colName);
    void x_AppendColumnValue(const string& colName,
        const string& colVal);
    bool x_HasCdsQuery(const CSeq_annot& annot) const;
    bool x_HasCdsSubject(const CSeq_annot& annot) const;
    bool x_IsComparison(const CSeq_annot& annot) const;

    string x_GetAccessionVersion(const CSeq_feat& seq_feat) const;
    string x_GetAccessionVersion(const CUser_object& user_obj) const;
    string x_GetLocalID(const CSeq_feat& seq_feat) const;
    string x_GetLocalID(const CUser_object& user_obj) const;

    CRef<CSeq_table> mMatchTable;
    map<string, size_t> mColnameToIndex;
    bool mMatchTableInitialized;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
