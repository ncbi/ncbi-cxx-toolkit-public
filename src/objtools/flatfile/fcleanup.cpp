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
 * File Name: fcleanup.cpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * This file contains some specific cleanup functionality
 */

#include <ncbi_pch.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <serial/iterator.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objtools/edit/cds_fix.hpp>

#include <objtools/flatfile/flatfile_parse_info.hpp>

#include <objtools/flatfile/flatfile_parser.hpp>

#include "utilfun.h"
#include "loadfeat.h"
#include "fcleanup.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "fcleanup.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static void MoveSourceDescrToTop(CSeq_entry& entry)
{
    if (entry.IsSeq())
        return;

    CBioseq_set&    seq_set = entry.SetSet();
    const CSeqdesc* source  = nullptr;
    if (seq_set.IsSetDescr()) {
        for (const auto& desc : seq_set.GetDescr().Get()) {
            if (desc->IsSource()) {
                source = desc;
                break;
            }
        }
    }

    for (CTypeIterator<CBioseq> bioseq(Begin(entry)); bioseq; ++bioseq) {
        if (bioseq->IsSetDescr()) {
            TSeqdescList& descrs = bioseq->SetDescr().Set();
            for (TSeqdescList::iterator desc = descrs.begin(); desc != descrs.end(); ++desc) {
                if ((*desc)->IsSource()) {
                    if (! source) {
                        source = *desc;
                        seq_set.SetDescr().Set().push_back(*desc);
                        descrs.erase(desc);
                    }
                    break;
                }
            }
        }
    }
}

NCBI_UNUSED
static bool IsEmptyMolInfo(const CMolInfo& mol_info)
{
    return ! (mol_info.IsSetBiomol() || mol_info.IsSetCompleteness() || mol_info.IsSetGbmoltype() ||
              mol_info.IsSetTech() || mol_info.IsSetTechexp());
}


static void LookForProductName(CSeq_feat& feat)
{
    if (feat.IsSetData() && feat.GetData().IsProt()) {
        CProt_ref& prot_ref = feat.SetData().SetProt();

        if (! prot_ref.IsSetName() || prot_ref.GetName().empty() || (prot_ref.GetName().size() == 1 && prot_ref.GetName().front() == "unnamed")) {
            if (feat.IsSetQual()) {
                for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end(); ++qual) {
                    if ((*qual)->IsSetQual() && (*qual)->GetQual() == "product" && (*qual)->IsSetVal()) {
                        prot_ref.SetName().clear();
                        prot_ref.SetName().push_back((*qual)->GetVal());
                        feat.SetQual().erase(qual);

                        if (feat.GetQual().empty())
                            feat.ResetQual();
                        break;
                    }
                }
            }
        }
    }
}

/*
bool FeatLocCmp(const CRef<CSeq_feat>& a, const CRef<CSeq_feat>& b)
{
    if (! a->IsSetLocation() || ! b->IsSetLocation())
        return false;

    return a->GetLocation().GetStart(eExtreme_Biological) < b->GetLocation().GetStart(eExtreme_Biological);
}

static void SortFeaturesByLocation(CSeq_annot& annot)
{
    if (! annot.IsFtable())
        return;

    TSeqFeatList& feats = annot.SetData().SetFtable();
    feats.sort(FeatLocCmp);
}
*/

static bool IsConversionPossible(const vector<pair<TSeqPos, TSeqPos>>& ranges)
{
    bool convert = true;

    size_t sz = ranges.size();
    for (size_t i = 1; i < sz; ++i) {
        if (ranges[i].first != ranges[i - 1].second && ranges[i].first != ranges[i - 1].second + 1) {
            convert = false;
            break;
        }
    }

    return convert;
}

static void SetNewInterval(const CSeq_interval* first, const CSeq_interval* last, const TSeqPos& from, TSeqPos& to, CSeq_loc& loc)
{
    CRef<CSeq_interval> new_int(new CSeq_interval);

    if (first) {
        new_int->Assign(*first);
    }

    new_int->SetFrom(from);
    new_int->SetTo(to);

    if (last) {
        if (last->IsSetFuzz_from()) {
            new_int->SetFuzz_from().Assign(last->GetFuzz_from());
        }
        if (last->IsSetFuzz_to()) {
            new_int->SetFuzz_to().Assign(last->GetFuzz_to());
        }
    }

    loc.SetInt(*new_int);
}

static void ConvertMixToInterval(CSeq_loc& loc)
{
    const CSeq_loc_mix::Tdata& locs           = loc.GetMix().Get();
    const CSeq_interval*       first_interval = nullptr;
    const CSeq_interval*       last_interval  = nullptr;

    vector<pair<TSeqPos, TSeqPos>> ranges;
    for (const auto& cur_loc : locs) {
        if (cur_loc->IsInt()) {

            const CSeq_interval& interval = cur_loc->GetInt();
            ranges.push_back(make_pair(interval.GetFrom(), interval.GetTo()));

            if (! first_interval) {
                first_interval = &interval;
            } else if (first_interval->GetStart(eExtreme_Biological) > interval.GetStart(eExtreme_Biological)) {
                first_interval = &interval;
            }

            if (! last_interval) {
                last_interval = &interval;
            } else if (last_interval->GetStop(eExtreme_Biological) < interval.GetStop(eExtreme_Biological)) {
                last_interval = &interval;
            }
        } else if (cur_loc->IsPnt()) {
            const CSeq_point& point = cur_loc->GetPnt();
            ranges.push_back(make_pair(point.GetPoint(), point.GetPoint()));
        } else {
            // TODO process the other types of intervals
            return;
        }
    }

    if (! first_interval) {
        return;
    }

    sort(ranges.begin(), ranges.end());

    if (IsConversionPossible(ranges)) {
        SetNewInterval(first_interval, last_interval, ranges.front().first, ranges.back().second, loc);
    }
}

static void ConvertPackedIntToInterval(CSeq_loc& loc)
{
    const CPacked_seqint::Tdata& ints = loc.GetPacked_int();

    vector<pair<TSeqPos, TSeqPos>> ranges;
    for (const auto& interval : ints) {
        ranges.push_back(make_pair(interval->GetFrom(), interval->GetTo()));
    }

    sort(ranges.begin(), ranges.end());

    if (IsConversionPossible(ranges)) {
        SetNewInterval(loc.GetPacked_int().GetStartInt(eExtreme_Biological), loc.GetPacked_int().GetStopInt(eExtreme_Biological), ranges.front().first, ranges.back().second, loc);
    }
}


void g_InstantiateMissingProteins(CSeq_entry_Handle entryHandle)
{
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    auto&          scope = entryHandle.GetScope();

    int protein_id_counter = 99;

    for (CFeat_CI cds_it(entryHandle, sel); cds_it; ++cds_it) {
        auto pCds = cds_it->GetSeq_feat();
        if (! sequence::IsPseudo(*pCds, scope)) {
            if (! pCds->IsSetProduct()) {
                auto pNewCds = Ref(new CSeq_feat());
                pNewCds->Assign(*pCds);
                string idLabel;
                auto   pProteinId =
                    edit::GetNewProtId(scope.GetBioseqHandle(pNewCds->GetLocation()), protein_id_counter, idLabel, false);
                pNewCds->SetProduct().SetWhole().Assign(*pProteinId);
                CSeq_feat_EditHandle pCdsEditHandle(*cds_it);
                pCdsEditHandle.Replace(*pNewCds);
                pCds = pNewCds;
            } else if (pCds->GetProduct().GetId() &&
                       scope.Exists(*pCds->GetProduct().GetId())) {
                continue;
            }
            // Add protein
            CCleanup::AddProtein(*pCds, scope);
            auto protHandle = scope.GetBioseqHandle(pCds->GetProduct());
            if (protHandle) {
                CFeat_CI feat_ci(protHandle, CSeqFeatData::eSubtype_prot);
                if (! feat_ci) {
                    string protName = CCleanup::GetProteinName(*pCds, entryHandle);
                    if (NStr::IsBlank(protName)) {
                        protName = "hypothetical protein";
                    }
                    feature::AddProteinFeature(*(protHandle.GetCompleteBioseq()), protName, *pCds, scope);
                }
            }
        }
    }
}


void FinalCleanup(TEntryList& seq_entries)
{
    CCleanup cleanup;
    cleanup.SetScope(&GetScope());

    //+++++
    /*{
        CBioseq_set bio_set;
        bio_set.SetClass(CBioseq_set::eClass_genbank);

        bio_set.SetSeq_set() = seq_entries;
        CNcbiOfstream ostr("before_cleanup.asn");
        ostr << MSerial_AsnText << bio_set;
    }*/
    //+++++

    for (auto& entry : seq_entries) {
        //+++++
        /*{
            CNcbiOfstream ostr("ttt.txt");
            ostr << MSerial_AsnText << *entry;
        }*/
        //+++++

        MoveSourceDescrToTop(*entry);

        cleanup.ExtendedCleanup(*entry);

        //+++++
        /*{
            CNcbiOfstream ostr("ttt1.txt");
            ostr << MSerial_AsnText << *entry;
        }*/
        //+++++

        // TODO the functionality below probably should be moved to Cleanup
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            CSeq_feat* gene     = nullptr;
            bool       gene_set = false;

            for (CTypeIterator<CSeq_feat> feat(Begin(*bioseq)); feat; ++feat) {

                LookForProductName(*feat);

                if (feat->IsSetData() && feat->GetData().IsGene()) {
                    if (gene_set) {
                        gene = nullptr;
                    } else {
                        gene_set = true;
                        gene     = &(*feat);
                    }
                }

                if (feat->IsSetLocation()) {
                    // modification of packed_int location (chenge it to a single interval if possible)
                    CSeq_loc& loc = feat->SetLocation();
                    if (loc.IsPacked_int()) {
                        ConvertPackedIntToInterval(loc);
                    } else if (loc.IsMix()) {

                        if (feat->IsSetData() && feat->GetData().IsProt()) {
                            ConvertMixToInterval(loc);
                        }
                    }
                }
            }

            if (gene && gene->IsSetLocation()) {

                CSeq_loc& loc = gene->SetLocation();

                // changes in gene
                if (! loc.GetId()) {
                    continue;
                }

                CBioseq_Handle bioseq_h = GetScope().GetBioseqHandle(*bioseq);
                if (! bioseq_h) {
                    continue;
                }

                CSeqdesc_CI mol_info(bioseq_h, CSeqdesc::E_Choice::e_Molinfo);

                if (mol_info && mol_info->GetMolinfo().IsSetBiomol() && mol_info->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA && bioseq->IsSetId()) {

                    const CBioseq::TId& ids = bioseq->GetId();
                    if (! ids.empty()) {

                        const CSeq_id& id = *ids.front();
                        if (id.Which() == loc.GetId()->Which()) {
                            loc.SetId(id);
                        }
                    }
                }
            }
        }

        /*
        for (CTypeIterator<CSeq_feat> feat(Begin(*entry)); feat; ++feat) {
            LookForProductName(*feat);

            // TODO the functionality below probably should be moved to Cleanup
            if (feat->IsSetLocation()) {
                CSeq_loc& loc = feat->SetLocation();

                // changes in gene
                if (feat->IsSetData() && feat->GetData().IsGene()) {
                    if (! loc.GetId()) {
                        continue;
                    }

                    CBioseq_Handle bioseq_h = GetScope().GetBioseqHandle(loc);
                    if (! bioseq_h) {
                        continue;
                    }

                    CConstRef<CBioseq> bioseq = bioseq_h.GetBioseqCore();
                    CSeqdesc_CI        mol_info(bioseq_h, CSeqdesc::E_Choice::e_Molinfo);

                    if (mol_info && mol_info->GetMolinfo().IsSetBiomol() && mol_info->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA && bioseq->IsSetId()) {
                        const CBioseq::TId& ids = bioseq->GetId();
                        if (! ids.empty()) {
                            const CSeq_id& id = *ids.front();
                            if (id.Which() == loc.GetId()->Which()) {
                                loc.SetId(id);
                            }
                        }
                    }
                }

                // modification of packed_int location (chenge it to a single interval if possible)
                if (loc.IsPacked_int()) {
                    ConvertPackedIntToInterval(loc);
                }
            }
        }
        */

        // for (CTypeIterator<CSeq_annot> annot(Begin(*entry)); annot; ++annot) {
        //     SortFeaturesByLocation(*annot);
        // }

        // for (CTypeIterator<CSeq_annot> annot(Begin(*entry)); annot; ++annot) {
        //     CreatePubFromFeats(*annot);
        // }
    }
}

END_NCBI_SCOPE
