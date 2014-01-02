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
 * Author: Aleksey Grichenko
 *
 * File Description:  LDS v.2 data manager.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/stream_utils.hpp>
#include <util/checksum.hpp>
#include <util/format_guess.hpp>
#include <db/sqlite/sqlitewrapp.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/seqalign_exception.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/lds2/lds2.hpp>
#include <objtools/lds2/lds2_expt.hpp>
#include <set>
#include <map>
#include <stack>


#define NCBI_USE_ERRCODE_X Objtools_LDS2

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Hooks and helper classes for indexing data files

typedef CLDS2_Database::TSeqIdSet TSeqIdSet;
typedef SLDS2_AnnotIdInfo::TRange TAnnotRange;

class CLDS2_ObjectParser
{
public:
    typedef SLDS2_File::TFormat TFormat;

    CLDS2_ObjectParser(CLDS2_Manager&   mgr,
                       Int8             file_id,
                       TFormat          format,
                       CNcbiIstream&    in,
                       CLDS2_Database&  db);
    ~CLDS2_ObjectParser(void) {}

    // Try to parse the next blob, return true on success
    bool ParseNext(SLDS2_Blob::EBlobType blob_type = SLDS2_Blob::eUnknown);

    void AddAnnot(SLDS2_Annot::EType         annot_type,
                  const string*              annot_name,
                  const SLDS2_Annot::TIdMap& ids);

    void AddBioseq(const TSeqIdSet& ids);

    void BeginBlob(void);
    void ResetBlob(void);
    // Set current blob position relative to the last blob position
    void SetBlobOffset(Int8 pos) {
        m_CurBlobPos = m_LastBlobPos + pos;
    }

    // Effective blob type may be different from the real top level blob
    // (e.g. when splitting a bioseq-set).
    void EndBlob(SLDS2_Blob::EBlobType blob_type);

    void SetGBBioseqSet(bool value) {
        m_IsGBBioseqSet = value;
    }
    // Check if it's necessary to split the current bioseq-set (if any)
    bool GetSplitBioseqSet(void) const {
        if (m_BlobType != SLDS2_Blob::eBioseq_set) {
            return false;
        }
        if (m_Manager.GetGBReleaseMode() == CLDS2_Manager::eGB_Ignore) {
            return false;
        }
        if (m_Manager.GetGBReleaseMode() == CLDS2_Manager::eGB_Force) {
            return true;
        }
        // Guess
        return m_IsGBBioseqSet;
    }

    int GetSeqAlignGroupSize(void) const {
        return m_Manager.GetSeqAlignGroupSize();
    }

private:
    static TTypeInfo sx_GetObjectTypeInfo(SLDS2_Blob::EBlobType blob_type);

    SLDS2_Blob::EBlobType x_GetBlobType(void);

    typedef CLDS2_Database::TLDS2Annots TAnnots;

    struct SBioseqInfo {
        TSeqIdSet   ids;
    };
    typedef vector< AutoPtr<SBioseqInfo> > TBioseqs;

    CLDS2_Manager&           m_Manager;
    CNcbiIstream&            m_Stream;
    CLDS2_Database&          m_Db;

    Int8                     m_CurFileId;
    ESerialDataFormat        m_Format;
    Int8                     m_CurBlobPos;
    Int8                     m_LastBlobPos; // count bytes already read
    SLDS2_Blob::EBlobType    m_BlobType;
    SLDS2_Blob::EBlobType    m_LastBlobType;

    TSeqIdSet                m_BioseqIds;
    TAnnots                  m_Annots;
    TBioseqs                 m_Bioseqs;

    // Splitting GB releases
    bool                     m_IsGBBioseqSet;
};


// Hook for reading seq-ids.
class CLDS2_Seq_id_Hook : public CSkipObjectHook
{
public:
    CLDS2_Seq_id_Hook(void)
        : m_Seq_id(new CSeq_id)
    {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

    // Start collecting seq-ids to a new set.
    void PushSet(TSeqIdSet& ids) {
        m_Stack.push(&ids);
    }

    // Remove currently used set from the stack.
    void PopSet(void) {
        if ( !m_Stack.empty() ) {
            m_Stack.pop();
        }
    }

    // Guard for collecting seq-ids. Will automatically release
    // the seq-id set upon destruction.
    class CGuard
    {
    public:
        CGuard(CLDS2_Seq_id_Hook& hook, TSeqIdSet& ids)
            : m_Hook(hook)
        {
            m_Hook.PushSet(ids);
        }
        ~CGuard(void)
        {
            m_Hook.PopSet();
        }

    private:
        CLDS2_Seq_id_Hook& m_Hook;

        CGuard(const CGuard&);
        void operator=(const CGuard&);
    };

private:
    typedef stack<TSeqIdSet*> TIdStack;

    CRef<CSeq_id>  m_Seq_id;
    TIdStack       m_Stack;
};


void CLDS2_Seq_id_Hook::SkipObject(CObjectIStream& in,
                                   const CObjectTypeInfo& info)
{
    DefaultRead(in, ObjectInfo(*m_Seq_id));
    if ( m_Stack.empty() ) {
        return;
    }
    m_Stack.top()->insert(CSeq_id_Handle::GetHandle(*m_Seq_id));
}


// Hook for detecting seq-annot type. Reads types of all
// annotations (they must all be the same).
// This hook also collects seq-ids and ranges for feats,
// aligns and graphs.
class CLDS2_AnnotType_Hook : public CSkipObjectHook
{
public:
    CLDS2_AnnotType_Hook(void) : m_Ids(0) {}
    ~CLDS2_AnnotType_Hook(void) {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

    void ResetAnnot(SLDS2_Annot::TIdMap* ids)
    {
        m_Type.clear();
        m_Ids = ids;
    }

    const string& GetType(void) const { return m_Type; }

private:
    string               m_Type;
    SLDS2_Annot::TIdMap* m_Ids;
};


void CLDS2_AnnotType_Hook::SkipObject(CObjectIStream& in,
                                      const CObjectTypeInfo& info)
{
    _ASSERT(m_Type.empty()  ||  m_Type == info.GetName());
    m_Type = info.GetName();
    // Parse feats, aligns and graphs here. For other annot types only ids
    // will be collected.
    if (m_Ids  &&  m_Type == "Seq-feat") {
        CSeq_feat feat;
        DefaultRead(in, ObjectInfo(feat));
        ITERATE(CSeq_loc, it, feat.GetLocation()) {
            (*m_Ids)[it.GetSeq_id_Handle()].range.CombineWith(it.GetRange());
        }
        if ( feat.IsSetProduct() ) {
            ITERATE(CSeq_loc, it, feat.GetProduct()) {
                (*m_Ids)[it.GetSeq_id_Handle()].range.CombineWith(it.GetRange());
            }
        }
    }
    else if (m_Ids  &&  m_Type == "Seq-align") {
        CSeq_align align;
        DefaultRead(in, ObjectInfo(align));
        // Hack: if dim is not set, iterate rows until CSeqalignException
        // is thrown.
        CSeq_align::TDim dim = align.IsSetDim() ? align.GetDim() : 0;
        try {
            for (CSeq_align::TDim row = 0; row < dim  ||  dim == 0; row++) {
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(row));
                (*m_Ids)[idh].range.CombineWith(align.GetSeqRange(row));
            }
        }
        catch (CSeqalignException) {
        }
    }
    else if (m_Ids  &&  m_Type == "Seq-graph") {
        CSeq_graph graph;
        DefaultRead(in, ObjectInfo(graph));
        ITERATE(CSeq_loc, it, graph.GetLoc()) {
            (*m_Ids)[it.GetSeq_id_Handle()].range.CombineWith(it.GetRange());
        }
    }
    else {
        DefaultSkip(in, info);
    }
}


class CLDS2_AnnotDesc_Hook : public CSkipObjectHook
{
public:
    CLDS2_AnnotDesc_Hook(void) : m_Named(false) {}
    ~CLDS2_AnnotDesc_Hook(void) {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

    void ResetName(void)
    {
        m_Named = false;
        m_Name.clear();
    }
    bool IsNamed(void) const { return m_Named; }
    const string& GetName(void) const { return m_Name; }

private:
    bool   m_Named;
    string m_Name;
};


void CLDS2_AnnotDesc_Hook::SkipObject(CObjectIStream& in,
                                      const CObjectTypeInfo& info)
{
    if ( m_Named ) {
        // Ignore multiple names
        DefaultSkip(in, info);
    }
    else {
        CAnnotdesc desc;
        DefaultRead(in, ObjectInfo(desc));
        if ( desc.IsName() ) {
            m_Named = true;
            m_Name = desc.GetName();
        }
    }
}


// Hook for collecting annotations.
class CLDS2_Annot_Hook : public CSkipObjectHook
{
public:
    // id_hook and type_hook must be already attached to the input
    // stream.
    CLDS2_Annot_Hook(CLDS2_ObjectParser&   parser,
                     CLDS2_Seq_id_Hook&    id_hook,
                     CLDS2_AnnotDesc_Hook& desc_hook,
                     CLDS2_AnnotType_Hook& type_hook)
        : m_Parser(parser),
          m_IdHook(id_hook),
          m_DescHook(desc_hook),
          m_TypeHook(type_hook)
    {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

private:
    CLDS2_ObjectParser&    m_Parser;
    CLDS2_Seq_id_Hook&     m_IdHook;
    CLDS2_AnnotDesc_Hook&  m_DescHook;
    CLDS2_AnnotType_Hook&  m_TypeHook;
    TSeqIdSet              m_IdSet;
    SLDS2_Annot::TIdMap    m_IdMap;
};


void CLDS2_Annot_Hook::SkipObject(CObjectIStream& in,
                                  const CObjectTypeInfo& info)
{
    m_IdSet.clear();
    m_IdMap.clear();
    CLDS2_Seq_id_Hook::CGuard guard(m_IdHook, m_IdSet);

    m_DescHook.ResetName();
    // annot-type hook may be not empty, e.g. after reading seq-hist
    // with alignments in it. Reset it now.
    m_TypeHook.ResetAnnot(&m_IdMap);

    DefaultSkip(in, info);

    SLDS2_Annot::EType annot_type = SLDS2_Annot::eUnknown;

    if (m_TypeHook.GetType() == "Seq-feat") {
        annot_type = SLDS2_Annot::eSeq_feat;
    }
    else if (m_TypeHook.GetType() == "Seq-align") {
        annot_type = SLDS2_Annot::eSeq_align;
    }
    else if (m_TypeHook.GetType() == "Seq-graph") {
        annot_type = SLDS2_Annot::eSeq_graph;
    }
    ITERATE(TSeqIdSet, id, m_IdSet) {
        // If an id was not included in the range map, add the whole sequence.
        if (m_IdMap.find(*id) == m_IdMap.end()) {
            m_IdMap[*id].range = TAnnotRange::GetWhole();
        }
    }
    m_IdSet.clear();
    m_TypeHook.ResetAnnot(0);

    m_Parser.AddAnnot(
        annot_type,
        m_DescHook.IsNamed() ? &m_DescHook.GetName() : NULL,
        m_IdMap);
}


// Hook for splitting a bioseq-set into seq-entries.
class CLDS2_SeqEntry_Hook : public CSkipObjectHook
{
public:
    // id_hook and type_hook must be already attached to the input
    // stream.
    CLDS2_SeqEntry_Hook(CLDS2_ObjectParser& parser)
        : m_Parser(parser),
          m_Nested(false)
    {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

private:
    CLDS2_ObjectParser& m_Parser;
    bool                m_Nested;
};


void CLDS2_SeqEntry_Hook::SkipObject(CObjectIStream& in,
                                     const CObjectTypeInfo& info)
{
    // Any annotations attached to the top level bioseq-set are discarded.
    // Probably, there should be no such annotations in GB releases.
    if ( !m_Nested  &&  m_Parser.GetSplitBioseqSet() ) {
        m_Nested = true;
        m_Parser.SetBlobOffset(NcbiStreamposToInt8(in.GetStreamPos()));
        m_Parser.BeginBlob();
        DefaultSkip(in, info);
        m_Parser.EndBlob(SLDS2_Blob::eBioseq_set_element);
        m_Nested = false;
    }
    else {
        DefaultSkip(in, info);
    }
}


// Hook for checking class and descr of a bioseq-set.
// 
class CLDS2_BioseqSet_Hook : public CSkipClassMemberHook
{
public:
    CLDS2_BioseqSet_Hook(CLDS2_ObjectParser&   parser)
        : m_Parser(parser),
          m_Class(CBioseq_set::eClass_not_set),
          m_HaveDescr(false),
          m_Nested(false)
    {}

    virtual void SkipClassMember(CObjectIStream& in,
                                 const CObjectTypeInfoMI& info);

private:
    CLDS2_ObjectParser& m_Parser;
    CBioseq_set::EClass m_Class;
    bool                m_HaveDescr;
    bool                m_Nested;
};


void CLDS2_BioseqSet_Hook::SkipClassMember(CObjectIStream& in,
                                           const CObjectTypeInfoMI& info)
{
    if ( m_Nested ) {
        DefaultSkip(in, info);
        return;
    }
    // This code assumes that 'class' and 'descr' members are read
    // before 'seq-set'. If this is not true, most hooks must be
    // re-written so that the whole bioseq-set is read (not skipped),
    // it's properties checked and the set is split depending on the
    // results.
    if (info.GetMemberInfo()->GetId().GetName() == "class") {
        CObjectTypeInfo obj_ti(info.GetItemInfo()->GetTypeInfo());
        m_Class = CBioseq_set::EClass(
            in.ReadEnum(obj_ti.GetEnumeratedTypeValues()));
    }
    else if (info.GetMemberInfo()->GetId().GetName() == "descr") {
        DefaultSkip(in, info);
        m_HaveDescr = true;
    }
    else if (info.GetMemberInfo()->GetId().GetName() == "seq-set") {
        // Set GB release flag in the parser.
        m_Parser.SetGBBioseqSet(
            (m_Class == CBioseq_set::eClass_not_set ||
            m_Class == CBioseq_set::eClass_genbank)
            &&
            !m_HaveDescr);
        m_Nested = true;
        DefaultSkip(in, info);
        m_Nested = false;
    }
    else {
        DefaultSkip(in, info);
    }
}


// Hook for collecting bioseq ids -- allows to ignore seq-ids
// from other parts of bioseq (inst segments, history etc.)
class CLDS2_BioseqIds_Hook : public CSkipClassMemberHook
{
public:
    CLDS2_BioseqIds_Hook(CLDS2_ObjectParser&   parser,
                         CLDS2_Seq_id_Hook&    id_hook)
        : m_Parser(parser),
          m_IdHook(id_hook)
    {}

    virtual void SkipClassMember(CObjectIStream& in,
                                 const CObjectTypeInfoMI& info);

private:
    CLDS2_ObjectParser&   m_Parser;
    CLDS2_Seq_id_Hook&    m_IdHook;
    TSeqIdSet             m_Ids;
};


void CLDS2_BioseqIds_Hook::SkipClassMember(CObjectIStream& in,
                                           const CObjectTypeInfoMI& info)
{
    m_Ids.clear();
    CLDS2_Seq_id_Hook::CGuard guard(m_IdHook, m_Ids);

    DefaultSkip(in, info);

    m_Parser.AddBioseq(m_Ids);
}


CLDS2_ObjectParser::CLDS2_ObjectParser(CLDS2_Manager&   mgr,
                                       Int8             file_id,
                                       TFormat          format,
                                       CNcbiIstream&    in,
                                       CLDS2_Database&  db)
    : m_Manager(mgr),
      m_Stream(in),
      m_Db(db),
      m_CurFileId(file_id),
      m_Format(eSerial_None),
      m_CurBlobPos(0),
      m_LastBlobPos(0),
      m_BlobType(SLDS2_Blob::eUnknown),
      m_LastBlobType(SLDS2_Blob::eUnknown),
      m_IsGBBioseqSet(false)
{
    switch ( format ) {
    case CFormatGuess::eBinaryASN:
        m_Format = eSerial_AsnBinary;
        break;
    case CFormatGuess::eTextASN:
        m_Format = eSerial_AsnText;
        break;
    case CFormatGuess::eXml:
        m_Format = eSerial_Xml;
        break;
    default:
        break;
    }
    ResetBlob();
}


TTypeInfo
CLDS2_ObjectParser::sx_GetObjectTypeInfo(SLDS2_Blob::EBlobType blob_type)
{
    switch ( blob_type ) {
    case SLDS2_Blob::eSeq_entry:
        return CType<CSeq_entry>().GetTypeInfo();
    case SLDS2_Blob::eBioseq:
    case SLDS2_Blob::eBioseq_set_element:
        return CType<CBioseq>().GetTypeInfo();
    case SLDS2_Blob::eBioseq_set:
        return CType<CBioseq_set>().GetTypeInfo();
    case SLDS2_Blob::eSeq_annot:
        return CType<CSeq_annot>().GetTypeInfo();
    case SLDS2_Blob::eSeq_align_set:
        return CType<CSeq_align_set>().GetTypeInfo();
    case SLDS2_Blob::eSeq_align:
        return CType<CSeq_align>().GetTypeInfo();
    case SLDS2_Blob::eSeq_submit:
        return CType<CSeq_submit>().GetTypeInfo();
    default:
        break;
    }
    return NULL;
}


void CLDS2_ObjectParser::BeginBlob(void)
{
    // When starting a new blob, the previous one must be finished and reset.
    if (!m_BioseqIds.empty()  ||
        !m_Annots.empty()  ||
        !m_Bioseqs.empty()) {
        LDS2_THROW(eIndexerError, "Unfinished blob in the data file.");
    }
}


void CLDS2_ObjectParser::ResetBlob(void)
{
    m_BioseqIds.clear();
    m_Annots.clear();
    m_Bioseqs.clear();
}


static const SLDS2_Blob::EBlobType kExpectedBlobTypes[] = {
    SLDS2_Blob::eSeq_entry,
    SLDS2_Blob::eBioseq,
    SLDS2_Blob::eBioseq_set,
    SLDS2_Blob::eSeq_annot,
    SLDS2_Blob::eSeq_align_set,
    SLDS2_Blob::eSeq_align,
    SLDS2_Blob::eSeq_submit
};


SLDS2_Blob::EBlobType CLDS2_ObjectParser::x_GetBlobType(void)
{
    SLDS2_Blob::EBlobType ret = SLDS2_Blob::eUnknown;

    // Read some data - just to detect the object type.
    // The data will be pushed back after testing.
    char buf[1024];
    m_Stream.read(buf, 1024);
    int sz = (int)m_Stream.gcount();
    m_Stream.clear();
    if (sz == 0) {
        // The stream is empty - nothing to check. Force eof detection.
        m_Stream.peek();
        return ret;
    }
    CNcbiIstrstream test_str(buf, sz);
    auto_ptr<CObjectIStream> test_in(CObjectIStream::Open(
        m_Format, test_str));
    switch ( m_Format ) {
    case eSerial_AsnText:
    case eSerial_Xml:
        {
            // Read type name, set the blob type
            string obj_type = test_in->ReadFileHeader();
            if (obj_type == "Seq-entry") {
                ret = SLDS2_Blob::eSeq_entry;
            }
            else if (obj_type == "Bioseq") {
                ret = SLDS2_Blob::eBioseq;
            }
            else if (obj_type == "Bioseq-set") {
                ret = SLDS2_Blob::eBioseq_set;
            }
            else if (obj_type == "Seq-annot") {
                ret = SLDS2_Blob::eSeq_annot;
            }
            else if (obj_type == "Seq-align-set") {
                ret = SLDS2_Blob::eSeq_align_set;
            }
            else if (obj_type == "Seq-align") {
                ret = SLDS2_Blob::eSeq_align;
            }
            else if (obj_type == "Seq-submit") {
                ret = SLDS2_Blob::eSeq_submit;
            }
            break;
        }
    case eSerial_AsnBinary:
        {
            int num_types = sizeof(kExpectedBlobTypes)/
                sizeof(kExpectedBlobTypes[0]);
            for (int i = 0; i < num_types; i++) {
                ret = kExpectedBlobTypes[i];
                try {
                    TTypeInfo type_info = sx_GetObjectTypeInfo(ret);
                    test_in->Skip(type_info);
                    break;
                }
                catch (CEofException) {
                    // Not enough data in the test stream, but the blob type
                    // is correct.
                    break;
                }
                catch (CSerialException ex) {
                    if (ex.GetErrCode() == CSerialException::eEOF) {
                        // Same as CEofException
                        break;
                    }
                    // Reset blob type
                    ret = SLDS2_Blob::eUnknown;
                }
                test_in->SetStreamPos(0); // rewind
            }
            break;
        }
    default:
        break;
    }
    CStreamUtils::Stepback(m_Stream, buf, sz);
    return ret;
}


void CLDS2_ObjectParser::EndBlob(SLDS2_Blob::EBlobType blob_type)
{
    if (blob_type == SLDS2_Blob::eUnknown) {
        return;
    }

    // Add blob to the database
    Int8 blob_id = m_Db.AddBlob(m_CurFileId, blob_type, m_CurBlobPos);

    // Add each bioseq to the database
    ITERATE(TBioseqs, it, m_Bioseqs) {
        // Check for seq-id conflicts
        if (m_Manager.GetDuplicateIdMode() !=
            CLDS2_Manager::eDuplicate_Store) {
            CSeq_id_Handle dup;
            ITERATE(TSeqIdSet, id, (*it)->ids) {
                // 0 - no such id yet
                // >0 - single id
                // -1 - conflict (multiple ids)
                if ( m_Db.GetBioseqId(*id) != 0) {
                    dup = *id;
                    break;
                }
            }
            if ( dup ) {
                // Remove from the list of known ids so that all
                // annotations become external (???).
                m_BioseqIds.erase(dup);
                if (m_Manager.GetDuplicateIdMode() ==
                    CLDS2_Manager::eDuplicate_Skip) {
                    ERR_POST_X(8, Warning <<
                        "Bioseq with duplicate seq-id found: " <<
                        dup.AsString() <<
                        " -- skipping.");
                    continue; // next bioseq
                }
                else {
                    LDS2_THROW(eDuplicateId,
                        "Bioseqs with duplicate seq-id found: " +
                        dup.AsString());
                }
            }
        }
        m_Db.AddBioseq(blob_id, (*it)->ids);
    }

    // Add annotations
    NON_CONST_ITERATE(TAnnots, it, m_Annots) {
        SLDS2_Annot& annot = **it;
        annot.blob_id = blob_id;
        NON_CONST_ITERATE(SLDS2_Annot::TIdMap, id, annot.ref_ids) {
            SLDS2_AnnotIdInfo& ref_id = id->second;
            ref_id.external = true;
            // If the blob can contain bioseqs, check if the annotation
            // is external. Each id has its own external flag.
            if (blob_type == SLDS2_Blob::eSeq_entry  ||
                blob_type == SLDS2_Blob::eBioseq ||
                blob_type == SLDS2_Blob::eBioseq_set  ||
                blob_type == SLDS2_Blob::eBioseq_set_element || 
                blob_type == SLDS2_Blob::eSeq_submit ) 
            {
                if (m_BioseqIds.find(id->first) != m_BioseqIds.end()) {
                    ref_id.external = false;
                }
            }
        }
        m_Db.AddAnnot(annot);
    }
    ResetBlob();
}


bool CLDS2_ObjectParser::ParseNext(SLDS2_Blob::EBlobType blob_type)
{
    // Make sure it's a supported format
    if (m_Format == eSerial_None) return false;

    if (blob_type == SLDS2_Blob::eUnknown) {
        if (m_LastBlobType != SLDS2_Blob::eUnknown) {
            // Try the last known type.
            if ( ParseNext(m_LastBlobType) ) {
                return true;
            }
        }
        // Peek the type if there's no last type or it is wrong.
        blob_type = x_GetBlobType();
    }
    m_BlobType = blob_type;
    if (m_BlobType == SLDS2_Blob::eUnknown) {
        return false;
    }

    BeginBlob();
    SetBlobOffset(0);

    // Prepare to load multiple seq-aligns into a single blob.
    int count = 0;
    while (m_BlobType == blob_type) {
        auto_ptr<CObjectIStream> objstr(CObjectIStream::Open(m_Format, m_Stream));

        // Hook for reading seq-ids and storing them to a collection.
        CLDS2_Seq_id_Hook id_hook;
        CObjectHookGuard<CSeq_id> seq_id_guard(id_hook, objstr.get());

        // Hook which enables reading seq-ids only while parsing bioseq.ids
        // member (to ignore seq-ids in other members like inst or hist).
        CLDS2_BioseqIds_Hook bioseq_ids_hook(*this, id_hook);
        CObjectHookGuard<CBioseq> bioseq_ids_guard(
            "id", bioseq_ids_hook, objstr.get());

        // Hook for detecting annotation type.
        CLDS2_AnnotType_Hook annot_type_hook;
        CObjectHookGuard<CSeq_feat> seq_feat_guard(annot_type_hook,
            objstr.get());
        CObjectHookGuard<CSeq_graph> seq_graph_guard(annot_type_hook,
            objstr.get());
        CObjectHookGuard<CSeq_align> seq_align_guard(annot_type_hook,
            objstr.get());

        CLDS2_AnnotDesc_Hook annot_desc_hook;
        CObjectHookGuard<CAnnotdesc> annot_desc_guard(annot_desc_hook,
            objstr.get());

        // Hook for indexing seq-annots. Uses annot_type_hook to check
        // seq-annot type.
        CLDS2_Annot_Hook annot_hook(*this,
            id_hook, annot_desc_hook, annot_type_hook);
        CObjectHookGuard<CSeq_annot> seq_annot_guard(annot_hook,
            objstr.get());

        // Hooks for checking bioseq-set class and descr members.
        // There presence and value may affect splitting bioseq-set
        // into seq-entries.
        CLDS2_BioseqSet_Hook bioseq_set_hook(*this);
        CObjectHookGuard<CBioseq_set> bioseq_set_class_guard(
            "class", bioseq_set_hook, objstr.get());
        CObjectHookGuard<CBioseq_set> bioseq_set_descr_guard(
            "descr", bioseq_set_hook, objstr.get());
        CObjectHookGuard<CBioseq_set> bioseq_set_seqset_guard(
            "seq-set", bioseq_set_hook, objstr.get());

        // Seq-entry hook is used to split bioseq-set into individual
        // seq-entries.
        CLDS2_SeqEntry_Hook entry_hook(*this);
        CObjectHookGuard<CSeq_entry> entry_guard(entry_hook, objstr.get());

        // Seq-id set used to collect seq-ids from top level seq-aligns
        // and seq-align-sets.
        SLDS2_Annot::TIdMap align_ids;
        bool is_align = m_BlobType == SLDS2_Blob::eSeq_align  ||
            m_BlobType == SLDS2_Blob::eSeq_align_set;
        if ( is_align ) {
            annot_type_hook.ResetAnnot(&align_ids);
        }

        try {
            TTypeInfo type_info = sx_GetObjectTypeInfo(m_BlobType);
            objstr->Skip(type_info);
            // Store seq-aligns and seq-align-sets as blobs
            if ( is_align ) {
                _ASSERT(annot_type_hook.GetType() == "Seq-align");
                AddAnnot(SLDS2_Annot::eSeq_align, NULL, align_ids);
                annot_type_hook.ResetAnnot(0);
            }
            m_LastBlobPos += NcbiStreamposToInt8(objstr->GetStreamPos());
            m_LastBlobType = m_BlobType;
        }
        catch (CSerialException) {
            ResetBlob();
            m_BlobType = SLDS2_Blob::eUnknown;
            return false;
        }

        // Loading multiple seq-aligns?
        if (m_BlobType != SLDS2_Blob::eSeq_align) {
            break;
        }
        try {
            // Check next object type. If at EOF, catch the exception and
            // save the collected aligns.
            objstr->Close(); // Pushback unparsed data if any.
            blob_type = x_GetBlobType();
        }
        catch (CEofException) {
            break;
        }
        // Collected enough aligns?
        if (++count >= GetSeqAlignGroupSize()) {
            break;
        }
    }
    // Do not store bioseq-set if it has been split and stored as
    // separate seq-entries.
    if ( !GetSplitBioseqSet() ) {
        EndBlob(m_BlobType);
    }
    SetGBBioseqSet(false); // reset GB release flag
    return m_BlobType != SLDS2_Blob::eUnknown;
}


void CLDS2_ObjectParser::AddAnnot(SLDS2_Annot::EType         annot_type,
                                  const string*              annot_name,
                                  const SLDS2_Annot::TIdMap& ids)
{
    SLDS2_Annot* annot = new SLDS2_Annot;
    annot->type = annot_type;
    annot->is_named = (annot_name != NULL);
    if ( annot_name ) {
        annot->name = *annot_name;
    }
    annot->ref_ids.insert(ids.begin(), ids.end());
    m_Annots.push_back(annot);
}


void CLDS2_ObjectParser::AddBioseq(const TSeqIdSet& ids)
{
    SBioseqInfo* info = new SBioseqInfo;
    info->ids.insert(ids.begin(), ids.end());
    m_Bioseqs.push_back(info);

    // Remember ids used in bioseqs.
    m_BioseqIds.insert(ids.begin(), ids.end());
}


// LDS2 Manager implementation

CLDS2_Manager::CLDS2_Manager(const string& db_file)
    : m_GBReleaseMode(eGB_Ignore),
      m_DupIdMode(eDuplicate_Store),
      m_ErrorMode(eError_Report),
      m_FastaFlags(CFastaReader::fAssumeNuc  |
                   CFastaReader::fAllSeqIds  |
                   CFastaReader::fOneSeq     |
                   CFastaReader::fNoSeqData  |
                   CFastaReader::fParseGaps  |
                   CFastaReader::fParseRawID),
      m_SeqAlignGroupSize(0)
{
    SetDbFile(db_file);
    // Initialize default handlers
    RegisterUrlHandler(new CLDS2_UrlHandler_File);
    RegisterUrlHandler(new CLDS2_UrlHandler_GZipFile);
}


CLDS2_Manager::~CLDS2_Manager(void)
{
}


void CLDS2_Manager::RegisterUrlHandler(CLDS2_UrlHandler_Base* handler)
{
    _ASSERT(handler);
    m_Handlers[handler->GetHandlerName()] = Ref(handler);
}


void CLDS2_Manager::SetDbFile(const string& db_file)
{
    m_Files.clear();
    m_Db.Reset(new CLDS2_Database(db_file));
}


void CLDS2_Manager::ResetData(void)
{
    m_Db->Create();
    m_Files.clear();
}


void CLDS2_Manager::AddDataFile(const string& data_file)
{
    // Use absolute paths. (?)
    m_Files.insert(CDirEntry::CreateAbsolutePath(data_file));
}


void CLDS2_Manager::AddDataDir(const string& data_dir, EDirMode mode)
{
    CDir dir(data_dir);
    CDir::TEntries subs = dir.GetEntries("*", CDir::fIgnoreRecursive);
    ITERATE(CDir::TEntries, it, subs) {
        const CDirEntry& sub = **it;
        if ( sub.IsDir()  &&  (mode == eDir_Recurse)) {
            AddDataDir(sub.GetPath(), mode);
        }
        else if ( sub.IsFile() ) {
            AddDataFile(sub.GetPath());
        }
    }
}


bool CLDS2_Manager::x_IsGZipFile(const SLDS2_File& file_info)
{
    auto_ptr<CNcbiIstream> in(new CNcbiIfstream(file_info.name.c_str(), ios::binary));
    return CFormatGuess::Format(*in) == CFormatGuess::eGZip;
}


CLDS2_UrlHandler_Base*
CLDS2_Manager::x_GetUrlHandler(const SLDS2_File& file_info)
{
    CLDS2_UrlHandler_Base* handler = NULL;
    string handler_name = CLDS2_UrlHandler_File::s_GetHandlerName();
    THandlersByUrl::const_iterator url_it = m_HandlersByUrl.find(file_info.name);
    if (url_it != m_HandlersByUrl.end()) {
        handler_name = url_it->second;
    }
    // Autodetect compressed files
    else if ( x_IsGZipFile(file_info) ) {
        handler_name = CLDS2_UrlHandler_GZipFile::s_GetHandlerName();
    }
    THandlers::iterator h_it = m_Handlers.find(handler_name);
    if (h_it == m_Handlers.end()) {
        // No such handler
        if (m_ErrorMode == eError_Throw) {
            LDS2_THROW(eIndexerError,
                "Can not find URL handler: " + url_it->second);
        }
        else if (m_ErrorMode == eError_Report) {
            ERR_POST_X(10, Error <<
                "Can not find URL handler: " + url_it->second);
        }
    }
    else {
        handler = h_it->second.GetPointerOrNull();
    }
    return handler;
}


SLDS2_File CLDS2_Manager::x_GetFileInfo(const string&                file_name,
                                        CRef<CLDS2_UrlHandler_Base>& handler)
{
    SLDS2_File info(file_name);

    // Find handler for the file or use the default one
    handler.Reset(x_GetUrlHandler(info));
    if ( handler ) {
        handler->FillInfo(info);
        // Make sure handler name is set
        info.handler = handler->GetHandlerName();
    }
    return info;
}


static bool IsSupportedFormat(CFormatGuess::EFormat format)
{
    switch ( format ) {
    case CFormatGuess::eBinaryASN:
    case CFormatGuess::eTextASN:
    case CFormatGuess::eXml:
    case CFormatGuess::eFasta:
        return true;
    default:
        break;
    }
    return false;
}


void CLDS2_Manager::UpdateData(void)
{
    if ( !CDirEntry(m_Db->GetDbFile()).Exists() ) {
        // Create the database if it does not exist yet.
        // This will also be triggered for ":memory:" database.
        m_Db->Create();
    }
    else {
        m_Db->Open();
    }

    // Add all known files from the DB
    m_Db->GetFileNames(m_Files);

    m_Db->BeginUpdate();
    ITERATE(TFiles, it, m_Files) {
        CRef<CLDS2_UrlHandler_Base> handler;
        SLDS2_File file_info = x_GetFileInfo(*it, handler);
        SLDS2_File db_info = m_Db->GetFileInfo(*it);
        if (!file_info.exists()  ||  !IsSupportedFormat(file_info.format)) {
            // the file does not exist
            if (db_info.id != 0) {
                // remove the file from the database
                m_Db->DeleteFile(db_info.id);
            }
            if ( file_info.exists() ) {
                // Unsupported format
                if (m_ErrorMode == eError_Throw) {
                    LDS2_THROW(eIndexerError,
                        "Unrecognized file format: " + *it);
                }
                else if (m_ErrorMode == eError_Report) {
                    ERR_POST_X(9, Error <<
                        "Unrecognized file format: " + *it);
                }
            }
            continue;
        }
        // By now the handler must be set.
        _ASSERT(handler);
        if (db_info.id == 0) {
            // new file
            m_Db->AddFile(file_info);
            x_ParseFile(file_info, *handler);
        }
        else {
            // existing file
            file_info.id = db_info.id;
            if (file_info != db_info) {
                m_Db->UpdateFile(file_info);
                x_ParseFile(file_info, *handler);
            }
        }
    }
    m_Db->EndUpdate();
}


void CLDS2_Manager::x_ParseFile(const SLDS2_File&      info,
                                CLDS2_UrlHandler_Base& handler)
{
    // Always open file as binary. Otherwise on Win32 file positions will
    // be invalid.
    auto_ptr<CNcbiIstream> in(handler.OpenStream(info, 0, m_Db));
    _ASSERT(in.get());
    int parsed_entries = 0;
    switch ( info.format ) {
    case CFormatGuess::eBinaryASN:
    case CFormatGuess::eTextASN:
    case CFormatGuess::eXml:
        {
            CLDS2_ObjectParser parser(*this,
                info.id, info.format, *in, *m_Db);
            while ( !in->eof() ) {
                try {
                    if ( !parser.ParseNext() ) {
                        if ( in->eof() ) {
                            // End of file - stop reading
                            break;
                        }
                        // Failed to parse object. Ignore the rest of the file.
                        if (m_ErrorMode == eError_Throw) {
                            LDS2_THROW(eIndexerError,
                                "Unrecognized top level object in " +
                                info.name);
                        }
                        else if (m_ErrorMode == eError_Report) {
                            ERR_POST_X(6, Warning <<
                                "Unrecognized top level object in " <<
                                info.name);
                        }
                        break;
                    }
                    parsed_entries++;
                }
                catch (CEofException) {
                    break;
                }
            }
            if (parsed_entries == 0) {
                // Nothing found in the file
                m_Db->DeleteFile(info.id);
            }
            break;
        }
    case CFormatGuess::eFasta:
        {
            // Count seq-entries found
            try {
                // Can not use ScanFastaFile which requires file stream.
                // Can not use CStreamLineReader since it does not track
                // stream position.
                CBufferedLineReader lr(*in);
                CFastaReader reader(lr, m_FastaFlags);
                while ( !lr.AtEOF() ) {
                    try {
                        Int8 pos = NcbiStreamposToInt8(lr.GetPosition());
                        CRef<CSeq_entry> se  = reader.ReadOneSeq();
                        if ( !se->IsSeq() ) {
                            continue;
                        }
                        Int8 blob_id = m_Db->AddBlob(info.id,
                            SLDS2_Blob::eSeq_entry, pos);
                        // Index bioseq
                        TSeqIdSet ids;
                        const CBioseq& bs = se->GetSeq();
                        ITERATE(CBioseq::TId, id, bs.GetId()) {
                            ids.insert(CSeq_id_Handle::GetHandle(**id));
                        }
                        m_Db->AddBioseq(blob_id, ids);
                        parsed_entries++;
                    } catch (CObjReaderParseException&) {
                        if ( !lr.AtEOF() ) {
                            throw;
                        }
                    }
                }
            }
            catch (CException) {
                m_Db->DeleteFile(info.id);
                if (m_ErrorMode == eError_Throw) {
                    throw;
                }
                else if (m_ErrorMode == eError_Report) {
                    ERR_POST_X(7, Warning <<
                        "Failed to parse fasta file " << info.name);
                }
                return;
            }
            if (parsed_entries == 0) {
                // Nothing in the file
                m_Db->DeleteFile(info.id);
            }
            break;
        }
    default:
        if (m_ErrorMode == eError_Throw) {
            LDS2_THROW(eIndexerError,
                "Unsupported data file format: " + info.name);
        }
        else if (m_ErrorMode == eError_Report) {
            ERR_POST_X(5, Warning <<
                "Unsupported data file format: " << info.name);
        }
        m_Db->DeleteFile(info.id);
        break;
    }
    if (parsed_entries > 0) {
        handler.SaveChunks(info, *m_Db);
    }
}


void CLDS2_Manager::AddDataUrl(const string& url, const string& handler_name)
{
    m_Files.insert(url);
    m_HandlersByUrl[url] = handler_name;
}


END_SCOPE(objects)
END_NCBI_SCOPE
