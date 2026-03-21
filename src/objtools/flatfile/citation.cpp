/* $Id$
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
 * File Name: citation.cpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *      Functionality was moved from C-toolkit (utilpub.c file).
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objmgr/scope.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <serial/serial.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seq/Seq_descr.hpp>


#include <objtools/flatfile/flatdefn.h>
#include <objtools/flatfile/flatfile_parser.hpp>

#include "utilfun.h"
#include "ftaerr.hpp"
#include "loadfeat.h"
#include "citation.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "citation.cpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/////////////////////
// class CPubInfo
CPubInfo::CPubInfo() :
    cit_num_(-1),
    bioseq_(nullptr),
    pub_equiv_(nullptr),
    pub_(nullptr)
{
}

const CPub_equiv* CPubInfo::GetPubEquiv() const
{
    if (pub_equiv_)
        return pub_equiv_;

    if (pub_ && pub_->IsEquiv())
        return &(pub_->GetEquiv());

    return nullptr;
};

void CPubInfo::SetBioseq(const CBioseq* bioseq)
{
    bioseq_ = bioseq;
}

void CPubInfo::SetPubEquiv(const CPub_equiv* pub_equiv)
{
    pub_       = nullptr;
    pub_equiv_ = pub_equiv;

    cit_num_ = -1;
    if (pub_equiv_) {
        for (const auto& pub : pub_equiv_->Get()) {
            if (pub->IsGen() && pub->GetGen().IsSetSerial_number()) {
                cit_num_ = pub->GetGen().GetSerial_number();
                break;
            }
        }
    }
}

void CPubInfo::SetPub(const CPub* pub)
{
    pub_equiv_ = nullptr;
    pub_       = pub;

    cit_num_ = -1;
    if (pub_) {
        if (pub_->IsGen())
            cit_num_ = pub_->GetGen().GetSerial_number();
        else if (pub_->IsEquiv()) {
            for (const auto& cur_pub : pub_->GetEquiv().Get()) {
                if (cur_pub->IsGen() && cur_pub->GetGen().IsSetSerial_number()) {
                    cit_num_ = cur_pub->GetGen().GetSerial_number();
                    break;
                }
            }
        }
    }
}

static void FindCitInDescr(std::vector<CPubInfo>& pubs, const TSeqdescList& descrs, const CBioseq* bioseq)
{
    for (const auto& descr : descrs) {
        if (descr->IsPub()) {
            CPubInfo pub_info;
            pub_info.SetBioseq(bioseq);
            pub_info.SetPubEquiv(&descr->GetPub().GetPub());

            pubs.push_back(pub_info);
        }
    }
}

static void FindCitInFeats(std::vector<CPubInfo>& pubs, const CBioseq::TAnnot& annots)
{
    for (const auto& annot : annots) {
        if (! annot->IsSetData() || ! annot->GetData().IsFtable()) /* feature table */
            continue;


        for (const auto& feat : annot->GetData().GetFtable()) {
            if (feat->IsSetData()) {
                const CSeq_id* id = nullptr;
                if (feat->IsSetLocation())
                    id = feat->GetLocation().GetId();

                CPubInfo pub_info;
                if (id) {
                    CBioseq_Handle bioseq_handle = GetScope().GetBioseqHandle(*id);
                    if (bioseq_handle)
                        pub_info.SetBioseq(GetScope().GetBioseqHandle(*id).GetBioseqCore());
                    else
                        continue;
                }

                if (feat->GetData().IsPub()) {
                    pub_info.SetPubEquiv(&feat->GetData().GetPub().GetPub());
                    pubs.push_back(pub_info);
                } else if (feat->GetData().IsImp() && feat->IsSetCit()) {
                    const CPub_set& pub_set = feat->GetCit();

                    for (const auto& pub : pub_set.GetPub()) {
                        pub_info.SetPub(pub);
                        pubs.push_back(pub_info);
                    }
                }
            }
        }
    }
}

static int GetCitSerialFromQual(const CGb_qual& qual)
{
    const Char* p = qual.GetVal().c_str();
    while (*p && ! isdigit(*p))
        ++p;

    if (*p)
        return fta_atoi(p);

    return -1;
}

void SetMinimumPub(const CPubInfo& pub_info, TPubList& pubs)
{
    const CPub_equiv* pub_equiv = pub_info.GetPubEquiv();
    const CPub*       pub       = nullptr;

    CRef<CPub> new_pub;
    if (pub_equiv) {
        for (const auto& cur_pub : pub_equiv->Get()) {
            if (cur_pub->IsMuid() || cur_pub->IsPmid()) {
                if (new_pub.Empty()) {
                    new_pub.Reset(new CPub);
                    new_pub->Assign(*cur_pub);
                } else {
                    CRef<CPub_equiv> new_pub_equiv(new CPub_equiv);
                    new_pub_equiv->Set().push_back(new_pub);

                    new_pub.Reset(new CPub);
                    new_pub->Assign(*cur_pub);
                    new_pub_equiv->Set().push_back(new_pub);

                    new_pub.Reset(new CPub);
                    new_pub->SetEquiv(*new_pub_equiv);

                    pubs.push_back(new_pub);
                    return;
                }
            }
        }

        const TPubList& equiv_pubs = pub_equiv->Get();
        if (! equiv_pubs.empty())
            pub = *equiv_pubs.begin();
    } else
        pub = pub_info.GetPub();

    if (new_pub.NotEmpty()) {
        pubs.push_back(new_pub);
        return;
    }

    if (pub && pub->IsGen()) {
        if (pub->GetGen().IsSetSerial_number() && ! pub_info.GetPub()) // pub points to the first pub in pub_equiv
        {
            const TPubList& equiv_pubs = pub_equiv->Get();
            if (equiv_pubs.size() > 1) {
                TPubList::const_iterator cur_pub = equiv_pubs.begin();
                ++cur_pub;
                pub = *(cur_pub);
            }
        }
    }

    if (pub && (pub->IsMuid() || pub->IsPmid())) {
        new_pub.Reset(new CPub);
        new_pub->Assign(*pub);
        pubs.push_back(new_pub);
        return;
    }

    string label;
    if (pub && ! pub->GetLabel(&label, CPub::fLabel_Unique)) {
        new_pub.Reset(new CPub);
        new_pub->Assign(*pub);
        pubs.push_back(new_pub);
        return;
    }

    new_pub.Reset(new CPub);
    new_pub->SetGen().SetCit(label);
    pubs.push_back(new_pub);
}

static void ProcessCit(const std::vector<CPubInfo>& pubs, CBioseq::TAnnot& annots, const CBioseq* bioseq)
{
    for (auto& annot : annots) {
        if (! annot->IsSetData() || ! annot->GetData().IsFtable())
            continue;

        for (auto& feat : annot->SetData().SetFtable()) {
            if (feat->IsSetQual()) {
                TQualVector& quals = feat->SetQual();

                TPubList cit_pubs;
                for (TQualVector::iterator qual = quals.begin(); qual != quals.end();) {
                    if ((*qual)->IsSetQual() && (*qual)->GetQual() == "citation") {
                        int ser_num = GetCitSerialFromQual(*(*qual));
                        qual        = quals.erase(qual);

                        bool found = false;
                        for (const CPubInfo& pub : pubs) {
                            if (pub.GetSerial() == ser_num) {
                                if (bioseq && pub.GetBioseq() && bioseq != pub.GetBioseq())
                                    continue;

                                SetMinimumPub(pub, cit_pubs);

                                found = true;
                                break;
                            }
                        }

                        if (! found) {
                            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_NoRefForCiteQual, "No Reference found for Citation qualifier [{}]", ser_num);
                        }
                    } else
                        ++qual;
                }

                if (! cit_pubs.empty())
                    feat->SetCit().SetPub().swap(cit_pubs);
            }
        }
    }
}

void ProcessCitations(TEntryList& seq_entries)
{
    std::vector<CPubInfo> pubs;

    for (const auto& entry : seq_entries) {
        for (CTypeConstIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            if (bio_set->IsSetDescr())
                FindCitInDescr(pubs, bio_set->GetDescr(), nullptr);

            if (bio_set->IsSetAnnot())
                FindCitInFeats(pubs, bio_set->GetAnnot());
        }

        for (CTypeConstIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            if (bioseq->IsSetDescr())
                FindCitInDescr(pubs, bioseq->GetDescr(), &(*bioseq));

            if (bioseq->IsSetAnnot())
                FindCitInFeats(pubs, bioseq->GetAnnot());
        }
    }

    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            if (bio_set->IsSetAnnot())
                ProcessCit(pubs, bio_set->SetAnnot(), nullptr);
        }

        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            if (bioseq->IsSetAnnot())
                ProcessCit(pubs, bioseq->SetAnnot(), &(*bioseq));
        }
    }
}

END_NCBI_SCOPE
