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
 * Author: Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:  CLDS_Object implementation.
 *
 */


#include <ncbi_pch.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_util.hpp>

#include <objtools/readers/fasta.hpp>

#include <objtools/lds/lds_object.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_util.hpp>
#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_query.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>
#include <serial/iterator.hpp>

#define TRY_FAST_TITLE 1
#define CREATE_SCOPES 1

#define NCBI_USE_ERRCODE_X   Objtools_LDS_Object


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// @internal
class CLDS_FastaScanner : public IFastaEntryScan
{
public:
    CLDS_FastaScanner(CLDS_Object& obj,
                      int          file_id,
                      int          type_id);

    virtual void EntryFound(CRef<CSeq_entry> se,
                            CNcbiStreampos   stream_position);
private:
    CLDS_Object& m_Obj;
    int          m_FileId;
    int          m_TypeId;
};

CLDS_FastaScanner::CLDS_FastaScanner(CLDS_Object& obj,
                                     int          file_id,
                                     int          type_id)
 : m_Obj(obj),
   m_FileId(file_id),
   m_TypeId(type_id)
{}

void CLDS_FastaScanner::EntryFound(CRef<CSeq_entry> se,
                                   CNcbiStreampos   stream_position)
{
    if (!se->IsSeq())
        return;

    SFastaFileMap::SFastaEntry  fasta_entry;
    fasta_entry.stream_offset = stream_position;

    // extract sequence info

    const CSeq_entry::TSeq& bioseq = se->GetSeq();
    const CSeq_id* sid = bioseq.GetFirstId();
    fasta_entry.seq_id = sid->AsFastaString();

    fasta_entry.all_seq_ids.resize(0);
    if (bioseq.CanGetId()) {
        const CBioseq::TId& seq_ids = bioseq.GetId();
        string id_str;
        ITERATE(CBioseq::TId, it, seq_ids) {
            const CBioseq::TId::value_type& vt = *it;
            id_str = vt->AsFastaString();
            fasta_entry.all_seq_ids.push_back(id_str);
        }
    }

    if (bioseq.CanGetDescr()) {
        const CSeq_descr& d = bioseq.GetDescr();
        if (d.CanGet()) {
            const CSeq_descr_Base::Tdata& data = d.Get();
            if (!data.empty()) {
                CSeq_descr_Base::Tdata::const_iterator it =
                                                    data.begin();
                if (it != data.end()) {
                    CRef<CSeqdesc> ref_desc = *it;
                    ref_desc->GetLabel(&fasta_entry.description,
                                        CSeqdesc::eContent);
                }
            }
        }
    }

    // store entry record

    // concatenate all ids
    string seq_ids;
    ITERATE(SFastaFileMap::SFastaEntry::TFastaSeqIds,
            it_id, fasta_entry.all_seq_ids) {
        seq_ids.append(*it_id);
        seq_ids.append(" ");
    }

    m_Obj.SaveObject(m_FileId,
                     fasta_entry.seq_id,
                     fasta_entry.description,
                     seq_ids,
                     fasta_entry.stream_offset,
                     m_TypeId);

}


void CLDS_Object::DeleteUpdateCascadeFiles(const CLDS_Set& files_deleted,
                                           const CLDS_Set& files_updated)
{
    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;
    DeleteCascadeFiles(files_deleted, &objects_deleted, &annotations_deleted);
    UpdateCascadeFiles(files_updated);
    if ( files_deleted.any() || files_updated.any() ) {
        // re-index
        BuildSeqIdIdx();
    }
}


void CLDS_Object::DeleteCascadeFiles(const CLDS_Set& file_ids,
                                     CLDS_Set* objects_deleted,
                                     CLDS_Set* annotations_deleted)
{
    if (file_ids.none())
        return;

    //
    //  Delete records from "object" table
    //
    {{
    CBDB_FileCursor cur(m_db.object_db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        int fid = m_db.object_db.file_id;
        if (fid && LDS_SetTest(file_ids, fid)) {
/*
            int object_attr_id = m_db.object_db.object_attr_id;

            if (object_attr_id) {  // delete dependent object attr
                m_db.object_attr_db.object_attr_id = object_attr_id;
                m_db.object_attr_db.Delete();
            }
*/
            int object_id = m_db.object_db.object_id;

            objects_deleted->set(object_id);
            m_db.object_db.Delete();
        }
    }

    }}

    //
    // Delete "annot2obj"
    //
    {{
    CBDB_FileCursor cur(m_db.annot2obj_db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        int object_id = m_db.annot2obj_db.object_id;
        if (object_id && LDS_SetTest(*objects_deleted, object_id)) {
            m_db.annot2obj_db.Delete();
        }
    }

    }}

    //
    // Delete "annotation"
    //
    {{
    CBDB_FileCursor cur(m_db.annot_db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        if ( !m_db.object_db.file_id.IsNull() ) {
            int fid = m_db.object_db.file_id;
            if (fid && LDS_SetTest(file_ids, fid)) {
                int annot_id = m_db.annot_db.annot_id;
                annotations_deleted->set(annot_id);
                m_db.annot_db.Delete();
            }
        }
    }

    }}

    //
    // Delete "seq_id_list"
    //
    {{

    {{
    CLDS_Set::enumerator en = objects_deleted->first();
    for ( ; en.valid(); ++en) {
        int id = *en;
        m_db.seq_id_list.object_id = id;
        m_db.seq_id_list.Delete();
    }
    }}

    CLDS_Set::enumerator en = annotations_deleted->first();
    for ( ; en.valid(); ++en) {
        int id = *en;
        m_db.seq_id_list.object_id = id;
        m_db.seq_id_list.Delete();
    }

    }}

}


void CLDS_Object::UpdateCascadeFiles(const CLDS_Set& file_ids)
{
    if (file_ids.none()) {
        return;
    }

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;
    DeleteCascadeFiles(file_ids, &objects_deleted, &annotations_deleted);

    CLDS_Set::enumerator en(file_ids.first());
    for ( ; en.valid(); ++en) {
        int fid = *en;
        m_db.file_db.file_id = fid;

        if (m_db.file_db.Fetch() == eBDB_Ok) {
            string fname(m_db.file_db.file_name);
            CFormatGuess::EFormat format =
                (CFormatGuess::EFormat)(int)m_db.file_db.format;

            LOG_POST_X(1, Info << "<< Updating file >>: " << fname);

            UpdateFileObjects(fid, fname, format);
        }
    } // ITERATE
}


class CLDS_SkipObjectHook : public CReadObjectHook
{
public:
    virtual void ReadObject(CObjectIStream& in,
                            const CObjectInfo& obj) {
        DefaultSkip(in, obj);
    }
};


class CLDS_Seq_ids : public CObject
{
public:
    typedef vector<CRef<CSeq_id> > TIds;
    typedef vector<int> TGis;
    void clear()
        {
            m_Ids.clear();
            m_Gis.clear();
        }
    void AddSeq_id(const CSeq_id& id)
        {
            if ( id.IsGi() ) {
                AddGi(id.GetGi());
            }
            else if ( m_Ids.empty() || !m_Ids.back()->Equals(id) ) {
                m_Ids.push_back(Ref(SerialClone(id)));
            }
        }
    void AddGi(int gi)
        {
            if ( m_Gis.empty() || m_Gis.back() != gi ) {
                m_Gis.push_back(gi);
            }
        }

    TIds m_Ids;
    TGis m_Gis;
};

class CLDS_CollectSeq_idsReader : public CSkipObjectHook
{
public:
    CLDS_CollectSeq_idsReader(void)
        : m_Seq_id(new CSeq_id()), m_Collect(0)
        {
        }

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& type) {
        if ( m_Collect ) {
            DefaultRead(in, ObjectInfo(*m_Seq_id));
            m_Collect->AddSeq_id(*m_Seq_id);
        }
        else {
            DefaultSkip(in, type);
        }
    }

    void Collect(CLDS_Seq_ids* ids) {
        m_Collect = ids;
    }

    class CGuard
    {
    public:
        CGuard(CLDS_CollectSeq_idsReader& reader, CLDS_Seq_ids& ids)
            : m_Reader(reader)
            {
                reader.Collect(&ids);
            }
        ~CGuard()
            {
                m_Reader.Collect(0);
            }
    private:
        CLDS_CollectSeq_idsReader& m_Reader;

        CGuard(const CGuard&);
        void operator=(const CGuard&);
    };

private:
    CRef<CSeq_id> m_Seq_id;
    CLDS_Seq_ids* m_Collect;
};


class PLessObjectPtr
{
public:
    bool operator()(const CObjectInfo& a, const CObjectInfo& b) const {
        return a.GetObjectPtr() < b.GetObjectPtr();
    }
};


class CLDS_Seq_idsCollector : public CReadClassMemberHook
{
public:
    typedef map<CObjectInfo, CRef<CLDS_Seq_ids>, PLessObjectPtr> TIdsMap;

    CLDS_Seq_idsCollector(CLDS_CollectSeq_idsReader* collector)
        : m_Collector(collector)
        {
        }

    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& member) {
        CRef<CLDS_Seq_ids>& ids = m_Ids[member.GetClassObject()];
        ids = new CLDS_Seq_ids();
        CLDS_CollectSeq_idsReader::CGuard guard(*m_Collector, *ids);
        DefaultSkip(in, member);
    }

    CLDS_Seq_ids* GetIds(const CObjectInfo& obj) {
        TIdsMap::iterator iter = m_Ids.find(obj);
        return iter == m_Ids.end()? 0: iter->second.GetPointer();
    }
    void ClearIds(void) {
        m_Ids.clear();
    }

private:
    CRef<CLDS_CollectSeq_idsReader> m_Collector;
    TIdsMap m_Ids;
};


class CLDS_GBReleaseReadHook : public CReadClassMemberHook
{
public:
    CLDS_GBReleaseReadHook(CLDS_Object& lobj,
                           CLDS_CoreObjectsReader& objects);
    ~CLDS_GBReleaseReadHook(void);

    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& member);

    void Remove(CObjectIStream& in) {
        if ( !m_Removed ) {
            m_Removed = true;
            CObjectTypeInfo type = CType<CBioseq_set>();
            type.FindMember("seq-set").ResetLocalReadHook(in);
        }
    }
    bool Separate(void) const {
        return m_Separate;
    }

private:
    CLDS_Object& m_LObj;
    CLDS_CoreObjectsReader& m_Objects;
    bool m_Removed;
    bool m_Separate;
};


CLDS_GBReleaseReadHook::CLDS_GBReleaseReadHook(CLDS_Object& lobj,
                                               CLDS_CoreObjectsReader& objects)
    : m_LObj(lobj),
      m_Objects(objects),
      m_Removed(false),
      m_Separate(false)
{
}


CLDS_GBReleaseReadHook::~CLDS_GBReleaseReadHook(void)
{
}


void CLDS_GBReleaseReadHook::ReadClassMember(CObjectIStream& in,
                                             const CObjectInfoMI& member)
{
    Remove(in);
    CBioseq_set* seq_set = CType<CBioseq_set>::Get(member.GetClassObject());
    _ASSERT(seq_set);
    if ( seq_set ) {
        switch ( m_LObj.GetGBReleasMode() ) {
        case CLDS_Object::eForceGBRelease:
            m_Separate = true;
            break;
        case CLDS_Object::eGuessGBRelease:
            if ( (!seq_set->IsSetClass() ||
                  seq_set->GetClass() == CBioseq_set::eClass_genbank) &&
                 //!seq_set->IsSetId() &&
                 //!seq_set->IsSetColl() &&
                 //!seq_set->IsSetLevel() &&
                 //!seq_set->IsSetRelease() &&
                 //!seq_set->IsSetDate() &&
                 !seq_set->IsSetDescr() ) {
                m_Separate = true;
            }
            break;
        default:
            break;
        }
    }
    if ( m_Separate ) {
        m_Objects.Reset();
        LOG_POST_X(3, Info << CTime(CTime::eCurrent) <<
                   ": Scanning combined Bioseq-set found in: " <<
                   m_Objects.GetFileName());
        int entry_count = 0, object_count = 0;
        // iterate over the sequence of entries
        CRef<CSeq_entry> se(new CSeq_entry);
        for ( CIStreamContainerIterator it(in, member); it; ++it ) {
            CNcbiStreampos pos = in.GetStreamPos();
            it >> *se;
            ++entry_count;
            m_LObj.SaveObject(&m_Objects, &m_Objects.GetObjectsVector()[0]);
            object_count += m_LObj.SaveObjects(m_Objects, true);
        }
        LOG_POST_X(3, Info << CTime(CTime::eCurrent) << ": LDS: "
                   << object_count
                   << " object(s) found in "
                   << entry_count << " Seq-entries in: "
                   << m_Objects.GetFileName());
    }
    else {
        DefaultRead(in, member);
    }
}


bool CLDS_Object::UpdateBinaryASNObject(CObjectIStream& in,
                                        CLDS_CoreObjectsReader& objects,
                                        CObjectTypeInfo type)
{
    CNcbiStreampos start_pos = in.GetStreamPos();
    objects.Reset();
    LOG_POST_X(4, Info
               << "Trying ASN.1 binary top level object:"
               << type.GetName() );
    CRef<CLDS_GBReleaseReadHook> hook;
    try {
        if ( m_GBReleaseMode != eNoGBRelease &&
             type == CType<CBioseq_set>() ) {
            // try to avoid loading full GenBank release Bioseq-set
            hook = new CLDS_GBReleaseReadHook(*this, objects);
            type.FindMember("seq-set").SetLocalReadHook(in, hook);
        }
        CObjectInfo object_info(type);
        CStopWatch sw(CStopWatch::eStart);
        in.Read(object_info);
        if ( hook && hook->Separate() ) {
            LOG_POST_X(5, Info
                       << "Binary ASN.1 combined object found: "
                       << type.GetName()
                       << " in " << sw.Elapsed());
        }
        else {
            LOG_POST_X(5, Info
                       << "Binary ASN.1 top level object found: "
                       << type.GetName()
                       << " in " << sw.Elapsed());
        }
        if ( hook ) {
            hook->Remove(in);
        }
        return true;
    }
    catch (CEofException& ) {
    }
    catch (CException& _DEBUG_ARG(e)) {
        _TRACE("  failed to read: " << e.GetMsg());
    }
    if ( hook ) {
        hook->Remove(in);
    }
    in.SetStreamPos(start_pos);
    return false;
}


int CLDS_Object::SaveObjects(CLDS_CoreObjectsReader& objects,
                             bool internal)
{
    int ret = 0;
    CLDS_CoreObjectsReader::TObjectVector& objs = objects.GetObjectsVector();
    if ( !objs.empty() ) {
        size_t count = objs.size();
        if ( !internal ) {
            LOG_POST_X(3, Info << CTime(CTime::eCurrent) <<
                       ": Saving " << count <<
                       " object(s) found in: " << objects.GetFileName());
        }
        for (size_t i = 0; i < count; ++i) {
            CLDS_CoreObjectsReader::SObjectDetails& obj_info = objs[i];
            // If object is not in the database yet.
            if (obj_info.ext_id == 0) {
                SaveObject(&objects, &obj_info);
                ++ret;
            }
        }
        if ( !internal ) {
            LOG_POST_X(3, Info << CTime(CTime::eCurrent) << ": LDS: "
                       << count
                       << " object(s) found in: "<<objects.GetFileName());
        }
        objects.ClearObjectsVector();
    }
    else {
        if ( !internal ) {
            if ( objects.GetTotalObjects() == 0 ) {
                LOG_POST_X(4, Info <<
                           "LDS: No objects found in:" <<
                           objects.GetFileName());
            }
            else {
                LOG_POST_X(4, Info <<
                           "LDS: No more objects found in:" <<
                           objects.GetFileName());
            }
        }
    }
    if ( m_Seq_idsCollector ) {
        m_Seq_idsCollector->ClearIds();
    }
    return ret;
}


void CLDS_Object::UpdateBinaryASNObjects(int file_id,
                                         const string& file_name)
{
    vector<CObjectTypeInfo> types;
    types.push_back(CType<CBioseq_set>());
    types.push_back(CType<CSeq_entry>());
    types.push_back(CType<CBioseq>());
    types.push_back(CType<CSeq_annot>());
    types.push_back(CType<CSeq_align>());
    types.push_back(CType<CSeq_align_set>());
    vector<CObjectTypeInfo> skip_types;
    skip_types.push_back(CType<CSeq_data>());
    skip_types.push_back(CType<CSeq_ext>());
    skip_types.push_back(CType<CSeq_hist>());

    LOG_POST_X(2, Info << CTime(CTime::eCurrent) <<
               ": Scanning file: " << file_name);

    CRef<CLDS_CollectSeq_idsReader> seq_id_hook(new CLDS_CollectSeq_idsReader);
    m_Seq_idsCollector = new CLDS_Seq_idsCollector(seq_id_hook);
    CRef<CLDS_CoreObjectsReader> objects
        (new CLDS_CoreObjectsReader(file_id, file_name));
    auto_ptr<CObjectIStream>
        in(CObjectIStream::Open(file_name, eSerial_AsnBinary));

    {{ // setup hooks
        ITERATE ( vector<CObjectTypeInfo>, it, types ) {
            it->SetLocalReadHook(*in, objects);
        }
        CRef<CLDS_SkipObjectHook> skipper(new CLDS_SkipObjectHook);
        ITERATE ( vector<CObjectTypeInfo>, it, skip_types ) {
            it->SetLocalReadHook(*in, skipper);
        }
        CObjectTypeInfo seq_id_type = CType<CSeq_id>();
        seq_id_type.SetLocalSkipHook(*in, seq_id_hook);
        CObjectTypeInfo annot_type = CType<CSeq_annot>();
        annot_type.FindMember("data").SetLocalReadHook(*in, m_Seq_idsCollector);
    }}

    size_t last_type = 0;
    while ( in->HaveMoreData() ) {
        // first try previous type
        bool found = UpdateBinaryASNObject(*in, *objects, types[last_type]);
        if ( !found ) {
            // then all remaining possible types
            for ( size_t i = 0; i < types.size(); ++i ) {
                if ( i != last_type ) { // already tried
                    if ( UpdateBinaryASNObject(*in, *objects, types[i]) ) {
                        found = true;
                        last_type = i;
                        break;
                    }
                }
            }
        }
        if ( !found ) {
            break;
        }
        SaveObjects(*objects, false);
    }
}


void CLDS_Object::UpdateFileObjects(int file_id,
                                    const string& file_name,
                                    CFormatGuess::EFormat format)
{
    FindMaxObjRecId();

    if (format == CFormatGuess::eBinaryASN ) {
        UpdateBinaryASNObjects(file_id, file_name);
    }
    else if (format == CFormatGuess::eTextASN ||
             format == CFormatGuess::eXml) {

        LOG_POST_X(2, Info << CTime(CTime::eCurrent) <<
                   ": Scanning file: " << file_name);

        CLDS_CoreObjectsReader sniffer(file_id, file_name);
        ESerialDataFormat stream_format = FormatGuess2Serial(format);

        CNcbiIfstream str_input(file_name.c_str(), IOS_BASE::binary);
        auto_ptr<CObjectIStream> input(CObjectIStream::Open(stream_format,
                                                            str_input));
        CRef<CLDS_CollectSeq_idsReader> seq_id_hook(new CLDS_CollectSeq_idsReader);
        m_Seq_idsCollector = new CLDS_Seq_idsCollector(seq_id_hook);
        CObjectTypeInfo seq_id_type = CType<CSeq_id>();
        seq_id_type.SetLocalSkipHook(*input, seq_id_hook);
        CObjectTypeInfo annot_type = CType<CSeq_annot>();
        annot_type.FindMember("data").SetLocalReadHook(*input, m_Seq_idsCollector);

        sniffer.Probe(*input);

        SaveObjects(sniffer, false);
    } else if ( format == CFormatGuess::eFasta ){

        int type_id;
        {{
        map<string, int>::const_iterator it = m_ObjTypeMap.find("FastaEntry");
        _ASSERT(it != m_ObjTypeMap.end());
        type_id = it->second;
        }}

        CNcbiIfstream input(file_name.c_str(), IOS_BASE::binary);

        CLDS_FastaScanner fscan(*this, file_id, type_id);
        ScanFastaFile(&fscan,
                      input,
                      CFastaReader::fAssumeNuc  |
                      CFastaReader::fAllSeqIds  |
                      CFastaReader::fOneSeq     |
                      CFastaReader::fNoSeqData  |
                      CFastaReader::fParseGaps  |
                      CFastaReader::fParseRawID);
    } else {
        LOG_POST_X(5, Info << "Unsupported file format: " << file_name);
    }


}


int CLDS_Object::SaveObject(int file_id,
                            const string& seq_id,
                            const string& description,
                            const string& seq_ids,
                            CNcbiStreampos pos,
                            int type_id)
{
    ++m_MaxObjRecId;
    EBDB_ErrCode err;
/*
    m_db.object_attr_db.object_attr_id = m_MaxObjRecId;
    m_db.object_attr_db.object_title = description;
    EBDB_ErrCode err = m_db.object_attr_db.Insert();
    BDB_CHECK(err, "LDS::ObjectAttribute");
*/
    m_db.object_db.object_id = m_MaxObjRecId;
    m_db.object_db.file_id = file_id;
    m_db.object_db.seqlist_id = 0;
    m_db.object_db.object_type = type_id;
    Int8 ipos = NcbiStreamposToInt8(pos);
    m_db.object_db.file_pos = ipos;
//    m_db.object_db.object_attr_id = m_MaxObjRecId;
    m_db.object_db.TSE_object_id = 0;
    m_db.object_db.parent_object_id = 0;
    m_db.object_db.object_title.Set(description.c_str(),
        CBDB_FieldStringBase::eTruncateOnOverflowLogError);
    m_db.object_db.seq_ids = seq_ids;

    string ups = seq_id;
    NStr::ToUpper(ups);
    m_db.object_db.primary_seqid = ups;

    LOG_POST_X(6, Info << "Saving Fasta object: " << seq_id);

    err = m_db.object_db.Insert();
    BDB_CHECK(err, "LDS::Object");

    return m_MaxObjRecId;
}


int CLDS_Object::SaveObject(CLDS_CoreObjectsReader* objects,
                            CLDS_CoreObjectsReader::SObjectDetails* obj_info,
                            bool force_object)
{
    int top_level_id, parent_id;

    _ASSERT(obj_info->ext_id == 0);  // Making sure the object is not in the DB yet

    if (obj_info->is_top_level) {
        top_level_id = parent_id = 0;
    }
    else {
        // Find the direct parent
        {{
            CLDS_CoreObjectsReader::SObjectDetails* parent_obj_info
                = objects->FindObjectInfo(obj_info->parent_offset);
            _ASSERT(parent_obj_info);
            parent_id = parent_obj_info->ext_id;
            if ( parent_id == 0 ) { // not yet in the database
                // Recursively save the parent
                parent_id = SaveObject(objects, parent_obj_info, true);
            }
        }}

        // Find the top level grand parent
        {{
            CLDS_CoreObjectsReader::SObjectDetails* top_obj_info
                = objects->FindObjectInfo(obj_info->top_level_offset);
            _ASSERT(top_obj_info);
            top_level_id = top_obj_info->ext_id;
            if ( top_level_id == 0 ) { // not yet in the database
                // Recursively save the parent
                top_level_id = SaveObject(objects, top_obj_info, true);
            }
        }}

    }

    const string& type_name = obj_info->info.GetName();

    map<string, int>::const_iterator it = m_ObjTypeMap.find(type_name);
    if (it == m_ObjTypeMap.end()) {
        LOG_POST_X(7, Info << "Unrecognized type: " << type_name);
        return 0;
    }
    int type_id = it->second;


    string id_str;
    string title;
    string all_ids;

    ++m_MaxObjRecId;

    if ( IsObject(*obj_info, &id_str, &title, &all_ids) || force_object ) {
        m_db.object_db.primary_seqid = NStr::ToUpper(id_str);

        obj_info->ext_id = m_MaxObjRecId; // Keep external id for the next scan
        EBDB_ErrCode err;
/*
        m_db.object_attr_db.object_attr_id = m_MaxObjRecId;
        m_db.object_attr_db.object_title = molecule_title;
        m_db.object_attr_db.seq_ids = NStr::ToUpper(all_seq_id);
        EBDB_ErrCode err = m_db.object_attr_db.Insert();
        BDB_CHECK(err, "LDS::ObjectAttr");
*/
        m_db.object_db.object_id = m_MaxObjRecId;
        m_db.object_db.file_id = objects->GetFileId();
        m_db.object_db.seqlist_id = 0;  // TODO:
        m_db.object_db.object_type = type_id;
        Int8 i8 = NcbiStreamposToInt8(obj_info->offset);
        m_db.object_db.file_pos = i8;
//        m_db.object_db.object_attr_id = m_MaxObjRecId;
        m_db.object_db.TSE_object_id = top_level_id;
        m_db.object_db.parent_object_id = parent_id;
        m_db.object_db.object_title = title;
        m_db.object_db.seq_ids = NStr::ToUpper(all_ids);


//        LOG_POST_X(8, Info<<"Saving object: " << type_name << " " << id_str);

        err = m_db.object_db.Insert();
        BDB_CHECK(err, "LDS::Object");

    }
    else if ( CSeq_annot* annot = CType<CSeq_annot>().Get(obj_info->info)) {
        // Set of seq ids referenced in the annotation
        //
        set<string> ref_seq_ids;
        CLDS_Seq_ids *ids =
            m_Seq_idsCollector? m_Seq_idsCollector->GetIds(obj_info->info): 0;
        if ( ids ) {
            ITERATE ( CLDS_Seq_ids::TIds, it, ids->m_Ids ) {
                const CSeq_id& id = **it;
                ref_seq_ids.insert(id.AsFastaString());
            }

            CLDS_Seq_ids::TGis& gis = ids->m_Gis;
            sort(gis.begin(), gis.end());
            gis.erase(unique(gis.begin(), gis.end()), gis.end());
            CSeq_id id;
            ITERATE ( CLDS_Seq_ids::TGis, it, gis ) {
                id.SetGi(*it);
                ref_seq_ids.insert(id.AsFastaString());
            }
            //LOG_POST_X(9, Info <<
            //           "Saving " << ref_seq_ids.size() <<
            //           " Seq-ids in Seq-annot");
        }
        else if ( annot->CanGetData()) {
            // Check for alignment in annotation
            //
            const CSeq_annot_Base::C_Data& adata = annot->GetData();
            if (adata.Which() == CSeq_annot_Base::C_Data::e_Align) {
                const CSeq_annot_Base::C_Data::TAlign& al_list =
                    adata.GetAlign();
                ITERATE (CSeq_annot_Base::C_Data::TAlign, it, al_list){
                    if (!(*it)->CanGetSegs())
                        continue;

                    const CSeq_align::TSegs& segs = (*it)->GetSegs();
                    switch (segs.Which())
                        {
                        case CSeq_align::C_Segs::e_Std:
                        {
                            const CSeq_align_Base::C_Segs::TStd& std_list =
                                segs.GetStd();
                            ITERATE (CSeq_align_Base::C_Segs::TStd, it2, std_list) {
                                const CRef<CStd_seg>& seg = *it2;
                                const CStd_seg::TIds& ids = seg->GetIds();

                                ITERATE(CStd_seg::TIds, it3, ids) {
                                    ref_seq_ids.insert((*it3)->AsFastaString());

                                } // ITERATE

                            } // ITERATE
                        }
                        break;
                        case CSeq_align::C_Segs::e_Denseg:
                        {
                            const CSeq_align_Base::C_Segs::TDenseg& denseg =
                                segs.GetDenseg();
                            const CDense_seg::TIds& ids = denseg.GetIds();

                            ITERATE (CDense_seg::TIds, it3, ids) {
                                ref_seq_ids.insert((*it3)->AsFastaString());
                            } // ITERATE

                        }
                        break;
                        //case CSeq_align::C_Segs::e_Packed:
                        //case CSeq_align::C_Segs::e_Disc:
                        default:
                            break;
                        }

                } // ITERATE
            }
        }

        // Save all seq ids referred by the alignment
        //
        ITERATE (set<string>, it, ref_seq_ids) {
            m_db.seq_id_list.object_id = m_MaxObjRecId;
            m_db.seq_id_list.seq_id = it->c_str();

            EBDB_ErrCode err = m_db.seq_id_list.Insert();
            BDB_CHECK(err, "LDS::seq_id_list");
        }

        obj_info->ext_id = m_MaxObjRecId; // Keep external id for the next scan

        m_db.annot_db.annot_id = m_MaxObjRecId;
        m_db.annot_db.file_id = objects->GetFileId();
        m_db.annot_db.annot_type = type_id;
        Int8 i8 = NcbiStreamposToInt8(obj_info->offset);
        m_db.annot_db.file_pos = i8;
        m_db.annot_db.TSE_object_id = top_level_id;
        m_db.annot_db.parent_object_id = parent_id;
/*
        LOG_POST_X(9, Info << "Saving annotation: "
                           << type_name
                           << " "
                           << id_str
                           << " "
                           << (!top_level_id ? "Top Level. " : " ")
                           << "offs="
                           << obj_info->offset
                  );
*/

        EBDB_ErrCode err = m_db.annot_db.Insert();
        BDB_CHECK(err, "LDS::Annotation");

        m_db.annot2obj_db.object_id = parent_id;
        m_db.annot2obj_db.annot_id = m_MaxObjRecId;

        err = m_db.annot2obj_db.Insert();
        BDB_CHECK(err, "LDS::Annot2Obj");

    }

    return obj_info->ext_id;
}


CScope* CLDS_Object::GetScope(void)
{
    if ( !m_Scope && m_TSE ) {
        m_Scope = new CScope(*m_TSE_Manager);
        m_Scope->AddTopLevelSeqEntry(*m_TSE);
    }
    return m_Scope;
}


bool
CLDS_Object::IsObject(const CLDS_CoreObjectsReader::SObjectDetails& parse_info,
                      string* object_str_id,
                      string* object_title,
                      string* object_all_ids)
{
    if ( CREATE_SCOPES && parse_info.is_top_level ) {
        m_TSE_Manager = CObjectManager::GetInstance();
        m_Scope.Reset();
        m_TSE_Info = parse_info.info;
        m_TSE.Reset();
        if ( CSeq_entry* obj = CType<CSeq_entry>().Get(m_TSE_Info) ) {
            m_TSE = obj;
            m_TSE->Parentize();
            return true;
        }
        else if ( CBioseq_set* obj = CType<CBioseq_set>().Get(m_TSE_Info) ) {
            m_TSE = new CSeq_entry;
            m_TSE->SetSet(*obj);
            m_TSE->Parentize();
            return true;
        }
        else if ( CBioseq* obj = CType<CBioseq>().Get(m_TSE_Info) ) {
            m_TSE = new CSeq_entry;
            m_TSE->SetSeq(*obj);
            m_TSE->Parentize();
            GetBioseqInfo(parse_info, *obj,
                          object_str_id, object_title, object_all_ids);
            return true;
        }
        else if ( CSeq_annot* obj = CType<CSeq_annot>().Get(m_TSE_Info) ) {
            m_TSE = new CSeq_entry;
            m_TSE->SetSet().SetSeq_set();
            m_TSE->SetSet().SetAnnot().push_back(Ref(obj));
            m_TSE->Parentize();
            GetAnnotInfo(parse_info, *obj,
                         object_str_id, object_title, object_all_ids);
            return true;
        }
        else if ( CSeq_align* obj = CType<CSeq_align>().Get(m_TSE_Info) ) {
            CRef<CSeq_annot> annot(new CSeq_annot);
            CSeq_annot::TData::TAlign& arr = annot->SetData().SetAlign();
            arr.push_back(Ref(obj));
            m_TSE = new CSeq_entry;
            m_TSE->SetSet().SetSeq_set();
            m_TSE->SetSet().SetAnnot().push_back(annot);
            m_TSE->Parentize();
            GetAnnotInfo(parse_info, *annot,
                         object_str_id, object_title, object_all_ids);
            return true;
        }
        else if (CSeq_align_set* obj=CType<CSeq_align_set>().Get(m_TSE_Info)) {
            CRef<CSeq_annot> annot(new CSeq_annot);
            CSeq_annot::TData::TAlign& arr = annot->SetData().SetAlign();
            NON_CONST_ITERATE ( CSeq_align_set::Tdata, it, obj->Set() ) {
                arr.push_back(*it);
            }
            m_TSE = new CSeq_entry;
            m_TSE->SetSet().SetSeq_set();
            m_TSE->SetSet().SetAnnot().push_back(annot);
            m_TSE->Parentize();
            GetAnnotInfo(parse_info, *annot,
                         object_str_id, object_title, object_all_ids);
            return true;
        }
    }

    if ( CBioseq* obj = CType<CBioseq>().Get(parse_info.info) ) {
        GetBioseqInfo(parse_info, *obj,
                      object_str_id, object_title, object_all_ids);
        return true;
    }
    else if ( CType<CSeq_annot>().Get(parse_info.info) ||
              CType<CSeq_align>().Get(parse_info.info) ||
              CType<CSeq_align_set>().Get(parse_info.info) ) {
        return false;
    }
    return true;
}


void CLDS_Object::GetBioseqInfo(const CLDS_CoreObjectsReader::SObjectDetails& /*obj_info*/,
                                const CBioseq& bioseq,
                                string* object_str_id,
                                string* object_title,
                                string* object_all_ids)
{
    const CSeq_id* seq_id = bioseq.GetFirstId();
    if ( seq_id ) {
        *object_str_id = seq_id->AsFastaString();
    }

    if ( TRY_FAST_TITLE && sequence::GetTitle(bioseq, object_title) ) {
        // Good, we've got title fast way.
    }
    else if (CScope* scope = GetScope()) { // we are under OM here
        CBioseq_Handle bio_handle = scope->GetBioseqHandle(bioseq);
        if ( bio_handle ) {
            *object_title = sequence::GetTitle(bio_handle);
            //LOG_POST_X(10, Info<<"object title: "<<*molecule_title);
        }
        else {
            // the last resort
            bioseq.GetLabel(object_title, CBioseq::eBoth);
        }

    }
    else {  // non-OM controlled object
        bioseq.GetLabel(object_title, CBioseq::eBoth);
    }

    ITERATE ( CBioseq::TId, it, bioseq.GetId() ) {
        const CSeq_id* seq_id = *it;
        if ( seq_id ) {
            object_all_ids->append(seq_id->AsFastaString());
            object_all_ids->append(" ");
        }
    }
}


void CLDS_Object::GetAnnotInfo(const CLDS_CoreObjectsReader::SObjectDetails& obj_info,
                               const CSeq_annot& annot,
                               string* object_str_id,
                               string* object_title,
                               string* object_all_ids)
{
    set<string> ref_seq_ids;
    CLDS_Seq_ids *ids =
        m_Seq_idsCollector? m_Seq_idsCollector->GetIds(obj_info.info): 0;
    if ( ids ) {
        ITERATE ( CLDS_Seq_ids::TIds, it, ids->m_Ids ) {
            const CSeq_id& id = **it;
            string str_id = id.AsFastaString();
            ref_seq_ids.insert(NStr::ToUpper(str_id));
        }

        CLDS_Seq_ids::TGis& gis = ids->m_Gis;
        sort(gis.begin(), gis.end());
        gis.erase(unique(gis.begin(), gis.end()), gis.end());
        CSeq_id id;
        ITERATE ( CLDS_Seq_ids::TGis, it, gis ) {
            id.SetGi(*it);
            string str_id = id.AsFastaString();
            ref_seq_ids.insert(NStr::ToUpper(str_id));
        }
        //LOG_POST_X(9, Info <<
        //           "Saving " << ref_seq_ids.size() <<
        //           " Seq-ids in Seq-annot");
    }
    else if ( annot.CanGetData() ) {
        // Check for alignment in annotation
        //
        const CSeq_annot_Base::C_Data& adata = annot.GetData();
        if ( adata.IsAlign() ) {
            const CSeq_annot_Base::C_Data::TAlign& al_list = adata.GetAlign();
            ITERATE (CSeq_annot_Base::C_Data::TAlign, it, al_list){
                if (!(*it)->CanGetSegs())
                    continue;

                const CSeq_align::TSegs& segs = (*it)->GetSegs();
                switch (segs.Which())
                    {
                    case CSeq_align::C_Segs::e_Std:
                    {
                        const CSeq_align_Base::C_Segs::TStd& std_list =
                            segs.GetStd();
                        ITERATE (CSeq_align_Base::C_Segs::TStd, it2, std_list) {
                            const CRef<CStd_seg>& seg = *it2;
                            const CStd_seg::TIds& ids = seg->GetIds();

                            ITERATE(CStd_seg::TIds, it3, ids) {
                                string str_id = (*it3)->AsFastaString();
                                ref_seq_ids.insert(NStr::ToUpper(str_id));

                            } // ITERATE

                        } // ITERATE
                    }
                    break;
                    case CSeq_align::C_Segs::e_Denseg:
                    {
                        const CSeq_align_Base::C_Segs::TDenseg& denseg =
                            segs.GetDenseg();
                        const CDense_seg::TIds& ids = denseg.GetIds();

                        ITERATE (CDense_seg::TIds, it3, ids) {
                            string str_id = (*it3)->AsFastaString();
                            ref_seq_ids.insert(NStr::ToUpper(str_id));
                        } // ITERATE

                    }
                    break;
                    //case CSeq_align::C_Segs::e_Packed:
                    //case CSeq_align::C_Segs::e_Disc:
                    default:
                        break;
                    }

            } // ITERATE
        }
        else {
            for (CTypeConstIterator<CSeq_id> it(ConstBegin(annot)); it; ++it) {
                const CSeq_id& id = *it;
                string str_id = id.AsFastaString();
                ref_seq_ids.insert(NStr::ToUpper(str_id));
            }
        }
    }

    // Save all seq ids referred by the alignment
    //
    ITERATE (set<string>, it, ref_seq_ids) {
        object_all_ids->append(*it);
        object_all_ids->append(" ");
    }
}


int CLDS_Object::FindMaxObjRecId()
{
    if (m_MaxObjRecId) {
        return m_MaxObjRecId;
    }

    LDS_GETMAXID(m_MaxObjRecId, m_db.object_db, object_id);

    int ann_rec_id = 0;
    LDS_GETMAXID(ann_rec_id, m_db.annot_db, annot_id);

    if (ann_rec_id > m_MaxObjRecId) {
        m_MaxObjRecId = ann_rec_id;
    }

    return m_MaxObjRecId;
}


static bool s_GetSequenceBase(const CObject_id& obj_id,
                              SLDS_SeqIdBase*  seqid_base)
{
    switch (obj_id.Which()) {
    case CObject_id::e_Id:
        seqid_base->int_id = obj_id.GetId();
        seqid_base->str_id.erase();
        return true;
    case CObject_id::e_Str:
        seqid_base->int_id = 0;
        seqid_base->str_id = obj_id.GetStr();
        return true;
    default:
        break;
    }
    return false;
}


static bool s_GetSequenceBase(const CPDB_seq_id& pdb_id,
                              SLDS_SeqIdBase*  seqid_base)
{
    seqid_base->int_id = 0;
    seqid_base->str_id = pdb_id.GetMol().Get();
    seqid_base->str_id += '|';
    char chain = (char) pdb_id.GetChain();
    if ( chain == '|' ) {
        seqid_base->str_id += "VB";
    }
    else if ( chain == '\0' ) {
        seqid_base->str_id += ' ';
    }
    else if ( islower((unsigned char)chain) ) {
        seqid_base->str_id.append(2, chain);
    }
    else {
        seqid_base->str_id += chain;
    }
    return true;
}


void LDS_GetSequenceBase(const CSeq_id&   seq_id,
                         SLDS_SeqIdBase*  seqid_base)
{
    _ASSERT(seqid_base);

    int   obj_id_int = 0;
    const CTextseq_id* obj_id_txt = 0;

    switch (seq_id.Which()) {
    case CSeq_id::e_Local:
        if ( s_GetSequenceBase(seq_id.GetLocal(), seqid_base) ) {
            return;
        }
        break;
    case CSeq_id::e_Gibbsq:
        obj_id_int = seq_id.GetGibbsq();
        break;
    case CSeq_id::e_Gibbmt:
        obj_id_int = seq_id.GetGibbmt();
        break;
    case CSeq_id::e_Giim:
        obj_id_int = seq_id.GetGiim().GetId();
        break;
    case CSeq_id::e_Genbank:
        obj_id_txt = &seq_id.GetGenbank();
        break;
    case CSeq_id::e_Embl:
        obj_id_txt = &seq_id.GetEmbl();
        break;
    case CSeq_id::e_Pir:
        obj_id_txt = &seq_id.GetPir();
        break;
    case CSeq_id::e_Swissprot:
        obj_id_txt = &seq_id.GetSwissprot();
        break;
    case CSeq_id::e_Patent:
        {{
            seqid_base->int_id = 0;
            seqid_base->str_id = "";
            const CId_pat& pat = seq_id.GetPatent().GetCit();
            pat.GetLabel(&seqid_base->str_id);
        }}
        return;
    case CSeq_id::e_Other:
        obj_id_txt = &seq_id.GetOther();
        break;
    case CSeq_id::e_General:
        {{
            seqid_base->int_id = 0;
            seqid_base->str_id = "";
            seq_id.GetGeneral().GetLabel(&seqid_base->str_id);
        }}
        return;
    case CSeq_id::e_Gi:
        obj_id_int = seq_id.GetGi();
        break;
    case CSeq_id::e_Ddbj:
        obj_id_txt = &seq_id.GetDdbj();
        break;
    case CSeq_id::e_Prf:
        obj_id_txt = &seq_id.GetPrf();
        break;
    case CSeq_id::e_Pdb:
        if ( s_GetSequenceBase(seq_id.GetPdb(), seqid_base) ) {
            return;
        }
        break;
    case CSeq_id::e_Tpg:
        obj_id_txt = &seq_id.GetTpg();
        break;
    case CSeq_id::e_Tpe:
        obj_id_txt = &seq_id.GetTpe();
        break;
    case CSeq_id::e_Tpd:
        obj_id_txt = &seq_id.GetTpd();
        break;
    case CSeq_id::e_Gpipe:
        obj_id_txt = &seq_id.GetGpipe();
        break;
    default:
        _ASSERT(0);
        break;
    }

    const string* id_str = 0;

    if (obj_id_int) {
        seqid_base->int_id = obj_id_int;
        seqid_base->str_id.erase();
        return;
    }

    if (obj_id_txt) {
        if (obj_id_txt->CanGetAccession()) {
            const CTextseq_id::TAccession& acc =
                                obj_id_txt->GetAccession();
            id_str = &acc;
        } else {
            if (obj_id_txt->CanGetName()) {
                const CTextseq_id::TName& name =
                    obj_id_txt->GetName();
                id_str = &name;
            }
        }
    }

    if (id_str) {
        seqid_base->int_id = 0;
        seqid_base->str_id = *id_str;
        return;
    }

    LOG_POST_X(11, Warning
               << "SeqId indexer: unsupported type "
               << seq_id.AsFastaString());

    seqid_base->Init();

}

bool LDS_GetSequenceBase(const string&   seq_id_str,
                         SLDS_SeqIdBase* seqid_base,
                         CSeq_id*        conv_seq_id)
{
    if ( seq_id_str.empty() ) {
        return false;
    }

    _ASSERT(seqid_base);

    CRef<CSeq_id> tmp_seq_id;

    if (conv_seq_id == 0) {
        tmp_seq_id.Reset((conv_seq_id = new CSeq_id));

    }

    bool can_convert = true;

    try {
        conv_seq_id->Set(seq_id_str);
    } catch (CSeqIdException&) {
        try {
            conv_seq_id->Set(CSeq_id::e_Local, seq_id_str);
        } catch (CSeqIdException&) {
            can_convert = false;
            LOG_POST_X(12, Error <<
                       "Cannot convert seq id string: " << seq_id_str);
            seqid_base->Init();
        }
    }

    if (can_convert) {
        LDS_GetSequenceBase(*conv_seq_id, seqid_base);
    }

    return can_convert;
}


/// Scanner functor to build id index
///
/// @internal
///
class CLDS_BuildIdIdx
{
public:
    CLDS_BuildIdIdx(CLDS_Database& db, bool control_dups)
    : m_db(db),
        m_coll(db.GetTables()),
        m_SeqId(new CSeq_id),
        m_ControlDups(control_dups),
        m_Query(new CLDS_Query(db)),
        m_ObjIds(bm::BM_GAP)
    {
        if (m_ControlDups) {
            m_SequenceFind.reset(new CLDS_Query::CSequenceFinder(*m_Query));
        }
    }

    void operator()(SLDS_ObjectDB& dbf)
    {
        int object_id = dbf.object_id; // PK

        if (!dbf.primary_seqid.IsNull()) {
            dbf.primary_seqid.ToString(m_PriSeqId_Str);

            x_AddToIdx(m_PriSeqId_Str, object_id);
        }

        dbf.seq_ids.ToString(m_SeqId_Str);
        vector<string> seq_id_arr;
        NStr::Tokenize(m_SeqId_Str, " ", seq_id_arr, NStr::eMergeDelims);
        ITERATE (vector<string>, it, seq_id_arr) {
            const string& seq_id_str = *it;
            if (NStr::CompareNocase(seq_id_str,m_PriSeqId_Str)==0) {
                continue;
            }
            x_AddToIdx(seq_id_str, object_id);
        }
    }

private:
    void x_AddToIdx(const string& seq_id_str, int rec_id)
    {
        bool can_convert =
            LDS_GetSequenceBase(seq_id_str, &m_SBase, &*m_SeqId);
        if (can_convert) {
            if (m_ControlDups) {
                _ASSERT(m_SequenceFind.get());
                CLDS_Set& cand = m_SequenceFind->GetCandidates();
                cand.clear();
                m_SequenceFind->Screen(m_SBase);
                if (cand.any()) {
                    CLDS_Set dup_ids(bm::BM_GAP);
                    m_SequenceFind->FindInCandidates(seq_id_str, &dup_ids);

                    if (dup_ids.any()) {
                        unsigned id = dup_ids.get_first();
                        m_Query->ReportDuplicateObjectSeqId(seq_id_str,
                                                            id,
                                                            rec_id);
                    }
                }
            }

            x_AddToIdx(m_SBase, rec_id);
        }
    }

    void x_AddToIdx(const SLDS_SeqIdBase& sbase, int rec_id)
    {
        if (sbase.int_id) {
            _TRACE("int id: "<<sbase.int_id<<" -> "<<rec_id);
            m_coll.obj_seqid_int_idx.id = sbase.int_id;
            m_coll.obj_seqid_int_idx.row_id = rec_id;
            m_coll.obj_seqid_int_idx.Insert();
        }
        else if (!sbase.str_id.empty()) {
            _TRACE("str id: "<<sbase.str_id<<" -> "<<rec_id);
            m_coll.obj_seqid_txt_idx.id = sbase.str_id;
            m_coll.obj_seqid_txt_idx.row_id = rec_id;
            m_coll.obj_seqid_txt_idx.Insert();
        }
    }

private:
    CLDS_BuildIdIdx(const CLDS_BuildIdIdx&);
    CLDS_BuildIdIdx& operator=(const CLDS_BuildIdIdx&);

private:
    CLDS_Database&         m_db;
    SLDS_TablesCollection& m_coll;
    string                 m_PriSeqId_Str;
    string                 m_SeqId_Str;
    CRef<CSeq_id>          m_SeqId;
    SLDS_SeqIdBase         m_SBase;
    bool                   m_ControlDups; ///< Control id duplicates
    auto_ptr<CLDS_Query>                  m_Query;
    auto_ptr<CLDS_Query::CSequenceFinder> m_SequenceFind;
    CLDS_Set               m_ObjIds;  ///< id set for duplicate search
};

void CLDS_Object::BuildSeqIdIdx()
{
    m_db.obj_seqid_int_idx.Truncate();
    m_db.obj_seqid_txt_idx.Truncate();

    LOG_POST_X(13, Info << "Building sequence id index on objects...");

    CLDS_BuildIdIdx func(m_DataBase, m_ControlDupIds);
    BDB_iterate_file(m_db.object_db, func);
}


CLDS_Object::CLDS_Object(CLDS_Database& db,
                         const map<string, int>& obj_map)
: m_DataBase(db),
  m_db(db.GetTables()),
  m_ObjTypeMap(obj_map),
  m_MaxObjRecId(0),
  m_ControlDupIds(false),
  m_GBReleaseMode(eDefaultGBReleaseMode)
{
}


CLDS_Object::~CLDS_Object()
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
