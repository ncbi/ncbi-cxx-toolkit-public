/* fcleanup.cpp
 *
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
 * File Name:  fcleanup.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
 * This file contains some specific cleanup functionality
 */
#include <ncbi_pch.hpp>

#include <objtools/flatfile/ftacpp.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <serial/iterator.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>

#include <objtools/flatfile/fta_parser.h>

#include <objtools/flatfile/ftamain.h>

#include "loadfeat.h"
#include "fcleanup.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "fcleanup.cpp"

/**********************************************************/
// TODO To be moved to cleanup
/*static void CreatePubFromFeats(ncbi::objects::CSeq_annot& annot)
{
    if (!annot.IsFtable())
        return;

    TSeqFeatList& feats = annot.SetData().SetFtable();
    for (TSeqFeatList::iterator feat = feats.begin(); feat != feats.end();)
    {
        if (!(*feat)->IsSetCit() || !(*feat)->GetCit().IsPub() || !(*feat)->IsSetLocation())
        {
            ++feat;
            continue;
        }

        ncbi::objects::CBioseq_Handle bioseq_h = GetScope().GetBioseqHandle((*feat)->GetLocation());
        ncbi::objects::CBioseq_EditHandle bioseq = GetScope().GetBioseqEditHandle(*bioseq_h.GetCompleteBioseq());

        bool feat_imp = (*feat)->IsSetData() && (*feat)->GetData().IsImp();

        ncbi::objects::CPub_set& pubs = (*feat)->SetCit();
        NON_CONST_ITERATE(TPubList, pub, pubs.SetPub())
        {
            ncbi::CRef<ncbi::objects::CSeqdesc> new_descr(new ncbi::objects::CSeqdesc);
            ncbi::objects::CPubdesc& pubdescr = new_descr->SetPub();

            if ((*pub)->IsEquiv())
                pubdescr.SetPub((*pub)->SetEquiv());
            else
                pubdescr.SetPub().Set().push_back(*pub);

            pubdescr.SetReftype(2);

            if (feat_imp)
            {
                const ncbi::objects::CImp_feat& imp = (*feat)->GetData().GetImp();
                if (imp.IsSetKey() && imp.GetKey() == "Site-ref")
                    pubdescr.SetReftype(1);
            }

            bioseq.SetDescr().Set().push_back(new_descr);
        }

        feat = feats.erase(feat);
    }
}*/

static void MoveSourceDescrToTop(ncbi::objects::CSeq_entry& entry)
{
    if (entry.IsSeq())
        return;

    ncbi::objects::CBioseq_set& seq_set = entry.SetSet();
    const ncbi::objects::CSeqdesc* source = nullptr;
    if (seq_set.IsSetDescr())
    {
        ITERATE(TSeqdescList, desc, seq_set.GetDescr().Get())
        {
            if ((*desc)->IsSource())
            {
                source = *desc;
                break;
            }
        }
    }

    for (ncbi::CTypeIterator<ncbi::objects::CBioseq> bioseq(Begin(entry)); bioseq; ++bioseq)
    {
        if (bioseq->IsSetDescr())
        {
            TSeqdescList& descrs = bioseq->SetDescr().Set();
            for (TSeqdescList::iterator desc = descrs.begin(); desc != descrs.end(); ++desc)
            {
                if ((*desc)->IsSource())
                {
                    if (source == nullptr)
                    {
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

static bool IsEmptyMolInfo(const ncbi::objects::CMolInfo& mol_info)
{
    return !(mol_info.IsSetBiomol() || mol_info.IsSetCompleteness() || mol_info.IsSetGbmoltype() ||
             mol_info.IsSetTech() || mol_info.IsSetTechexp());
}

static void MoveAnnotToTop(ncbi::objects::CSeq_entry& seq_entry)
{
    if (!seq_entry.IsSet() || !seq_entry.GetSet().IsSetClass() || seq_entry.GetSet().GetClass() != ncbi::objects::CBioseq_set::eClass_segset)
        return;

    ncbi::objects::CBioseq_set* parts = nullptr;
    ncbi::objects::CBioseq_set& bio_set = seq_entry.SetSet();

    NON_CONST_ITERATE(TEntryList, entry, bio_set.SetSeq_set())
    {
        if ((*entry)->IsSet() && (*entry)->GetSet().IsSetClass() && (*entry)->GetSet().GetClass() == ncbi::objects::CBioseq_set::eClass_parts)
        {
            parts = &(*entry)->SetSet();
            break;
        }
    }

    if (parts != nullptr && parts->IsSetAnnot())
    {

        ncbi::objects::CBioseq::TAnnot& annot = bio_set.SetSeq_set().front()->SetAnnot();
        annot.splice(annot.end(), parts->SetAnnot());
        parts->ResetAnnot();
    }
}

static void MoveBiomolToTop(ncbi::objects::CSeq_entry& seq_entry)
{
    if (!seq_entry.IsSet() || !seq_entry.GetSet().IsSetClass() || seq_entry.GetSet().GetClass() != ncbi::objects::CBioseq_set::eClass_segset)
        return;

    if (seq_entry.IsSetDescr())
    {
        ITERATE(TSeqdescList, desc, seq_entry.GetDescr().Get())
        {
            if ((*desc)->IsMolinfo())
                return;
        }
    }

    ncbi::objects::CBioseq_set* parts = nullptr;
    ncbi::objects::CBioseq_set& bio_set = seq_entry.SetSet();

    NON_CONST_ITERATE(TEntryList, entry, bio_set.SetSeq_set())
    {
        if ((*entry)->IsSet() && (*entry)->GetSet().IsSetClass() && (*entry)->GetSet().GetClass() == ncbi::objects::CBioseq_set::eClass_parts)
        {
            parts = &(*entry)->SetSet();
            break;
        }
    }

    if (parts != nullptr)
    {
        int biomol = ncbi::objects::CMolInfo::eBiomol_unknown;
        ITERATE(TEntryList, entry, parts->GetSeq_set())
        {
            if (!(*entry)->IsSeq())
                return;

            const ncbi::objects::CBioseq& bioseq = (*entry)->GetSeq();
            if (bioseq.IsSetDescr())
            {
                ITERATE(TSeqdescList, desc, bioseq.GetDescr().Get())
                {
                    if ((*desc)->IsMolinfo() && (*desc)->GetMolinfo().IsSetBiomol())
                    {
                        int cur_biomol = (*desc)->GetMolinfo().GetBiomol();
                        if (biomol == ncbi::objects::CMolInfo::eBiomol_unknown)
                            biomol = cur_biomol;
                        else if (biomol != cur_biomol)
                            return;
                    }
                }
            }
        }

        if (biomol != ncbi::objects::CMolInfo::eBiomol_unknown)
        {
            NON_CONST_ITERATE(TEntryList, entry, parts->SetSeq_set())
            {
                ncbi::objects::CBioseq& bioseq = (*entry)->SetSeq();
                if (bioseq.IsSetDescr())
                {
                    TSeqdescList& descrs = bioseq.SetDescr().Set();
                    for (TSeqdescList::iterator desc = descrs.begin(); desc != descrs.end(); ++desc)
                    {
                        if ((*desc)->IsMolinfo())
                        {
                            (*desc)->SetMolinfo().ResetBiomol();
                            if (IsEmptyMolInfo((*desc)->GetMolinfo()))
                                descrs.erase(desc);
                            break;
                        }
                    }
                }
            }

            ncbi::CRef<ncbi::objects::CSeqdesc> new_descr(new ncbi::objects::CSeqdesc);
            new_descr->SetMolinfo().SetBiomol(biomol);

            seq_entry.SetDescr().Set().push_back(new_descr);
        }
    }
}

static void LookForProductName(ncbi::objects::CSeq_feat& feat)
{
    if (feat.IsSetData() && feat.GetData().IsProt())
    {
        ncbi::objects::CProt_ref& prot_ref = feat.SetData().SetProt();

        if (!prot_ref.IsSetName() || prot_ref.GetName().empty() || (prot_ref.GetName().size() == 1 && prot_ref.GetName().front() == "unnamed"))
        {
            if (feat.IsSetQual())
            {
                NON_CONST_ITERATE(TQualVector, qual, feat.SetQual())
                {
                    if ((*qual)->IsSetQual() && (*qual)->GetQual() == "product" && (*qual)->IsSetVal())
                    {
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

/*bool FeatLocCmp(const ncbi::CRef<ncbi::objects::CSeq_feat>& a, const ncbi::CRef<ncbi::objects::CSeq_feat>& b)
{
    if (!a->IsSetLocation() || !b->IsSetLocation())
        return false;

    return a->GetLocation().GetStart(ncbi::objects::eExtreme_Biological) < b->GetLocation().GetStart(ncbi::objects::eExtreme_Biological);
}

static void SortFeaturesByLocation(ncbi::objects::CSeq_annot& annot)
{
    if (!annot.IsFtable())
        return;

    TSeqFeatList& feats = annot.SetData().SetFtable();
    feats.sort(FeatLocCmp);
}*/

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

static void SetNewInterval(const ncbi::objects::CSeq_interval* first, const ncbi::objects::CSeq_interval* last, const ncbi::TSeqPos& from, ncbi::TSeqPos& to, ncbi::objects::CSeq_loc& loc)
{
    CRef<ncbi::objects::CSeq_interval> new_int(new ncbi::objects::CSeq_interval);

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

static void ConvertMixToInterval(ncbi::objects::CSeq_loc& loc)
{
    const ncbi::objects::CSeq_loc_mix::Tdata& locs = loc.GetMix().Get();
    const ncbi::objects::CSeq_interval* first_interval = nullptr;
    const ncbi::objects::CSeq_interval* last_interval = nullptr;

    vector<pair<TSeqPos, TSeqPos>> ranges;
    ITERATE(ncbi::objects::CSeq_loc_mix::Tdata, cur_loc, locs) {
        if ((*cur_loc)->IsInt()) {

            const ncbi::objects::CSeq_interval& interval = (*cur_loc)->GetInt();
            ranges.push_back(make_pair(interval.GetFrom(), interval.GetTo()));

            if (first_interval == nullptr) {
                first_interval = &interval;
            }
            else if (first_interval->GetStart(ncbi::objects::eExtreme_Biological) > interval.GetStart(ncbi::objects::eExtreme_Biological)) {
                first_interval = &interval;
            }

            if (last_interval == nullptr) {
                last_interval = &interval;
            }
            else if (last_interval->GetStop(ncbi::objects::eExtreme_Biological) < interval.GetStop(ncbi::objects::eExtreme_Biological)) {
                last_interval = &interval;
            }
        }
        else if ((*cur_loc)->IsPnt()) {
            const ncbi::objects::CSeq_point& point = (*cur_loc)->GetPnt();
            ranges.push_back(make_pair(point.GetPoint(), point.GetPoint()));
        }
        else {
            // TODO process the other types of intervals
            return;
        }
    }

    if (first_interval == nullptr) {
        return;
    }

    sort(ranges.begin(), ranges.end());

    if (IsConversionPossible(ranges)) {

        SetNewInterval(first_interval, last_interval, ranges.front().first, ranges.back().second, loc);
    }
}

static void ConvertPackedIntToInterval(ncbi::objects::CSeq_loc& loc)
{
    const ncbi::objects::CPacked_seqint::Tdata& ints = loc.GetPacked_int();

    vector<pair<TSeqPos, TSeqPos>> ranges;
    ITERATE(ncbi::objects::CPacked_seqint::Tdata, interval, ints) {
        ranges.push_back(make_pair((*interval)->GetFrom(), (*interval)->GetTo()));
    }

    sort(ranges.begin(), ranges.end());

    if (IsConversionPossible(ranges)) {

        SetNewInterval(loc.GetPacked_int().GetStartInt(ncbi::objects::eExtreme_Biological), loc.GetPacked_int().GetStopInt(ncbi::objects::eExtreme_Biological),
                       ranges.front().first, ranges.back().second, loc);
    }
}

void FinalCleanup(TEntryList& seq_entries)
{
    ncbi::objects::CCleanup cleanup;
    cleanup.SetScope(&GetScope());

    //+++++
    /*{
        ncbi::objects::CBioseq_set bio_set;
        bio_set.SetClass(ncbi::objects::CBioseq_set::eClass_genbank);

        bio_set.SetSeq_set() = seq_entries;
        ncbi::CNcbiOfstream ostr("before_cleanup.asn");
        ostr << MSerial_AsnText << bio_set;
    }*/
    //+++++

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        //+++++
        /*{
            ncbi::CNcbiOfstream ostr("ttt.txt");
            ostr << MSerial_AsnText << **entry;
        }*/
        //+++++

        MoveSourceDescrToTop(**entry);

        ncbi::CConstRef<ncbi::objects::CCleanupChange> cleanup_change = cleanup.ExtendedCleanup(*(*entry));

        //+++++
        /*{
            ncbi::CNcbiOfstream ostr("ttt1.txt");
            ostr << MSerial_AsnText << **entry;
        }*/
        //+++++

        // TODO the functionality below probably should be moved to Cleanup
        for (ncbi::CTypeIterator<ncbi::objects::CBioseq> bioseq(Begin(**entry)); bioseq; ++bioseq) {

            ncbi::objects::CSeq_feat* gene = nullptr;
            bool gene_set = false;

            for (ncbi::CTypeIterator<ncbi::objects::CSeq_feat> feat(Begin(*bioseq)); feat; ++feat) {

                LookForProductName(*feat);

                if (feat->IsSetData() && feat->GetData().IsGene()) {
                    if (gene_set) {
                        gene = nullptr;
                    }
                    else {
                        gene_set = true;
                        gene = &(*feat);
                    }
                }

                if (feat->IsSetLocation()) {

                    // modification of packed_int location (chenge it to a single interval if possible)
                    ncbi::objects::CSeq_loc& loc = feat->SetLocation();
                    if (loc.IsPacked_int()) {
                        ConvertPackedIntToInterval(loc);
                    }
                    else if (loc.IsMix()) {

                        if (feat->IsSetData() && feat->GetData().IsProt()) {
                            ConvertMixToInterval(loc);
                        }
                    }
                }
            }

            if (gene != nullptr && gene->IsSetLocation()) {

                ncbi::objects::CSeq_loc& loc = gene->SetLocation();

                // changes in gene
                if (loc.GetId() == nullptr) {
                    continue;
                }

                ncbi::objects::CBioseq_Handle bioseq_h = GetScope().GetBioseqHandle(*bioseq);
                if (!bioseq_h) {
                    continue;
                }

                ncbi::objects::CSeqdesc_CI mol_info(bioseq_h, ncbi::objects::CSeqdesc::E_Choice::e_Molinfo);

                if (mol_info && mol_info->GetMolinfo().IsSetBiomol() && mol_info->GetMolinfo().GetBiomol() == ncbi::objects::CMolInfo::eBiomol_mRNA && bioseq->IsSetId()) {

                    const ncbi::objects::CBioseq::TId& ids = bioseq->GetId();
                    if (!ids.empty()) {

                        const ncbi::objects::CSeq_id& id = *ids.front();
                        if (id.Which() == loc.GetId()->Which()) {
                            loc.SetId(id);
                        }
                    }
                }
            }
        }

/*        for (ncbi::CTypeIterator<ncbi::objects::CSeq_feat> feat(Begin(*(*entry))); feat; ++feat)
        {
            LookForProductName(*feat);

            // TODO the functionality below probably should be moved to Cleanup
            if (feat->IsSetLocation()) {

                ncbi::objects::CSeq_loc& loc = feat->SetLocation();

                // changes in gene
                if (feat->IsSetData() && feat->GetData().IsGene()) {

                    if (loc.GetId() == nullptr) {
                        continue;
                    }

                    ncbi::objects::CBioseq_Handle bioseq_h = GetScope().GetBioseqHandle(loc);
                    if (!bioseq_h) {
                        continue;
                    }

                    ncbi::CConstRef<ncbi::objects::CBioseq> bioseq = bioseq_h.GetBioseqCore();
                    ncbi::objects::CSeqdesc_CI mol_info(bioseq_h, ncbi::objects::CSeqdesc::E_Choice::e_Molinfo);

                    if (mol_info && mol_info->GetMolinfo().IsSetBiomol() && mol_info->GetMolinfo().GetBiomol() == ncbi::objects::CMolInfo::eBiomol_mRNA && bioseq->IsSetId()) {

                        const ncbi::objects::CBioseq::TId& ids = bioseq->GetId();
                        if (!ids.empty()) {

                            const ncbi::objects::CSeq_id& id = *ids.front();
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
        }*/

        MoveBiomolToTop(*(*entry));
        MoveAnnotToTop(*(*entry));
        for (ncbi::CTypeIterator<ncbi::objects::CSeq_entry> cur_entry(Begin(*(*entry))); cur_entry; ++cur_entry)
        {
            MoveBiomolToTop(*cur_entry);
            MoveAnnotToTop(*cur_entry);
        }

        //for (ncbi::CTypeIterator<ncbi::objects::CSeq_annot> annot(Begin(*(*entry))); annot; ++annot)
        //{
        //    SortFeaturesByLocation(*annot);
        //}

        //for (ncbi::CTypeIterator<ncbi::objects::CSeq_annot> annot(Begin(*(*entry))); annot; ++annot)
        //{
        //    CreatePubFromFeats(*annot);
        //}
    }

}
