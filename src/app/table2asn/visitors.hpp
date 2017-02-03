#ifndef __TABLE2ASN_VISITORS_HPP_INCLUDED__
#define __TABLE2ASN_VISITORS_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

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
            NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.GetSet().GetSeq_set())
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

};



END_NCBI_SCOPE


#endif
