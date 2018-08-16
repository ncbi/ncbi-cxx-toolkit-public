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

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Contact_info.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/seqblock/GB_block.hpp>

#include "wgs_params.hpp"
#include "wgs_master.hpp"
#include "wgs_utils.hpp"
#include "wgs_asn.hpp"
#include "wgs_tax.hpp"
#include "wgs_seqentryinfo.hpp"
#include "wgs_med.hpp"
#include "wgs_text_accession.hpp"


namespace wgsparse
{

static void GetSeqIdStr(const CBioseq::TId& ids, string& id_str)
{
    for (auto& id : ids) {
        if (id->IsGeneral()) {
            id->GetGeneral().GetLabel(&id_str);
            id_str = "general id " + id_str;
            break;
        }
        else if (id->IsLocal()) {

            if (id->GetLocal().IsStr()) {
                id_str = id->GetLocal().GetStr();
            }
            else if (id->GetLocal().IsId()) {
                id_str = NStr::IntToString(id->GetLocal().GetId());
            }

            if (!id_str.empty()) {
                id_str = "local id " + id_str;
                break;
            }
        }
        else if (id->GetTextseq_Id() != nullptr) {

            const CTextseq_id* text_id = id->GetTextseq_Id();
            if (text_id->IsSetAccession()) {
                id_str = "accession \"" + text_id->GetAccession() + '\"';
                break;
            }
        }
    }
}

static void GetSeqIdStrFromEntry(const CSeq_entry& entry, string& id_str)
{
    if (id_str.empty()) {

        if (entry.IsSeq()) {

            if (entry.GetSeq().IsSetId()) {
                GetSeqIdStr(entry.GetSeq().GetId(), id_str);
            }
        }
        else if (entry.IsSet()) {

            if (entry.GetSet().IsSetSeq_set()) {
                for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
                    GetSeqIdStrFromEntry(*cur_entry, id_str);
                }
            }
        }
    }
}

static size_t CheckPubs(CSeq_entry& entry, const string& file, list<string>& common_pubs, bool& reject, CPubCollection& all_pubs)
{
    CSeq_descr* descrs = nullptr;
    if (!GetNonConstDescr(entry, descrs)) {
        return 0;
    }

    size_t num_of_pubs = 0;
    list<string> pubs;

    if (descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Set()) {
            if (descr->IsPub()) {
                if (!HasPubOfChoice(descr->GetPub(), CPub::e_Sub)) {
                    ++num_of_pubs;
                }

                string pubdesc_key = all_pubs.AddPub(descr->SetPub(), GetParams().IsMedlineLookup());
                
                CRef<CPubdesc> pubdesc = all_pubs.GetPubInfo(pubdesc_key).m_desc;
                _ASSERT(pubdesc.NotEmpty());

                descr->SetPub(*pubdesc);
                pubs.push_back(pubdesc_key);
            }
        }
    }

    if (pubs.empty()) {
        ERR_POST_EX(0, 0, Info << "Submission from file \"" << file << "\" is lacking publications.");
        return 0;
    }

    if (common_pubs.empty()) {
        common_pubs.swap(pubs);
    }
    else {

        for (auto common_pub = common_pubs.begin(); common_pub != common_pubs.end();) {

            bool found = false;
            const CPubInfo& common_pub_info = all_pubs.GetPubInfo(*common_pub);

            for (auto pub = pubs.begin(); pub != pubs.end(); ++pub) {

                const CPubInfo& pub_info = all_pubs.GetPubInfo(*pub);
                if (pub_info.m_pubdesc_key == common_pub_info.m_pubdesc_key) {
                    pubs.erase(pub);
                    found = true;
                    break;
                }

                if (HasPubOfChoice(*pub_info.m_desc, CPub::e_Sub)) {
                    continue;
                }

                if (pub_info.m_pmid > 0 && common_pub_info.m_pmid == pub_info.m_pmid) { // Same PMID in different pubs

                    if (!GetParams().IsDblinkOverride()) {
                        ERR_POST_EX(0, 0, Error << "Varying results returned from PubMed for article with PMID " << pub_info.m_pmid << ".");
                        reject = true;
                    }
                }
            }

            if (found) {
                ++common_pub;
            }
            else {
                common_pub = common_pubs.erase(common_pub);

                string id_str;
                GetSeqIdStrFromEntry(entry, id_str);
                ERR_POST_EX(0, 0, Warning << "Inconsistent pubs other than Cit-sub found amongst the input data. First occurence is in record with " << id_str << " from file \"" << file << "\".");
            }
        }
    }

    return num_of_pubs;
}

typedef void(*StringProcessFunc) (const CSeqdesc& , list<string>& , const CDataChecker& );

static void CollectDataFromDescr(const CSeq_entry& entry, list<string>& container, StringProcessFunc process, const CDataChecker& checker)
{
    if (entry.IsSeq() && !entry.GetSeq().IsNa()) {
        return;
    }

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            for (auto& descr : descrs->Get()) {
                process(*descr, container, checker);
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for_each(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                    [&container, &process, &checker](const CRef<CSeq_entry>& cur_entry) { CollectDataFromDescr(*cur_entry, container, process, checker); });
    }
}

static void ProcessComment(const CSeqdesc& descr, list<string>& comments, const CDataChecker& checker)
{
    if (descr.IsComment() && !descr.GetComment().empty() && checker.IsStringPresent(descr.GetComment())) {
        comments.push_back(descr.GetComment());
    }
}

static void ProcessStructuredComment(const CSeqdesc& descr, list<string>& comments, const CDataChecker& checker)
{
    if (IsUserObjectOfType(descr, "StructuredComment")) {

        const CUser_object& user_obj = descr.GetUser();
        const string& comment = ToString(user_obj);

        if (checker.IsStringPresent(comment)) {
            comments.push_back(comment);
        }
    }
}

typedef function<void(const CSeq_entry&)> CommentErrorReportFunc;

static void CommentErrorReport(const CSeq_entry& entry, const string& comment_type, const string& file)
{
    string id_str;
    GetSeqIdStrFromEntry(entry, id_str);
    if (id_str.empty()) {
        id_str = "Unknown";
    }
    ERR_POST_EX(0, 0, Warning << comment_type << " descriptor differences were detected. First occurence is in record with " << id_str << " from file \"" << file << "\". Comments will not be removed from contigs, and the master Bioseq will not receive a comment descriptor.");
}

static void CheckComments(const CSeq_entry& entry, StringProcessFunc process, bool& comments_not_set, list<string>& comments, CommentErrorReportFunc report_error)
{
    if (comments_not_set) {

        CDataChecker checker(true, comments);
        CollectDataFromDescr(entry, comments, process, checker);
        comments_not_set = comments.empty();
    }
    else {

        if (!comments.empty()) {

            list<string> common_comments;
            CDataChecker checker(false, comments);

            CollectDataFromDescr(entry, common_comments, process, checker);

            if (common_comments.size() < comments.size()) {
                comments.swap(common_comments);
            }

            if (comments.empty()) {
                report_error(entry);
            }
        }
    }
}

static bool IsBiosourcesSame(CBioSource& bio_src_master, const CBioSource& bio_src)
{
    if ((bio_src_master.IsSetGenome() != bio_src.IsSetGenome()) ||
        (bio_src_master.IsSetGenome() && bio_src_master.GetGenome() != bio_src.GetGenome())) {
        return false;
    }

    if ((bio_src_master.IsSetOrigin() != bio_src.IsSetOrigin()) ||
        (bio_src_master.IsSetOrigin() && bio_src_master.GetOrigin() != bio_src.GetOrigin())) {
        return false;
    }

    if (bio_src_master.IsSetIs_focus() != bio_src.IsSetIs_focus()) {
        return false;
    }

    bool ret = true;
    if (bio_src_master.IsSetSubtype() != bio_src.IsSetSubtype()) {
        ret = false;
    }

    if (ret && bio_src_master.IsSetSubtype()) {
        
        for (auto subtype_it = bio_src_master.SetSubtype().begin(); subtype_it != bio_src_master.SetSubtype().end();) {

            auto subtype_found = find_if(bio_src.GetSubtype().begin(), bio_src.GetSubtype().end(), [&subtype_it](const CRef<CSubSource>& a){ return a->Equals(**subtype_it); });

            if (subtype_found == bio_src.GetSubtype().end()) {
                subtype_it = bio_src_master.SetSubtype().erase(subtype_it);
                ret = false;
            }
            else {
                ++subtype_it;
            }
        }

        if (bio_src_master.GetSubtype().empty()) {
            bio_src_master.ResetSubtype();
        }
    }

    return ret;
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
                ERR_POST_EX(0, 0, Critical << "Multiple BioSource descriptors encountered in record from file \"" << file << "\".");
                ret = false;
            }
            else if (num_of_biosources < 1) {
                ERR_POST_EX(0, 0, Warning << "Submission from file \"" << file << "\" is lacking BioSource.");
            }
            else {

                auto biosource_it = find_if(descrs->Get().begin(), descrs->Get().end(), is_biosource);
                if (info.m_biosource.Empty()) {
                    info.m_biosource.Reset(new CBioSource);
                    info.m_biosource->Assign((*biosource_it)->GetSource());
                }
                else {
                    
                    if (!IsBiosourcesSame(*info.m_biosource, (*biosource_it)->GetSource())) {
                        info.m_same_biosource = false;
                    }
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
            auto entry_it = find_if(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
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

static void SortOrgRef(COrg_ref& org_ref)
{
    if (org_ref.IsSetDb()) {

        COrg_ref::TDb& db_tags = org_ref.SetDb();
        sort(db_tags.begin(), db_tags.end(), [](const CRef<CDbtag>& tag1, const CRef<CDbtag>& tag2)
            {
                if (tag1.Empty() || !tag1->IsSetDb()) {
                    return true;
                }

                if (tag2.Empty() || !tag2->IsSetDb()) {
                    return false;
                }

                return tag1->GetDb() < tag2->GetDb();
            });
    }

    if (org_ref.IsSetMod()) {
        COrg_ref::TMod& mods = org_ref.SetMod();
        mods.sort();
    }
}

static bool CheckSameOrgRefs(list<COrgRefInfo>& org_refs)
{
    if (org_refs.empty()) {
        return true;
    }

    auto cur_org_ref = org_refs.begin();
    CRef<COrg_ref>& first_org_ref = cur_org_ref->m_org_ref;
    SortOrgRef(*first_org_ref);

    for (++cur_org_ref; cur_org_ref != org_refs.end(); ++cur_org_ref) {
        CRef<COrg_ref>& next_org_ref = cur_org_ref->m_org_ref;
        SortOrgRef(*next_org_ref);

        if (!first_org_ref->Equals(*next_org_ref)) {
            return false;
        }
    }

    return true;
}

static bool DBLinkProblemReport(const CMasterInfo& info)
{
    bool reject = false;
    if (info.m_dblink.NotEmpty() && info.m_dblink_state != eDblinkNoProblem) {
        if (info.m_dblink_state & eDblinkDifferentDblink) {
            ERR_POST_EX(0, 0, Critical << "The files being processed contain DBLink User-objects that are not identical in content. The first difference was encountered at sequence \"" <<
                        info.m_dblink_diff_info.second << "\" of input file \"" << info.m_dblink_diff_info.first << "\".");
            reject = true;
        }
        if (info.m_dblink_state & eDblinkNoDblink) {
            string err_msg = "The files being processed contain some records that lack DBLink User-objects. The first record that lacks a DBLink was encountered at sequence \"" +
                info.m_dblink_empty_info.second + "\" of input file \"" + info.m_dblink_empty_info.first + "\". ";

            if (GetParams().IsDblinkOverride()) {
                ERR_POST_EX(0, 0, Warning << err_msg << "Continue anyway.");
            }
            else {
                ERR_POST_EX(0, 0, Critical << err_msg << "Rejecting the whole project.");
                reject = true;
            }
        }
    }
    return reject;
}

static void ReplaceFieldInDBLink(const TIdContainer& values, const string& tag, CUser_object& user_obj)
{
    if (!values.empty()) {

        vector<string> value_list(values.begin(), values.end());

        CConstRef<CUser_field> field = user_obj.GetFieldRef(tag);
        if (field.NotEmpty()) {
            user_obj.SetField(tag).SetData().SetStrs().swap(value_list);
        }
        else {
            user_obj.AddField(tag, value_list);
        }
    }
}

static void CheckSetOfIds(CRef<CUser_object>& user_obj, const string& tag, const TIdContainer& ids, const string& what, const string& cmd_line_param, bool& reject)
{
    TIdContainer cur_ids;

    if (user_obj.NotEmpty() && user_obj->IsSetData()) {
        auto tag_field = user_obj->GetFieldRef(tag);
        if (tag_field.NotEmpty() && tag_field->IsSetData() && tag_field->GetData().IsStrs()) {

            for (auto& id : tag_field->GetData().GetStrs()) {
                cur_ids.insert(id);
            }
        }
    }

    if (!cur_ids.empty()) {

        if (!ids.empty() && ids != cur_ids) {

            CNcbiOstrstream msg;
            msg << "Submission supplied " << tag << " values do not match the ones provided in command line: \"" << NStr::Join(cur_ids, ",") << "\" vs \"" << NStr::Join(ids, ",") << ends;
            if (GetParams().IsDblinkOverride()) {
                ERR_POST_EX(0, 0, Warning << msg.str() << "\". Using values from the command line.");
            }
            else {
                reject = true;
                ERR_POST_EX(0, 0, Critical << msg.str() << "\". Rejecting the whole project.");
            }

            ReplaceFieldInDBLink(ids, tag, *user_obj);
        }
    }
    else if (!ids.empty()) {
        if (user_obj.NotEmpty()) {
            ReplaceFieldInDBLink(ids, tag, *user_obj);
        }
        ERR_POST_EX(0, 0, Info << "All records from files being processed are lacking " << what << ". Using command line \"" << cmd_line_param << "\" values.");
    }
}

static CUser_object* GetDBLinkFromIdMasterBioseq(CRef<CSeq_entry>& bioseq)
{
    CUser_object* dblink_user_obj = nullptr;
    if (bioseq.NotEmpty() && bioseq->IsSetDescr() && bioseq->GetDescr().IsSet()) {

        auto dblink_user_obj_it = find_if(bioseq->SetDescr().Set().begin(), bioseq->SetDescr().Set().end(), [](const CRef<CSeqdesc>& descr) { return IsUserObjectOfType(*descr, "DBLink"); });
        if (dblink_user_obj_it != bioseq->GetDescr().Get().end()) {

            dblink_user_obj = &(*dblink_user_obj_it)->SetUser();
        }
    }

    return dblink_user_obj;
}

static void ReportLackOfDBLinkData(const string& tag, const string& val, bool& reject)
{
    CNcbiOstrstream msg;
    msg << "The DBLink User-object content from the files being processed lacks \"" << tag << ":" << val << "\" link that is present in the current WGS-Master for this WGS project. " << ends;

    if (GetParams().IsDblinkOverride()) {
        ERR_POST_EX(0, 0, Warning << msg.str() << "Using DBLink from the input data.");
    }
    else {
        reject = true;
        ERR_POST_EX(0, 0, Critical << msg.str() << "Rejecting the whole project.");
    }
}

static void ReportLackOfDBLinkDataAll(const string& tag, const CUser_field::C_Data::TStrs& vals, bool& reject)
{
    for (auto& val : vals) {
        ReportLackOfDBLinkData(tag, val, reject);
    }
}

static void ReportNewDBLinkData(const string& tag, const string& val, bool& reject)
{
    CNcbiOstrstream msg;
    msg << "The DBLink User-object content from the files being processed contains new link \"" << tag << ":" << val << "\", not present in the current WGS-Master for this WGS project. " << ends;

    if (GetParams().IsDblinkOverride()) {
        ERR_POST_EX(0, 0, Warning << msg.str() << "Using DBLink from the input data.");
    }
    else {
        reject = true;
        ERR_POST_EX(0, 0, Critical << msg.str() << "Rejecting the whole project.");
    }
}

static void ReportNewDBLinkDataAll(const string& tag, const CUser_field::C_Data::TStrs& , bool& )
{
    ERR_POST_EX(0, 0, Warning << "The DBLink User-object content from the files being processed contains new type of link \"" << tag << "\", not present in the current WGS-Master for this WGS project.");
}

typedef void(*ReportIssue)(const string& tag, const string& val, bool& reject);
typedef void(*ReportAllIssues)(const string& tag, const CUser_field::C_Data::TStrs& vals, bool& reject);

static void CheckCurrentDBLinkConsistency(const CUser_object& first, const CUser_object& second, bool& reject, ReportIssue report, ReportAllIssues reportAll)
{
    if (first.IsSetData()) {

        for (auto& id_field : first.GetData()) {

            if (id_field->IsSetLabel() && id_field->GetLabel().IsStr() &&
                id_field->IsSetData() && id_field->GetData().IsStrs()) {

                auto& tag = id_field->GetLabel().GetStr();
                auto cur_field = second.GetFieldRef(tag);
                if (cur_field.Empty()) {

                    reportAll(tag, id_field->GetData().GetStrs(), reject);
                }
                else {

                    auto& cur_vals = cur_field->GetData().GetStrs();
                    for (auto& val : id_field->GetData().GetStrs()) {

                        auto cur_val = find_if(cur_vals.begin(), cur_vals.end(), [&val](const string& item) { return item == val; });
                        if (cur_val == cur_vals.end()) {
                            report(tag, val, reject);
                        }
                    }
                }
            }
        }
    }
}

static void FixDBLink(CRef<CUser_object>& user_obj)
{
    if (user_obj.Empty()) {
        user_obj.Reset(new CUser_object);
        user_obj->SetType().SetStr("DBLink");
    }

    ReplaceFieldInDBLink(GetParams().GetBioProjectIds(), "BioProject", *user_obj);
    ReplaceFieldInDBLink(GetParams().GetBioSampleIds(), "BioSample", *user_obj);
    ReplaceFieldInDBLink(GetParams().GetSRAIds(), "Sequence Read Archive", *user_obj);
}

static void CheckMasterDblink(CMasterInfo& info)
{
    CheckSetOfIds(info.m_dblink, "BioProject", GetParams().GetBioProjectIds(), "BioProject Accession Numbers", "-B", info.m_reject);
    CheckSetOfIds(info.m_dblink, "BioSample", GetParams().GetBioSampleIds(), "BioSample ids", "-C", info.m_reject);
    CheckSetOfIds(info.m_dblink, "Sequence Read Archive", GetParams().GetSRAIds(), "SRA accessions", "-C", info.m_reject);

    FixDBLink(info.m_dblink);

    if (info.m_dblink_state == eDblinkNoDblink) {
        info.m_dblink_state = eDblinkNoProblem;
    }

    CUser_object* id_dblink_user_obj = GetDBLinkFromIdMasterBioseq(info.m_id_master_bioseq);
    if (GetParams().IsDblinkOverride() && id_dblink_user_obj && info.m_dblink->GetFieldRef("BioSample").Empty()) {

        auto biosample_field = id_dblink_user_obj->GetFieldRef("BioSample");
        if (biosample_field.NotEmpty() && biosample_field->IsSetData() && biosample_field->GetData().IsStrs()) {
            ERR_POST_EX(0, 0, Warning << "The DBLink User-object content from the files being processed lacks BioSample link that is present in the current WGS-Master for this WGS project. Using the one from the current Master.");
            info.m_dblink->AddField("BioSample", biosample_field->GetData().GetStrs());
        }
    }

    if (id_dblink_user_obj == nullptr) {
        
        if (info.m_id_master_bioseq.NotEmpty()) {
            if (info.m_dblink->IsSetData()) {

                for (auto& user_field : info.m_dblink->GetData()) {
                    if (user_field->IsSetLabel() && user_field->GetLabel().IsStr()) {
                        ERR_POST_EX(0, 0, Warning << "The DBLink User - object content from the files being processed contains new type of link \"" << user_field->GetLabel().GetStr() << "\", not present in the current WGS-Master for this WGS project.");
                    }
                }
            }
        }

        return;
    }

    CheckCurrentDBLinkConsistency(*id_dblink_user_obj, *info.m_dblink, info.m_reject, ReportLackOfDBLinkData, ReportLackOfDBLinkDataAll);
    CheckCurrentDBLinkConsistency(*info.m_dblink, *id_dblink_user_obj, info.m_reject, ReportNewDBLinkData, ReportNewDBLinkDataAll);
}

static string GetAccessionValue(size_t val_len, int val)
{
    CNcbiOstrstream sstream;
    sstream << setfill('0') << setw(2) << GetParams().GetAssemblyVersion() << setw(val_len) << val << ends;
    return sstream.str();
}

static const size_t LENGTH_NOT_SET = -1;

static CRef<CSeq_id> CreateAccession(int last_accession_num, size_t accession_len)
{
    size_t max_accession_len = GetMaxAccessionLen(last_accession_num);

    if (accession_len == LENGTH_NOT_SET) {
        accession_len = max_accession_len;
    }

    CRef<CSeq_id> ret;
    if (accession_len != max_accession_len) {

        CNcbiStrstream msg;
        msg << "Incorrect format for accessions, given the total number of contigs in the project: \"N+2+" << accession_len << "\" was used, but only \"N+2+" << max_accession_len << "\" is needed." << ends;

        if (GetParams().GetSource() == eNCBI) {
            ERR_POST_EX(0, 0, Critical << msg.str());
            return ret;
        }

        ERR_POST_EX(0, 0, Info << msg.str());
    }

    string id_num(accession_len + 2, '0');

    CRef<CTextseq_id> text_id(new CTextseq_id);
    text_id->SetAccession(GetParams().GetIdPrefix() + id_num);

    id_num = GetAccessionValue(accession_len, 0);
    text_id->SetName(GetParams().GetIdPrefix() + id_num);

    text_id->SetVersion(GetParams().GetAssemblyVersion());

    auto set_fun = FindSetTextSeqIdFunc(GetParams().GetIdChoice());
    _ASSERT(set_fun != nullptr && "There should be a valid SetTextId function. Validate the ID choice.");

    if (set_fun == nullptr) {
        return ret;
    }

    ret.Reset(new CSeq_id);
    (ret->*set_fun)(*text_id);

    return ret;
}

static void SetMolInfo(CBioseq& bioseq, CMolInfo::TBiomol biomol)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    CMolInfo& mol_info = descr->SetMolinfo();

    if (GetParams().IsTsa()) {

        bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);
        mol_info.SetTech(CMolInfo::eTech_tsa);

        int fix_tech = GetParams().GetFixTech();
        CMolInfo::TBiomol cur_biomol = biomol;
        if (fix_tech & eFixMolBiomol) {
            cur_biomol = CMolInfo::eBiomol_transcribed_RNA;
        }

        if (fix_tech & eFixBiomol_mRNA) {
            cur_biomol = CMolInfo::eBiomol_mRNA;
        }
        else if (fix_tech & eFixBiomol_rRNA) {
            cur_biomol = CMolInfo::eBiomol_rRNA;
        }
        else if (fix_tech & eFixBiomol_ncRNA) {
            cur_biomol = CMolInfo::eBiomol_ncRNA;
        }

        mol_info.SetBiomol(cur_biomol);
    }
    else {

        bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);

        CMolInfo::ETech tech = GetParams().IsTls() ? CMolInfo::eTech_targeted : CMolInfo::eTech_wgs;
        mol_info.SetTech(tech);
        mol_info.SetBiomol(CMolInfo::eBiomol_genomic);
    }

    bioseq.SetDescr().Set().push_back(descr);
}

static void CreatePub(CBioseq& bioseq, const CPubdesc& pubdescr)
{
    CRef<CSeqdesc> descr(new CSeqdesc);

    descr->SetPub().Assign(pubdescr);
    bioseq.SetDescr().Set().push_back(descr);
}

static bool IsResetGenome()
{
    return GetParams().GetSource() == eNCBI ||
        (GetParams().GetUpdateMode() != eUpdateAssembly &&
        GetParams().GetUpdateMode() != eUpdateNew &&
        GetParams().GetUpdateMode() != eUpdateFull);
}

static bool CreateBiosource(CBioseq& bioseq, CBioSource& biosource, const list<COrgRefInfo>& org_refs)
{

    bool is_tax_lookup = GetParams().IsTaxonomyLookup();
    if (!PerformTaxLookup(biosource, org_refs, is_tax_lookup) && is_tax_lookup) {
        ERR_POST_EX(0, 0, Critical << "Taxonomy lookup failed on Master Bioseq. Cannot proceed.");
        return false;
    }

    if (IsResetGenome()) {
        biosource.ResetGenome();
    }

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetSource().Assign(biosource);
    bioseq.SetDescr().Set().push_back(descr);

    if (GetParams().IsTsa() && biosource.IsSetOrg() && biosource.GetOrg().IsSetTaxname()) {

        CRef<CSeqdesc> title(new CSeqdesc);
        title->SetTitle("TSA: " + biosource.GetOrg().GetTaxname() + ", transcriptome shotgun assembly");
        bioseq.SetDescr().Set().push_back(title);
    }

    return true;
}

static void AddField(CUser_object& user_obj, const string& label, const string& val)
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(label);
    field->SetString(val);

    user_obj.SetData().push_back(field);
}

static void CreateUserObject(const CMasterInfo& info, CBioseq& bioseq)
{
    CRef<CUser_object> user_obj(new CUser_object);
    CObject_id& obj_id = user_obj->SetType();

    static const string ACCESSION_FIRST("_accession_first");
    static const string ACCESSION_LAST("_accession_last");

    string accession_first_label,
        accession_last_label,
        accession_first_val,
        accession_last_val;

    int first = 1,
        last = info.m_num_of_entries;

    if (GetParams().GetUpdateMode() == eUpdateExtraContigs) {

        _ASSERT(info.m_current_master && "Should not be NULL in this mode");
        first += info.m_current_master->m_last_contig;
        last += info.m_current_master->m_last_contig;
    }

    if (GetParams().IsTsa()) {
        obj_id.SetStr("TSA-RNA-List");
        accession_first_label = "TSA";
        accession_last_label = "TSA";
    }
    else if (GetParams().IsTls()) {
        obj_id.SetStr("TLSProjects");
        accession_first_label = "TLS";
        accession_last_label = "TLS";
    }
    else {
        obj_id.SetStr("WGSProjects");
        accession_first_label = "WGS";
        accession_last_label = "WGS";
    }

    accession_first_label += ACCESSION_FIRST;
    accession_last_label += ACCESSION_LAST;

    size_t max_accession_len = GetMaxAccessionLen(last);
    accession_first_val = GetAccessionValue(max_accession_len, first);
    accession_last_val = GetAccessionValue(max_accession_len, last);

    AddField(*user_obj, accession_first_label, GetParams().GetIdPrefix() + accession_first_val);
    AddField(*user_obj, accession_last_label, GetParams().GetIdPrefix() + accession_last_val);

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser().Assign(*user_obj);
    bioseq.SetDescr().Set().push_back(descr);
}

static void CreateProtCountUserObject(size_t num_of_prot_seq, CBioseq& bioseq)
{
    CRef<CSeqdesc> descr(new CSeqdesc);

    CUser_object& user_obj = descr->SetUser();
    user_obj.SetType().SetStr("WGSProteinStatistics");

    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr("TotalContigProteins");
    field->SetInt(static_cast<int>(num_of_prot_seq));
    user_obj.SetData().push_back(field);

    bioseq.SetDescr().Set().push_back(descr);
}


static bool CreateDateDescr(CBioseq& bioseq, const CDate& date, EDateIssues issue, bool is_update_date)
{
    if (date.Which() == CDate::e_not_set || issue != eDateNoIssues)
        return false;

    CRef<CSeqdesc> descr(new CSeqdesc);

    if (is_update_date) {
        descr->SetUpdate_date().Assign(date);
    }
    else {
        descr->SetCreate_date().Assign(date);
    }

    bioseq.SetDescr().Set().push_back(descr);

    return true;
}

static void AddComment(CBioseq& bioseq, const string& comment)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetComment(comment);

    bioseq.SetDescr().Set().push_back(descr);
}

static void CreateDbLink(CBioseq& bioseq, CUser_object& user_obj)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser(user_obj);

    bioseq.SetDescr().Set().push_back(descr);
}

static void UpdateDbLink(CBioseq& bioseq, CUser_object& user_obj)
{
    auto& descrs = bioseq.SetDescr().Set();
    for (auto descr = descrs.begin(); descr != descrs.end();) {

        if (IsUserObjectOfType(**descr, "GenomeProjectsDB")) {
            ERR_POST_EX(0, 0, Warning << "Removing GPID User-object in favour of BioProject.");
            descr = descrs.erase(descr);
        }
        else if (IsUserObjectOfType(**descr, "DBLink")) {
            (*descr)->SetUser(user_obj);
            return;
        }
        else {
            ++descr;
        }
    }

    CreateDbLink(bioseq, user_obj);
}

static const string TPA_KEYWORD("TPA:assembly");

static bool FixTpaKeyword(set<string>& keywords)
{
    static const string TPA_KEYWORD_OLD("TPA:reassembly");

    bool ret = false,
         reset_tpa_keyword = !GetParams().GetTpaKeyword().empty();

    if (GetParams().IsVDBMode()) {

        auto tpa_keyword_it = keywords.find(TPA_KEYWORD);
        if (tpa_keyword_it != keywords.end()) {
            ret = true;

            if (reset_tpa_keyword) {
                keywords.erase(tpa_keyword_it);
            }
        }

        tpa_keyword_it = keywords.find(TPA_KEYWORD_OLD);
        if (tpa_keyword_it != keywords.end()) {
            ret = true;

            if (reset_tpa_keyword) {
                keywords.erase(tpa_keyword_it);
            }
        }

        if (ret && reset_tpa_keyword) {
            keywords.insert(GetParams().GetTpaKeyword());
        }
    }

    return ret;
}

static CGB_block* ProcessKeywords(CBioseq& bioseq, const CMasterInfo& info)
{
    CRef<CSeqdesc> descr;
    if (GetParams().IsVDBMode()) {

        descr.Reset(new CSeqdesc);
        for (auto& keyword : info.m_keywords) {
            if (!keyword.empty()) {
                descr->SetGenbank().SetKeywords().push_back(keyword);
            }
        }
    }
    else {

        if (GetParams().IsTsa() && info.m_has_targeted_keyword) {
            descr.Reset(new CSeqdesc);
            descr->SetGenbank().SetKeywords().push_back("Targeted");
        }
        else if (GetParams().IsWgs() && info.m_has_gmi_keyword) {
            descr.Reset(new CSeqdesc);
            descr->SetGenbank().SetKeywords().push_back("GMI");
        }
    }

    if (descr.NotEmpty()) {
        bioseq.SetDescr().Set().push_back(descr);
    }

    return descr.NotEmpty() ? &descr->SetGenbank() : nullptr;
}

static void AddTpaKeyword(CBioseq& bioseq, CGB_block* gb_block)
{
    CRef<CSeqdesc> descr;
    if (gb_block == nullptr) {
        descr.Reset(new CSeqdesc);
        gb_block = &descr->SetGenbank();
        bioseq.SetDescr().Set().push_back(descr);
    }

    gb_block->SetKeywords().push_back(GetParams().GetTpaKeyword().empty() ? TPA_KEYWORD : GetParams().GetTpaKeyword());
}

static void AddPub(CBioseq& bioseq, const CPub_equiv& pub)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetPub().SetPub().Assign(pub);
    bioseq.SetDescr().Set().push_back(descr);
}

bool CheckCitArtPmidPrepub(const CPub_equiv& pub, bool inpress, bool check_pmid, string* iso_jta)
{
    if (!pub.IsSet()) {
        return false;
    }

    bool got_pmid = false,
         got_inpress = false;

    for (auto cur_pub : pub.Get()) {

        if (cur_pub->IsPmid()) {
            got_pmid = true;
        }
        else if (cur_pub->IsArticle()) {

            const CCit_art& cit_art = cur_pub->GetArticle();
            if (cit_art.IsSetFrom() && cit_art.GetFrom().IsJournal()) {

                const CCit_jour& journal = cit_art.GetFrom().GetJournal();

                if (inpress) {
                    if (journal.IsSetImp() && journal.GetImp().IsSetPrepub() && journal.GetImp().GetPrepub() == CImprint::ePrepub_in_press) {
                        got_inpress = true;
                    }
                }
                else {
                    if (!journal.IsSetImp() || !journal.GetImp().IsSetPrepub()) {
                        got_inpress = true;
                    }
                }

                if (iso_jta && journal.IsSetTitle() && journal.GetTitle().IsSet()) {

                    auto& titles = journal.GetTitle().Get();
                    auto iso_jta_it = find_if(titles.begin(), titles.end(), [](const CRef<CTitle::C_E>& title) { return title->IsIso_jta(); });
                    if (iso_jta_it != titles.end()) {
                        *iso_jta = (*iso_jta_it)->GetIso_jta();
                    }
                }
            }

            if (check_pmid && cit_art.IsSetIds() && cit_art.GetIds().IsSet()) {

                auto& ids = cit_art.GetIds().Get();
                auto pubmed_id = find_if(ids.begin(), ids.end(), [](const CRef<CArticleId>& id) { return id->IsPubmed(); });
                if (pubmed_id != ids.end()) {
                    got_pmid = true;
                }
            }
        }
    }

    if (!check_pmid) {
        return got_inpress;
    }

    return !got_pmid && got_inpress;
}

static size_t GetNumOfPubs(const CPub_equiv& pub, CPub::E_Choice type)
{
    size_t ret = 0;
    if (pub.IsSet()) {
        ret = count_if(pub.Get().begin(), pub.Get().end(), [&type](const CRef<CPub>& cur_pub) { return cur_pub->Which() == type; });
    }

    return ret;
}

static CRef<CSeq_entry> CreateMasterBioseq(CMasterInfo& info, CRef<CCit_sub>& cit_sub, CRef<CContact_info>& contact_info, CMolInfo::TBiomol biomol)
{
    CRef<CBioseq> bioseq(new CBioseq);
    CRef<CSeq_entry> ret;

    int last_accession_num = info.m_num_of_entries;
    size_t accession_len = LENGTH_NOT_SET;

    CRef<CSeq_id> id = CreateAccession(last_accession_num, accession_len);
    if (id.Empty()) {
        return ret;
    }

    _ASSERT(id->GetTextseq_Id() != nullptr && id->GetTextseq_Id()->IsSetName() && "CreateAccession(...) should create 'TextId' with 'Name' attribute");
    info.m_master_file_name = id->GetTextseq_Id()->GetName();

    bioseq->SetId().push_back(id);
    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);

    bioseq->SetInst().SetLength(GetParams().GetIdChoice() == CSeq_id::e_Other ? info.m_whole_len : info.m_num_of_entries);

    SetMolInfo(*bioseq, biomol);

    // Keywords
    bool is_tpa_keyword_present = false;
    CGB_block* gb_block = nullptr;

    if (info.m_keywords_set) {
        is_tpa_keyword_present = FixTpaKeyword(info.m_keywords);
        gb_block = ProcessKeywords(*bioseq, info);
    }

    if (GetParams().IsTpa() && !is_tpa_keyword_present) {
        AddTpaKeyword(*bioseq, gb_block);
    }

    // Comments
    if (info.m_common_comments.empty() && GetParams().GetSource() != eNCBI) {

        if (info.m_common_structured_comments.empty()) {
            ERR_POST_EX(0, 0, Info << "All contigs are missing both text and structured comments.");
        }
    }

    for (auto& comment : info.m_common_comments) {
        AddComment(*bioseq, comment);
    }

    for (auto& structured_comment : info.m_common_structured_comments) {
        bioseq->SetDescr().Set().push_back(BuildStructuredComment(structured_comment));
    }

    // CitSub
    if (GetParams().GetUpdateMode() == eUpdateAssembly && info.m_current_master && info.m_current_master->m_cit_sub.NotEmpty()) {
        AddPub(*bioseq, *info.m_current_master->m_cit_sub);
    }

    if (cit_sub.NotEmpty()) {

        if (cit_sub->IsSetImp()) {
            if (!cit_sub->IsSetDate() && cit_sub->GetImp().IsSetDate()) {
                cit_sub->SetDate().Assign(cit_sub->GetImp().GetDate());
            }
            cit_sub->ResetImp();
        }

        bioseq->SetDescr().Set().push_back(CreateCitSub(*cit_sub, contact_info));
    }

    // CitArts
    CSeq_descr::Tdata::iterator pub_to_be_removed;
    bool is_set_pub_to_be_removed = false,
         more_than_one_cit_art = false;
    
    
    if (info.m_current_master) {

        more_than_one_cit_art = info.m_current_master->m_cit_arts.size() > 1;
        for (auto cit_art : info.m_current_master->m_cit_arts) {

            AddPub(*bioseq, *cit_art);

            if (more_than_one_cit_art && CheckCitArtPmidPrepub(*cit_art, true, true)) {

                pub_to_be_removed = bioseq->SetDescr().Set().end();
                --pub_to_be_removed;
                is_set_pub_to_be_removed = true;
            }
        }
    }

    size_t num_of_arts = 0,
           num_of_subs = 0;

    const CPub_equiv* pub_article = nullptr;
    for (auto& pubdescr : info.m_common_pubs) {

        const CPubInfo& pub_info = info.m_pubs.GetPubInfo(pubdescr);
        CreatePub(*bioseq, *pub_info.m_desc);

        size_t cur_num_of_arts = GetNumOfPubs(pub_info.m_desc->GetPub(), CPub::e_Article);

        if (num_of_arts == 0 && cur_num_of_arts == 1) {
            pub_article = &pub_info.m_desc->GetPub();
        }

        num_of_arts += cur_num_of_arts;
        if (GetNumOfPubs(pub_info.m_desc->GetPub(), CPub::e_Sub)) {
            ++num_of_subs;
        }
    }

    if (is_set_pub_to_be_removed && GetParams().GetUpdateMode() == eUpdateAssembly && GetParams().GetSource() != eNCBI &&
        num_of_arts == 1 && CheckCitArtPmidPrepub(*pub_article, false, true)) {
        bioseq->SetDescr().Set().erase(pub_to_be_removed);
    }
    
    if (num_of_subs < info.m_num_of_pubs && num_of_subs == 0) {
        info.m_num_of_pubs = 0;
    }

    if (info.m_biosource.NotEmpty()) {
        if (!CreateBiosource(*bioseq, *info.m_biosource, info.m_org_refs)) {
            return ret;
        }
    }

    if (GetParams().GetSource() != eNCBI) {
        info.m_update_date_present = CreateDateDescr(*bioseq, *info.m_update_date, info.m_update_date_issues, true);
        info.m_creation_date_present = CreateDateDescr(*bioseq, *info.m_creation_date, info.m_creation_date_issues, false);
    }

    if (info.m_num_of_entries) {
        CreateUserObject(info, *bioseq);
    }

    if (info.m_num_of_prot_seq) {
        CreateProtCountUserObject(info.m_num_of_prot_seq, *bioseq);
    }

    if (info.m_dblink_state == eDblinkNoProblem && info.m_dblink.NotEmpty()) {
        UpdateDbLink(*bioseq, *info.m_dblink);
    }

    ret.Reset(new CSeq_entry);
    ret->SetSeq(*bioseq);

    return ret;
}

static bool IsDupIds(const list<string>& ids)
{
    set<string> unique_ids;
    for (auto& id : ids) {
        if (!unique_ids.insert(id).second) {

            ERR_POST_EX(0, 0, Error << "Found duplicated general or local id: \"" << id << "\".");
            return true;
        }
    }

    return false;
}

static bool NeedToGetAccessionPrefix() {

    return GetParams().IsUpdateScaffoldsMode() && GetParams().IsAccessionAssigned() && GetParams().GetScaffoldPrefix().empty();
}

static void ReportDateProblem(EDateIssues issue, string date_type, bool is_error)
{
    if (issue == eDateMissing) {
        ERR_POST_EX(0, 0, (is_error ? Error : Info) <<
                    date_type << " date is missing from one or more input submissions.Will not propagate " <<
                    date_type << " date to the master record.");
    }
    else if (issue == eDateDiff) {
        ERR_POST_EX(0, 0, (is_error ? Error : Info) <<
                    "Different " << date_type << " dates encountered amongst input submissions.Will not propagate " <<
                    date_type << " date to the master record.");
    }
}

static bool IsDateFound(const CSeq_descr::Tdata& descrs, CSeqdesc::E_Choice choice)
{
    auto date = find_if(descrs.begin(), descrs.end(), [choice](const CRef<CSeqdesc>& desc){ return desc->Which() == choice; });
    return date != descrs.end();
}

static bool IsDatePresent(const CSeq_entry& entry, CSeqdesc::E_Choice choice)
{
    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs) && descrs->IsSet()) {
        if (IsDateFound(descrs->Get(), choice)) {
            return true;
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            if (IsDatePresent(*cur_entry, choice)) {
                return true;
            }
        }
    }

    return false;
}

static bool GetFastaSeqIdLabel(const CSeq_entry& entry, string& label)
{
    if (entry.IsSeq()) {
        if (entry.GetSeq().IsNa()) {
            entry.GetLabel(&label, CSeq_entry::eContent);
            return true;
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
                if (GetFastaSeqIdLabel(*cur_entry, label)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static void ReportMissingOrDiffCitSub(const CSeq_entry& entry, bool missing)
{
    string label;
    if (!GetFastaSeqIdLabel(entry, label)) {
        label = "Unknown";
    }

    if (missing) {
        ERR_POST_EX(0, 0, Error << "Required Cit-sub is missing from the record with id \"" << label << "\".");
    }
    else {
        ERR_POST_EX(0, 0, Error << "Different Cit-subs encountered amongst the data. First appearance is in record with id \"" << label << "\".");
    }
}

static bool EqualNoDate(CCit_sub& a, CCit_sub& b)
{
    CRef<CDate> date_a,
                date_b;

    if (a.IsSetDate()) {
        date_a.Reset(new CDate);
        date_a->Assign(a.GetDate());
        a.ResetDate();
    }

    if (b.IsSetDate()) {
        date_b.Reset(new CDate);
        date_b->Assign(b.GetDate());
        b.ResetDate();
    }

    bool ret = a.Equals(b);

    if (date_a.NotEmpty()) {
        a.SetDate().Assign(*date_a);
    }
    if (date_b.NotEmpty()) {
        b.SetDate().Assign(*date_b);
    }

    return ret;
}

static bool CheckCitSubsInBioseqSet(CCitSubInfo& cit_sub_info, CSeq_submit& submit)
{
    _ASSERT(submit.IsEntrys() && "Should be entries");

    bool ret = true;

    CCit_sub* first_cit_sub = nullptr;

    for (auto entry : submit.GetData().GetEntrys()) {

        CCit_sub* cit_sub = nullptr;

        const CSeq_descr* descrs = nullptr;
        if (GetDescr(*entry, descrs)) {

            if (descrs && descrs->IsSet()) {

                for (auto descr : descrs->Get()) {

                    if (descr->IsPub()) {

                        cit_sub = GetNonConstCitSub(descr->SetPub());
                        if (cit_sub) {
                            if (GetParams().IsSetSubmissionDate()) {
                                cit_sub->SetDate().SetStd().Assign(GetParams().GetSubmissionDate());
                            }
                            break;
                        }
                    }
                }
            }
        }
        else {
            ret = false;
            break;
        }

        if (cit_sub == nullptr) {

            ReportMissingOrDiffCitSub(*entry, true);
            ret = false;
            break;
        }

        if (GetParams().GetSource() == eEMBL || GetParams().GetSource() == eDDBJ) {
            
            if (cit_sub_info.m_cit_sub.NotEmpty()) {

                if (!EqualNoDate(*cit_sub_info.m_cit_sub, *cit_sub)) {
                    ReportMissingOrDiffCitSub(*entry, false);
                    ret = false;
                    break;
                }

                if (cit_sub->IsSetDate()) {

                    string date_str = ToString(cit_sub->GetDate());

                    bool found = false;
                    for (auto& date_info : cit_sub_info.m_dates) {
                        if (date_info.first == date_str) {
                            ++date_info.second.second;
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        cit_sub_info.AddDate(cit_sub->GetDate());
                    }
                }
            }
            else {
                cit_sub_info.m_cit_sub.Reset(new CCit_sub);
                cit_sub_info.m_cit_sub->Assign(*cit_sub);
                cit_sub_info.AddDate(cit_sub->GetDate());
            }
        }
        else {

            if (first_cit_sub) {
                if (!EqualNoDate(*first_cit_sub, *cit_sub)) {
                    ReportMissingOrDiffCitSub(*entry, false);
                    ret = false;
                    break;
                }
            }
            else {
                first_cit_sub = cit_sub;
            }
        }
    }

    return ret;
}

struct CEntryOrderInfo
{
    string m_first_id_str,
           m_id_str,
           m_accession;
    size_t m_seq_len,
           m_file_num;
};

static void AddOrderInfo(const CSeq_entry& entry, list<CEntryOrderInfo>& seq_order, size_t file_num)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa() && entry.GetSeq().IsSetId() && !entry.GetSeq().GetId().empty() && entry.GetSeq().IsSetLength()) {

        CEntryOrderInfo info;
        info.m_seq_len = entry.GetSeq().GetLength();
        info.m_accession = GetSeqIdAccession(entry.GetSeq());
        info.m_id_str = GetSeqIdLocalOrGeneral(entry.GetSeq());
        info.m_first_id_str = GetSeqIdKey(entry.GetSeq());
        info.m_file_num = file_num;

        seq_order.push_back(info);
        return;
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            AddOrderInfo(*cur_entry, seq_order, file_num);
        }
    }
}

static void SortSequences(list<CEntryOrderInfo>& seq_order)
{
    ESortOrder sort_order = GetParams().GetSortOrder();
    if (GetParams().IsAccessionAssigned()) {
        sort_order = eByAccession;
    }

    if (sort_order != eUnsorted) {

        // CR lambda assignment?
        typedef bool (*TSortPredicat)(const CEntryOrderInfo& , const CEntryOrderInfo& );

        TSortPredicat sort_predicat = nullptr;
        switch (sort_order) {
            case eByAccession:
                sort_predicat = [](const CEntryOrderInfo& a, const CEntryOrderInfo& b) { return a.m_accession < b.m_accession; };
                break;
            case eById:
                sort_predicat = [](const CEntryOrderInfo& a, const CEntryOrderInfo& b){ return a.m_id_str < b.m_id_str; };
                break;
            case eSeqLenDesc:
                sort_predicat = [](const CEntryOrderInfo& a, const CEntryOrderInfo& b){ return a.m_seq_len > b.m_seq_len; };
                break;
            case eSeqLenAsc:
                sort_predicat = [](const CEntryOrderInfo& a, const CEntryOrderInfo& b){ return a.m_seq_len < b.m_seq_len; };
                break;
            default:
                ;
        }

        _ASSERT(sort_predicat != nullptr && "sort_predicat should have a valid function pointer");

        seq_order.sort([&sort_predicat](const CEntryOrderInfo& a, const CEntryOrderInfo& b){ return a.m_file_num == b.m_file_num ? sort_predicat(a, b) : a.m_file_num < b.m_file_num; });
    }
}

static void BuildSortOrderMap(const list<CEntryOrderInfo>& seq_order, map<string, int>& order_of_entries, int last_contig)
{
    int cur_num = last_contig + 1;
    for (auto seq_info : seq_order) {
        order_of_entries[seq_info.m_first_id_str] = cur_num++;
    }
}

static bool IsSubmitBlockSet(const CSeq_submit& seq_submit)
{
    return seq_submit.IsSetSub() && seq_submit.GetSub().IsSetContact() && seq_submit.GetSub().GetContact().IsSetContact();
}

static void ReportVersionProblem(int master_accession_ver, int current_master_accession_ver, bool& reject)
{
    if (master_accession_ver < current_master_accession_ver) {
        ERR_POST_EX(0, 0, Critical << "The assembly-version of current WGS master (" << ToStringLeadZeroes(current_master_accession_ver - 1, 2) <<
                    ") is the same or higher than the assembly-version used for the contigs " << ToStringLeadZeroes(master_accession_ver, 2) << ".");
        reject = true;
    }
    else {

        if (GetParams().GetSource() != eNCBI && GetParams().IsDblinkOverride()) {
            ERR_POST_EX(0, 0, Warning << "Non-sequential assembly-version " << ToStringLeadZeroes(master_accession_ver, 2) << " is used for contigs : Expected " <<
                        ToStringLeadZeroes(current_master_accession_ver, 2) <<  ": Allowed, based on source or override switch.");
            SetAssemblyVersion(master_accession_ver);
        }
        else {
            ERR_POST_EX(0, 0, Critical << "Non-sequential assembly-version " << ToStringLeadZeroes(master_accession_ver, 2) << " is used for contigs : Expected " <<
                        ToStringLeadZeroes(current_master_accession_ver, 2) << ".");
            reject = true;
        }
    }
}

static TSeqPos AccumulateLength(const CSeq_entry& entry)
{
    TSeqPos ret = 0;
    if (entry.IsSeq() && entry.GetSeq().IsNa() && entry.GetSeq().IsSetLength()) {
        ret += entry.GetSeq().GetLength();
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            ret += AccumulateLength(*cur_entry);
        }
    }

    return ret;
}

static bool IsValidDeltaSeq(const CSeq_entry& entry)
{
    return entry.GetSeq().IsSetInst() && entry.GetSeq().GetInst().IsSetRepr() && entry.GetSeq().GetInst().GetRepr() == CSeq_inst::eRepr_delta &&
        entry.GetSeq().GetInst().IsSetExt() && entry.GetSeq().GetInst().GetExt().IsDelta() && entry.GetSeq().GetInst().GetExt().GetDelta().IsSet();
}

static bool IsValidAccessionType(const string& accession)
{
    CSeq_id::EAccessionInfo acc_info = CSeq_id::IdentifyAccession(accession);
    CSeq_id::E_Choice acc_type = CSeq_id::GetAccType(acc_info);

    return acc_type == CSeq_id::e_Genbank || acc_type == CSeq_id::e_Embl || acc_type == CSeq_id::e_Ddbj ||
        (acc_type == CSeq_id::e_Other && GetParams().GetIdChoice() == CSeq_id::e_Other);
}

static bool IsValidDdbjEmblScaffold(const string& accession)
{
    static const size_t SCAFFOLD_PREFIX_LEN = 2;
    static const size_t VALID_ACCESSION_LEN = 10;

    bool ret = false;
    if ((GetParams().GetSource() == eDDBJ || GetParams().GetSource() == eEMBL || IsScaffoldPrefix(accession, SCAFFOLD_PREFIX_LEN)) &&
        isdigit(accession[SCAFFOLD_PREFIX_LEN]) && accession.size() == VALID_ACCESSION_LEN) {

        ret = IsDigits(accession.begin() + SCAFFOLD_PREFIX_LEN, accession.end());
    }

    return ret;
}

static bool IsValidAccessionFormat(const string& accession, const CCurrentMasterInfo& info, const string& prefix, size_t no_prefix_acc_len)
{
    size_t start_of_prefix = 0;
    if (NStr::StartsWith(accession, "NZ_")) {
        start_of_prefix = 3;
    }

    size_t end_of_prefix = start_of_prefix;
    for (; end_of_prefix < accession.size(); ++end_of_prefix) {
        if (accession[end_of_prefix] < 'A' || accession[end_of_prefix] > 'Z') {
            break;
        }
    }

    static const size_t PREFIX_LEN = 4;
    if (end_of_prefix - start_of_prefix != PREFIX_LEN || !NStr::StartsWith(accession, prefix) ||
        accession.size() - start_of_prefix != no_prefix_acc_len ||
        !IsDigits(accession.begin() + end_of_prefix, accession.end())) {
        return false;
    }

    int version = (accession[end_of_prefix] - '0') * 10 + (accession[end_of_prefix + 1] - '0');
    if (version != info.m_version) {
        return false;
    }

    int contig_num = NStr::StringToInt(accession.substr(end_of_prefix + 2));
    return contig_num <= info.m_last_contig;
}

static void CheckScaffoldsFarPointers(const CSeq_entry& entry, const CCurrentMasterInfo& info, bool& reject)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa() && !GetParams().GetScaffoldPrefix().empty()) {

        if (!IsValidDeltaSeq(entry)) {

            ERR_POST_EX(0, 0, Critical << "Empty or incorrect sequence data for scaffolds provided.");
            reject = true;
            return;
        }

        const string& prefix = GetParams().GetIdPrefix();
        size_t no_prefix_acc_len = info.GetNoPrefixAccessionLen();

        bool bad_far_pointer = false;

        for (auto& loc : entry.GetSeq().GetInst().GetExt().GetDelta().Get()) {

            if (loc->IsLoc()) {
                if (!loc->GetLoc().IsInt()) {
                    ERR_POST_EX(0, 0, Warning << "Other than simple interval \"from..to\" encountered in scaffolds deltas.");
                }

                string cur_accession;
                for (CSeq_loc_CI cur_loc(loc->GetLoc()); cur_loc; ++cur_loc) {
                    
                    cur_accession.clear();

                    const CSeq_id& cur_id = cur_loc.GetSeq_id();
                    if (cur_id.IsGi() && GetParams().IsDblinkOverride()) {
                        continue;
                    }

                    if (HasTextAccession(cur_id)) {
                        cur_accession = cur_id.GetTextseq_Id()->GetAccession();
                    }
                    else {
                        bad_far_pointer = true;
                        break;
                    }

                    if (GetParams().IsDblinkOverride() && !NStr::StartsWith(cur_accession, prefix)) {
                        bad_far_pointer = !IsValidAccessionType(cur_accession);
                        if (bad_far_pointer) {
                            break;
                        }
                        continue;
                    }
                    
                    if (IsValidDdbjEmblScaffold(cur_accession)) {
                        continue;
                    }

                    bad_far_pointer = !IsValidAccessionFormat(cur_accession, info, prefix, no_prefix_acc_len);
                    if (bad_far_pointer) {
                        break;
                    }
                }

                if (bad_far_pointer) {
                    if (!cur_accession.empty()) {
                        cur_accession = NStr::Quote(cur_accession);
                        cur_accession += ' ';
                    }
                    ERR_POST_EX(0, 0, Critical << "Missing or incorrect far pointer OSLT " << cur_accession << "provided for scaffolds.");
                    reject = true;
                    return;
                }
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            CheckScaffoldsFarPointers(*cur_entry, info, reject);
        }
    }
}

static CRef<CDate> GetMostRelevantDate(const map<string, pair<CRef<CDate>, size_t>>& dates_info)
{
    CRef<CDate> ret;
    size_t date_freq = 0;

    for (auto cur_date : dates_info) {
        if (cur_date.second.second > date_freq) {
            ret = cur_date.second.first;
            date_freq = cur_date.second.second;
        }
        else if (cur_date.second.second == date_freq) {
            if (cur_date.second.first->Compare(*ret) == CDate::eCompare_before) {
                ret = cur_date.second.first;
            }
        }
    }

    return ret;
}

static void ReplaceDatesInCitSub(list<string>& common_pubs, CPubCollection& pubs, const CDate& date)
{
    for (auto& pub : common_pubs) {

        CPubInfo& pub_info = pubs.GetPubInfo(pub);
        CCit_sub* cit_sub = GetNonConstCitSub(*pub_info.m_desc);
        if (cit_sub && cit_sub->IsSetDate()) {
            cit_sub->SetDate().Assign(date);
        }
    }
}

static bool CheckUniqueAccs(const list<string>& accs)
{
    if (accs.empty()) {
        return true;
    }

    auto prev = accs.begin();
    auto next = prev;

    for (++next; next != accs.end(); ++next) {
        if (*next == *prev) {
            ERR_POST_EX(0, 0, Critical << "Found duplicated accession \"" << *prev << "\".");
            return false;
        }
        ++prev;
    }
    return true;
}

static bool CheckAccLengths(const list<string>& accs)
{
    if (accs.empty()) {
        return true;
    }

    auto prev = accs.begin();
    auto next = prev;

    CTextAccessionContainer prev_acc(*prev);

    for (++next; next != accs.end(); ++next) {
        if (next->size() != prev->size()) {

            ERR_POST_EX(0, 0, Critical << "Contigs accessions have different lengths, hence different formats. The first pair is \"" << *prev << "\" and \"" << *next << "\".\n");
            return false;
        }

        CTextAccessionContainer next_acc(*next);
        if (next_acc.GetNumber() - prev_acc.GetNumber() != 1) {

            ERR_POST_EX(0, 0, (GetParams().IsDblinkOverride() ? Warning : Critical) << "Contigs accessions are not contiguous. The first pair is \"" << *prev << "\" and \"" << *next << "\".\n");
            if (!GetParams().IsDblinkOverride()) {
                return false;
            }
        }

        ++prev;
        prev_acc.swap(next_acc);
    }
    return true;
}

static bool GetScaffoldAccessionPrefix(const CSeq_entry& entry, string& prefix)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa() && entry.GetSeq().IsSetId()) {
        for (auto id : entry.GetSeq().GetId()) {

            if (HasTextAccession(*id)) {
                CTextAccessionContainer text_acc(id->GetTextseq_Id()->GetAccession());
                if (text_acc.IsValid()) {
                    prefix = text_acc.GetPrefix();
                    return true;
                }
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            if (GetScaffoldAccessionPrefix(*cur_entry, prefix)) {
                return true;
            }
        }
    }

    return false;
}

static bool IsScaffoldAccessionPrefixValid(const string& prefix)
{
    if (prefix.empty()) {

        ERR_POST_EX(0, 0, Critical << "Scaffold submission is missing accession prefix.");
        return false;
    }

    static const map<EScaffoldType, list<string>> POSSIBLE_PREFICIES = {
        { eRegularGenomic, { "CH", "DS", "EM", "EN", "EP", "EQ", "FA", "GG", "GL", "JH", "KB", "KD", "KE", "KI", "KK", "KL", "KN", "KQ", "KV", "KZ" } },
        { eRegularChromosomal, { "CM" } },
        { eGenColGenomic, { "CH", "DS", "EM", "EN", "EP", "EQ", "FA", "GG", "GL", "JH", "KB", "KD", "KE", "KI", "KK", "KL", "KN", "KQ", "KV", "KZ" } },
        { eTPAGenomic, { "GJ" } },
        { eTPAChromosomal, { "GK" } }
    };

    static const map<EScaffoldType, string> SCAFFOLD_TYPE_NAMES = {
        { eRegularGenomic, "regular" },
        { eRegularChromosomal, "chromosomal" },
        { eGenColGenomic, "regular GenCol" },
        { eTPAGenomic, "regular TPA" },
        { eTPAChromosomal, "chromosomal TPA" }
    };

    auto cur_preficies = POSSIBLE_PREFICIES.find(GetParams().GetScaffoldType());
    string scaffold_type_str("unknown");

    bool valid = false;
    if (cur_preficies != POSSIBLE_PREFICIES.end()) {

        scaffold_type_str = SCAFFOLD_TYPE_NAMES.at(GetParams().GetScaffoldType());
        valid = find(cur_preficies->second.begin(), cur_preficies->second.end(), prefix) != cur_preficies->second.end();
    }

    if (!valid) {
        ERR_POST_EX(0, 0, Critical << "Accession prefix \"" << prefix << "\" provided in input submissions do not match the type of scaffolds: \"" << scaffold_type_str << "\". Rejecting the whole set.");
    }

    return valid;
}

static void ConvertAccessionsToRanges(const list<string>& accs, list<TAccessionRange>& ranges)
{
    TAccessionRange range;
    CTextAccessionContainer last_acc;
    for (auto acc : accs) {
        if (!range.first.IsValid()) {
            range.first.Set(acc);
            last_acc.Set(acc);
        }
        else {
            CTextAccessionContainer cur_acc(acc);
            if (cur_acc.GetPrefix() != last_acc.GetPrefix() || last_acc.GetNumber() + 1 != cur_acc.GetNumber()) {
                range.second.swap(last_acc);
                ranges.push_back(range);

                range.first.Set(acc);
            }

            last_acc.swap(cur_acc);
        }
    }

    range.second.swap(last_acc);
    ranges.push_back(range);
}

static bool CheckContigsAccessionsRange(const list<TAccessionRange>& ranges, TSeqPos num_of_entries)
{
    const CTextAccessionContainer& first = ranges.front().first;
    const CTextAccessionContainer& last = ranges.back().second;

    bool is_reject = false,
         is_warning = false;

    if (first.GetAccession().size() <= GetParams().GetAccession().size() || last.GetAccession().size() <= GetParams().GetAccession().size()) {
        is_warning = is_reject = true;
    }
    else {
        if (GetParams().GetSource() == eDDBJ || GetParams().GetSource() == eEMBL) {
            if (last.GetNumber() - first.GetNumber() + 1 != num_of_entries) {
                is_warning = true;
                is_reject = !GetParams().IsDblinkOverride();
            }
        }
        else if (first.GetNumber() != 1 && last.GetNumber() != num_of_entries) {
            is_warning = true;
            is_reject = !GetParams().IsDblinkOverride();
        }
    }

    if (is_warning) {
        ERR_POST_EX(0, 0, (is_reject ? Critical : Warning) << "The range of accessions in input data is incorrect.");
    }

    return is_reject;
}

static bool CheckScaffoldsAccessionsRange(const list<TAccessionRange>& ranges)
{
    static const size_t MAX_ACCESSION_NUMBER = 999999;
    size_t starts_from_one = 0,
           ends_with_max = 0;

    for (auto range : ranges) {
        if (range.first.GetNumber() == 1) {
            ++starts_from_one;
        }
        else if (range.second.GetNumber() == MAX_ACCESSION_NUMBER) {
            ++ends_with_max;
        }
    }

    size_t bad_ranges = ranges.size() - min(starts_from_one, ends_with_max);

    static const size_t MAX_BAD_RANGES = 10;
    static const size_t MIN_BAD_RANGES = 2;

    bool is_reject = false;
    if (bad_ranges > MAX_BAD_RANGES) {
        is_reject = true;
        ERR_POST_EX(0, 0, Critical << "The number (" << bad_ranges << ") of non-contiguous scaffolds accession ranges exceeds the threshold of 10. Rejecting the whole set.");
    }
    else if (bad_ranges > MIN_BAD_RANGES) {
        ERR_POST_EX(0, 0, Warning << "A total of " << bad_ranges << " non-contiguous scaffolds accession ranges exist for this WGS project. Usually there are no more than two.");
    }

    return is_reject;
}

static void MergeRanges(list<TAccessionRange>& ranges)
{
    if (!ranges.empty()) {

        ranges.sort();

        list<TAccessionRange> res;
        CTextAccessionContainer start = ranges.front().first,
                                end = ranges.front().second;

        for (auto range : ranges) {
            if (range.first.GetPrefix() != start.GetPrefix() || range.first.GetNumber() + 1 > end.GetNumber()) {
                res.push_back({start, end});
                start = range.first;
                end = range.second;
            }
            else {
                end = range.second;
            }
        }
        res.push_back({ start, end });

        ranges.swap(res);
    }
}

static CRef<CSeqdesc> CreateScaffoldsUserObject(const TAccessionRange& range)
{
    CRef<CSeqdesc> descr(new CSeqdesc);

    descr->SetUser().SetType().SetStr("WGS-Scaffold-List");
    descr->SetUser().AddField("Accession_first", range.first.GetAccession());
    descr->SetUser().AddField("Accession_last", range.second.GetAccession());
    return descr;
}

static void CreateScaffoldsUserObjects(const list<TAccessionRange>& ranges, CSeq_descr::Tdata& descrs)
{
    // CR lambda assignment?
    typedef bool(*TFindPredicat)(const CRef<CSeqdesc>&);

    TFindPredicat predicat = nullptr;
    if (GetParams().IsWgs()) {
        predicat = [](const CRef<CSeqdesc>& descr) { return IsUserObjectOfType(*descr, "WGSProjects"); };
    }
    else if (GetParams().IsTls()) {
        predicat = [](const CRef<CSeqdesc>& descr) { return IsUserObjectOfType(*descr, "TLSProjects"); };
    }
    else if (GetParams().IsTsa()) {
        predicat = [](const CRef<CSeqdesc>& descr) { return IsUserObjectOfType(*descr, "TSA-RNA-List") || IsUserObjectOfType(*descr, "TSA-mRNA-List"); };
    }

    _ASSERT(predicat != nullptr && "predicat should have a valid function pointer");

    CSeq_descr::Tdata::iterator insertion_point = find_if(descrs.begin(), descrs.end(), predicat);

    for (auto range : ranges) {
        CRef<CSeqdesc> descr = CreateScaffoldsUserObject(range);
        insertion_point = descrs.insert(insertion_point, descr);
        ++insertion_point;
    }
}

static bool CheckScaffoldsOrganism(const CSeq_descr::Tdata& descrs, const CBioSource& biosource)
{
    if (!biosource.IsSetOrg() || !biosource.GetOrg().IsSetTaxname()) {
        return false;
    }

    const string& taxname = biosource.GetOrg().GetTaxname();

    auto biosrc_descr = find_if(descrs.begin(), descrs.end(),
                                [&taxname](const CRef<CSeqdesc>& descr)
                                {
                                    return descr->IsSource() && descr->GetSource().IsSetOrg() &&
                                        descr->GetSource().GetOrg().IsSetTaxname() && descr->GetSource().GetOrg().GetTaxname() == taxname;
                                });

    return biosrc_descr != descrs.end();
}

static CRef<CSeq_entry> AddScaffoldsToMaster(CMasterInfo& master_info, list<TAccessionRange>& ranges)
{
    CRef<CSeq_entry> ret;
    if (master_info.m_id_master_bioseq.Empty() || !master_info.m_id_master_bioseq->IsSeq() || ranges.empty()) {
        master_info.m_reject = true;
        return ret;
    }

    auto& descrs = master_info.m_id_master_bioseq->SetDescr().Set();
    for (auto descr = descrs.begin(); descr != descrs.end();) {

        bool to_remove = false;
        if (IsUserObjectOfType(**descr, "WGS-Scaffold-List")) {

            CConstRef<CUser_field> acc_first = (*descr)->GetUser().GetFieldRef("Accession_first");

            if (acc_first.NotEmpty()) {

                CConstRef<CUser_field> acc_last = (*descr)->GetUser().GetFieldRef("Accession_last");
                if (acc_last.Empty()) {
                    acc_last = acc_first;
                }

                if (acc_first->IsSetData() && acc_first->GetData().IsStr() && acc_last->IsSetData() && acc_last->GetData().IsStr()) {
                    
                    ranges.push_back({ acc_first->GetData().GetStr(), acc_last->GetData().GetStr() });
                    to_remove = true;
                }
            }
        }

        if (to_remove) {
            descr = descrs.erase(descr);
        }
        else {
            ++descr;
        }
    }

    MergeRanges(ranges);
    CreateScaffoldsUserObjects(ranges, descrs);

    if (!CheckScaffoldsOrganism(descrs, *master_info.m_biosource)) {
        ERR_POST_EX(0, 0, Error << "One or more scaffolds have altered organisms.");
    }

    if (master_info.m_dblink_state == eDblinkNoProblem && master_info.m_dblink.NotEmpty()) {
        UpdateDbLink(master_info.m_id_master_bioseq->SetSeq(), *master_info.m_dblink);
    }

    ret.Reset(new CSeq_entry);
    ret->Assign(*master_info.m_id_master_bioseq);

    return ret;
}

bool CreateMasterBioseqWithChecks(CMasterInfo& master_info)
{
    const list<string>& files = GetParams().GetInputFiles();

    bool ret = true,
         same_submit = true;

    CRef<CContact_info> master_contact_info;
    CRef<CCit_sub> master_cit_sub;
    CSeqEntryCommonInfo common_info;
    CMolInfo::TBiomol biomol = CMolInfo::eBiomol_unknown;

    master_info.m_same_biosource = true;
    list<CEntryOrderInfo> seq_order;

    size_t cur_file_num = 0;

    for (auto& file : files) {

        CNcbiIfstream in(file);

        if (!in) {
            ERR_POST_EX(0, 0, "Failed to open submission \"" << file << "\" for reading. Cannot proceed.");
            ret = false;
            break;
        }

        if (master_info.m_input_type == eUnknownType) {
            GetInputTypeFromFile(in, master_info.m_input_type);
        }

        bool first = true;
        while (in && !in.eof()) {

            CRef<CSeq_submit> seq_submit = GetSeqSubmit(in, master_info.m_input_type);
            if (seq_submit.Empty()) {

                if (first) {
                    ERR_POST_EX(0, 0, "Failed to read " << GetSeqSubmitTypeName(master_info.m_input_type) << " from file \"" << file << "\". Cannot proceed.");
                    ret = false;
                }
                break;
            }

            first = false;

            if (!FixSeqSubmit(seq_submit, master_info.m_accession_ver, true, master_info.m_reject)) {
                ERR_POST_EX(0, 0, "Wrapper GenBank set has non-empty annotation (Seq-annot), which is not allowed. Cannot process this submission \"" << file << "\".");
                ret = false;
                break;
            }

            if (GetParams().GetUpdateMode() == eUpdateAssembly && master_info.m_accession_ver > 0 && GetParams().IsAccessionAssigned()) {

                _ASSERT(master_info.m_current_master && "Should not be NULL in this mode");
                if (master_info.m_accession_ver == master_info.m_current_master->m_version) {
                    SetAssemblyVersion(master_info.m_accession_ver);
                }
                else {
                    ReportVersionProblem(master_info.m_accession_ver, master_info.m_current_master->m_version, master_info.m_reject);
                }
            }

            if (!IsSubmitBlockSet(*seq_submit)) {
                if (master_info.m_input_type == eSeqSubmit) {
                    ERR_POST_EX(0, 0, "Submission \"" << file << "\" is missing Submit-block.");
                }
                else if (same_submit) {
                    same_submit = CheckCitSubsInBioseqSet(master_info.m_cit_sub_info, *seq_submit);
                }
            }
            else if (master_info.m_input_type != eSeqSubmit || GetParams().GetSource() == eNCBI) {

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
            }

            if (!seq_submit->IsSetData()) {
                ERR_POST_EX(0, 0, "Failed to read Seq-entry from file \"" << file << "\". Cannot proceed.");
                break;
            }

            for (auto entry : seq_submit->GetData().GetEntrys()) {

                if (NeedToGetAccessionPrefix()) {

                    string prefix;
                    GetScaffoldAccessionPrefix(*entry, prefix);
                    if (GetParams().GetSource() == eNCBI && !IsScaffoldAccessionPrefixValid(prefix)) {
                        master_info.m_reject = true;
                    }

                    SetScaffoldPrefix(prefix);
                }

                if (GetParams().GetSource() == eNCBI) {
                    if (!master_info.m_update_date_present) {
                        master_info.m_update_date_present = IsDatePresent(*entry, CSeqdesc::e_Update_date);
                    }
                    if (!master_info.m_creation_date_present) {
                        master_info.m_creation_date_present = IsDatePresent(*entry, CSeqdesc::e_Create_date);
                    }

                    master_info.m_need_to_remove_dates = master_info.m_creation_date_present || master_info.m_update_date_present;
                }

                CSeqEntryInfo info(master_info.m_keywords_set, master_info.m_keywords);
                info.m_biomol = biomol;
                if (!CheckSeqEntry(*entry, file, info, common_info)) {
                    master_info.m_reject = true;
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

                if (info.m_biomol_state == eBiomolSet) {
                    biomol = info.m_biomol;
                }

                master_info.m_has_targeted_keyword = master_info.m_has_targeted_keyword || info.m_has_targeted_keyword;
                master_info.m_has_gmi_keyword = master_info.m_has_gmi_keyword || info.m_has_gmi_keyword;
                master_info.m_has_gb_block = master_info.m_has_gb_block || info.m_has_gb_block;
                master_info.m_num_of_prot_seq += info.m_num_of_prot_seq;

                if (!GetParams().IsUpdateScaffoldsMode()) {

                    if (!GetParams().IsKeepRefs()) {

                        master_info.m_num_of_pubs = max(CheckPubs(*entry, file, master_info.m_common_pubs, master_info.m_reject, master_info.m_pubs), master_info.m_num_of_pubs);
                        CheckComments(*entry, ProcessComment, master_info.m_common_comments_not_set, master_info.m_common_comments,
                                      [&file](const CSeq_entry& cur_entry) { CommentErrorReport(cur_entry, "Comment", file); });
                    }

                    CheckComments(*entry, ProcessStructuredComment, master_info.m_common_structured_comments_not_set, master_info.m_common_structured_comments,
                                  [&file](const CSeq_entry& cur_entry) { CommentErrorReport(cur_entry, "Structured comment", file); });
                }

                if (!CheckBioSource(*entry, master_info, file)) {
                    master_info.m_reject = true;
                }

                if (master_info.m_dblink_state != eDblinkAllProblems) {
                    CheckDblink(*entry, master_info, file);
                }


                if (!master_info.m_has_genome_project_id) {
                    master_info.m_has_genome_project_id = CheckGPID(*entry);
                }

                CollectOrgRefs(*entry, master_info.m_org_refs);

                if (GetParams().GetSource() != eNCBI) {
                    if (master_info.m_update_date_issues == eDateNoIssues) {
                        master_info.m_update_date_issues = CheckDates(*entry, CSeqdesc::e_Update_date, *master_info.m_update_date);
                        ReportDateProblem(master_info.m_update_date_issues, "Update", true);
                    }

                    if (master_info.m_creation_date_issues == eDateNoIssues) {
                        master_info.m_creation_date_issues = CheckDates(*entry, CSeqdesc::e_Create_date, *master_info.m_creation_date);
                        ReportDateProblem(master_info.m_creation_date_issues, "Create", GetParams().GetSource() != eDDBJ);
                    }
                }

                ++master_info.m_num_of_entries;

                if (GetParams().GetIdChoice() == CSeq_id::e_Other) {
                    master_info.m_whole_len += AccumulateLength(*entry);
                }

                AddOrderInfo(*entry, seq_order, cur_file_num);

                if (!master_info.m_reject && master_info.m_current_master && GetParams().IsUpdateScaffoldsMode()) {
                    CheckScaffoldsFarPointers(*entry, *master_info.m_current_master, master_info.m_reject);
                }

                master_info.m_object_ids.splice(master_info.m_object_ids.end(), info.m_object_ids);
            }

            if (!ret) {
                break;
            }
        }

        if (!ret) {
            break;
        }

        if (GetParams().IsAccessionsSortedInFile()) {
            ++cur_file_num;
        }
    }

    if (GetParams().IsTaxonomyLookup()) {
        LookupCommonOrgRefs(master_info.m_org_refs);
    }
    else {
        for (auto& org_ref_info : master_info.m_org_refs) {
            org_ref_info.m_org_ref_after_lookup = org_ref_info.m_org_ref;
        }
    }

    master_info.m_same_org = CheckSameOrgRefs(master_info.m_org_refs);

    if (same_submit && !master_info.m_cit_sub_info.m_dates.empty()) {

        if (master_info.m_cit_sub_info.m_dates.size() > 1) {
            ERR_POST_EX(0, 0, Warning << "Encountered Cit-subs with date differences only. Using the most frequent/earliest one.");
        }
        
        CRef<CDate> the_date = GetMostRelevantDate(master_info.m_cit_sub_info.m_dates);
        ReplaceDatesInCitSub(master_info.m_common_pubs, master_info.m_pubs, *the_date);
    }

    master_info.m_reject = master_info.m_reject || DBLinkProblemReport(master_info);

    common_info.m_acc_assigned.sort();
    if (GetParams().IsAccessionAssigned()) {
        if (!CheckUniqueAccs(common_info.m_acc_assigned)) {
            master_info.m_reject = true;
        }
        else {
            if (GetParams().GetUpdateMode() == eUpdateFull || GetParams().GetUpdateMode() == eUpdateNew || GetParams().GetUpdateMode() == eUpdateAssembly || GetParams().GetUpdateMode() == eUpdateExtraContigs) {
                if (!CheckAccLengths(common_info.m_acc_assigned)) {
                    master_info.m_reject = true;
                }
            }
        }
    }

    if (IsDupIds(master_info.m_object_ids)) {
        master_info.m_reject = true;
    }

    SortSequences(seq_order);

    int last_contig = 0;
    if (GetParams().GetUpdateMode() == eUpdateExtraContigs && master_info.m_current_master) {
        last_contig = master_info.m_current_master->m_last_contig;
    }

    BuildSortOrderMap(seq_order, master_info.m_order_of_entries, last_contig);

    static const string SCAFFOLD_PREFIX = "CM";
    if (GetParams().GetUpdateMode() == eUpdateScaffoldsNew && GetParams().GetScaffoldPrefix() == SCAFFOLD_PREFIX && !GetParams().IsAccessionAssigned()) {
        ERR_POST_EX(0, 0, Critical << "Command line switches \"-u 3\" and \"-j 1\" require \"-c T\": htgs database no longet assigning chromosomal accessions. Cannot proceed.");
        master_info.m_reject = true;
    }

    // TODO Some HTGS code

    if (!common_info.m_acc_assigned.empty()) {
        ConvertAccessionsToRanges(common_info.m_acc_assigned, common_info.m_acc_ranges);

        if (!master_info.m_reject) {
            if (GetParams().GetUpdateMode() == eUpdateNew || GetParams().GetUpdateMode() == eUpdateAssembly) {
                master_info.m_reject = CheckContigsAccessionsRange(common_info.m_acc_ranges, master_info.m_num_of_entries);
            }
            else if (GetParams().GetUpdateMode() == eUpdateScaffoldsNew) {
                master_info.m_reject = CheckScaffoldsAccessionsRange(common_info.m_acc_ranges);
            }
        }
    }

    if (master_info.m_has_genome_project_id) {

        bool is_reject = !GetParams().IsDblinkOverride();
        ERR_POST_EX(0, 0, (is_reject ? Critical : Warning) <<
                    "One or more of the contigs or scaffolds being processed makes use of the legacy GenomeProjectsDB User-object. A DBLink User-object with a BioProject Accession Number should be used instead. " <<
                    (is_reject ? "Rejecting the whole project." : "They all will be removed."));

        if (is_reject) {
            master_info.m_reject = is_reject;
        }
    }

    CheckMasterDblink(master_info);
    if (master_info.m_reject) {
        return false;
    }

    if (master_cit_sub.Empty() && master_info.m_input_type == eSeqSubmit) {
        same_submit = false;
    }

    if (same_submit) {
        master_info.m_got_cit_sub = true;
    }
    else {
        master_contact_info.Reset();
        master_cit_sub.Reset();
    }

    if (GetParams().IsUpdateScaffoldsMode()) {

        master_info.m_master_bioseq = AddScaffoldsToMaster(master_info, common_info.m_acc_ranges);
    }
    else {
        master_info.m_master_bioseq = CreateMasterBioseq(master_info, master_cit_sub, master_contact_info, biomol);

        if (GetParams().IsStripAuthors() && GetParams().GetUpdateMode() != eUpdateScaffoldsNew) {
            StripAuthors(*master_info.m_master_bioseq);
        }
    }

    return ret;
}

}
