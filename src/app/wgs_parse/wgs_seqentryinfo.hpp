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
    bool m_keywords_set;
    bool m_has_tpa_keyword;
    bool m_has_targeted_keyword;
    bool m_has_gmi_keyword;
    bool m_bad_biomol;
    bool m_bad_tech;
    bool m_bad_mol;
    bool m_secondary_accessions;
    bool m_hist_secondary_differs;

    size_t m_num_of_prot_seq;
    size_t m_num_of_nuc_seq;

    EChromosomeSubtypeStatus m_chromosome_subtype_status;
    EIdProblem m_id_problem;
    EDBNameProblem m_dbname_problem;
    ESeqIdStatus m_seqid_state;
    EBiomolStatus m_biomol_state;

    CSeq_id::E_Choice m_seqid_type;
    CMolInfo::TBiomol m_biomol;

    set<string> m_keywords;
    list<string> m_object_ids;

    string m_dbname;
    string m_diff_dbname;
    string m_cur_seqid;

    CSeqEntryInfo() :
        m_seqset(false),
        m_keywords_set(false),
        m_has_tpa_keyword(false),
        m_has_targeted_keyword(false),
        m_has_gmi_keyword(false),
        m_bad_biomol(false),
        m_bad_tech(false),
        m_bad_mol(false),
        m_secondary_accessions(false),
        m_hist_secondary_differs(false),
        m_num_of_prot_seq(0),
        m_num_of_nuc_seq(0),
        m_chromosome_subtype_status(eChromosomeSubtypeValid),
        m_id_problem(eIdNoProblem),
        m_dbname_problem(eDBNameNoProblem),
        m_seqid_state(eSeqIdOK),
        m_biomol_state(eBiomolNotSet),
        m_seqid_type(CSeq_id::e_not_set),
        m_biomol(CMolInfo::eBiomol_unknown)
    {}
};

struct CSeqEntryCommonInfo
{
    bool m_nuc_warn;
    bool m_prot_warn;

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

struct CPubDescriptionInfo
{
    list<CRef<CPubdesc>> m_pubdescr_synonyms;
    CRef<CPubdesc> m_pubdescr_lookup;
    int m_pmid;

    CPubDescriptionInfo() :
        m_pmid(0)
    {}
};

struct COrgRefInfo
{
    CRef<COrg_ref> m_org_ref,
        m_org_ref_after_lookup;
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
    bool m_reject;

    list<CPubDescriptionInfo> m_common_pubs;
    set<string> m_common_comments;
    set<string> m_common_structured_comments;
    CRef<CBioSource> m_biosource;
    list<COrgRefInfo> m_org_refs;
    list<string> m_object_ids;

    pair<string, string> m_dblink_empty_info; // filename and bioseq ID of the first sequence with lack of DBLink
    pair<string, string> m_dblink_diff_info;  // filename and bioseq ID of the first sequence with different DBLink
    CRef<CUser_object> m_dblink;
    int m_dblink_state;

    CRef<CSeq_entry> m_master_bioseq;

    int m_num_of_entries;

    CMasterInfo() :
        m_num_of_pubs(0),
        m_common_comments_not_set(true),
        m_common_structured_comments_not_set(true),
        m_has_targeted_keyword(false),
        m_has_gmi_keyword(false),
        m_has_genome_project_id(false),
        m_same_org(false),
        m_reject(false),
        m_dblink_state(eDblinkNoProblem),
        m_num_of_entries(0)
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