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
#include <util/checksum.hpp>
#include <util/format_guess.hpp>
#include <db/sqlite/sqlitewrapp.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objtools/error_codes.hpp>
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

class CLDS2_ObjectParser
{
public:
    CLDS2_ObjectParser(CLDS2_Manager& mgr,
                       Int8             file_id,
                       CObjectIStream&  in,
                       CLDS2_Database&  db)
        : m_Manager(mgr),
          m_In(in),
          m_Db(db),
          m_CurFileId(file_id),
          m_BlobType(SLDS2_Blob::eUnknown),
          m_IsGBBioseqSet(false)
    {
        ResetBlob();
    }

    ~CLDS2_ObjectParser(void) {}

    // Try to parse the next blob, return true on success
    bool ParseNext(void);

    void AddAnnot(SLDS2_Annot::EAnnotType annot_type,
                  const TSeqIdSet&        ids);

    void AddBioseq(const TSeqIdSet& ids);

    void BeginBlob(void);
    void ResetBlob(void);
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

private:
    static TTypeInfo sx_GetObjectTypeInfo(SLDS2_Blob::EBlobType blob_type);

    struct SAnnotInfo {
        SLDS2_Annot::EAnnotType type;
        TSeqIdSet               ids;
    };
    typedef vector< AutoPtr<SAnnotInfo> > TAnnots;

    struct SBioseqInfo {
        TSeqIdSet   ids;
    };
    typedef vector< AutoPtr<SBioseqInfo> > TBioseqs;

    CLDS2_Manager&          m_Manager;
    CObjectIStream&         m_In;
    CLDS2_Database&         m_Db;

    Int8                    m_CurFileId;
    CNcbiStreampos          m_CurBlobPos;
    SLDS2_Blob::EBlobType   m_BlobType;

    TSeqIdSet               m_BioseqIds;
    TAnnots                 m_Annots;
    TBioseqs                m_Bioseqs;

    // Splitting GB releases
    bool                    m_IsGBBioseqSet;
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
class CLDS2_AnnotType_Hook : public CSkipObjectHook
{
public:
    CLDS2_AnnotType_Hook(void) {}
    ~CLDS2_AnnotType_Hook(void) {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

    const string& GetType(void) const { return m_Type; }
    void ResetType(void) { m_Type.clear(); }

private:
    string m_Type;
};


void CLDS2_AnnotType_Hook::SkipObject(CObjectIStream& in,
                                      const CObjectTypeInfo& info)
{
    _ASSERT(m_Type.empty()  ||  m_Type == info.GetName());
    m_Type = info.GetName();
    DefaultSkip(in, info);
}


// Hook for collecting annotations.
class CLDS2_Annot_Hook : public CSkipObjectHook
{
public:
    // id_hook and type_hook must be already attached to the input
    // stream.
    CLDS2_Annot_Hook(CLDS2_ObjectParser&   parser,
                     CLDS2_Seq_id_Hook&    id_hook,
                     CLDS2_AnnotType_Hook& type_hook)
        : m_Parser(parser),
          m_IdHook(id_hook),
          m_TypeHook(type_hook)
    {}

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& info);

private:
    CLDS2_ObjectParser&    m_Parser;
    CLDS2_Seq_id_Hook&     m_IdHook;
    CLDS2_AnnotType_Hook&  m_TypeHook;
    TSeqIdSet              m_Ids;
};


void CLDS2_Annot_Hook::SkipObject(CObjectIStream& in,
                                  const CObjectTypeInfo& info)
{
    m_Ids.clear();
    CLDS2_Seq_id_Hook::CGuard guard(m_IdHook, m_Ids);

    // annot-type hook may be not empty, e.g. after reading seq-hist
    // with alignments in it. Reset it now.
    m_TypeHook.ResetType();
    DefaultSkip(in, info);

    SLDS2_Annot::EAnnotType annot_type = SLDS2_Annot::eUnknown;

    if (m_TypeHook.GetType() == "Seq-feat") {
        annot_type = SLDS2_Annot::eSeq_feat;
    }
    else if (m_TypeHook.GetType() == "Seq-align") {
        annot_type = SLDS2_Annot::eSeq_align;
    }
    else if (m_TypeHook.GetType() == "Seq-graph") {
        annot_type = SLDS2_Annot::eSeq_graph;
    }
    m_TypeHook.ResetType();

    m_Parser.AddAnnot(annot_type, m_Ids);
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
    m_CurBlobPos = m_In.GetStreamPos();
}


void CLDS2_ObjectParser::ResetBlob(void)
{
    m_BioseqIds.clear();
    m_Annots.clear();
    m_Bioseqs.clear();
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
    ITERATE(TAnnots, it, m_Annots) {
        CLDS2_Database::TAnnotRefSet refs;
        ITERATE(TSeqIdSet, id, (*it)->ids) {
            CLDS2_Database::TAnnotRef ref(*id, true);
            // If the blob can contain bioseqs, check if the annotation
            // is external. Each id has its own external flag.
            if (blob_type == SLDS2_Blob::eSeq_entry  ||
                blob_type == SLDS2_Blob::eBioseq ||
                blob_type == SLDS2_Blob::eBioseq_set  ||
                blob_type == SLDS2_Blob::eBioseq_set_element) {
                if (m_BioseqIds.find(*id) != m_BioseqIds.end()) {
                    ref.second = false;
                }
            }
            refs.push_back(ref);
        }
        m_Db.AddAnnot(blob_id, (*it)->type, refs);
    }
    ResetBlob();
}


static const SLDS2_Blob::EBlobType kExpectedBlobTypes[] = {
    SLDS2_Blob::eSeq_entry,
    SLDS2_Blob::eBioseq,
    SLDS2_Blob::eBioseq_set,
    SLDS2_Blob::eSeq_annot,
    SLDS2_Blob::eSeq_align_set,
    SLDS2_Blob::eSeq_align
};

bool CLDS2_ObjectParser::ParseNext(void)
{
    BeginBlob();

    // Hook for reading seq-ids and storing them to a collection.
    CLDS2_Seq_id_Hook id_hook;
    CObjectHookGuard<CSeq_id> seq_id_guard(id_hook, &m_In);

    // Hook which enables reading seq-ids only while parsing bioseq.ids
    // member (to ignore seq-ids in other members like inst or hist).
    CLDS2_BioseqIds_Hook bioseq_ids_hook(*this, id_hook);
    CObjectHookGuard<CBioseq> bioseq_ids_guard("id", bioseq_ids_hook, &m_In);

    // Hook for detecting annotation type.
    CLDS2_AnnotType_Hook annot_type_hook;
    CObjectHookGuard<CSeq_feat> seq_feat_guard(annot_type_hook, &m_In);
    CObjectHookGuard<CSeq_graph> seq_graph_guard(annot_type_hook, &m_In);
    CObjectHookGuard<CSeq_align> seq_align_guard(annot_type_hook, &m_In);

    // Hook for indexing seq-annots. Uses annot_type_hook to check
    // seq-annot type.
    CLDS2_Annot_Hook annot_hook(*this, id_hook, annot_type_hook);
    CObjectHookGuard<CSeq_annot> seq_annot_guard(annot_hook, &m_In);

    // Hooks for checking bioseq-set class and descr members.
    // There presence and value may affect splitting bioseq-set
    // into seq-entries.
    CLDS2_BioseqSet_Hook bioseq_set_hook(*this);
    CObjectHookGuard<CBioseq_set> bioseq_set_class_guard(
        "class", bioseq_set_hook, &m_In);
    CObjectHookGuard<CBioseq_set> bioseq_set_descr_guard(
        "descr", bioseq_set_hook, &m_In);
    CObjectHookGuard<CBioseq_set> bioseq_set_seqset_guard(
        "seq-set", bioseq_set_hook, &m_In);

    // Seq-entry hook is used to split bioseq-set into individual
    // seq-entries.
    CLDS2_SeqEntry_Hook entry_hook(*this);
    CObjectHookGuard<CSeq_entry> entry_guard(entry_hook, &m_In);

    // Seq-id set used to collect seq-ids from top level seq-aligns
    // and seq-align-sets.
    TSeqIdSet align_ids;

    int num_types = sizeof(kExpectedBlobTypes)/sizeof(kExpectedBlobTypes[0]);
    m_BlobType = SLDS2_Blob::eUnknown;

    for (int i = 0; i < num_types; i++) {
        try {
            // Rewind the stream and try the next type.
            if (m_CurBlobPos != m_In.GetStreamPos()) {
                m_In.SetStreamPos(m_CurBlobPos);
            }
            m_BlobType = kExpectedBlobTypes[i];
            TTypeInfo type_info = sx_GetObjectTypeInfo(m_BlobType);

            // Different hooks are required for aligns/align-sets and
            // all other objects.
            bool is_align = m_BlobType == SLDS2_Blob::eSeq_align  ||
                m_BlobType == SLDS2_Blob::eSeq_align_set;
            auto_ptr<CLDS2_Seq_id_Hook::CGuard> align_guard;
            _ASSERT(align_ids.empty());
            if ( is_align ) {
                align_guard.reset(
                    new CLDS2_Seq_id_Hook::CGuard(id_hook, align_ids));
            }

            m_In.Skip(type_info);

            // Store seq-aligns and seq-align-sets as blobs
            if ( is_align ) {
                AddAnnot(SLDS2_Annot::eSeq_align, align_ids);
                align_ids.clear();
            }
        }
        catch (CSerialException) {
            ResetBlob();
            m_BlobType = SLDS2_Blob::eUnknown;
            continue;
        }
        break;
    }
    // Do not store bioseq-set if it has been split and stored as
    // separate seq-entries.
    if ( !GetSplitBioseqSet() ) {
        EndBlob(m_BlobType);
    }
    SetGBBioseqSet(false); // reset GB release flag
    return m_BlobType != SLDS2_Blob::eUnknown;
}


void CLDS2_ObjectParser::AddAnnot(SLDS2_Annot::EAnnotType annot_type,
                                  const TSeqIdSet&        ids)
{
    SAnnotInfo* info = new SAnnotInfo;
    info->type = annot_type;
    info->ids.insert(ids.begin(), ids.end());
    m_Annots.push_back(info);
}


void CLDS2_ObjectParser::AddBioseq(const TSeqIdSet& ids)
{
    SBioseqInfo* info = new SBioseqInfo;
    info->ids.insert(ids.begin(), ids.end());
    m_Bioseqs.push_back(info);

    // Remember ids used in bioseqs.
    m_BioseqIds.insert(ids.begin(), ids.end());
}


// Fasta file scanner
class CLDS2_FastaParser : public IFastaEntryScan
{
public:
    CLDS2_FastaParser(Int8            file_id,
                      CLDS2_Database& db)
        : m_FileId(file_id),
          m_Db(db),
          m_Count(0)
    {}

    virtual void EntryFound(CRef<CSeq_entry> se,
                            CNcbiStreampos   stream_position);

    int GetEntriesCount(void) const { return m_Count; }

private:
    Int8            m_FileId;
    CLDS2_Database& m_Db;
    int             m_Count; // number of parsed entries
};


void CLDS2_FastaParser::EntryFound(CRef<CSeq_entry> se,
                                   CNcbiStreampos   stream_position)
{
    if (!se->IsSeq()) {
        // ignore non-sequence entries
        return;
    }

    Int8 blob_id = m_Db.AddBlob(m_FileId,
        SLDS2_Blob::eSeq_entry, stream_position);

    // Index bioseq
    TSeqIdSet ids;
    const CBioseq& bs = se->GetSeq();
    ITERATE(CBioseq::TId, id, bs.GetId()) {
        ids.insert(CSeq_id_Handle::GetHandle(**id));
    }
    m_Db.AddBioseq(blob_id, ids);
    m_Count++;
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
                   CFastaReader::fParseRawID)
{
    SetDbFile(db_file);
}


CLDS2_Manager::~CLDS2_Manager(void)
{
}


void CLDS2_Manager::SetDbFile(const string& db_file)
{
    m_Files.clear();
    m_Db.Reset(new CLDS2_Database(db_file));
    m_Db->SetSQLiteFlags(
        CSQLITE_Connection::eDefaultFlags |
        CSQLITE_Connection::fVacuumManual |
        CSQLITE_Connection::fJournalMemory |
        // SyncOn slows down the database write access. Turn it off.
        CSQLITE_Connection::fSyncOff);
}


void CLDS2_Database::SetSQLiteFlags(int flags)
{
    m_DbFlags = flags;
    m_Conn.reset();
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


SLDS2_File CLDS2_Manager::x_GetFileInfo(const string& file_name) const
{
    SLDS2_File info(file_name);

    CFile f(file_name);
    if ( f.Exists() ) {
        info.crc = ComputeFileCRC32(info.name);
        info.size = f.GetLength();
        CFile::SStat stat;
        if ( f.Stat(&stat) ) {
            info.time = stat.mtime_nsec;
        }
        CFormatGuess fg;
        info.format = fg.Format(info.name);
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
        m_Db->Create();
    }
    else {
        m_Db->Open();
    }

    // Add all known files from the DB
    m_Db->GetFileNames(m_Files);

    ITERATE(TFiles, it, m_Files) {
        SLDS2_File file_info = x_GetFileInfo(*it);
        SLDS2_File db_info = m_Db->GetFileInfo(*it);
        if (!file_info.exists()  ||  !IsSupportedFormat(file_info.format)) {
            // the file does not exist
            if (db_info.id != 0) {
                // remove the file from the database
                m_Db->DeleteFile(db_info.id);
            }
            continue;
        }
        if (db_info.id == 0) {
            // new file
            m_Db->AddFile(file_info);
            x_ParseFile(file_info);
        }
        else {
            // existing file
            file_info.id = db_info.id;
            if (file_info != db_info) {
                m_Db->UpdateFile(file_info);
                x_ParseFile(file_info);
            }
        }
    }
    // Update statistics - this makes queries up to 100 times faster
    // in big databases.
    m_Db->Analyze();
}


void CLDS2_Manager::x_ParseFile(const SLDS2_File& info)
{
    // Always open file as binary. Otherwise on Win32 file positions will
    // be invalid.
    CNcbiIfstream fin(info.name.c_str(), ios::in | ios::binary);
    auto_ptr<CObjectIStream> in;
    switch ( info.format ) {
    case CFormatGuess::eBinaryASN:
        in.reset(CObjectIStream::Open(eSerial_AsnBinary, fin));
        break;
    case CFormatGuess::eTextASN:
        in.reset(CObjectIStream::Open(eSerial_AsnText, fin));
        break;
    case CFormatGuess::eXml:
        in.reset(CObjectIStream::Open(eSerial_Xml, fin));
        break;
    case CFormatGuess::eFasta:
        {
            CLDS2_FastaParser fasta_parser(info.id, *m_Db);
            try {
                ScanFastaFile(&fasta_parser, fin, m_FastaFlags);
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
            if (fasta_parser.GetEntriesCount() == 0) {
                // Nothing in the file
                m_Db->DeleteFile(info.id);
            }
            return;
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
        return;
    }
    int parsed_entries = 0;
    while ( in->HaveMoreData() ) {
        CLDS2_ObjectParser parser(*this, info.id, *in, *m_Db);
        try {
            CNcbiStreampos pos = in->GetStreamPos();
            if ( !parser.ParseNext() ) {
                // Failed to parse object. Ignore the rest of the file.
                if (m_ErrorMode == eError_Throw) {
                    LDS2_THROW(eIndexerError,
                        "Unrecognized top-level object in " +
                        info.name + ", offset " +
                        NStr::Int8ToString(pos));
                }
                else if (m_ErrorMode == eError_Report) {
                    ERR_POST_X(6, Warning <<
                        "Unrecognized top level object in " <<
                        info.name << ", offset " << pos);
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
}


END_SCOPE(objects)
END_NCBI_SCOPE
