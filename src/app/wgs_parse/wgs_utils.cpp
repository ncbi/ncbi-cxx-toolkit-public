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

}