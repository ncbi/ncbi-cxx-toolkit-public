/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef WGS_SEQENTRYINFO_H
#define WGS_SEQENTRYINFO_H

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include "wgs_text_accession.hpp"
#include "wgs_enums.hpp"
#include "wgs_pubs.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace wgsparse
{
enum EChromosomeSubtypeStatus
{
    eChromosomeSubtypeValid,
    eChromosomeSubtypeMissingInBacteria,
    eChromosomeSubtypeMissing
};

enum EIdProblem
{
    eIdNoProblem,
    eIdNoDbTag,
    eIdManyGeneralIds
};

enum EDBNameProblem
{
    eDBNameNoProblem,
    eDBNameBadNucDB,
    eDBNameBadProtDB
};

enum ESeqIdStatus
{
    eSeqIdOK,
    eSeqIdDifferent,
    eSeqIdMultiple,
    eSeqIdIncorrect
};

enum EBiomolStatus
{
    eBiomolNotSet,
    eBiomolSet,
    eBiomolDiffers
};

struct CSeqEntryInfo
{
    bool m_seqset;
    bool m_has_tpa_keyword;
    bool m_has_targeted_keyword;
    bool m_has_gmi_keyword;
    bool m_bad_biomol;
    bool m_bad_tech;
    bool m_bad_mol;
    bool m_bad_accession;
    bool m_secondary_accessions;
    bool m_hist_secondary_differs;
    bool m_update_date_present;
    bool m_creation_date_present;
    bool m_has_gb_block;

    // keywords data got from CMasterInfo
    bool& m_keywords_set;
    set<string>& m_keywords;

    size_t m_num_of_prot_seq;
    size_t m_num_of_nuc_seq;
    size_t m_num_of_accsessions;

    EChromosomeSubtypeStatus m_chromosome_subtype_status;
    EIdProblem m_id_problem;
    EDBNameProblem m_dbname_problem;
    ESeqIdStatus m_seqid_state;
    EBiomolStatus m_biomol_state;

    CSeq_id::E_Choice m_seqid_type;
    CMolInfo::TBiomol m_biomol;

    list<string> m_object_ids;

    string m_dbname;
    string m_diff_dbname;
    string m_cur_seqid;

    CSeqEntryInfo(bool& keywords_set, set<string>& keywords) :
        m_seqset(false),
        m_has_tpa_keyword(false),
        m_has_targeted_keyword(false),
        m_has_gmi_keyword(false),
        m_bad_biomol(false),
        m_bad_tech(false),
        m_bad_mol(false),
        m_bad_accession(false),
        m_secondary_accessions(false),
        m_hist_secondary_differs(false),
        m_update_date_present(false),
        m_creation_date_present(false),
        m_has_gb_block(false),
        m_keywords_set(keywords_set),
        m_keywords(keywords),
        m_num_of_prot_seq(0),
        m_num_of_nuc_seq(0),
        m_num_of_accsessions(0),
        m_chromosome_subtype_status(eChromosomeSubtypeValid),
        m_id_problem(eIdNoProblem),
        m_dbname_problem(eDBNameNoProblem),
        m_seqid_state(eSeqIdOK),
        m_biomol_state(eBiomolNotSet),
        m_seqid_type(CSeq_id::e_not_set),
        m_biomol(CMolInfo::eBiomol_unknown)
    {}
};

typedef pair<CTextAccessionContainer, CTextAccessionContainer> TAccessionRange;

struct CSeqEntryCommonInfo
{
    bool m_nuc_warn;
    bool m_prot_warn;

    list<string> m_acc_assigned;
    list<TAccessionRange> m_acc_ranges;

    CSeqEntryCommonInfo() :
        m_nuc_warn(false),
        m_prot_warn(false)
    {}

    bool IsReplaceDbname() const
    {
        return !m_nuc_warn && !m_prot_warn;
    }
};

enum EDBLinkProblem
{
    eDblinkNoProblem,
    eDblinkNoDblink = 1 << 0,
    eDblinkDifferentDblink = 1 << 1,
    eDblinkAllProblems = eDblinkNoDblink | eDblinkDifferentDblink
};

struct COrgRefInfo
{
    CRef<COrg_ref> m_org_ref,
        m_org_ref_after_lookup;
};

struct CIdInfo
{
    string m_dbtag,
           m_accession,
           m_file;

    bool operator<(const CIdInfo& other) const
    {
        return m_accession < other.m_accession;
    }
};

enum EDateIssues
{
    eDateNoIssues,
    eDateDiff,
    eDateMissing
};

struct CCurrentMasterInfo
{
    string m_accession;
    int m_version,
        m_first_contig,
        m_last_contig;
    size_t m_num_len; // length of the numeric part of an accession

    CRef<CPub_equiv> m_cit_sub;
    CRef<CDate> m_cit_sub_date;
    list<CRef<CPub_equiv>> m_cit_arts;

    CCurrentMasterInfo() :
        m_version(0),
        m_first_contig(0),
        m_last_contig(0),
        m_num_len(0),
        m_cit_sub_date(new CDate)
    {}

    size_t GetNoPrefixAccessionLen() const
    {
        auto digit_pos = find_if(m_accession.begin(), m_accession.end(), [](char c) { return isdigit(c); } );
        return m_accession.size() - (digit_pos - m_accession.begin());
    }
};


struct CCitSubInfo
{
    CRef<CCit_sub> m_cit_sub;
    map<string, pair<CRef<CDate>, size_t>> m_dates;

    void AddDate(const CDate& date)
    {
        string key = ToString(date);
        auto& info = m_dates[key];
        if (info.first.Empty()) {
            info.first.Reset(new CDate);
            info.first->Assign(date);
        }
        ++info.second;
    }
};


struct CMasterInfo
{
    size_t m_num_of_pubs;

    bool m_common_comments_not_set;
    bool m_common_structured_comments_not_set;
    bool m_has_targeted_keyword;
    bool m_has_gmi_keyword;
    bool m_has_genome_project_id;
    bool m_same_org;
    bool m_same_biosource;
    bool m_reject;
    bool m_has_gb_block;
    bool m_gpid;
    bool m_got_cit_sub;

    CPubCollection m_pubs;

	list<string> m_common_pubs;
    list<string> m_common_comments;
    list<string> m_common_structured_comments;
    CRef<CBioSource> m_biosource;
    list<COrgRefInfo> m_org_refs;
    list<string> m_object_ids;

    list<CIdInfo> m_id_infos;

    pair<string, string> m_dblink_empty_info; // filename and bioseq ID of the first sequence with lack of DBLink
    pair<string, string> m_dblink_diff_info;  // filename and bioseq ID of the first sequence with different DBLink
    CRef<CUser_object> m_dblink;
    int m_dblink_state;

    CRef<CDate> m_update_date;
    CRef<CDate> m_creation_date;
    bool m_update_date_present;
    EDateIssues m_update_date_issues;
    bool m_creation_date_present;
    EDateIssues m_creation_date_issues;
    bool m_need_to_remove_dates;

    CRef<CSeq_entry> m_master_bioseq;
    CRef<CSeq_entry> m_id_master_bioseq;

    map<string, int> m_order_of_entries; // string - current first Seq-id, int - order (to be used in accession assignment)

    TSeqPos m_num_of_entries;
    TSeqPos m_whole_len;
    int m_accession_ver;

    string m_master_file_name;

    bool m_keywords_set;
    set<string> m_keywords;

    size_t m_num_of_prot_seq;

    CCurrentMasterInfo* m_current_master;

    CCitSubInfo m_cit_sub_info;
    EInputType m_input_type;


    CMasterInfo() :
        m_num_of_pubs(0),
        m_common_comments_not_set(true),
        m_common_structured_comments_not_set(true),
        m_has_targeted_keyword(false),
        m_has_gmi_keyword(false),
        m_has_genome_project_id(false),
        m_same_org(false),
        m_same_biosource(false),
        m_reject(false),
        m_has_gb_block(false),
        m_gpid(false),
        m_got_cit_sub(false),
        m_dblink_state(eDblinkNoProblem),
        m_update_date(new CDate),
        m_creation_date(new CDate),
        m_update_date_present(false),
        m_update_date_issues(eDateNoIssues),
        m_creation_date_present(false),
        m_creation_date_issues(eDateNoIssues),
        m_need_to_remove_dates(false),
        m_num_of_entries(0),
        m_whole_len(0),
        m_accession_ver(-1),
        m_keywords_set(false),
        m_num_of_prot_seq(0),
        m_current_master(nullptr),
        m_input_type(eUnknownType)
    {}

    void SetDblinkEmpty(const string& file, const string& id)
    {
        m_dblink_state |= eDblinkNoDblink;
        if (m_dblink_empty_info.first.empty()) {
            m_dblink_empty_info.first = file;
            m_dblink_empty_info.second = id;
        }
    }

    void SetDblinkDifferent(const string& file, const string& id)
    {
        m_dblink_state |= eDblinkDifferentDblink;
        if (m_dblink_diff_info.first.empty()) {
            m_dblink_diff_info.first = file;
            m_dblink_diff_info.second = id;
        }
    }
};


}

#endif // WGS_SEQENTRYINFO_H
