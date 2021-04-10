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
 * Author:   Sergiy Gotvyanskyy
 *
 * File Description:
 *   Operator to construct cross reference link of
 *   mRNA and CDS matchin by label and location
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/general/Object_id.hpp>

// Object Manager includes
//#include <objmgr/object_manager.hpp>
//#include <objmgr/scope.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/edit/link_cds_mrna.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace 
{
bool IsmRNA(const CSeq_feat::TData& feat_data)
{
    if (feat_data.IsRna() &&
        feat_data.GetRna().IsSetType() &&
        feat_data.GetRna().GetType() == CRNA_ref::eType_mRNA)
        return true;
    else
        return false;
}

template<typename T>
struct EqualsPred
{
    CRef<T> m_other;
    EqualsPred(){};
    EqualsPred(const CRef<T>& other) : m_other(other){};

    bool operator()(const CRef<T>& r)
    {
        return (m_other == r || (m_other->Equals(*r)));
    }
};

template<typename C, typename T>
void AddIfUniqueElement(C& cont, CRef<T>& element)
{
    typename C::iterator it = find_if(cont.begin(), cont.end(), EqualsPred<T>(element));
    if (it == cont.end())
    {
        cont.push_back(element);
    }
}

void CreateReference(CSeq_feat& dest, const CSeq_feat& src)
{
    CRef<CSeqFeatXref> newref(new CSeqFeatXref());

    if (src.IsSetId())
        newref->SetId().Assign(src.GetId());

    if (src.IsSetData())
    {
        switch (src.GetData().Which())
        {
        case CSeqFeatData::e_Rna:
            newref->SetData().SetRna().SetType(src.GetData().GetRna().GetType());
            break;
        default:
            newref->SetData().Select(src.GetData().Which());
        }
    }

    AddIfUniqueElement(dest.SetXref(), newref);
}


void xLinkCDSmRNAbyLabelAndLocation(CSeq_annot::C_Data::TFtable& ftable)
{ 
    for (CSeq_annot::TData::TFtable::iterator feat_it = ftable.begin();
        ftable.end() != feat_it; ++feat_it)
    {
        CRef<CSeq_feat> feature = (*feat_it);

        if (!feature->IsSetData()) 
            continue;

        if (! (feature->GetData().IsCdregion() || IsmRNA(feature->GetData())))
            continue;

        string feat_label;
        feature::GetLabel(*feature, &feat_label, feature::fFGL_Content);

        // locate counterpart starting from next element
        CSeq_annot::TData::TFtable::iterator opp_it = feat_it;
        ++opp_it;

        CRef<CSeq_feat> best_fit;

        for(; opp_it != ftable.end(); ++opp_it)
        {
            CRef<CSeq_feat> current = *opp_it;

            if (!current->IsSetData()) continue;

            if ( (IsmRNA(current->GetData()) && feature->GetData().IsCdregion()) ||
                (current->GetData().IsCdregion() && IsmRNA(feature->GetData())) )
            {
                string label;
                feature::GetLabel(*current, &label, feature::fFGL_Content);

                if (NStr::Compare(feat_label, label) != 0)
                    continue; 

                CRef<CSeq_feat> cds, mrna;
                if (feature->GetData().IsCdregion())
                {
                    cds = feature;
                    mrna = current;
                }
                else
                {
                    mrna = feature;
                    cds = current;
                }

                // is this the best?
                // TODO: add choosing of the best mRNA*CDS combinations 
                // instead of getting the first one
                sequence::ECompare located = sequence::Compare(mrna->GetLocation(),
                    cds->GetLocation(), 0, sequence::fCompareOverlapping);
                switch (located)
                {
                case sequence::eContains:      ///< First CSeq_loc contains second
                case sequence::eSame:          ///< CSeq_locs contain each other
                    best_fit = current;
                    break;
                default:
                    break;
                }          

            } 
        }  // inner loop locating counterpart
        if (best_fit.NotEmpty())
        {
            // create cross references
            CreateReference(*feature, *best_fit);
            CreateReference(*best_fit, *feature);
#ifdef _DEBUG_000
            cout << "Adding: "
                << feat_label << ":" << label
                //<< CSeqFeatData::SelectionName(element->GetData().Which())
                << endl;
#endif
            break; 
            // stop the inner loop
        }
    } // iterate over feature table
}
}

void CCDStomRNALinkBuilder::Operate(CSeq_entry& entry)
{
    LinkCDSmRNAbyLabelAndLocation(entry);
}

void CCDStomRNALinkBuilder::LinkCDSmRNAbyLabelAndLocation(CSeq_entry& entry)
{ 
    if (entry.IsSeq())
    {
        LinkCDSmRNAbyLabelAndLocation(entry.SetSeq());
    }
    else
    if (entry.IsSet())
    {
        LinkCDSmRNAbyLabelAndLocation(entry.SetSet());
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            LinkCDSmRNAbyLabelAndLocation(**it);
        }
    }
}

void CCDStomRNALinkBuilder::LinkCDSmRNAbyLabelAndLocation(objects::CBioseq_set& bioseq)
{
    if (!bioseq.IsSetAnnot())
        return;

    for (CBioseq::TAnnot::iterator annot_it = bioseq.SetAnnot().begin(); bioseq.SetAnnot().end() != annot_it; ++annot_it)
    {
        if (!(**annot_it).IsFtable()) continue;

        CSeq_annot::C_Data::TFtable& ftable = (**annot_it).SetData().SetFtable();

        xLinkCDSmRNAbyLabelAndLocation(ftable);
    }
}

void CCDStomRNALinkBuilder::LinkCDSmRNAbyLabelAndLocation(CBioseq& bioseq)
{ 
    if (!bioseq.IsSetAnnot())
        return;

    for (CBioseq::TAnnot::iterator annot_it = bioseq.SetAnnot().begin(); bioseq.SetAnnot().end() != annot_it; ++annot_it)
    {
        if (!(**annot_it).IsFtable()) continue;

        CSeq_annot::C_Data::TFtable& ftable = (**annot_it).SetData().SetFtable();

        xLinkCDSmRNAbyLabelAndLocation(ftable);
    }
}

END_NCBI_SCOPE

