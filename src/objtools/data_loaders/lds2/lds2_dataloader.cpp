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
 * File Description:  LDS2 dataloader.
 *
 */


#include <ncbi_pch.hpp>

#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_LDS2_Loader

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

DEFINE_STATIC_FAST_MUTEX(sx_LDS2_Lock);

#define LDS2_GUARD() CFastMutexGuard guard(sx_LDS2_Lock)


string CLDS2_DataLoader::GetLoaderNameFromArgs(void)
{
    return "LDS2_dataloader";
}


string CLDS2_DataLoader::GetLoaderNameFromArgs(const string& db_path)
{
    string abs_path = db_path;
    // Try to get absolute path, if failed, use as-is.
    if (db_path != ":memory:") {
        try {
            abs_path = CDirEntry::CreateAbsolutePath(db_path);
        }
        catch (CFileException) {
        }
    }
    return "LDS2_dataloader:" + abs_path;
}


string CLDS2_DataLoader::GetLoaderNameFromArgs(CLDS2_Database& lds_db)
{
    return GetLoaderNameFromArgs(lds_db.GetDbFile());
}


CLDS2_DataLoader::TRegisterLoaderInfo
CLDS2_DataLoader::RegisterInObjectManager(
    CObjectManager&            om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority
    )
{
    TSimpleMaker maker;
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CLDS2_DataLoader::TRegisterLoaderInfo
CLDS2_DataLoader::RegisterInObjectManager(
    CObjectManager&            om,
    const string&              db_path,
    CFastaReader::TFlags       fasta_flags,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority
    )
{
    CLDS2_LoaderMaker maker(db_path, fasta_flags);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CLDS2_DataLoader::TRegisterLoaderInfo
CLDS2_DataLoader::RegisterInObjectManager(
    CObjectManager&            om,
    CLDS2_Database&            lds_db,
    CFastaReader::TFlags       fasta_flags,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CLDS2_LoaderMaker maker(lds_db, fasta_flags);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CLDS2_DataLoader::
CLDS2_LoaderMaker::CLDS2_LoaderMaker(const string&        db_path,
                                     CFastaReader::TFlags fasta_flags)
    : m_DbPath(db_path),
      m_FastaFlags(fasta_flags)
{
    m_Name = CLDS2_DataLoader::GetLoaderNameFromArgs(db_path);
}


CLDS2_DataLoader::
CLDS2_LoaderMaker::CLDS2_LoaderMaker(CLDS2_Database&      db,
                                     CFastaReader::TFlags fasta_flags)
    : m_Db(&db),
      m_DbPath(db.GetDbFile()),
      m_FastaFlags(fasta_flags)
{
    m_Name = CLDS2_DataLoader::GetLoaderNameFromArgs(m_DbPath);
}


CDataLoader*
CLDS2_DataLoader::CLDS2_LoaderMaker::CreateLoader(void) const
{
    LDS2_GUARD();

    if ( !m_Db ) {
        m_Db.Reset(new CLDS2_Database(m_DbPath));
    }
    CLDS2_DataLoader* dl = new CLDS2_DataLoader(m_Name, *m_Db, m_FastaFlags);
    return dl;
}


CLDS2_DataLoader::CLDS2_DataLoader(const string&  dl_name)
 : CDataLoader(dl_name)
{
    // This leaves the LDS2 manager uninitialized!
}


CLDS2_DataLoader::CLDS2_DataLoader(const string&        dl_name,
                                   CLDS2_Database&      lds_db,
                                   CFastaReader::TFlags fasta_flags)
 : CDataLoader(dl_name),
   m_Db(&lds_db)
{
    if (fasta_flags == -1) {
        fasta_flags =
            CFastaReader::fAssumeNuc  |
            CFastaReader::fOneSeq     |
            CFastaReader::fParseRawID |
            CFastaReader::fStrictGuess;
    }
    m_FastaFlags = fasta_flags;
    // Initialize default handlers
    RegisterUrlHandler(new CLDS2_UrlHandler_File);
    RegisterUrlHandler(new CLDS2_UrlHandler_GZipFile);
    if ( m_Db ) {
        m_Db->BeginRead();
    }
}


CLDS2_DataLoader::~CLDS2_DataLoader()
{
    if ( m_Db ) {
        m_Db->EndRead();
    }
}


void CLDS2_DataLoader::RegisterUrlHandler(CLDS2_UrlHandler_Base* handler)
{
    _ASSERT(handler);
    m_Handlers[handler->GetHandlerName()] = Ref(handler);
}


CLDS2_UrlHandler_Base*
CLDS2_DataLoader::x_GetUrlHandler(const SLDS2_File& info)
{
    CLDS2_UrlHandler_Base* ret = NULL;
    THandlers::iterator it = m_Handlers.find(info.handler);
    if (it != m_Handlers.end()) {
        ret = it->second.GetPointerOrNull();
    }
    return ret;
}


// Blob-id is Int8. Define all necessary types and hepler functions.

template<>
struct PConvertToString<Int8>
    : public unary_function<Int8, string>
{
    string operator()(Int8 v) const
        {
            return NStr::Int8ToString(v);
        }
};

typedef CBlobIdFor<Int8> CBlobIdInt8;

inline
static CLDS2_DataLoader::TBlobId s_Int8ToBlobId(Int8 id)
{
    return CBlobIdKey(new CBlobIdInt8(id));
}



CRef<CSeq_entry> CLDS2_DataLoader::x_LoadFastaTSE(CNcbiIstream&     in,
                                                  const SLDS2_Blob& blob)
{
    CStreamLineReader lr(in);
    CFastaReader fr(lr, m_FastaFlags);
    return fr.ReadOneSeq();
}


CRef<CSeq_entry> CLDS2_DataLoader::x_LoadTSE(const SLDS2_Blob& blob)
{
    _ASSERT(blob.id > 0);
    CRef<CSeq_entry> entry(new CSeq_entry);
    SLDS2_File finfo = m_Db->GetFileInfo(blob.file_id);
    CRef<CLDS2_UrlHandler_Base> handler(x_GetUrlHandler(finfo));
    if ( !handler ) {
        ERR_POST_X(2, "Error loading blob: URL handler '" <<
            finfo.handler << "' not found");
        // Return null on errors
        entry.Reset();
        return entry;
    }
    auto_ptr<CNcbiIstream> in(handler->OpenStream(finfo, blob.file_pos, m_Db));
    _ASSERT(in.get());
    if (finfo.format == CFormatGuess::eFasta) {
        return x_LoadFastaTSE(*in, blob);
    }
    try {
        auto_ptr<CObjectIStream> obj_in;
        switch ( finfo.format ) {
        case CFormatGuess::eBinaryASN:
            obj_in.reset(CObjectIStream::Open(eSerial_AsnBinary, *in));
            break;
        case CFormatGuess::eTextASN:
            obj_in.reset(CObjectIStream::Open(eSerial_AsnText, *in));
            break;
        case CFormatGuess::eXml:
            obj_in.reset(CObjectIStream::Open(eSerial_Xml, *in));
            break;
        default:
            return CRef<CSeq_entry>(); // Unknown format, fail
        }
        switch ( blob.type ) {
        case SLDS2_Blob::eSeq_entry:
            *obj_in >> *entry;
            break;
        case SLDS2_Blob::eBioseq:
            *obj_in >> entry->SetSeq();
            break;
        case SLDS2_Blob::eBioseq_set:
            *obj_in >> entry->SetSet();
            break;
        case SLDS2_Blob::eBioseq_set_element:
            {
                // The entries have no headers!
                CObjectTypeInfo objtypeinfo =
                    CObjectTypeInfo(CType<CSeq_entry>());
                CObjectInfo objinfo(entry.GetPointer(),
                    objtypeinfo.GetTypeInfo());
                obj_in->Read(objinfo, CObjectIStream::eNoFileHeader);
                break;
            }
        case SLDS2_Blob::eSeq_annot:
            {
                CRef<CSeq_annot> annot(new CSeq_annot);
                *obj_in >> *annot;
                entry->SetSet().SetAnnot().push_back(annot);
                break;
            }
        case SLDS2_Blob::eSeq_align_set:
            {
                CSeq_align_set aln_set;
                *obj_in >> aln_set;
                CRef<CSeq_annot> annot(new CSeq_annot);
                ITERATE(CSeq_align_set::Tdata, it, aln_set.Set()) {
                    annot->SetData().SetAlign().push_back(*it);
                }
                entry->SetSet().SetAnnot().push_back(annot);
                break;
            }
        case SLDS2_Blob::eSeq_align:
            {
                CRef<CSeq_align> align(new CSeq_align);
                *obj_in >> *align;
                CRef<CSeq_annot> annot(new CSeq_annot);
                annot->SetData().SetAlign().push_back(align);
                entry->SetSet().SetAnnot().push_back(annot);
                break;
            }
        case SLDS2_Blob::eSeq_submit:
            {
                CRef<CSeq_submit> submit( new CSeq_submit );
                *obj_in >> *submit;
                if( submit->IsEntrys() ) {
                    entry->SetSet().SetSeq_set();
                    copy( submit->GetData().GetEntrys().begin(),
                        submit->GetData().GetEntrys().end(),
                        back_inserter(entry->SetSet().SetSeq_set()) );
                }
                break;
            }
        default:
            entry.Reset();
            break;
        }
    }
    catch (CException& ex) {
        ERR_POST_X(1, "Error loading blob: " << ex);
        // Return null on errors
        entry.Reset();
    }
    return entry;
}


void CLDS2_DataLoader::x_LoadBlobs(const TBlobSet& blobs,
                                   TTSE_LockSet&   locks)
{
    CDataSource* data_source = GetDataSource();
    _ASSERT(data_source);

    ITERATE(TBlobSet, it, blobs) {
        _ASSERT(it->id);
        TBlobId blob_id = s_Int8ToBlobId(it->id);
        CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(blob_id);
        if ( !load_lock.IsLoaded() ) {
            CRef<CSeq_entry> seq_entry = x_LoadTSE(*it);
            if ( !seq_entry ) {
                NCBI_THROW2(CBlobStateException, eBlobStateError,
                            "cannot load blob",
                            CBioseq_Handle::fState_no_data);
            }
            load_lock->SetSeq_entry(*seq_entry);
            load_lock.SetLoaded();
        }
        locks.insert(load_lock);
    }
}


CDataLoader::TTSE_LockSet
CLDS2_DataLoader::GetRecords(const CSeq_id_Handle& idh,
                             EChoice               choice)
{
    TTSE_LockSet locks;
    TBlobSet blobs;
    switch ( choice ) {
    case eBlob:
    case eBioseq:
    case eCore:
    case eBioseqCore:
    case eSequence:
        m_Db->GetBioseqBlobs(idh, blobs);
        break;
    case eFeatures:
    case eGraph:
    case eAlign:
    case eAnnot:
        m_Db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_Internal, blobs);
        break;
    case eExtFeatures:
    case eExtGraph:
    case eExtAlign:
    case eExtAnnot:
        m_Db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_External, blobs);
        break;
    case eOrphanAnnot:
        // Make sure there's no bioseq with the id in the database
        if (m_Db->GetBioseqId(idh) > 0) {
            return locks; // No orphan annots
        }
        m_Db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_External, blobs);
        break;
    case eAll:
        m_Db->GetBioseqBlobs(idh, blobs);
        m_Db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_All, blobs);
        break;
    }
    x_LoadBlobs(blobs, locks);
    return locks;
}


CLDS2_DataLoader::TTSE_LockSet
CLDS2_DataLoader::GetExternalRecords(const CBioseq_Info& bioseq)
{
    // Base class version stops on the first synonym returning at least one
    // annot. For LDS2 we need to check all synonyms.
    TTSE_LockSet ret;
    ITERATE (CBioseq_Info::TId, it, bioseq.GetId()) {
        TTSE_LockSet ret2 = GetRecords(*it, eExtAnnot);
        if ( !ret2.empty() ) {
            ret.insert(ret2.begin(), ret2.end());
        }
    }
    return ret;
}


CLDS2_DataLoader::TTSE_LockSet
CLDS2_DataLoader::GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                          const SAnnotSelector* /*sel*/)
{
    return GetRecords(idh, eExtAnnot);
}


CLDS2_DataLoader::TTSE_LockSet
CLDS2_DataLoader::GetExternalAnnotRecords(const CBioseq_Info&   bioseq,
                                          const SAnnotSelector* /*sel*/)
{
    return GetExternalRecords(bioseq);
}


CDataLoader::TTSE_Lock
CLDS2_DataLoader::ResolveConflict(const CSeq_id_Handle& id,
                                  const TTSE_LockSet&   tse_set)
{
    // Select blob with the highest blob-id.
    TTSE_Lock best_tse;
    Int8 best_oid = 0;
    ITERATE (TTSE_LockSet, it, tse_set) {
        const TTSE_Lock& tse = *it;
        const CBlobIdInt8* blob_id =
            dynamic_cast<const CBlobIdInt8*>(&*tse->GetBlobId());
        if ( blob_id ) {
            Int8 oid = blob_id->GetValue();
            if ( !best_tse || oid > best_oid ) {
                best_tse = tse;
                best_oid = oid;
            }
        }
    }
    return best_tse;
}


CLDS2_DataLoader::TBlobId
CLDS2_DataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    SLDS2_Blob blob = m_Db->GetBlobInfo(idh);
    return s_Int8ToBlobId(blob.id);
}


bool CLDS2_DataLoader::CanGetBlobById(void) const
{
    return true;
}


CLDS2_DataLoader::TTSE_Lock
CLDS2_DataLoader::GetBlobById(const TBlobId& blob_id)
{
    Int8 oid;
    if ( const CBlobIdInt8* int8_blob_id =
         dynamic_cast<const CBlobIdInt8*>(&*blob_id) ) {
        oid = int8_blob_id->GetValue();
    }
    else {
        return TTSE_Lock();
    }

    CDataSource* data_source = GetDataSource();
    _ASSERT(data_source);

    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(blob_id);
    if ( !load_lock.IsLoaded() ) {
        SLDS2_Blob blob = m_Db->GetBlobInfo(oid);
        CRef<CSeq_entry> seq_entry = x_LoadTSE(blob);
        if ( !seq_entry ) {
            NCBI_THROW2(CBlobStateException, eBlobStateError,
                        "cannot load blob",
                        CBioseq_Handle::fState_no_data);
        }
        load_lock->SetSeq_entry(*seq_entry);
        load_lock.SetLoaded();
    }
    return TTSE_Lock(load_lock);
}


void CLDS2_DataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    m_Db->GetSynonyms(idh, ids);
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_LDS2(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_LDS2);
}


const string kDataLoader_LDS2_DriverName("lds");

class CLDS2_DataLoaderCF : public CDataLoaderFactory
{
public:
    CLDS2_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_LDS2_DriverName) {}
    virtual ~CLDS2_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CLDS2_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CLDS2_DataLoader::RegisterInObjectManager(om).GetLoader();
    }

    const string& db_path =
        GetParam(GetDriverName(), params,
                 kCFParam_LDS2_DbPath, false);

    string fasta_flags_str =
        GetParam(GetDriverName(), params,
                 kCFParam_LDS2_FastaFlags, false, "-1");
    CFastaReader::TFlags fasta_flags =
        NStr::StringToInt(fasta_flags_str, 0, 0);

    if ( !db_path.empty() ) {
        // Use db path
        return CLDS2_DataLoader::RegisterInObjectManager(
            om,
            db_path,
            fasta_flags,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CLDS2_DataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_LDS2(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CLDS2_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_lds2(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_LDS2(info_list, method);
}


END_NCBI_SCOPE
