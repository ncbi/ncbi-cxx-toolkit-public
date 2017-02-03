#ifndef __TABLE2ASN_VISITORS_HPP_INCLUDED__
#define __TABLE2ASN_VISITORS_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
    template<typename _M>
    void VisitAllBioseqs(const objects::CSeq_entry& entry, _M m)
    {
        if (entry.IsSeq())
        {
            m(entry.GetSeq());
        }
        else
        if (entry.IsSet() && !entry.GetSet().GetSeq_set().empty())
        {
            ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.GetSet().GetSeq_set())
            {
                VisitAllBioseqs(**it_se, m);
            }
        }
    };

    template<typename _M>
    void VisitAllBioseqs(objects::CSeq_entry& entry, _M m)
    {
        if (entry.IsSeq())
        {
            m(entry.SetSeq());
        }
        else
        if (entry.IsSet() && !entry.GetSet().GetSeq_set().empty())
        {
            NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.SetSet().SetSeq_set())
            {
                VisitAllBioseqs(**it_se, m);
            }
        }
    };

    template<typename _M>
    void VisitAllFeatures(objects::CSeq_entry_EditHandle& entry_h, _M m)
    {
        for (objects::CFeat_CI feat_it(entry_h); feat_it; ++feat_it)
        {
            m(*(CSeq_feat*)feat_it->GetOriginalSeq_feat().GetPointer());
        }
    }

    template<typename _Method>
    void VisitAllFeatures(CBioseq& bioseq, _Method m)
    {
        if (bioseq.IsSetAnnot() && !bioseq.GetAnnot().empty())
        {
            NON_CONST_ITERATE(CBioseq::TAnnot, annot_it, bioseq.SetAnnot())
            {
                if (!(**annot_it).IsFtable())
                    continue;

                NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, ft_it, (**annot_it).SetData().SetFtable())
                {
                   m(bioseq, **ft_it);
                }
            }
        }
    }


    template<typename _Method, typename _BioseqFilter>
    void VisitAllFeatures(objects::CSeq_entry& entry, _BioseqFilter bf, _Method m)
    {
        VisitAllBioseqs(entry, [m, bf](CBioseq& bioseq)
        {
            if (bf(bioseq))
               VisitAllFeatures(bioseq, m);
        });
    }

    template<typename _M>
    void VisitAllFeatures(objects::CSeq_entry& entry, _M m)
    {
        VisitAllFeatures(entry,
            [](CBioseq&){return true; }, // all fit filter
            m);
    }
};



END_NCBI_SCOPE


#endif
