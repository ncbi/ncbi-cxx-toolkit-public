// This is generated file, you may modify it freely
#ifndef OBJECTS_SEQSET_BIOSEQ_SET_HPP
#define OBJECTS_SEQSET_BIOSEQ_SET_HPP


// generated includes
#include <objects/seqset/Bioseq_set_Base.hpp>

// generated classes
class CBioseq_set : public CBioseq_set_Base
{
    typedef CBioseq_set_Base Tparent;
public:
    // constructor for static/automatic objects
    CBioseq_set(void);
    // destructor
    ~CBioseq_set(void);

    // create dynamically allocated object
    static CBioseq_set* NewInstance(void)
        {
            return new CBioseq_set(NCBI_NS_NCBI::CObject::eCanDelete);
        }

    // public getters
    const TSeq_set& GetSeq_set(void) const
        {
            return Tparent::GetSeq_set();
        }
    TSeq_set& SetSeq_set(void)
        {
            return Tparent::SetSeq_set();
        }

protected:
    // hidden constructor for dynamic objects
    CBioseq_set(NCBI_NS_NCBI::CObject::ECanDelete);

};


#endif // OBJECTS_SEQSET_BIOSEQ_SET_HPP
