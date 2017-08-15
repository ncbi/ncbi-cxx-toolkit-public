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

#include <ncbi_pch.hpp>

#include <map>
#include <functional>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Contact_info.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include "wgs_params.hpp"
#include "wgs_master.hpp"
#include "wgs_utils.hpp"
#include "wgs_asn.hpp"
#include "wgs_tax.hpp"
#include "wgs_seqentryinfo.hpp"


namespace wgsparse
{

static CRef<CSeq_submit> GetSeqSubmitFromSeqSubmit(CNcbiIfstream& in)
{
    CRef<CSeq_submit> ret(new CSeq_submit);
    in >> MSerial_AsnText >> *ret;

    return ret;
}

static CRef<CSeq_submit> GetSeqSubmitFromSeqEntry(CNcbiIfstream& in)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    in >> MSerial_AsnText >> *entry;

    if (entry->IsSet() && !entry->GetSet().IsSetClass()) {
        entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    }

    CRef<CSeq_submit> ret(new CSeq_submit);
    ret->SetData().SetEntrys().push_back(entry);

    return ret;
}

static CRef<CSeq_submit> GetSeqSubmitFromBioseqSet(CNcbiIfstream& in)
{
    CRef<CBioseq_set> bioseq_set(new CBioseq_set);
    in >> MSerial_AsnText >> *bioseq_set;

    CRef<CSeq_submit> ret(new CSeq_submit);
    if (!bioseq_set->IsSetAnnot() && !bioseq_set->IsSetDescr()) {

        CSeq_submit::C_Data::TEntrys& entries = ret->SetData().SetEntrys();
        entries.splice(entries.end(), bioseq_set->SetSeq_set());
    }
    else {

        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSet(*bioseq_set);
        ret->SetData().SetEntrys().push_back(entry);
    }

    return ret;
}

static CRef<CSeq_submit> GetSeqSubmit(CNcbiIfstream& in, EInputType type)
{
    static const map<EInputType, CRef<CSeq_submit>(*)(CNcbiIfstream&)> SEQSUBMIT_LOADERS = {
        { eSeqSubmit, GetSeqSubmitFromSeqSubmit },
        { eSeqEntry, GetSeqSubmitFromSeqEntry },
        { eBioseqSet, GetSeqSubmitFromBioseqSet }
    };

    CRef<CSeq_submit> ret;

    try {
        ret = SEQSUBMIT_LOADERS.at(type)(in);
    }
    catch (CException&) {
        ret.Reset();
    }

    return ret;
}

struct CPubDescription
{
    list<CRef<CPubdesc>> m_pubs;
};

static size_t CheckPubs(const CSeq_entry& entry, const string& file, list<CPubDescription>& common_pubs)
{
    const CSeq_descr* descrs = nullptr;
    if (!GetDescr(entry, descrs)) {
        return 0;
    }

    size_t num_of_pubs = 0;

    if (!common_pubs.empty()) {
        // TODO

        return num_of_pubs;
    }

    list<CRef<CPubdesc>> pubs;

    if (descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Get()) {
            if (descr->IsPub()) {
                if (!IsPubdescContainsSub(descr->GetPub())) {
                    ++num_of_pubs;
                }

                CRef<CPubdesc> pubdesc(new CPubdesc);
                pubdesc->Assign(descr->GetPub());
                pubs.push_back(pubdesc);
            }
        }
    }

    if (pubs.empty()) {
        ERR_POST_EX(0, 0, Info << "Submission from file \"" << file << "\" is lacking publications.");
        return num_of_pubs;
    }

    if (common_pubs.empty()) {

        common_pubs.push_front(CPubDescription());
        common_pubs.front().m_pubs.swap(pubs);

        // TODO
    }

    else {
        // TODO
    }

    return num_of_pubs;
}

template <typename Func, typename Container> void CollectDataFromDescr(const CSeq_entry& entry, Container& container, Func process)
{
    if (entry.IsSeq() && !entry.GetSeq().IsNa()) {
        return;
    }

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            for (auto& descr : descrs->Get()) {
                process(*descr, container);
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for_each(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                     [&container, &process](const CRef<CSeq_entry>& cur_entry) { CollectDataFromDescr(*cur_entry, container, process); });
        }
    }
}

static void ProcessComment(const CSeqdesc& descr, set<string>& comments)
{
    if (descr.IsComment() && !descr.GetComment().empty()) {
        comments.insert(descr.GetComment());
    }
}


enum EDBLinkProblem
{
    eDblinkNoProblem,
    eDblinkNoDblink = 1 << 0,
    eDblinkDifferentDblink = 1 << 1,
    eDblinkAllProblems = eDblinkNoDblink | eDblinkDifferentDblink
};

struct CMasterInfo
{
    size_t m_num_of_pubs = 0;

    bool m_common_comments_not_set;
    bool m_common_structured_comments_not_set;
    bool m_has_targeted_keyword;
    bool m_has_gmi_keyword;
    bool m_has_genome_project_id;

    list<CPubDescription> m_common_pubs;
    set<string> m_common_comments;
    set<string> m_common_structured_comments;
    CRef<CBioSource> m_biosource;
    list<CRef<COrg_ref>> m_org_refs;

    pair<string, string> m_dblink_empty_info; // filename and bioseq ID of the first sequence with lack of DBLink
    pair<string, string> m_dblink_diff_info;  // filename and bioseq ID of the first sequence with different DBLink
    CRef<CUser_object> m_dblink;
    int m_dblink_state;

    int m_num_of_entries;

    CMasterInfo() :
        m_num_of_pubs(0),
        m_common_comments_not_set(true),
        m_common_structured_comments_not_set(true),
        m_has_targeted_keyword(false),
        m_has_gmi_keyword(false),
        m_has_genome_project_id(false),
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


static void CheckComments(const CSeq_entry& entry, CMasterInfo& info)
{
    if (info.m_common_comments_not_set) {
        CollectDataFromDescr(entry, info.m_common_comments, ProcessComment);
        info.m_common_comments_not_set = info.m_common_comments.empty();
    }
    else {

        if (!info.m_common_comments.empty()) {

            set<string> cur_comments;
            CollectDataFromDescr(entry, cur_comments, ProcessComment);

            set<string> common_comments;
            set_intersection(info.m_common_comments.begin(), info.m_common_comments.end(), cur_comments.begin(), cur_comments.end(), inserter(common_comments, common_comments.begin()));

            info.m_common_comments.swap(common_comments);
        }
    }
}

static bool IsUserObjectOfType(const CSeqdesc& descr, const string& type)
{
    return descr.IsUser() && descr.GetUser().IsSetType() && descr.GetUser().GetType().IsStr() &&
        descr.GetUser().GetType().GetStr() == type;
}

static void ProcessStructuredComment(const CSeqdesc& descr, set<string>& comments)
{
    if (IsUserObjectOfType(descr, "StructuredComment")) {

        const CUser_object& user_obj = descr.GetUser();
        string label;
        user_obj.GetLabel(&label);

        // TODO
        comments.insert(label);
    }
}

// TODO may be combined with 'CheckComments'
static void CheckStructuredComments(const CSeq_entry& entry, CMasterInfo& info)
{
    if (info.m_common_structured_comments_not_set) {
        CollectDataFromDescr(entry, info.m_common_structured_comments, ProcessStructuredComment);
        info.m_common_structured_comments_not_set = info.m_common_structured_comments.empty();
    }
    else {

        if (!info.m_common_structured_comments.empty()) {

            set<string> cur_comments;
            CollectDataFromDescr(entry, cur_comments, ProcessStructuredComment);

            set<string> common_comments;
            set_intersection(info.m_common_structured_comments.begin(), info.m_common_structured_comments.end(), cur_comments.begin(), cur_comments.end(), inserter(common_comments, common_comments.begin()));

            // TODO
            info.m_common_structured_comments.swap(common_comments);
        }
    }
}

static bool CheckBioSource(const CSeq_entry& entry, CMasterInfo& info, const string& file)
{
    bool ret = true;

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            auto is_biosource = [](const CRef<CSeqdesc>& descr) { return descr->IsSource(); };
            size_t num_of_biosources = count_if(descrs->Get().begin(), descrs->Get().end(), is_biosource);

            if (num_of_biosources > 1) {
                ERR_POST_EX(0, 0, Fatal << "Multiple BioSource descriptors encountered in record from file \"" << file << "\".");
                ret = false;
            }
            else if (num_of_biosources < 1) {
                ERR_POST_EX(0, 0, Warning << "Submission from file \"" << file << "\" is lacking BioSource.");
            }
            else {

                auto& biosource_it = find_if(descrs->Get().begin(), descrs->Get().end(), is_biosource);
                if (info.m_biosource.Empty()) {
                    info.m_biosource.Reset(new CBioSource);
                    info.m_biosource->Assign((*biosource_it)->GetSource());
                }
                else {
                    // TODO
                }
            }
        }
    }

    return ret;
}

struct CDBLinkInfo
{
    CRef<CUser_object> m_dblink;
    int m_dblink_state;

    string m_cur_bioseq_id;

    CDBLinkInfo() :
        m_dblink_state(eDblinkNoDblink)
    {
    }
};

static void CollectDblink(const CSeq_entry& entry, CDBLinkInfo& info)
{
    if (info.m_dblink_state == eDblinkDifferentDblink) {
        return;
    }

    if (info.m_cur_bioseq_id.empty() && entry.IsSeq()) {
        info.m_cur_bioseq_id = GetSeqIdStr(entry.GetSeq());
    }

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Get()) {
            if (IsUserObjectOfType(*descr, "DBLink")) {

                const CUser_object& user_obj = descr->GetUser();
                if (info.m_dblink.Empty()) {
                    info.m_dblink.Reset(new CUser_object);
                    info.m_dblink->Assign(user_obj);
                    info.m_dblink_state = eDblinkNoProblem;
                }
                else if (!info.m_dblink->Equals(user_obj)) {
                    info.m_dblink_state = eDblinkDifferentDblink;
                    return;
                }
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for_each(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                     [&info](const CRef<CSeq_entry>& cur_entry) { CollectDblink(*cur_entry, info); });
        }
    }
}

static void CheckDblink(const CSeq_entry& entry, CMasterInfo& info, const string& file)
{
    CDBLinkInfo dblink_info;
    CollectDblink(entry, dblink_info);

    if (dblink_info.m_cur_bioseq_id.empty()) {
        dblink_info.m_cur_bioseq_id = "Unknown";
    }

    if (dblink_info.m_dblink_state & eDblinkNoDblink) {
        info.SetDblinkEmpty(file, dblink_info.m_cur_bioseq_id);
    }
    else if (dblink_info.m_dblink_state & eDblinkDifferentDblink) {
        info.SetDblinkDifferent(file, dblink_info.m_cur_bioseq_id);
    }
    else {
        if (info.m_dblink.Empty()) {
            info.m_dblink = dblink_info.m_dblink;
        }
        else if (!info.m_dblink->Equals(*dblink_info.m_dblink)) {
            info.SetDblinkDifferent(file, dblink_info.m_cur_bioseq_id);
        }
    }
}

static bool HasGenomeProjectID(const CUser_object& user_obj)
{
    return user_obj.IsSetType() && user_obj.GetType().IsStr() && user_obj.GetType().GetStr() == "GenomeProjectsDB";
}

static bool CheckGPID(const CSeq_entry& entry)
{
    const CSeq_descr* descrs = nullptr;
    bool ret = false;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            for (auto& descr : descrs->Get()) {
                if (descr->IsUser()) {
                    ret = HasGenomeProjectID(descr->GetUser());
                    if (ret) {
                        break;
                    }
                }
            }
        }
    }

    if (!ret && entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            auto& entry_it = find_if(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                                     [](const CRef<CSeq_entry>& cur_entry) { return CheckGPID(*cur_entry); });
            ret = entry_it != entry.GetSet().GetSeq_set().end();
        }
    }

    return ret;
}

static bool SubmissionDiffers(const string& file, bool same_submit)
{
    if (GetParams().IsDblinkOverride()) {
        ERR_POST_EX(0, 0, "Submission \"" << file << "\" has different Submit block. Using Submit-block from the first submission.");
    }
    else {
        ERR_POST_EX(0, 0, "Submission \"" << file << "\" has different Submit block. Will not provide Cit-sub descriptor in master Bioseq. This can be overridden by setting \"-X T\" command line switch: it'll use Submit-block from the first file.");
        same_submit = false;
    }

    return same_submit;
}

static bool NeedToGetAccessionPrefix() {

    return GetParams().IsUpdateScaffoldsMode() && GetParams().IsAccessionAssigned() && GetParams().GetScaffoldPrefix().empty();
}

bool CreateMasterBioseqWithChecks()
{
    const list<string>& files = GetParams().GetInputFiles();

    bool ret = true,
         same_submit = true;

    CMasterInfo master_info;
    CRef<CContact_info> master_contact_info;
    CRef<CCit_sub> master_cit_sub;

    for (auto& file : files) {

        CNcbiIfstream in(file);

        if (!in) {
            ERR_POST_EX(0, 0, "Failed to open submission \"" << file << "\" for reading. Cannot proceed.");
            ret = false;
            break;
        }

        EInputType input_type = eSeqSubmit;
        GetInputTypeFromFile(in, input_type);

        bool first = true;
        while (in && !in.eof()) {

            CRef<CSeq_submit> seq_submit = GetSeqSubmit(in, input_type);
            if (seq_submit.Empty()) {

                if (first) {
                    static const map<EInputType, string> SEQSUBMIT_TYPE_STR = {
                        { eSeqSubmit, "Seq-submit" },
                        { eSeqEntry, "Seq-entry" },
                        { eBioseqSet, "Bioseq-set" }
                    };

                    ERR_POST_EX(0, 0, "Failed to read " << SEQSUBMIT_TYPE_STR.at(input_type) << " from file \"" << file << "\". Cannot proceed.");
                    ret = false;
                }
                break;
            }

            first = false;

            int accession_ver = -1;
            if (!FixSeqSubmit(seq_submit, accession_ver, true)) {
                ERR_POST_EX(0, 0, "Wrapper GenBank set has non-empty annotation (Seq-annot), which is not allowed. Cannot process this submission \"" << file << "\".");
                ret = false;
                break;
            }

            if (GetParams().GetUpdateMode() == eUpdateAssembly && accession_ver > 0 && GetParams().IsAccessionAssigned()) {
                // TODO
            }

            if (!seq_submit->IsSetSub()) {
                if (input_type == eSeqSubmit) {
                    ERR_POST_EX(0, 0, "Submission \"" << file << "\" is missing Submit-block.");
                }
                else if (same_submit) {
                    same_submit = true; // TODO WGSCheckCitSubsInBioseqSet(ssp, widp, *b);
                }
            }
            else if (input_type != eSeqSubmit || GetParams().GetSource() == eNCBI) {

                CSubmit_block& submit_block = seq_submit->SetSub();
                submit_block.ResetTool();

                if (!submit_block.IsSetContact()) {
                    ERR_POST_EX(0, 0, "Submit block from submission \"" << file << "\" is missing contact information.");
                }
                else {

                    submit_block.SetContact().ResetContact();
                    if (master_contact_info.Empty()) {
                        master_contact_info.Reset(new CContact_info);
                        master_contact_info->Assign(submit_block.GetContact());
                    }
                    else if (!master_contact_info->Equals(submit_block.GetContact())) {
                        same_submit = SubmissionDiffers(file, same_submit);
                    }
                }

                if (!submit_block.IsSetCit()) {
                    ERR_POST_EX(0, 0, "Submit block from submission \"" << file << "\" is missing Cit-sub.");
                }
                else {

                    CCit_sub& cit_sub = submit_block.SetCit();
                    if (GetParams().IsSetSubmissionDate()) {
                        cit_sub.SetDate().SetStd().Assign(GetParams().GetSubmissionDate());
                    }

                    if (master_cit_sub.Empty()) {
                        master_cit_sub.Reset(new CCit_sub);
                        master_cit_sub->Assign(cit_sub);
                    }
                    else if (!master_cit_sub->Equals(cit_sub)) {
                        same_submit = SubmissionDiffers(file, same_submit);
                    }
                }

                if (!seq_submit->IsSetData()) {
                    ERR_POST_EX(0, 0, "Failed to read Seq-entry from file \"" << file << "\". Cannot proceed.");
                    break;
                }

                CSeqEntryCommonInfo common_info;
                for (auto entry : seq_submit->GetData().GetEntrys()) {

                    if (NeedToGetAccessionPrefix()) {
                        // TODO: should eventually call SetScaffoldPrefix
                    }

                    CSeqEntryInfo info;
                    if (!CheckSeqEntry(*entry, file, info, common_info)) {
                        // TODO reject ///////widp->reject = TRUE;
                    }
                    else if (GetParams().IsTsa() && GetParams().GetFixTech() == eNoFix && info.m_biomol != CMolInfo::eBiomol_transcribed_RNA) {

                        string rna;
                        switch (info.m_biomol) {
                            case CMolInfo::eBiomol_mRNA:
                                rna = "mRNA";
                                break;
                            case CMolInfo::eBiomol_rRNA:
                                rna = "rRNA";
                                break;
                            default:
                                rna = "ncRNA";
                        }

                        ERR_POST_EX(0, 0, Warning << "Unusual biomol value \"" << rna << "\" has been used for this TSA project, instead of \"transcribed_RNA\".");
                    }

                    master_info.m_has_targeted_keyword = master_info.m_has_targeted_keyword || info.m_has_targeted_keyword;
                    master_info.m_has_gmi_keyword = master_info.m_has_gmi_keyword || info.m_has_gmi_keyword;

                    if (!GetParams().IsUpdateScaffoldsMode()) {

                        if (!GetParams().IsKeepRefs()) {

                            master_info.m_num_of_pubs = max(CheckPubs(*entry, file, master_info.m_common_pubs), master_info.m_num_of_pubs);
                            CheckComments(*entry, master_info);
                        }

                        CheckStructuredComments(*entry, master_info);
                    }

                    if (!CheckBioSource(*entry, master_info, file)) {
                        ;// TODO widp->reject = TRUE;
                    }

                    if (master_info.m_dblink_state != eDblinkAllProblems) {
                        CheckDblink(*entry, master_info, file);
                    }


                    if (!master_info.m_has_genome_project_id) {
                        master_info.m_has_genome_project_id = CheckGPID(*entry);
                    }

                    CollectOrgRefs(*entry, master_info.m_org_refs);

                    /*if (widp->source != FROM_NCBI) {
                        SeqEntryExplore(sep, widp->dates, WGSCollectCommonDates);
                        if (widp->dates->choice != 100) {
                            if (widp->dates->choice == 0) {
                                ErrPostEx(SEV_ERROR, ERR_SUBMISSION_UpdateDateMissing,
                                          "Update date is missing from one or more input submissions. Will not propagate update date to the master record.");
                                widp->dates->choice = 100;
                            }
                            else if (widp->dates->data.ptrvalue == NULL) {
                                ErrPostEx(SEV_ERROR, ERR_SUBMISSION_UpdateDatesDiffer,
                                          "Different update dates encountered amongst input submissions. Will not propagate update date to the master record.");
                                widp->dates->choice = 100;
                            }
                            else
                                widp->dates->choice = 0;
                        }

                        if (widp->dates->next->choice != 100) {
                            if (widp->dates->next->choice == 0) {
                                if (widp->source == FROM_DDBJ)
                                    sev = SEV_INFO;
                                else
                                    sev = SEV_ERROR;
                                ErrPostEx(sev, ERR_SUBMISSION_CreateDateMissing,
                                          "Create date is missing from one or more input submissions. Will not propagate create date to the master record.");
                                widp->dates->next->choice = 100;
                            }
                            else if (widp->dates->next->data.ptrvalue == NULL) {
                                ErrPostEx(SEV_ERROR, ERR_SUBMISSION_CreateDatesDiffer,
                                          "Different create dates encountered amongst input submissions. Will not propagate create date to the master record.");
                                widp->dates->next->choice = 100;
                            }
                            else
                                widp->dates->next->choice = 0;
                        }
                    }*/

                    ++master_info.m_num_of_entries;

                    if (info.m_seqid_type == CSeq_id::e_Other) {
                        // TODO
                    }

                    if (!GetParams().IsAccessionsSortedInFile()) {
                        // TODO
                    }

                    if (GetParams().IsUpdateScaffoldsMode()) {
                        // TODO
                    }
                }
            }
        }

        if (!ret) {
            break;
        }
    }


    return ret;
}

}