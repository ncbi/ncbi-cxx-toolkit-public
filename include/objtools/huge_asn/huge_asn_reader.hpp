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
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objtools/edit/huge_file.hpp>
#include <optional>
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CBioseq;
class CSeq_submit;
class CSeqdesc;
class CSeq_annot;

BEGIN_SCOPE(edit)


class NCBI_XHUGEASN_EXPORT CHugeAsnReader:
    public IHugeAsnSource,
    public CObject
{
public:
    using TFileSize = std::streamoff;

    CHugeAsnReader();
    CHugeAsnReader(CHugeFile* file, ILineErrorListener * pMessageListener);
    virtual ~CHugeAsnReader();

    void Open(CHugeFile* file, ILineErrorListener * pMessageListener) override;
    bool GetNextBlob() override;
    CRef<CSeq_entry> GetNextSeqEntry() override;
    CConstRef<CSubmit_block> GetSubmitBlock() const override;
    bool IsNotJustLocalOrGeneral() const;
    bool HasRefSeq() const;
    CRef<CSerialObject> ReadAny();

    struct TBioseqInfo;
    struct TBioseqSetInfo;
    using TBioseqSetList = std::list<TBioseqSetInfo>;
    using TBioseqList  = std::list<TBioseqInfo>;

    struct TBioseqInfo
    {
        TFileSize m_pos;
        TBioseqSetList::const_iterator m_parent_set;
        TSeqPos   m_length  = static_cast<TSeqPos>(-1);
        CConstRef<CSeq_descr> m_descr{};
        std::list<CConstRef<CSeq_id>> m_ids;
        CSeq_inst::TMol m_mol = CSeq_inst::eMol_not_set;
        CSeq_inst::TRepr m_repr = CSeq_inst::eRepr_not_set;
    };

    struct TBioseqSetInfo
    {
        TFileSize m_pos;
        TBioseqSetList::const_iterator m_parent_set;
        CBioseq_set::TClass m_class = CBioseq_set::eClass_not_set;
        CConstRef<CSeq_descr> m_descr{};
        TFileSize m_annot_pos{0};
        std::optional<int> m_Level = nullopt;
        bool HasAnnot() const { return m_annot_pos != 0; }
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

    enum class eAddTopEntry{ yes, no };
    virtual CRef<CSeq_entry> LoadSeqEntry(const TBioseqSetInfo& info, eAddTopEntry add_top_entry = eAddTopEntry::yes) const;

    const TBioseqInfo* FindBioseq(CConstRef<CSeq_id> seqid) const;
    CConstRef<CSeqdesc> GetClosestDescriptor(const TBioseqInfo& info, CSeqdesc::E_Choice choice) const;
    CConstRef<CSeqdesc> GetClosestDescriptor(const CSeq_id& id, CSeqdesc::E_Choice choice) const;

    // Direct loading methods
    CRef<CSeq_entry> LoadSeqEntry(CConstRef<CSeq_id> seqid) const;
    CRef<CBioseq> LoadBioseq(CConstRef<CSeq_id> seqid) const;

    bool IsMultiSequence() const override;
    bool HasHugeSetAnnot() const { return m_HasHugeSetAnnot; }
    static bool IsHugeSet(CBioseq_set::TClass setClass);


    virtual void FlattenGenbankSet();
    auto& GetTopEntry()       const { return m_top_entry; }
    auto& GetFlattenedIndex() const { return m_FlattenedIndex; }
    auto& GetTopIds()         const { return m_top_ids; }
    unique_ptr<CObjectIStream> MakeObjStream(TFileSize pos) const;

    const CBioseq_set::TClass* GetTopLevelClass() const;

    using t_more_hooks = std::function<void(CObjectIStream&)>;
    void ExtendReadHooks(t_more_hooks hooks);
    void ResetTopEntry();
    bool HasLoneProteins() const;
    bool HasNestedGenbankSets() const;

    using TSeqIdTypes = ct::const_bitset<CSeq_id::e_MaxChoice, CSeq_id::E_Choice>;
    const TSeqIdTypes& GetSeqIdTypes() const { return m_seq_id_types; }

protected:
    // temporary structure for indexing
    struct TBioseqInfoRec
    {
        list<CConstRef<CSeq_id>> m_ids;
        TSeqPos          m_length  = 0;
        CRef<CSeq_descr> m_descr{};
        CSeq_inst::TMol  m_mol = CSeq_inst::eMol_not_set;
        CSeq_inst::TRepr m_repr = CSeq_inst::eRepr_not_set;
    };

    struct TContext
    {
        std::deque<TBioseqInfoRec> bioseq_stack;
        std::deque<TBioseqSetList::iterator> bioseq_set_stack;
    };

    virtual void x_SetHooks(CObjectIStream& objStream, TContext& context);
    virtual void x_SetFeatIdHooks(CObjectIStream& objStream);
    virtual void x_SetBioseqHooks(CObjectIStream& objStream, TContext& context);
    virtual void x_SetBioseqSetHooks(CObjectIStream& objStream, TContext& context);

    using TStreamPos = streampos;
    TStreamPos GetCurrentPos() const;

private:
    void x_ResetIndex();
    void x_IndexNextAsn1();
    void x_ThrowDuplicateId(
        const TBioseqSetInfo& existingInfo,const TBioseqSetInfo& newInfo, const CSeq_id& duplicateId);

    std::tuple<CRef<CSeq_descr>, std::list<CRef<CSeq_annot>>> x_GetTopLevelDescriptors() const;

    ILineErrorListener *    mp_MessageListener = nullptr;
    TStreamPos              m_current_pos      = 0; // points to current blob in concatenated ASN.1 file
    CRef<CHugeFile>         m_file;
    std::list<t_more_hooks> m_more_hooks;

// global lists, readonly after indexing
protected:
    TBioseqList                     m_bioseq_list;
    TStreamPos                      m_next_pos         = 0; // points to next unprocessed blob in concatenated ASN.1 file
    int                             m_max_local_id     = 0;
    TBioseqSetList                  m_bioseq_set_list;
    CRef<CSeq_entry>                m_top_entry;
    std::list<CConstRef<CSeq_id>>   m_top_ids;
    bool m_HasHugeSetAnnot{ false };
private:
    CConstRef<CSubmit_block>  m_submit_block;
    TSeqIdTypes               m_seq_id_types;

// flattenization structures, readonly after flattenization, accept m_Current
    TBioseqIndex              m_bioseq_index;
    TBioseqSetIndex           m_FlattenedIndex;
    TBioseqSetList            m_FlattenedSets;
    TBioseqSetList::const_iterator  m_Current;
    const CBioseq_set::TClass* m_pTopLevelClass { nullptr };
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _HUGE_ASN_READER_HPP_INCLUDED_
