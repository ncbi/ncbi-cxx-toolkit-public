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


void xLinkCDSmRNAbyLabelAndLocation(CBioseq::TAnnot& annot)
{ 
    for (CBioseq::TAnnot::iterator annot_it = annot.begin(); annot.end() != annot_it; ++annot_it)
    {
        if (!(**annot_it).IsFtable()) continue;

        CRef<CSeq_annot> seq_annot(*annot_it);
        CSeq_annot::C_Data::TFtable& ftable = seq_annot->SetData().SetFtable();
        for (CSeq_annot::TData::TFtable::iterator feat_it = ftable.begin();
            ftable.end() != feat_it; ++feat_it)
        {
            CRef<CSeq_feat> feature = (*feat_it);

            if (!feature->IsSetData()) 
                continue;

            if (feature->GetData().IsCdregion() || IsmRNA(feature->GetData()))
            {
                string feat_label;
                feature::GetLabel(*feature, &feat_label, feature::fFGL_Content);

                // locate counterpart starting from next element
                CSeq_annot::TData::TFtable::iterator opp_it = feat_it;
                ++opp_it;

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
                        sequence::ECompare located = sequence::Compare(cds->GetLocation(), mrna->GetLocation(), 0);

                        if (located == sequence::eContained || located == sequence::eSame)
                        {
                            // create cross references
                            CreateReference(*cds, *mrna);
                            CreateReference(*mrna, *cds);
#ifdef _DEBUG_000
                            cout << "Adding: "
                                 << feat_label << ":" << label
                                 //<< CSeqFeatData::SelectionName(element->GetData().Which())
                                 << endl;
#endif
                            break; 
                            // stop the inner loop
                        }
                    } 
                }  // inner loop locating counterpart
            } // if cdregion or mRNA
        } // iterate over feature table
    } // iterate over annotations
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

    xLinkCDSmRNAbyLabelAndLocation(bioseq.SetAnnot());
}

void CCDStomRNALinkBuilder::LinkCDSmRNAbyLabelAndLocation(CBioseq& bioseq)
{ 
    if (!bioseq.IsSetAnnot())
        return;

    xLinkCDSmRNAbyLabelAndLocation(bioseq.SetAnnot());
}

END_NCBI_SCOPE

