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

#include <corelib/ncbistr.hpp>
#include <objmgr/scope.hpp>

#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Affil.hpp>

#include "wgs_utils.hpp"

namespace wgsparse
{

CScope& GetScope()
{
    static CScope scope(*CObjectManager::GetInstance());

    return scope;
}

static const int MAX_SEVEN_DIGITS_NUM = 9999999;
static const int MAX_SIX_DIGITS_NUM = 999999;

size_t GetMaxAccessionLen(int accession_num)
{
    size_t max_accession_len = 6;
    if (accession_num > MAX_SEVEN_DIGITS_NUM) {
        max_accession_len = 8;
    }
    else if (accession_num > MAX_SIX_DIGITS_NUM) {
        max_accession_len = 7;
    }

    return max_accession_len;
}

bool GetInputType(const std::string& str, EInputType& type)
{
    if (NStr::EqualNocase(str, "Seq-submit")) {
        type = eSeqSubmit;
    }
    else if (NStr::EqualNocase(str, "Bioseq-set")) {
        type = eBioseqSet;
    }
    else if (NStr::EqualNocase(str, "Seq-entry")) {
        type = eSeqEntry;
    }
    else {
        return false;
    }

    return true;
}

bool GetInputTypeFromFile(CNcbiIfstream& stream, EInputType& type)
{
    type = eSeqSubmit;

    string input_type;
    stream >> input_type;
    stream.seekg(0, ios_base::beg);

    return GetInputType(input_type, type);
}


bool GetDescr(const CSeq_entry& entry, const CSeq_descr* &descrs)
{
    bool ret = true;
    if (!entry.IsSeq() && !entry.IsSet()) {
        ret = false;
    }
    else {
        if (entry.IsSetDescr()) {
            descrs = &entry.GetDescr();
        }
    }

    return ret;
}

bool GetNonConstDescr(CSeq_entry& entry, CSeq_descr* &descrs)
{
    bool ret = true;
    if (entry.IsSeq()) {
        if (entry.GetSeq().IsSetDescr()) {
            descrs = &entry.SetSeq().SetDescr();
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetDescr()) {
            descrs = &entry.SetSet().SetDescr();
        }
    }
    else {
        ret = false;
    }

    return ret;
}

bool GetAnnot(const CSeq_entry& entry, const CBioseq::TAnnot* &annot)
{
    bool ret = true;
    if (entry.IsSeq()) {
        if (entry.GetSeq().IsSetAnnot()) {
            annot = &entry.GetSeq().GetAnnot();
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetAnnot()) {
            annot = &entry.GetSet().GetAnnot();
        }
    }
    else {
        ret = false;
    }

    return ret;
}

bool GetNonConstAnnot(CSeq_entry& entry, CBioseq::TAnnot* &annot)
{
    bool ret = true;
    if (entry.IsSeq()) {
        if (entry.GetSeq().IsSetAnnot()) {
            annot = &entry.SetSeq().SetAnnot();
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetAnnot()) {
            annot = &entry.SetSet().SetAnnot();
        }
    }
    else {
        ret = false;
    }

    return ret;
}

CSeqdesc* GetSeqdescr(CSeq_entry& entry, CSeqdesc::E_Choice type)
{
    CSeq_descr* descrs = nullptr;

    CSeqdesc* ret = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {
        for (auto descr : descrs->Set()) {
            if (descr->Which() == type) {
                ret = descr;
                break;
            }
        }
    }

    return ret;
}

TSetSeqIdFunc FindSetTextSeqIdFunc(CSeq_id::E_Choice choice)
{
    // The map below links an id type with a text id setter
    static const map<CSeq_id::E_Choice, TSetSeqIdFunc> SET_TEXT_ID = {
        { CSeq_id::e_Ddbj, &CSeq_id::SetDdbj },
        { CSeq_id::e_Embl, &CSeq_id::SetEmbl },
        { CSeq_id::e_Genbank, &CSeq_id::SetGenbank },
        { CSeq_id::e_Tpd, &CSeq_id::SetTpd },
        { CSeq_id::e_Tpg, &CSeq_id::SetTpg },
        { CSeq_id::e_Other, &CSeq_id::SetOther }
    };

    auto set_fun = SET_TEXT_ID.find(choice);
    if (set_fun == SET_TEXT_ID.end()) {
        return nullptr;
    }

    return set_fun->second;

}

bool HasLineage(const string& lineage_str, const string& lineage)
{
    return NStr::FindCase(lineage_str, lineage) != NPOS;
}


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

CRef<CSeq_submit> GetSeqSubmit(CNcbiIfstream& in, EInputType type)
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

string GetSeqSubmitTypeName(EInputType type)
{
    static const map<EInputType, string> SEQSUBMIT_TYPE_STR = {
        { eSeqSubmit, "Seq-submit" },
        { eSeqEntry, "Seq-entry" },
        { eBioseqSet, "Bioseq-set" }
    };

    return SEQSUBMIT_TYPE_STR.at(type);
}

string GetIdStr(const CObject_id& obj_id)
{
    string ret;
    if (obj_id.IsStr()) {
        ret = obj_id.GetStr();
    }
    else if (obj_id.IsId()) {
        ret = to_string(obj_id.GetId8());
    }

    return ret;
}

bool HasTextAccession(const CSeq_id& id)
{
    if (!id.IsGenbank() && !id.IsEmbl() && !id.IsDdbj() && !id.IsOther() && !id.IsTpd() && !id.IsTpe() && !id.IsTpg()) {
        return false;
    }
    return id.GetTextseq_Id() != nullptr && id.GetTextseq_Id()->IsSetAccession();
}

string GetSeqIdAccession(const CBioseq& bioseq)
{
    string ret;
    if (bioseq.IsSetId()) {

        auto cur_id = find_if(bioseq.GetId().begin(), bioseq.GetId().end(), [](const CRef<CSeq_id>& id) { return HasTextAccession(*id); });
        if (cur_id != bioseq.GetId().end()) {
            ret = (*cur_id)->GetTextseq_Id()->GetAccession();
        }
    }

    return ret;
}

string GetLocalOrGeneralIdStr(const CSeq_id& id)
{
    return id.IsLocal() ? GetIdStr(id.GetLocal()) : GetIdStr(id.GetGeneral().GetTag());
}

bool IsLocalOrGeneralId(const CSeq_id& id)
{
    return (id.IsGeneral() && id.GetGeneral().IsSetTag()) || id.IsLocal();
}

string GetSeqIdLocalOrGeneral(const CBioseq& bioseq)
{
    string ret;
    if (bioseq.IsSetId()) {

        auto cur_id = find_if(bioseq.GetId().begin(), bioseq.GetId().end(), [](const CRef<CSeq_id>& id) { return IsLocalOrGeneralId(*id); });
        if (cur_id != bioseq.GetId().end()) {
            ret = GetLocalOrGeneralIdStr(**cur_id);
        }
    }

    return ret;
}

string GetSeqIdKey(const CBioseq& bioseq)
{
    string id_str;
    if (bioseq.IsSetId() && !bioseq.GetId().empty()) {

        auto& first_id = bioseq.GetId().front();
        if (first_id->GetTextseq_Id() != nullptr) {
            if (first_id->GetTextseq_Id()->IsSetAccession()) {
                id_str = first_id->GetTextseq_Id()->GetAccession();
            }
        }
        else {
            id_str = GetSeqIdLocalOrGeneral(bioseq);
        }
    }

    return id_str;
}

string GetSeqIdStr(const CBioseq& bioseq)
{
    string id;
    if (bioseq.IsSetId() && !bioseq.GetId().empty()) {
        id = bioseq.GetId().front()->AsFastaString();
    }

    return id;
}

bool IsUserObjectOfType(const CSeqdesc& descr, const string& type)
{
    return descr.IsUser() && descr.GetUser().IsSetType() && descr.GetUser().GetType().IsStr() &&
        descr.GetUser().GetType().GetStr() == type;
}

string ToString(const CSerialObject& obj)
{
    CNcbiOstrstream stream;
    stream << MSerial_AsnText << obj << ends;

    return stream.str();
}

string ToStringKey(const CSerialObject& obj)
{
    string ret = ToString(obj);

    ret.erase(remove_if(ret.begin(), ret.end(), [](char& c) { return c == ' '; }), ret.end());
    ret.erase(remove_if(ret.begin(), ret.end(), [](char& c) { return c == '\n'; }), ret.end());

    return ret;
}

string::size_type GetLastSlashPos(const string& str)
{
    string::size_type ret = str.rfind('/');;
    if (ret == string::npos) {
        ret = str.rfind('\\');
    }

    return ret;
}

bool NeedToProcessId(const CSeq_id& id)
{
    return id.IsGenbank() || id.IsDdbj() || id.IsEmbl() || id.IsOther() || id.IsTpd() || id.IsTpe() || id.IsTpg();
}

const CCit_sub* GetCitSub(const CPubdesc& pub)
{
    const CCit_sub* ret = nullptr;
    if (pub.IsSetPub() && pub.GetPub().IsSet()) {

        auto cit_sub = find_if(pub.GetPub().Get().begin(), pub.GetPub().Get().end(), [](const CRef<CPub>& cur_pub) { return cur_pub->IsSub(); });
        if (cit_sub != pub.GetPub().Get().end()) {
            ret = &(*cit_sub)->GetSub();
        }
    }
    return ret;
}

CCit_sub* GetNonConstCitSub(CPubdesc& pub)
{
    CCit_sub* ret = nullptr;
    if (pub.IsSetPub() && pub.GetPub().IsSet() && !pub.GetPub().Get().empty()) {

        auto cit_sub = find_if(pub.SetPub().Set().begin(), pub.SetPub().Set().end(), [](const CRef<CPub>& cur_pub) { return cur_pub->IsSub(); });
        if (cit_sub != pub.SetPub().Set().end()) {
            ret = &(*cit_sub)->SetSub();
        }
    }
    return ret;
}

bool HasPubOfChoice(const CPubdesc& pub, CPub::E_Choice choice)
{
    if (pub.IsSetPub() && pub.GetPub().IsSet()) {

        return find_if(pub.GetPub().Get().begin(), pub.GetPub().Get().end(), [&choice](const CRef<CPub>& cur_pub) { return cur_pub->Which() == choice; }) != pub.GetPub().Get().end();
    }

    return false;
}

static void AddContactInfo(const CContact_info& contact, CCit_sub& cit_sub)
{
    if (cit_sub.IsSetAuthors() && cit_sub.GetAuthors().IsSetAffil()) {
        return;
    }

    if (contact.IsSetContact() && contact.GetContact().IsSetAffil()) {
        cit_sub.SetAuthors().SetAffil().Assign(contact.GetContact().GetAffil());
        if (cit_sub.GetAuthors().GetAffil().IsStd()) {
            cit_sub.SetAuthors().SetAffil().SetStd().ResetEmail();
            cit_sub.SetAuthors().SetAffil().SetStd().ResetFax();
            cit_sub.SetAuthors().SetAffil().SetStd().ResetPhone();
        }
    }
    else if (contact.IsSetAddress()) {

        string& std_affil = cit_sub.SetAuthors().SetAffil().SetStd().SetAffil();
        std_affil = NStr::Join(contact.GetAddress(), ",");
    }
}

CRef<CSeqdesc> CreateCitSub(const CCit_sub& cit_sub, const CContact_info* contact)
{
    CRef<CPub> pub(new CPub);
    pub->SetSub().Assign(cit_sub);

    CRef<CSeqdesc> descr(new CSeqdesc);

    CPubdesc& pubdescr = descr->SetPub();
    pubdescr.SetPub().Set().push_back(pub);

    if (contact) {
        AddContactInfo(*contact, pub->SetSub());
    }

    return descr;
}

CRef<CSeqdesc> BuildStructuredComment(const string& comment)
{
    CRef<CSeqdesc> descr(new CSeqdesc);

    CNcbiIstrstream stream(comment.c_str());
    CRef<CUser_object> user_obj(new CUser_object);
    stream >> MSerial_AsnText >> *user_obj;
    descr->SetUser(*user_obj);

    return descr;
}

bool IsDigits(string::const_iterator start, string::const_iterator end)
{
    for (; start != end; ++start) {
        if (!isdigit(*start))
            return false;
    }

    return true;
}

}
