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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "split_cache.hpp"

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

// Object manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

// cache
#include <objtools/data_loaders/genbank/readers/id1/reader_id1_cache.hpp>
#include <bdb/bdb_blobcache.hpp>

#include <objmgr/split/blob_splitter.hpp>
#include <objmgr/split/id2_compress.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const int kDefaultCacheBlobAge = 5; // keep objects for 5 days
static const int kDefaultCacheIdAge   = 1; // keep id resolution for 1 day


class CLog
{
public:
    CLog(const CSplitCacheApp* app)
        : m_App(app), m_Stream(0)
        {
        }

    ~CLog(void)
        {
            End();
        }

    void End(void)
        {
            if ( m_Stream ) {
                *m_Stream << NcbiEndl;
                m_Stream = 0;
            }
        }


    CNcbiOstream& Start(void)
        {
            if ( !m_Stream ) {
                m_Stream = &m_App->Info();
            }
            return *m_Stream;
        }
    
    operator CNcbiOstream&(void)
        {
            return Start();
        }

    class CFlusher
    {
    public:
        CFlusher(CNcbiOstream& out)
            : m_Stream(out)
            {
            }
        ~CFlusher(void)
            {
                m_Stream.flush();
            }

        template<typename T>
        CFlusher& operator<<(const T& t)
            {
                m_Stream << t;
                return *this;
            }
        
    private:
        CNcbiOstream& m_Stream;
    };

    template<typename T>
    CFlusher operator<<(const T& t)
        {
            Start() << t;
            return CFlusher(*m_Stream);
        }

private:
    const CSplitCacheApp* m_App;
    CNcbiOstream* m_Stream;
};


CSplitCacheApp::CSplitCacheApp(void)
    : m_DumpAsnText(false), m_DumpAsnBinary(false),
      m_Resplit(false), m_Recurse(false),
      m_RecursionLevel(0)
{
}


CSplitCacheApp::~CSplitCacheApp(void)
{
}


void CSplitCacheApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // set of entries to process
    arg_desc->AddOptionalKey("gi", "Gi",
                             "GI of the Seq-Entry to process",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("gi_list", "GiList",
                             "file with list of GIs to process",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("id", "SeqId",
                             "Seq-id of the Seq-Entry to process",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("id_list", "SeqIdList",
                             "file with list of Seq-ids to process",
                             CArgDescriptions::eInputFile);
    arg_desc->AddFlag("all",
                      "process all entries in cache");
    arg_desc->AddFlag("recurse",
                      "process all entries referenced by specified ones");
    arg_desc->AddFlag("verbose",
                      "issue additional trace messages");

    // cache parameters
    arg_desc->AddDefaultKey("cache_dir", "CacheDir",
                            "directory of GenBank cache",
                            CArgDescriptions::eInputFile,
                            ".genbank_cache");

    // split parameters
    arg_desc->AddDefaultKey
        ("chunk_size", "ChunkSize",
         "approximate size of chunks to create (in KB)",
         CArgDescriptions::eInteger,
         NStr::IntToString(SSplitterParams::kDefaultChunkSize/1024));

    arg_desc->AddFlag("compress",
                      "try to compress split data");
    arg_desc->AddFlag("keep_descriptions",
                      "do not strip descriptions");
    arg_desc->AddFlag("keep_sequence",
                      "do not strip sequence data");
    arg_desc->AddFlag("keep_annotations",
                      "do not strip annotations");

    arg_desc->AddFlag("resplit",
                      "resplit already splitted data");

    // debug parameters
    arg_desc->AddFlag("dump",
                      "dump blobs in ASN.1 text format");
    arg_desc->AddFlag("bdump",
                      "dump blobs in ASN.1 binary format");

    // Program description
    string prog_description = "Example of the C++ object manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CSplitCacheApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);

    SetupCache();
    
    Process();

    return 0;
}


void CSplitCacheApp::SetupCache(void)
{
    const CArgs& args = GetArgs();
    const CNcbiRegistry& reg = GetConfig();
    
    string cache_dir;
    {{ // set cache directory
        if ( args["cache_dir"] ) {
            cache_dir = args["cache_dir"].AsString();
        }
        else {
            cache_dir = reg.GetString("LOCAL_CACHE", "Path",
                                      cache_dir, CNcbiRegistry::eErrPost);
        }
        if ( cache_dir.empty() ) {
            ERR_POST(Fatal << "empty cache directory name");
        }
    }}

    {{ // create cache directory
        LINE("cache directory is \"" << cache_dir << "\"");
        {{
            // make sure our cache directory exists first
            CDir dir(cache_dir);
            if ( !dir.Exists() ) {
                dir.Create();
            }
        }}
    }}

    {{ // blob cache
        CBDB_Cache* cache;
        m_Cache.reset(cache = new CBDB_Cache());
        
        int blob_age = reg.GetInt("LOCAL_CACHE", "Age", kDefaultCacheBlobAge,
                                  CNcbiRegistry::eErrPost);

        // Cache cleaning
        // Objects age should be assigned in days, negative value
        // means cleaning is disabled
            
        if ( blob_age <= 0 ) {
            blob_age = kDefaultCacheBlobAge;
        }
        
        ICache::TTimeStampFlags flags =
            ICache::fTimeStampOnRead |
            ICache::fExpireLeastFrequentlyUsed |
            ICache::fPurgeOnStartup;
        cache->SetTimeStampPolicy(flags, blob_age*24*60*60);
        
        cache->Open(cache_dir.c_str(), "blobs");

        // purge old blobs
        CTime time_stamp(CTime::eCurrent);
        time_t age = time_stamp.GetTimeT();
        age -= 60 * 60 * 24 * blob_age;
        cache->Purge(age);
    }}

    {{ // set cache id age
        CBDB_Cache* cache;
        m_IdCache.reset(cache = new CBDB_Cache());
        
        int id_age = reg.GetInt("LOCAL_CACHE", "IdAge", kDefaultCacheIdAge,
                                CNcbiRegistry::eErrPost);
        
        if ( id_age <= 0 ) {
            id_age = kDefaultCacheIdAge;
        }

        ICache::TTimeStampFlags flags =
            ICache::fTimeStampOnCreate|
            ICache::fCheckExpirationAlways;
        cache->SetTimeStampPolicy(flags, id_age*24*60*60);
        
        cache->Open(cache_dir.c_str(), "ids");
    }}

    {{ // create object manager
        m_ObjMgr = CObjectManager::GetInstance();
    }}

    {{ // create loader
        m_Reader = new CCachedId1Reader(1, &*m_Cache, &*m_IdCache);
        m_Loader.Reset(CGBDataLoader::RegisterInObjectManager(
            *m_ObjMgr, m_Reader).GetLoader());
    }}

    {{ // Create scope
        m_Scope.Reset(new CScope(*m_ObjMgr));
        m_Scope->AddDefaults();
    }}
}


CNcbiOstream& CSplitCacheApp::Info(void) const
{
    for ( size_t i = 0; i < m_RecursionLevel; ++i ) {
        NcbiCout << "  ";
    }
    return NcbiCout;
}


class CSplitDataMaker
{
public:
    CSplitDataMaker(const SSplitterParams& params, int data_type)
        : m_Params(params), m_DataType(data_type)
        {
        }

    template<class C>
    void operator<<(const C& obj)
        {
            OpenDataStream() << obj;
            CloseDataStream(m_DataType);
        }

    CObjectOStream& OpenDataStream(void)
        {
            m_OStream.reset();
            m_MStream.reset(new CNcbiOstrstream);
            m_OStream.reset(CObjectOStream::Open(eSerial_AsnBinary,
                                                 *m_MStream));
            return *m_OStream;
        }

    void CloseDataStream(int data_type)
        {
            m_Data.Reset();
            m_Data.SetData_type(data_type);
            m_Data.SetData_format(eSerial_AsnBinary);
            m_Data.SetData_compression(m_Params.m_Compression);

            m_OStream.reset();
            size_t size = m_MStream->pcount();
            const char* data = m_MStream->str();
            m_MStream->freeze(false);
            CId2Compressor::Compress(m_Params, m_Data.SetData(), data, size);
            m_MStream.reset();
        }

    const CID2_Reply_Data& GetData(void) const
        {
            return m_Data;
        }

private:
    SSplitterParams m_Params;
    CID2_Reply_Data m_Data;
    int m_DataType;

    AutoPtr<CNcbiOstrstream> m_MStream;
    AutoPtr<CObjectOStream>  m_OStream;
};


string CSplitCacheApp::GetFileName(const string& key,
                                   const string& suffix,
                                   const string& ext)
{
    string dir = key;
    string file = key + suffix;
    if ( !suffix.empty() && !ext.empty() ) {
        dir = dir + CDirEntry::GetPathSeparator() + ext;
    }
    CDir(dir).CreatePath();
    return CDirEntry::MakePath(dir, file, ext);
}


void CSplitCacheApp::Process(void)
{
    const CArgs& args = GetArgs();

    m_SplitterParams.m_Verbose = args["verbose"];
    m_DumpAsnText = args["dump"];
    m_DumpAsnBinary = args["bdump"];
    if ( args["compress"] ) {
        m_SplitterParams.m_Compression = m_SplitterParams.eCompression_nlm_zip;
    }
    m_SplitterParams.m_DisableSplitDescriptions |= args["keep_descriptions"];
    m_SplitterParams.m_DisableSplitSequence |= args["keep_sequence"];
    m_SplitterParams.m_DisableSplitAnnotations |= args["keep_annotations"];
    m_Resplit = args["resplit"];
    m_Recurse = args["recurse"];
    m_SplitterParams.SetChunkSize(args["chunk_size"].AsInteger()*1024);

    if ( args["gi"] ) {
        ProcessGi(args["gi"].AsInteger());
    }
    if ( args["gi_list"] ) {
        CNcbiIstream& in = args["gi_list"].AsInputFile();
        int gi;
        while ( in >> gi ) {
            ProcessGi(gi);
        }
    }
    if ( args["id"] ) {
        CSeq_id id(args["id"].AsString());
        ProcessSeqId(id);
    }
    if ( args["id_list"] ) {
        CNcbiIstream& in = args["id_list"].AsInputFile();
        string id_name;
        while ( NcbiGetline(in, id_name, '\n') ) {
            CSeq_id id(id_name);
            ProcessSeqId(id);
        }
    }
}


void CSplitCacheApp::ProcessGi(int gi)
{
    CSeq_id id;
    id.SetGi(gi);
    ProcessSeqId(id);
}


void CSplitCacheApp::ProcessSeqId(const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( !m_ProcessedIds.insert(idh).second ) {
        // already processed
        return;
    }

    if ( m_RecursionLevel == 0 ) {
        m_Scope->ResetHistory();
    }

    LINE("Processing: " << id.AsFastaString());
    {{
        CLevelGuard level(m_RecursionLevel);
        if ( m_Resplit ) {
            CConstRef<CSeqref> seqref;
            int version;
            {{
                WAIT_LINE << "Resolving Seq-id...";
                seqref = m_Loader->GetSatSatkey(id);
                version = m_Reader->GetVersion(*seqref, 0);
            }}
            string blob_key = m_Reader->GetBlobKey(*seqref);
            if ( m_Cache->GetSize(blob_key, version,
                                  m_Reader->GetSkeletonSubkey()) ) {
                WAIT_LINE << "Removing old cache data...";
                m_Cache->Store(blob_key, version,
                               m_Reader->GetSkeletonSubkey(), 0, 0);
            }
        }
        CBioseq_Handle bh;
        {{
            WAIT_LINE << "Loading Bioseq...";
            bh = m_Scope->GetBioseqHandle(id);
        }}
        if ( bh ) {
            /*
              WAIT_LINE << "Loading Sequence...";
              string sequence;
              bh.GetSeqVector().GetSeqData(sequence);
            */
            ProcessBlob(bh.GetTopLevelEntry());
        }
        
        /*
        CId1Reader::TSeqrefs srs;
        m_Reader->ResolveSeq_id(srs, id, 0);
        if ( srs.empty() ) {
            LINE("Skipping: no blobs");
            return;
        }
        ITERATE ( CId1Reader::TSeqrefs, it, srs ) {
            ProcessBlob(id, **it);
        }
        */

        if ( m_Recurse ) {
            LINE("Processing referenced sequences:");
            CLevelGuard level(m_RecursionLevel);
            if ( bh ) {
                CSeqMap_CI it = bh.GetSeqMap()
                    .begin_resolved(&*m_Scope, 0, CSeqMap::fFindRef);
                while ( it ) {
                    ProcessSeqId(*it.GetRefSeqid().GetSeqId());
                    ++it;
                }
            }
        }
    }}
    LINE("End of processing: " << id.AsFastaString());
}


template<class C>
void Dump(CSplitCacheApp* app, const C& obj, ESerialDataFormat format,
          const string& key, const string& suffix = kEmptyStr)
{
    string ext;
    switch ( format ) {
    case eSerial_AsnText:   ext = "asn"; break;
    case eSerial_AsnBinary: ext = "asb"; break;
    case eSerial_Xml:       ext = "xml"; break;
    default:                ext = "asn"; break;
    }
    string file_name = app->GetFileName(key, suffix, ext);
    WAIT_LINE4(app) << "Dumping to " << file_name << " ...";
    AutoPtr<CObjectOStream> out(CObjectOStream::Open(file_name,
                                                     format));
    *out << obj;
}


enum EDataType
{
    eDataType_MainBlob = 0,
    eDataType_SplitInfo = 1,
    eDataType_Chunk = 2
};


template<class C>
void DumpData(CSplitCacheApp* app, const C& obj, EDataType data_type,
              const string& key, const string& suffix = kEmptyStr)
{
    string file_name = app->GetFileName(key, suffix, "bin");
    WAIT_LINE4(app) << "Storing to " << file_name << " ...";
    CSplitDataMaker data(app->GetParams(), data_type);
    data << obj;
    AutoPtr<CObjectOStream> out
        (CObjectOStream::Open(file_name, eSerial_AsnBinary));
    *out << data.GetData();
}


template<class C>
void StoreToCache(CSplitCacheApp* app, const C& obj, EDataType data_type,
                  const CSeqref& seqref, const string& suffix = kEmptyStr)
{
    string key = app->GetReader().GetBlobKey(seqref);
    WAIT_LINE4(app) << "Storing to cache " << key << "." << suffix << " ...";
    CNcbiOstrstream stream;
    {{
        CSplitDataMaker data(app->GetParams(), data_type);
        data << obj;
        AutoPtr<CObjectOStream> out
            (CObjectOStream::Open(eSerial_AsnBinary, stream));
        *out << data.GetData();
    }}
    size_t size = stream.pcount();
    line << setiosflags(ios::fixed) << setprecision(2) <<
        " " << setw(7) << (size/1024.0) << " KB";
    const char* data = stream.str();
    stream.freeze(false);
    app->GetCache().Store(key, seqref.GetVersion(), suffix, data, size);
}


void CSplitCacheApp::ProcessBlob(const CSeq_entry_Handle& tse)
{
    CConstRef<CSeqref> seqref = GetSeqref(tse);
    {
        CSeqref::TKeyByTSE key = seqref->GetKeyByTSE();
        pair<TProcessedBlobs::iterator, bool> ins =
            m_ProcessedBlobs.insert(key);
        if ( !ins.second ) {
            // already processed
            return;
        }
    }

    LINE("Processing blob "<< seqref->printTSE());
    CLevelGuard level(m_RecursionLevel);

    int version = m_Reader->GetVersion(*seqref, 0);
    if ( version > 1 ) {
        CTime time(time_t(version*60));
        LINE("Blob version: " << version << " - " << time.AsString());
    }
    else {
        LINE("Blob version: " << version);
    }

    string blob_key = m_Reader->GetBlobKey(*seqref);
    if ( m_Cache->GetSize(blob_key, version,
                          m_Reader->GetSkeletonSubkey()) ) {
        if ( !m_Resplit ) {
            LINE("Already splitted: skipping");
            return;
        }
    }

    if ( m_Reader->IsSNPSeqref(*seqref) ) {
        LINE("Skipping SNP blob: not implemented");
        return;
    }

    CConstRef<CSeq_entry> seq_entry;
    if ( !m_Reader->IsSNPSeqref(*seqref) ) {
        // get non-SNP blob
        seq_entry = tse.GetCompleteSeq_entry();
    }
    else {
        LINE("Skipping SNP blob: not implemented");
        return;
        /*
          SAnnotSelector sel;
          sel.SetMaxSize(1);
          sel.SetFeatSubtype(CSeqFeatData::eSubtype_variation);
          CFeat_CI it(bh, 0, 0, sel);
          if ( !it ) {
          LINE("Skipping SNP blob: empty");
          return;
          }
          const CSeq_annot& seq_annot = it.GetSeq_annot();
          CConstRef<CSeq_annot_Info> seq_annot_info =
          m_Scope->GetImpl().x_GetSeq_annot_Info(seq_annot);
          blob.Reset(&seq_annot_info->GetTSE_Info());
        */
    }

    if ( m_DumpAsnText ) {
        Dump(this, *seq_entry, eSerial_AsnText, blob_key);
    }
    if ( m_DumpAsnBinary ) {
        Dump(this, *seq_entry, eSerial_AsnBinary, blob_key);
    }

    size_t blob_size =
        m_Cache->GetSize(m_Reader->GetBlobKey(*seqref), version,
                         m_Reader->GetSeqEntrySubkey());
    if ( blob_size == 0 ) {
        LINE("Skipping: blob is not in cache");
        return;
    }
    if ( blob_size <= m_SplitterParams.m_MaxChunkSize ) {
        LINE("Skipping: blob is small enough: " << blob_size);
        return;
    }
    LINE("Blob size: " << blob_size);

    CBlobSplitter splitter(m_SplitterParams);
    if ( !splitter.Split(*seq_entry) ) {
        LINE("Skipping: no chunks after splitting");
        return;
    }

    const CSplitBlob& blob = splitter.GetBlob();
    if ( m_DumpAsnText ) {
        Dump(this, blob.GetMainBlob(), eSerial_AsnText, blob_key, "-main");
        Dump(this, blob.GetSplitInfo(), eSerial_AsnText, blob_key, "-split");
        ITERATE ( CSplitBlob::TChunks, it, blob.GetChunks() ) {
            string suffix = "-chunk-" + NStr::IntToString(it->first);
            Dump(this, *it->second, eSerial_AsnText, blob_key, suffix);
        }
    }
    if ( m_DumpAsnBinary ) {
        Dump(this, blob.GetMainBlob(), eSerial_AsnBinary, blob_key, "-main");
        Dump(this, blob.GetSplitInfo(), eSerial_AsnBinary, blob_key, "-split");
        ITERATE ( CSplitBlob::TChunks, it, blob.GetChunks() ) {
            string suffix = "-chunk-" + NStr::IntToString(it->first);
            Dump(this, *it->second, eSerial_AsnBinary, blob_key, suffix);
        }
    }
    {{ // storing split data
        DumpData(this, blob.GetMainBlob(), eDataType_MainBlob, blob_key, "-main");
        DumpData(this, blob.GetSplitInfo(), eDataType_SplitInfo, blob_key, "-split");
        ITERATE ( CSplitBlob::TChunks, it, blob.GetChunks() ) {
            string suffix = "-chunk-" + NStr::IntToString(it->first);
            DumpData(this, *it->second, eDataType_Chunk, blob_key, suffix);
        }
    }}
    {{ // storing split data into cache
        StoreToCache(this, blob.GetMainBlob(), eDataType_MainBlob, *seqref,
                     m_Reader->GetSkeletonSubkey());
        StoreToCache(this, blob.GetSplitInfo(), eDataType_SplitInfo, *seqref,
                     m_Reader->GetSplitInfoSubkey());
        ITERATE ( CSplitBlob::TChunks, it, blob.GetChunks() ) {
            StoreToCache(this, *it->second, eDataType_Chunk, *seqref,
                         m_Reader->GetChunkSubkey(it->first));
        }
    }}
}


CConstRef<CSeqref> CSplitCacheApp::GetSeqref(const CSeq_entry_Handle tse)
{
    CConstRef<CSeqref> seqref(&m_Loader->GetSeqref(tse.x_GetInfo().GetTSE_Info()));
    //CConstRef<CObject> id = tse.GetBlobId();
    //CConstRef<CSeqref> seqref(dynamic_cast<const CSeqref*>(id.GetPointer()));
    _ASSERT(seqref);
    return seqref;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return ncbi::objects::CSplitCacheApp().AppMain(argc, argv);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2004/07/26 14:13:32  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.23  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.22  2004/07/01 15:41:58  vasilche
* Do not reset DisableSplitDescriptions flag.
*
* Revision 1.21  2004/06/30 21:02:02  vasilche
* Added loading of external annotations from 26 satellite.
*
* Revision 1.20  2004/06/15 14:07:39  vasilche
* Added possibility to split sequences.
*
* Revision 1.19  2004/05/21 21:42:52  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.18  2004/04/28 17:06:26  vasilche
* Load split blobs from new ICache.
*
* Revision 1.17  2004/04/28 16:29:15  vasilche
* Store split results into new ICache.
*
* Revision 1.16  2004/03/16 16:03:11  vasilche
* Removed Windows EOL.
*
* Revision 1.15  2004/03/16 15:47:29  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.14  2004/02/09 19:18:55  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.13  2004/01/22 20:10:37  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.12  2004/01/13 16:55:57  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.11  2004/01/07 17:37:37  vasilche
* Fixed include path to genbank loader.
* Moved split_cache application.
*
* Revision 1.10  2003/12/30 16:06:15  vasilche
* Compression methods moved to separate header: id2_compress.hpp.
*
* Revision 1.9  2003/12/03 19:30:45  kuznets
* Misprint fixed
*
* Revision 1.8  2003/12/02 23:46:20  vasilche
* Fixed INTERNAL COMPILER ERROR on MSVC - splitted expression.
*
* Revision 1.7  2003/12/02 23:24:33  vasilche
* Added "-recurse" option to split all sequences referenced by SeqMap.
*
* Revision 1.6  2003/12/02 19:59:15  vasilche
* Added GetFileName() declaration.
*
* Revision 1.5  2003/12/02 19:12:24  vasilche
* Fixed compilation on MSVC.
*
* Revision 1.4  2003/11/28 20:27:44  vasilche
* Correctly print log lines in LINE macro.
*
* Revision 1.3  2003/11/26 23:05:00  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:03  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:32  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
