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
#include <objects/id2/ID2S_Split_Info.hpp>
#include <objects/id2/ID2S_Chunk_Id.hpp>
#include <objects/id2/ID2S_Chunk.hpp>

// Object manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/desc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/reader.hpp>

// cache
#include <objmgr/reader_id1_cache.hpp>
#include <bdb/bdb_blobcache.hpp>

#include "blob_splitter.hpp"

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(objects);


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


#define LINE CLog(this)
#define WAIT_LINE CLog line(this); line


CSplitCacheApp::CSplitCacheApp(void)
    : m_DumpAsnText(false), m_DumpAsnBinary(false),
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
    arg_desc->AddFlag("resolve_all",
                      "process all entries referenced by specified ones");

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
    
    {{ // set cache directory
        string cache_dir;
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
        LINE << "cache directory is \"" << cache_dir << "\"";
        {{
            // make sure our cache directory exists first
            CDir dir(cache_dir);
            if ( !dir.Exists() ) {
                dir.Create();
            }
        }}

        m_Cache.reset(new CBDB_BLOB_Cache());
        m_Cache->Open(cache_dir.c_str());
    }}

    {{ // set cache blob age
        int blob_age = reg.GetInt("LOCAL_CACHE", "Age", kDefaultCacheBlobAge,
                                  CNcbiRegistry::eErrPost);

        // Cache cleaning
        // Objects age should be assigned in days, negative value
        // means cleaning is disabled
            
        if ( blob_age <= 0 ) {
            blob_age = kDefaultCacheBlobAge;
        }
        
        // purge old blobs
        CTime time_stamp(CTime::eCurrent);
        time_t age = time_stamp.GetTimeT();
        age -= 60 * 60 * 24 * blob_age;
        m_Cache->Purge(age);
    }}

    {{ // set cache id age
        int id_age = reg.GetInt("LOCAL_CACHE", "IdAge", kDefaultCacheIdAge,
                                CNcbiRegistry::eErrPost);
        
        if ( id_age <= 0 ) {
            id_age = kDefaultCacheIdAge;
        }

        m_Cache->GetIntCache()->SetExpirationTime(id_age * 24 * 60 * 60);
    }}

    {{ // create loader
        m_Reader = new CCachedId1Reader(1, &*m_Cache, m_Cache->GetIntCache());
        m_Loader.Reset(new CGBDataLoader("GenBank", m_Reader));
    }}

    {{ // create object manager
        m_ObjMgr.Reset(new CObjectManager);
        m_ObjMgr->RegisterDataLoader(*m_Loader, CObjectManager::eDefault);
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


void CSplitCacheApp::Process(void)
{
    const CArgs& args = GetArgs();

    m_DumpAsnText = args["dump"];
    m_DumpAsnBinary = args["bdump"];
    if ( args["compress"] ) {
        m_SplitterParams.m_Compression = m_SplitterParams.eCompression_nlm_zip;
    }
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
    if ( m_RecursionLevel == 0 ) {
        m_Scope->ResetHistory();
    }

    LINE << "Processing: " << id.AsFastaString();
    CLevelGuard level(m_RecursionLevel);
    
    //m_Loader->GetRecords(CSeq_id_Handle::GetHandle(id), CDataLoader::eAll);
    CReader::TSeqrefs srs;
    m_Reader->RetrieveSeqrefs(srs, id, 0);
    if ( srs.empty() ) {
        LINE << "Skipping: no blobs";
        return;
    }
    ITERATE ( CReader::TSeqrefs, it, srs ) {
        ProcessBlob(**it);
    }
}


class CSplitDataMaker
{
public:
    CSplitDataMaker(const SSplitterParams& params)
        : m_Params(params)
        {
        }

    template<class C>
    void operator<<(const C& obj)
        {
            OpenDataStream() << obj;
            CloseDataStream(-1);
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
            m_Data.SetData_format(0);
            m_Data.SetData_compression(m_Params.m_Compression);
            m_Data.SetData().push_back(new vector<char>());

            m_OStream.reset();
            size_t size = m_MStream->pcount();
            const char* data = m_MStream->str();
            m_MStream->freeze(false);
            m_Params.Compress(*m_Data.SetData().front(), data, size);
            m_MStream.reset();
        }

    const CID2_Reply_Data& GetData(void) const
        {
            return m_Data;
        }

private:
    SSplitterParams m_Params;
    CID2_Reply_Data m_Data;

    AutoPtr<CNcbiOstrstream> m_MStream;
    AutoPtr<CObjectOStream>  m_OStream;
};


static
string GetFileName(const string& key, const string& suffix, const string& ext)
{
    string dir = key;
    string file = key + suffix;
    if ( !suffix.empty() && !ext.empty() ) {
        dir = dir + CDirEntry::GetPathSeparator() + ext;
    }
    CDir(dir).CreatePath();
    return CDirEntry::MakePath(dir, file, ext);
}

template<class C>
inline
void CSplitCacheApp::Dump(const C& obj, ESerialDataFormat format,
                          const string& key, const string& suffix)
{
    string ext;
    switch ( format ) {
    case eSerial_AsnText:   ext = "asn"; break;
    case eSerial_AsnBinary: ext = "asb"; break;
    case eSerial_Xml:       ext = "xml"; break;
    }
    string file_name = GetFileName(key, suffix, ext);
    WAIT_LINE << "Dumping to " << file_name << " ...";
    AutoPtr<CObjectOStream> out(CObjectOStream::Open(file_name, format));
    *out << obj;
}


template<class C>
inline
void CSplitCacheApp::DumpData(const C& obj,
                              const string& key, const string& suffix)
{
    string file_name = GetFileName(key, suffix, "bin");
    WAIT_LINE << "Storing to " << file_name << " ...";
    CSplitDataMaker data(m_SplitterParams);
    data << obj;
    AutoPtr<CObjectOStream> out
        (CObjectOStream::Open(file_name, eSerial_AsnBinary));
    *out << data.GetData();
}


void CSplitCacheApp::ProcessBlob(const CSeqref& seqref)
{
    LINE << "Processing blob "<< seqref.printTSE();
    CLevelGuard level(m_RecursionLevel);

    string blob_key = m_Reader->GetBlobKey(seqref);

    int version = m_Reader->GetVersion(seqref, 0);
    if ( version > 1 ) {
        CTime time(time_t(version*60));
        LINE << "Blob version: " << version << " - " << time.AsString();
    }
    else {
        LINE << "Blob version: " << version;
    }

    CSeq_id seq_id;
    seq_id.SetGi(seqref.GetGi());
    CBioseq_Handle bh;
    {{
        WAIT_LINE << "Loading...";
        bh = m_Scope->GetBioseqHandle(seq_id);
    }}
    if ( !bh ) {
        LINE << "Skipping: no bioseq???";
        return;
    }

    CConstRef<CSeq_entry> seq_entry;
    if ( !m_Reader->IsSNPSeqref(seqref) ) {
        // get non-SNP blob
        seq_entry.Reset(&bh.GetTopLevelSeqEntry());
    }
    else {
        LINE << "Skipping SNP blob: not implemented";
        return;
        /*
          SAnnotSelector sel;
          sel.SetMaxSize(1);
          sel.SetFeatSubtype(CSeqFeatData::eSubtype_variation);
          CFeat_CI it(bh, 0, 0, sel);
          if ( !it ) {
          LINE << "Skipping SNP blob: empty";
          return;
          }
          const CSeq_annot& seq_annot = it.GetSeq_annot();
          CConstRef<CSeq_annot_Info> seq_annot_info =
          m_Scope->GetImpl().x_GetSeq_annot_Info(seq_annot);
          blob.Reset(&seq_annot_info->GetTSE_Info());
        */
    }

    if ( m_DumpAsnText ) {
        Dump(*seq_entry, eSerial_AsnText, blob_key);
    }
    if ( m_DumpAsnBinary ) {
        Dump(*seq_entry, eSerial_AsnBinary, blob_key);
    }

    size_t blob_size = m_Cache->GetSize(m_Reader->GetBlobKey(seqref), version);
    if ( blob_size == 0 ) {
        LINE << "Skipping: blob is not in cache";
        return;
    }
    if ( blob_size <= m_SplitterParams.m_MaxChunkSize ) {
        LINE << "Skipping: blob is small enough: " << blob_size;
        return;
    }
    LINE << "Blob size: " << blob_size;

    CBlobSplitter splitter(m_SplitterParams);
    if ( !splitter.Split(*seq_entry) ) {
        LINE << "Skipping: no chunks after splitting";
        return;
    }

    const CSplittedBlob& blob = splitter.GetBlob();
    if ( m_DumpAsnText ) {
        Dump(blob.GetMainBlob(), eSerial_AsnText, blob_key, "-split-main");
        Dump(blob.GetSplitInfo(), eSerial_AsnText, blob_key, "-split-info");
        ITERATE ( CSplittedBlob::TChunks, it, blob.GetChunks() ) {
            string suffix = "-chunk-" + NStr::IntToString(it->first);
            Dump(*it->second, eSerial_AsnText, blob_key, suffix);
        }
    }
    if ( m_DumpAsnBinary ) {
        Dump(blob.GetMainBlob(), eSerial_AsnBinary, blob_key, "-split-main");
        Dump(blob.GetSplitInfo(), eSerial_AsnBinary, blob_key, "-split-info");
        ITERATE ( CSplittedBlob::TChunks, it, blob.GetChunks() ) {
            string suffix = "-chunk-" + NStr::IntToString(it->first);
            Dump(*it->second, eSerial_AsnBinary, blob_key, suffix);
        }
    }
    {{ // storing split data
        DumpData(blob.GetMainBlob(), blob_key, "-split-main");
        DumpData(blob.GetSplitInfo(), blob_key, "-split-info");
        ITERATE ( CSplittedBlob::TChunks, it, blob.GetChunks() ) {
            string suffix = "-chunk-" + NStr::IntToString(it->first);
            DumpData(*it->second, blob_key, suffix);
        }
    }}
}


CConstRef<CSeqref> CSplitCacheApp::GetSeqref(CBioseq_Handle bh)
{
    CConstRef<CTSE_Info> tse_info =
        m_Scope->GetImpl().GetTSE_Info(bh.GetTopLevelSeqEntry());
    return ConstRef(&m_Loader->GetSeqref(*tse_info));
}


END_SCOPE(objects);
END_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return ncbi::objects::CSplitCacheApp().AppMain(argc, argv);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/11/12 16:18:32  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
