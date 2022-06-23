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
#include <corelib/ncbiutil.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objtools/edit/huge_file.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_id;
class CBioseq;
class CSeq_submit;
class CSeq_descr;

BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CHugeAsnReader: public IHugeAsnSource
{
public:
    using TFileSize = std::streamoff;

    CHugeAsnReader();
    CHugeAsnReader(CHugeFile* file, ILineErrorListener * pMessageListener);
    virtual ~CHugeAsnReader();

    void Open(CHugeFile* file, ILineErrorListener * pMessageListener) override;
    bool GetNextBlob() override;
    CRef<CSeq_entry> GetNextSeqEntry() override;
    CConstRef<CSubmit_block> GetSubmitBlock() override { return m_submit_block; };
    CRef<CObject> ReadAny();

    struct TBioseqInfo;
    struct TBioseqSetInfo;
    using TBioseqSetList = std::list<TBioseqSetInfo>;
    using TBioseqList  = std::list<TBioseqInfo>;

    struct TBioseqInfo
    {
        TFileSize m_pos;
        TBioseqSetList::const_iterator m_parent_set;
        TSeqPos   m_length  = -1;
        CConstRef<CSeq_descr> m_descr;
        std::list<CConstRef<CSeq_id>> m_ids;
        CSeq_inst::TMol m_mol = CSeq_inst::eMol_not_set;
    };

    struct TBioseqSetInfo
    {
        TFileSize m_pos;
        TBioseqSetList::const_iterator m_parent_set;
        CBioseq_set::TClass m_class = CBioseq_set::eClass_not_set;
        CConstRef<CSeq_descr> m_descr;
    };


    using CRefLess = PPtrLess<CConstRef<CSeq_id>>;


    using TBioseqIndex = std::map<CConstRef<CSeq_id>, TBioseqList::const_iterator, CRefLess>;
    using TBioseqSetIndex = std::map<CConstRef<CSeq_id>, TBioseqSetList::const_iterator, CRefLess>;

    auto& GetBioseqs() const { return m_bioseq_list; };
    auto& GetBiosets() const { return m_bioseq_set_list; };
    auto GetFormat() const { return m_file->m_format; };
    auto GetMaxLocalId() const { return m_max_local_id; };

    // These metods are for CDataLoader, each top object is a 'blob'
    const TBioseqSetInfo* FindTopObject(CConstRef<CSeq_id> seqid) const;
    CRef<CSeq_entry> LoadSeqEntry(const TBioseqSetInfo& info) const;
    const TBioseqInfo* FindBioseq(CConstRef<CSeq_id> seqid) const;

    // Direct loading methods
    CRef<CSeq_entry> LoadSeqEntry(CConstRef<CSeq_id> seqid) const;
    CRef<CBioseq> LoadBioseq(CConstRef<CSeq_id> seqid) const;

    bool IsMultiSequence() override { return m_bioseq_index.size()>1; }

    void FlattenGenbankSet();
    CConstRef<CSeq_entry> GetTopEntry() const { return m_top_entry; }
    auto& GetFlattenedIndex() const { return m_FlattenedIndex; }
    auto& GetTopIds() const { return m_top_ids; }
    unique_ptr<CObjectIStream> MakeObjStream(TFileSize pos) const;
protected:
private:
    void x_ResetIndex();
    void x_IndexNextAsn1();

    ILineErrorListener * mp_MessageListener = nullptr;
    std::streampos m_streampos = 0;
    CHugeFile*     m_file = nullptr;
    int            m_max_local_id = 0;

// global lists, readonly after indexing
    TBioseqList               m_bioseq_list;
    TBioseqSetList            m_bioseq_set_list;
    CConstRef<CSubmit_block>  m_submit_block;

// flattenization structures, readonly after flattenization, accept m_Current
    TBioseqIndex              m_bioseq_index;
    TBioseqSetIndex           m_FlattenedIndex;
    CConstRef<CSeq_entry>     m_top_entry;
    std::list<CConstRef<CSeq_id>> m_top_ids;
    TBioseqSetList            m_FlattenedSets;
    TBioseqSetList::const_iterator  m_Current;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _HUGE_ASN_READER_HPP_INCLUDED_
