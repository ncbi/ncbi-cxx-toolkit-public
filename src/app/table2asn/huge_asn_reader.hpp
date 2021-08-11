/*  $Id$
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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*
*
*/

#ifndef _HUGE_ASN_READER_HPP_INCLUDED_
#define _HUGE_ASN_READER_HPP_INCLUDED_

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include "huge_file.hpp"

BEGIN_NCBI_SCOPE

class CObjectIStream;
namespace objects
{
    class CSeq_id;
    class CBioseq;
    class CSeq_submit;
    class CSeq_descr;
}

struct CRefLess
{
    bool operator()(const CConstRef<objects::CSeq_id>& l, const CConstRef<objects::CSeq_id>& r) const;
};

class CHugeAsnReader: public IHugeAsnSource
{
public:
    using TFileSize = std::streamoff;

    CHugeAsnReader();
    CHugeAsnReader(CHugeFile* file, objects::ILineErrorListener * pMessageListener);
    virtual ~CHugeAsnReader();

    void Open(CHugeFile* file, objects::ILineErrorListener * pMessageListener) override;
    bool GetNextBlob() override;
    CRef<objects::CSeq_entry> GetNextSeqEntry() override;
    CConstRef<objects::CSeq_submit> GetSubmit() override { return m_submit; };

    struct TBioseqInfo;
    struct TBioseqSetInfo;
    using TBioseqSetIndex = std::list<TBioseqSetInfo>;
    using TBioseqList  = std::list<TBioseqInfo>;

    struct TBioseqInfo
    {
        TFileSize m_pos;
        TBioseqSetIndex::iterator m_parent_set;
        TSeqPos   m_length  = -1;
    };

    struct TBioseqSetInfo
    {
        TFileSize m_pos;
        TBioseqSetIndex::iterator m_parent_set;
        objects::CBioseq_set::TClass m_class = objects::CBioseq_set::eClass_not_set;
        CRef<objects::CSeq_descr> m_descr;
    };

    using TBioseqIndex = std::map<CConstRef<objects::CSeq_id>, TBioseqList::iterator, CRefLess>;

    TBioseqList& GetBioseqs() { return m_bioseq_list; };
    TBioseqSetIndex& GetBiosets() { return m_bioseq_set_index; };
    auto GetFormat() const { return m_file->m_format; };
    auto GetMaxLocalId() const { return m_max_local_id; };

    // These metods are for CDataLoader, each top object is a 'blob'
    size_t FindTopObject(CConstRef<objects::CSeq_id> seqid) const;
    //CRef<objects::CSeq_entry> LoadSeqEntry(size_t id) const;
    CRef<objects::CSeq_entry> LoadSeqEntry(const TBioseqSetInfo& info) const;

    // Direct loading methods
    CRef<objects::CSeq_entry> LoadSeqEntry(CConstRef<objects::CSeq_id> seqid) const;
    CRef<objects::CBioseq> LoadBioseq(CConstRef<objects::CSeq_id> seqid) const;

    virtual void FlattenGenbankSet();

    void PrintAllSeqIds() const;
    bool IsMultiSequence() override { return m_flattened.size()>1; }
protected:
private:
    void x_ResetIndex();
    void x_IndexNextAsn1();

    objects::ILineErrorListener * mp_MessageListener = nullptr;
    CHugeFile*  m_file = nullptr;
    size_t      m_total_seqs = 0;
    size_t      m_total_sets = 0;
    int         m_max_local_id = 0;
    std::streampos m_streampos = 0;

    TBioseqList m_bioseq_list;
    TBioseqIndex m_bioseq_index;
    TBioseqSetIndex m_bioseq_set_index;
    CConstRef<objects::CSeq_submit> m_submit;

    unique_ptr<CObjectIStream> x_MakeObjStream(TFileSize pos) const;
    TBioseqSetIndex m_flattened;
    TBioseqSetIndex::iterator m_current;
};

END_NCBI_SCOPE

#endif // _HUGE_ASN_READER_HPP_INCLUDED_
