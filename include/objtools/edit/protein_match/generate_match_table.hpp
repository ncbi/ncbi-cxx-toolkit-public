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

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_annot;
class CSeq_feat;
class CSeq_align;
class CUser_object;
class CSeq_table;

class CMatchTabulate {

public:
    CMatchTabulate(CRef<CScope> db_scope);
    virtual ~CMatchTabulate(void);

    void GenerateMatchTable(
        const map<string, list<string>>& local_prot_ids,
        const map<string, list<string>>& prot_accessions,
        const map<string, string>& nuc_id_replacements,
        CObjectIStream& align_istr, 
        CObjectIStream& annot_istr);

    void WriteTable(CNcbiOstream& out) const;;

    static void WriteTable(const CSeq_table& table,
        CNcbiOstream& out);

    TSeqPos GetNum_rows(void) const;

    struct SProtMatchInfo {
        string nuc_accession;
        string prot_accession;
        string local_id;
        bool same;
    };

private:

    void x_InitMatchTable(void);

    void x_AppendNucleotide(
        const pair<string, bool>& nuc_match_info);
    void x_AppendMatchedProtein(
        const SProtMatchInfo& prot_match_info);

    void x_AppendNewProtein(
        const string& nuc_accession,
        const string& local_id);

    void x_AppendDeadProtein(
        const string& nuc_accession,
        const string& prot_accession);

    void x_ProcessAlignments(CObjectIStream& istr,
        const map<string, string>& nuc_id_replacements,
        map<string, bool>& nuc_match);

    bool x_IsPerfectAlignment(const CSeq_align& align) const;

    void x_ProcessProteins(
        CObjectIStream& istr,
        const map<string, bool>& nuc_accessions,
        const map<string, list<string>>& local_prot_ids,
        const map<string, list<string>>& prot_accessions,
        const map<string, string>& nuc_id_replacement_map);

    bool x_IsCdsComparison(const CSeq_annot& annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CSeq_annot& annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CUser_object& user_obj) const;
    CAnnotdesc::TName x_GetComparisonClass(const CSeq_annot& annot) const;
    const CSeq_feat& x_GetQuery(const CSeq_annot& compare_annot) const;
    const CSeq_feat& x_GetSubject(const CSeq_annot& compare_annot) const;

    bool x_FetchAccession(const CSeq_align& align, string& accession);


    void x_AddColumn(const string& colName);
    void x_AppendColumnValue(const string& colName,
        const string& colVal);
    bool x_HasCdsQuery(const CSeq_annot& annot) const;
    bool x_HasCdsSubject(const CSeq_annot& annot) const;
    bool x_IsComparison(const CSeq_annot& annot) const;
    string x_GetSubjectNucleotideAccession(const CSeq_annot& compare_annot);

    string x_GetAccession(const CSeq_feat& seq_feat) const;
    string x_GetAccessionVersion(const CSeq_feat& seq_feat) const;
    string x_GetAccessionVersion(const CUser_object& user_obj) const;
    string x_GetLocalID(const CSeq_feat& seq_feat) const;
    string x_GetLocalID(const CUser_object& user_obj) const;

    CRef<CSeq_table> mMatchTable;
    map<string, size_t> mColnameToIndex;
    bool mMatchTableInitialized;

    CRef<CScope> m_DBScope;
};


void g_ReadSeqTable(CNcbiIstream& in, CSeq_table& table);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
