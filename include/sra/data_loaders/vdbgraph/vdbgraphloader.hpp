#ifndef OBJTOOLS_DATA_LOADERS_SRA___VDBGRAPHLOADER__HPP
#define OBJTOOLS_DATA_LOADERS_SRA___VDBGRAPHLOADER__HPP

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
 * Author: Eugene Vasilchenko
 *
 * File Description: VDB Graph file data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CVDBGraphDataLoader_Impl;

//
// class CVDBGraphDataLoader is used to retrieve graphs from VDB files
//

class NCBI_XLOADER_VDBGRAPH_EXPORT CVDBGraphDataLoader : public CDataLoader
{
public:
    typedef vector<string> TVDBFiles;
    struct SLoaderParams
    {
        SLoaderParams(void);
        SLoaderParams(const string& vdb_file);
        SLoaderParams(const TVDBFiles& vdb_files);
        ~SLoaderParams(void);

        TVDBFiles m_VDBFiles;
    };


    typedef SRegisterLoaderInfo<CVDBGraphDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& vdb_file,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const TVDBFiles& vdb_files,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const string& vdb_files);
    static string GetLoaderNameFromArgs(const TVDBFiles& vdb_files);

    virtual CObjectManager::TPriority GetDefaultPriority(void) const override;
    
    typedef vector<CAnnotName> TAnnotNames;
    virtual TAnnotNames GetPossibleAnnotNames(void) const;

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh) override;
    virtual TBlobId GetBlobIdFromString(const string& str) const override;

    virtual bool CanGetBlobById(void) const override;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id) override;

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice) override;
    virtual TTSE_LockSet GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
                                                 const SAnnotSelector* sel,
                                                 TProcessedNAs* processed_nas) override;
    virtual void GetChunk(TChunk chunk) override;
    virtual void GetChunks(const TChunkSet& chunks) override;

private:
    typedef CParamLoaderMaker<CVDBGraphDataLoader, SLoaderParams> TMaker;
    friend class CParamLoaderMaker<CVDBGraphDataLoader, SLoaderParams>;

    // default constructor
    CVDBGraphDataLoader(void);
    // parametrized constructor
    CVDBGraphDataLoader(const string& loader_name,
                        const SLoaderParams& params);
    ~CVDBGraphDataLoader(void);

    static string GetLoaderNameFromArgs(const SLoaderParams& params);

    CRef<CVDBGraphDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_VDBGRAPH_EXPORT
const char kDataLoader_VDBGraph_DriverName[];

NCBI_XLOADER_VDBGRAPH_EXPORT void DataLoaders_Register_VDBGraph(void);

extern "C"
{

NCBI_XLOADER_VDBGRAPH_EXPORT
void NCBI_EntryPoint_xloader_vdbgraph(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SRA___VDBGRAPHLOADER__HPP
