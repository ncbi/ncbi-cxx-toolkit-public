// This is generated file, you may modify it freely
#ifndef OBJECTS_SEQSET_BIOSEQ_SET_HPP
#define OBJECTS_SEQSET_BIOSEQ_SET_HPP

// standard includes
#include <corelib/ncbistd.hpp>

// generated includes
#include <objects/seqset/Bioseq_set_Base.hpp>

// generated classes
class CBioseq_set : public CBioseq_set_Base
{
    typedef CBioseq_set_Base CBase;
public:
    CBioseq_set();
    ~CBioseq_set();

    const TSeq_set& GetSeq_set(void) const
        {
            return CBase::GetSeq_set();
        }
    TSeq_set& SetSeq_set(void)
        {
            return CBase::SetSeq_set();
        }
};

#endif // OBJECTS_SEQSET_BIOSEQ_SET_HPP
