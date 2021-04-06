#ifndef __TABLE2ASN_VISITORS_HPP_INCLUDED__
#define __TABLE2ASN_VISITORS_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

#include <objmgr/feat_ci.hpp>
#include <objects/seq/Seq_descr.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
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
                for (auto se : entry.SetSet().SetSeq_set())
                {
                    VisitAllBioseqs(*se, m);
                }
            }
    }

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
            for (auto se : entry.GetSet().GetSeq_set())
            {
                VisitAllBioseqs(*se, m);
            }
        }
    }

    template<typename _M>
    void VisitAllSeqDesc(objects::CSeq_entry& entry, bool skip_nucprot, _M m)
    {
        if (entry.IsSeq())
        {
            m(&entry.SetSeq(), entry.SetSeq().SetDescr());
            if (entry.GetSeq().GetDescr().Get().empty())
                entry.SetSeq().ResetDescr();
        }
        else
            if (entry.IsSet() && !entry.GetSet().GetSeq_set().empty())
            {
                if (skip_nucprot && entry.GetSet().IsSetClass() && entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot)
                {
                    m(nullptr, entry.SetSet().SetDescr());
                    if (entry.GetSet().GetDescr().Get().empty())
                        entry.SetSet().ResetDescr();

                    return;
                }

                for (auto se : entry.SetSet().SetSeq_set())
                {
                    VisitAllSeqDesc(*se, skip_nucprot, m);
                }
            }
    }

    template<typename _M>
    void VisitAllSeqDesc(objects::CSeq_entry& entry, _M m)
    {
        if (entry.IsSeq())
        {
            m(&entry.SetSeq(), entry.SetSeq().SetDescr());
            if (entry.GetSeq().GetDescr().Get().empty())
                entry.SetSeq().ResetDescr();
        }
        else
            if (entry.IsSet() && !entry.GetSet().GetSeq_set().empty())
            {
                bool go_deep = m(nullptr, entry.SetSet().SetDescr());
                if (entry.GetSet().GetDescr().Get().empty())
                    entry.SetSet().ResetDescr();

                if (go_deep)
                for (auto se : entry.SetSet().SetSeq_set())
                {
                    VisitAllSeqDesc(*se, m);
                }
            }
    }

    template<typename _Mset, typename _Mseq>
    void VisitAllSetandSeq(objects::CSeq_entry& entry, _Mset mset, _Mseq mseq)
    {
        if (entry.IsSeq())
        {
            mseq(entry.SetSeq());
        }
        else
            if (entry.IsSet() && !entry.GetSet().GetSeq_set().empty())
            {
                bool go_deep = mset(entry.SetSet());

                if (go_deep)
                for (auto se : entry.SetSet().SetSeq_set())
                {
                    VisitAllSetandSeq(*se, mset, mseq);
                }
            }
    }

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
            for (auto annot : bioseq.SetAnnot())
            {
                if (!annot->IsFtable())
                    continue;

                for (auto feat : annot->SetData().SetFtable())
                {
                   m(bioseq, *feat);
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
/*
    template<typename _M>
    void VisitBioseqFeatures(objects::CSeq_entry& entry, _M m)
    {
        VisitAllFeatures(entry,
            [](CBioseq&){return true; }, // all fit filter
            m);
    }
*/

    template<typename Method>
    void VisitAllFeatures(objects::CBioseq::TAnnot& annots, Method method)
    {
        for (auto pAnnot : annots) {
            if (!pAnnot->IsFtable()) {
                continue;
            }

            for (auto pSeqFeat : pAnnot->SetData().SetFtable()) {
                if (pSeqFeat) {
                    method(*pSeqFeat);
                }
            }
        }
    }


    template<typename Method>
    void VisitAllFeatures(objects::CSeq_entry& entry, Method method)
    {
        if (entry.IsSeq()) {
            if (entry.GetSeq().IsSetAnnot()) {
                VisitAllFeatures(entry.SetSeq().SetAnnot(), method);
            }
            return;
        }

        // is set:
        if(entry.GetSet().IsSetAnnot()) {
            VisitAllFeatures(entry.SetSet().SetAnnot(), method);
        }

        for (auto pSubEntry : entry.SetSet().SetSeq_set()) {
            if (pSubEntry) {
                VisitAllFeatures(*pSubEntry, method);
            }
        }
    }
}



END_NCBI_SCOPE


#endif
