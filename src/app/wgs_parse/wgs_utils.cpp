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
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>


#include "wgs_utils.hpp"

namespace wgsparse
{

CScope& GetScope()
{
    static CScope scope(*CObjectManager::GetInstance());

    return scope;
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


bool IsPubdescContainsSub(const CPubdesc& pub)
{
    if (pub.IsSetPub() && pub.GetPub().IsSet()) {

        const CPub_equiv::Tdata& pubs = pub.GetPub().Get();
        return find_if(pubs.begin(), pubs.end(), [](const CRef<CPub>& pub) { return pub->IsSub(); } ) != pubs.end();
    }

    return false;
}

string GetSeqIdStr(const CBioseq& bioseq)
{
    string id;
    if (bioseq.IsSetId() && !bioseq.GetId().empty()) {
        id = bioseq.GetId().front()->AsFastaString();
    }

    return id;
}

bool GetDescr(const CSeq_entry& entry, const CSeq_descr* &descrs)
{
    bool ret = true;
    if (entry.IsSeq()) {
        if (entry.GetSeq().IsSetDescr()) {
            descrs = &entry.GetSeq().GetDescr();
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetDescr()) {
            descrs = &entry.GetSet().GetDescr();
        }
    }
    else {
        ret = false;
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
        if (entry.GetSeq().IsSetDescr()) {
            annot = &entry.GetSeq().GetAnnot();
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetDescr()) {
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
        if (entry.GetSeq().IsSetDescr()) {
            annot = &entry.SetSeq().SetAnnot();
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetDescr()) {
            annot = &entry.SetSet().SetAnnot();
        }
    }
    else {
        ret = false;
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

    auto& set_fun = SET_TEXT_ID.find(choice);
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

}