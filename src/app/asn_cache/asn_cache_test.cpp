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
 * Authors:  Mike Diccucio Cheinan Marks
 *
 * File Description:
 * Test reading from the ID ASN.1 Cache.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_loader.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqfeat/Gb_qual.hpp>



USING_NCBI_SCOPE;
USING_SCOPE(objects);

static const CBioseq::TId &s_GetSeqIds(const vector<CSeq_id_Handle> &handles){
    static CBioseq::TId ids;
    ids.clear();
    ITERATE(vector<CSeq_id_Handle>, it, handles)
        ids.push_back(CRef<CSeq_id>(const_cast<CSeq_id*>(it->GetSeqId().GetNonNullPointer())));
    return ids;
}

static bool s_SameIds(const CBioseq::TId &ids1, const CBioseq::TId &ids2)
{
    if(ids1.size() != ids2.size())
        return false;
    ITERATE(CBioseq::TId, it1, ids1){
        CRef<CSeq_id> matching_id;
        ITERATE(CBioseq::TId, it2, ids2)
            if((*it2)->Match(**it1)){
                matching_id = *it2;
                break;
            }
        if(!matching_id)
            return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//  CAsnCacheApplication::


class CAsnCacheApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    
private: //member functions

    bool x_FindAnnotated(const CSeq_entry& entry);
    
    
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAsnCacheApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddKey("cache", "ASNCache",
                     "Path to ASN.1 cache",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("i", "AccessionList",
                            "List of accessions to retrieve",
                            CArgDescriptions::eInputFile,
                            "-");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "File to place ASN seq-entries in",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddFlag("test-loader",  "Test use of the ASN cache data loader");

    arg_desc->AddFlag("raw",  "Test raw retrieval only");
    arg_desc->AddFlag("text", "Use ASN.1 text output");
    arg_desc->AddFlag("find-annotated", "Find annotated accessions");
    arg_desc->AddFlag("dump-GP-7574", "Dump comments and seq-descs");
    arg_desc->AddFlag("dump-GP-8763", "Dump qualifiers");
    arg_desc->AddFlag("dump-proteins", "Dump protein seqs");
    arg_desc->AddFlag("no-serialize", "Do not reserialize the ASN.1");
    arg_desc->AddFlag("random-order", "Retrieve sequences in random order");
    arg_desc->AddFlag("test-warm", "Retrieve sequences twice, to check differences between cold-cache and warm-cache times");

    arg_desc->AddFlag("indexonly", "Print the index entry only, do not fetch the blob." );

    arg_desc->AddFlag("idonly", "Verify that ID information in the cache is available and accurate for the listed accessions." );
    arg_desc->AddFlag("verify-ids", "Verify that ID information in the cache is available and accurate for the listed accessions." );
    arg_desc->SetDependency("raw", CArgDescriptions::eExcludes, "indexonly");
    arg_desc->SetDependency("verify-ids", CArgDescriptions::eExcludes, "raw");
    arg_desc->SetDependency("verify-ids", CArgDescriptions::eExcludes, "indexonly");
    arg_desc->SetDependency("test-loader", CArgDescriptions::eExcludes, "raw");
    arg_desc->SetDependency("test-loader", CArgDescriptions::eExcludes, "indexonly");
    arg_desc->SetDependency("idonly", CArgDescriptions::eExcludes, "raw");
    arg_desc->SetDependency("idonly", CArgDescriptions::eExcludes, "indexonly");
    arg_desc->SetDependency("idonly", CArgDescriptions::eExcludes, "verify-ids");
    arg_desc->SetDependency("no-serialize", CArgDescriptions::eExcludes, "indexonly");
    arg_desc->SetDependency("text", CArgDescriptions::eExcludes, "raw");
    arg_desc->SetDependency("text", CArgDescriptions::eExcludes, "indexonly");
    arg_desc->SetDependency("text", CArgDescriptions::eExcludes, "idonly");
    arg_desc->SetDependency("text", CArgDescriptions::eExcludes, "no-serialize");

    arg_desc->AddFlag("get-multiple",
                      "If several entries match the specified id, get all of "
                      "them, not only latest one");
    arg_desc->SetDependency("get-multiple", CArgDescriptions::eExcludes, "idonly");
    arg_desc->SetDependency("get-multiple", CArgDescriptions::eExcludes, "test-loader");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CAsnCacheApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    CNcbiOstream& ostr = args["o"].AsOutputFile();
    CNcbiIstream& istr = args["i"].AsInputFile();

    bool readIndexOnly = args[ "indexonly" ];
    bool raw = args["raw"];
    bool serialize = !args["no-serialize"];
    bool verify_ids = args["verify-ids"];
    bool getIdOnly = args["idonly"];
    bool multiple = args["get-multiple"];

    vector< CConstRef<CSeq_entry> > entries;
    vector< CDataLoader::TIds > id_sets;
    vector< CSeq_id_Handle > ids;

    auto_ptr<CObjectOStream> os;

    string line;
    while (NcbiGetlineEOL(istr, line)) {
        if (line.empty()  ||  line[0] == '#') {
            continue;
        }
        try {
            CSeq_id id(line);
            ids.push_back(CSeq_id_Handle::GetHandle(id));
        }
        catch (CException& e) {
            LOG_POST(Error << "failed to convert "
                     << line << " to a SeqId: " << e.what());
        }
    }

    if (serialize) {
        if(getIdOnly) {
            id_sets.reserve(ids.size());
        }
        else {
            entries.reserve(ids.size());
        }
    }


    CRef<CDataLoader> loader;
    CRef<CAsnCache> cache;
    CRef<CDataSource> source;
    CRef<CScope> m_LocalCacheScope;

    if(args["test-loader"]){
        loader.Reset(CAsnCache_DataLoader::TDbMaker(args["cache"].AsString()).CreateLoader());
        source.Reset(new CDataSource(*loader));
    } else
        cache.Reset(new CAsnCache(args["cache"].AsString()));
    if(args["dump-proteins"]){        
        m_LocalCacheScope.Reset(new CScope (*CObjectManager::GetInstance()));
        string cache_path = args["cache"].AsString();
        CAsnCache_DataLoader::RegisterInObjectManager(*CObjectManager::GetInstance(), cache_path, CObjectManager::eDefault, 1 );
        string loader_name = CAsnCache_DataLoader::GetLoaderNameFromArgs(cache_path);
        if ( CObjectManager::GetInstance()->FindDataLoader(loader_name ) ) {
            loader_name = CObjectManager::GetInstance()->FindDataLoader(loader_name )->GetName()  ;
        } else {
        }
        m_LocalCacheScope->AddDataLoader(loader_name );
    }
    

    int num_cycles = args["test-warm"] ? 2 : 1;
    size_t count_failed = 0;

    for(int cycle = 0; cycle < num_cycles; cycle++){
        if (args["random-order"]) {
            random_shuffle(ids.begin(), ids.end());
        }
    
        size_t count = 0;
        CStopWatch sw;
        sw.Start();
    
        ITERATE(vector<CSeq_id_Handle>, id_it, ids){
            try {
                if ( readIndexOnly ) {
                    vector<CAsnIndex::SIndexInfo> info;
    
                    if (multiple) {
                        cache->GetMultipleIndexEntries( *id_it, info);
                    } else {
                        info.resize(1);
                        cache->GetIndexEntry( *id_it, info[0]);
                    }
    
                    ITERATE (vector<CAsnIndex::SIndexInfo>, info_it, info) {
                        ostr << *info_it << endl;
                    }
                } else if (raw) {
                    vector<CAsnCache::TBuffer> buffer(multiple ? 0 : 1);
                    bool success = multiple ? cache->GetMultipleRaw(*id_it, buffer)
                                            : cache->GetRaw(*id_it, buffer[0]);
                    if (success) {
                        if(serialize && cycle == 0) {
                            ITERATE (vector<CAsnCache::TBuffer>, buf_it, buffer) {
                                ostr.write((const char*)&(*buf_it)[0],
                                           buf_it->size());
                            }
                        }
                    } else {
                        LOG_POST(Error << "failed to retrieve: "
                                 << id_it->GetSeqId()->AsFastaString());
                        ++count_failed;
                    }
                } else if (getIdOnly) {
                    CDataLoader::TIds id_set;
                    if(loader)
                        loader->GetIds(*id_it, id_set);
                    else
                        cache->GetSeqIds(*id_it, id_set);
                    if(id_set.empty()) {
                        LOG_POST(Error << "failed to retrieve: "
                                 << id_it->GetSeqId()->AsFastaString());
                        ++count_failed;
                    } else if(serialize && cycle==0)
                        id_sets.push_back(id_set);
                } else {
                  vector< CRef<CSeq_entry> > entries_for_id;
                  if (multiple) {
                    entries_for_id = cache->GetMultipleEntries(*id_it);
                  } else if (loader) {
                    entries_for_id.push_back(CRef<CSeq_entry>(const_cast<CSeq_entry *>
                        ((*loader->GetRecords(*id_it, CDataLoader::eBioseq).begin())->GetCompleteTSE().GetPointer())));
                  } else {
                    entries_for_id.push_back(cache->GetEntry(*id_it));
                  }
                  ITERATE (vector< CRef<CSeq_entry> >, entry_it, entries_for_id) {
                    CRef<CSeq_entry> entry = *entry_it;
                    if(args["find-annotated"]) {
                        bool is_annotated = x_FindAnnotated(*entry);
                        CDataLoader::TIds id_set;
                        if(loader)
                            loader->GetIds(*id_it, id_set);
                        else
                            cache->GetSeqIds(*id_it, id_set);
                            cout<<"is_annotated" << "\t" 
                                << "original" << "\t"
                                << *id_it << "\t" 
                                << boolalpha << is_annotated << endl;
                        ITERATE(CDataLoader::TIds, id_it, id_set) {
                            cout<<"is_annotated" << "\t" 
                                << "alias" << "\t"
                                << *id_it << "\t" 
                                << boolalpha << is_annotated << endl;
                        }
                    }
                    if(entry && args["dump-GP-7574"]) {
                        cerr << *id_it << endl;
                        for(CTypeConstIterator<CSeqdesc> desc(*entry); desc; ++desc) {
                            switch ( desc->Which() ) {
                                case CSeqdesc::e_User:
                                case CSeqdesc::e_Comment:
                                    cout << MSerial_AsnText << *desc;
                                    break;
                                default: break;
                            }
                        }
                    }
                    if(entry && args["dump-GP-8763"]) {
                        cerr << *id_it << endl; bool first=true;
                        for(CTypeConstIterator<CGb_qual> desc(*entry); desc; ++desc) {
                            if(desc->GetQual() == "inference" ||
                               desc->GetQual() == "experiment" 
                            ) {
                                if(first) cout << *id_it << endl;  first=false;
                                cout << MSerial_AsnText << *desc;
                            }
                        }
                    }
                    
                    
                    if(entry && args["dump-proteins"]) {
                        for(CTypeConstIterator<CSeq_feat> feat(*entry); feat; ++feat) {
                            if( feat->GetData().Which() == CSeqFeatData::e_Cdregion &&
                                feat->IsSetProduct() ) {
                                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*feat->GetProduct().GetId());
                                CBioseq_Handle bsh = m_LocalCacheScope->GetBioseqHandle(idh);
                                if( bsh.CanGetInst() ) {
                                    if ( 
                                        bsh.GetInst().IsSetExt() &&
                                        bsh.GetInst().GetExt().IsDelta() ) {
                                        ITERATE(CDelta_ext::Tdata, delta, bsh.GetInst().GetExt().GetDelta().Get() ) {
                                            if( (*delta)->IsLoc() ) {
                                                idh = CSeq_id_Handle::GetHandle( *(*delta)->GetLoc().GetId() );
                                                break;
                                            }
                                        }
                                    }
                                } else  {
                                    LOG_POST(Warning<<"Can't get seqinst for " << idh);
                                }
                                cout << *id_it << "\t" 
                                    << idh << endl;
                                //auto_ptr<CBestPigMapper> m_mapper
                            }
                        }
                    }
                    if (entry) {
                        if (serialize && cycle==0) {
                            if( ! args["find-annotated"])
                                entries.push_back(entry);
                        }
                        if(verify_ids){
                            CDataLoader::TIds retrieved_ids;
                            if(loader)
                                loader->GetIds(*id_it, retrieved_ids);
                            else
                                cache->GetSeqIds(*id_it, retrieved_ids);
                            if(retrieved_ids.empty()){
                                LOG_POST(Error << "failed to retrieve ids for: "
                                         << id_it->GetSeqId()->AsFastaString());
                                ++count_failed;
                            } else if(!s_SameIds(s_GetSeqIds(retrieved_ids), ExtractBioseq(entry, *id_it)->GetId()))
                            {
                                LOG_POST(Error << "Mismatched ids for: "
                                         << id_it->GetSeqId()->AsFastaString() << ": retrieved:");
                                ITERATE(vector<CSeq_id_Handle>, it, retrieved_ids)
                                    LOG_POST(Error << it->GetSeqId()->AsFastaString());
                                LOG_POST(Error << "IDs in entry:");
                                ITERATE(CBioseq::TId, it, ExtractBioseq(entry, *id_it)->GetId())
                                    LOG_POST(Error << (*it)->AsFastaString());
                                ++count_failed;
                            } else
                                LOG_POST(Info << "Succesfully retrieved " << retrieved_ids.size()
                                              << " ids for " << id_it->GetSeqId()->AsFastaString());
                        }
                    } else {
                        LOG_POST(Error << "failed to retrieve: "
                                 << id_it->GetSeqId()->AsFastaString());
                        ++count_failed;
                    }
                  }
                }
    
                ++count;
            }
            catch (CException& e) {
                LOG_POST(Error << "failed to retrieve "
                         << line << ": " << e.what());
            }
        }
    
        double e = sw.Elapsed();
        LOG_POST(Error << "done cycle " << cycle+1 << ", " << count << " seqs / " << e << " seconds = "
                 << count / e << " seqs/sec ("
                 << count_failed << " failed to retrieve)");
    }
    if(args["dump-proteins"]) { return count_failed; }
    if(serialize) {
        if(getIdOnly){
            ITERATE(vector< CDataLoader::TIds >, it, id_sets){
                ITERATE(CDataLoader::TIds, id_it, *it){
                    if(id_it != it->begin())
                        ostr << ", ";
                    ostr << id_it->GetSeqId()->AsFastaString();
                }
                ostr << endl;
            }
        } else {
            if (args["text"]) {
                os.reset(CObjectOStream::Open(eSerial_AsnText, ostr));
            } else {
                os.reset(CObjectOStream::Open(eSerial_AsnBinary, ostr));
            }
            ITERATE(vector< CConstRef<CSeq_entry> >, it, entries)
                *os << **it;
        }
    }

    return count_failed;
}

bool CAsnCacheApplication::x_FindAnnotated(const CSeq_entry& entry)
{
    return 
       ( entry.IsSeq() && entry.GetSeq().IsSetAnnot() ) ||
       ( entry.IsSet() && entry.GetSet().IsSetAnnot() );
    
}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAsnCacheApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAsnCacheApplication().AppMain(argc, argv);
}
