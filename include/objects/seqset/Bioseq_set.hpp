// This is generated file, you may modify it freely
#ifndef OBJECTS_SEQSET_BIOSEQ_SET_HPP
#define OBJECTS_SEQSET_BIOSEQ_SET_HPP

// standard includes
#include <corelib/ncbistd.hpp>

// generated includes
#include <objects/seqset/Bioseq_set_Base.hpp>

// generated classes
class Bioseq_set : public Bioseq_set_Base
{
public:
    Bioseq_set();
    ~Bioseq_set();

    typedef NCBI_NS_STD::list< ::Seq_entry* > TSeqEntries;

    const TSeqEntries& GetSeq_set(void) const
        { return m_Seq_set; }
};

#endif // OBJECTS_SEQSET_BIOSEQ_SET_HPP
