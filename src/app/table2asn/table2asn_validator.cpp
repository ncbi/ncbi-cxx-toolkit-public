#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include "table2asn_validator.hpp"

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/cleanup/cleanup.hpp>


//CTable2AsnValidator::CTable2AsnValidator()

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

void CTable2AsnValidator::Validate(const CSeq_entry& entry)
{
   validator::CValidator validator(*CObjectManager::GetInstance());

   CScope scope(*CObjectManager::GetInstance());
    //scope.AddTopLevelSeqEntry(entry);

   //CConstRef<CValidError> errors = validator.Validate(entry, &scope);
}

void CTable2AsnValidator::Cleanup(CSeq_entry& entry)
{
    CCleanup cleanup;
    cleanup.BasicCleanup(entry, 0);

    LinkCDSmRNAbyLabelAndLocation(entry);

}

void CTable2AsnValidator::LinkCDSmRNAbyLabelAndLocation(CSeq_entry& entry)
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
void AddUniqueElement(C& cont, CRef<T>& element)
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

    AddUniqueElement(dest.SetXref(), newref);
}

void CTable2AsnValidator::LinkCDSmRNAbyLabelAndLocation(objects::CBioseq_set& bioseq)
{
    if (!bioseq.IsSetAnnot())
        return;

    LinkCDSmRNAbyLabelAndLocation(bioseq.SetAnnot());
}

void CTable2AsnValidator::LinkCDSmRNAbyLabelAndLocation(CBioseq& bioseq)
{ 
    if (!bioseq.IsSetAnnot())
        return;

    LinkCDSmRNAbyLabelAndLocation(bioseq.SetAnnot());
}

void CTable2AsnValidator::LinkCDSmRNAbyLabelAndLocation(CBioseq::TAnnot& annot)
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
                        sequence::ECompare located = sequence::Compare(cds->GetLocation(), mrna->GetLocation(), 0);

                        if (located == sequence::eContained || located == sequence::eSame)
                        {
                            // create cross references
                            CreateReference(*cds, *mrna);
                            CreateReference(*mrna, *cds);
#ifdef _DEBUG
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

#ifdef XXX
static void LinkCDSmRNAbyLabelAndLocationCallback (
  BioseqPtr bsp, 
  Pointer userdata
)

{
  SMFeatItemPtr PNTR  array;
  BioseqExtraPtr      bspextra;
  Uint2               entityID;
  SMFeatItemPtr       feat;
  Int4                i, j, best_index, best_diff, diff;
  Int4                num;
  ObjMgrDataPtr       omdp;

  if (bsp == NULL) return;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return;
  array = bspextra->featsByLabel;
  num = bspextra->numfeats;
  if (array == NULL || num < 1) return;

  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  /* labels are all grouped together - for each cds/mRNA in group of identical labels,
   * find match with best location.
   */
  for (i = 0; i < num - 1; i++) {
    feat = array [i];
    if (feat->sfp == NULL) {
      continue;
    } else if (feat->sfp->xref != NULL) {
      /* already assigned feat xref */
      continue;
    } else if (feat->sfp->idx.subtype != FEATDEF_CDS && feat->sfp->idx.subtype != FEATDEF_mRNA) {
      /* not interested in these feature types */
    } else {
      best_index = -1;
      for (j = i + 1; j < num && StringCmp (feat->label, array[j]->label) == 0; j++) {
        if (array[j]->sfp == NULL) {
          /* bad */
        } else if (array[j]->sfp->xref != NULL) {
          /* already assigned feat xref */
        } else if (feat->sfp->idx.subtype == FEATDEF_CDS) {
          if (array[j]->sfp->idx.subtype != FEATDEF_mRNA) {
            /* wrong feature type */
          } else if ((diff = SeqLocAinB (feat->sfp->location, array[j]->sfp->location)) < 0) {
            /* locations don't match */
          } else {
            if (best_index == -1) {
              /* don't have a best yet */
              best_index = j;
              best_diff = diff;
            } else if (diff < best_diff) {
              best_index = j;
              best_diff = diff;
            }
          }
        } else if (feat->sfp->idx.subtype == FEATDEF_mRNA) {
          if (array[j]->sfp->idx.subtype != FEATDEF_CDS) {
            /* wrong feature type */
          } else if ((diff = SeqLocAinB (array[j]->sfp->location, feat->sfp->location)) < 0) {
            /* locations don't match */
          } else {
            if (best_index == -1) {
              /* don't have a best yet */
              best_index = j;
              best_diff = diff;
            } else if (diff < best_diff) {
              best_index = j;
              best_diff = diff;
            }
          }
        }
      }
      if (best_index > -1) {
        CreateReciprocalLink (feat->sfp, array[best_index]->sfp);
      }
    }
  }
}
#endif

END_NCBI_SCOPE

