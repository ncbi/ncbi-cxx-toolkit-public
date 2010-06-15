#ifndef OBJTOOLS_DATA_LOADERS_BLASTDB___BDBLOADER_RMT__HPP
#define OBJTOOLS_DATA_LOADERS_BLASTDB___BDBLOADER_RMT__HPP

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
*  ===========================================================================
*
*  Author: Christiam Camacho
*
* ===========================================================================
*/

/** @file bdbloader_rmt.hpp
  * Data loader implementation that uses the blast databases remotely
  */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CRemoteBlastDbDataLoader 
    : public CBlastDbDataLoader
{
public:

    typedef SRegisterLoaderInfo<CRemoteBlastDbDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dbname = "nr",
        const EDbType dbtype = eUnknown,
        bool use_fixed_size_slices = true,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    /// All BLAST DB data loader instances have a name that starts with
    /// kNamePrefix
    static const string kNamePrefix;
    static string GetLoaderNameFromArgs(const SBlastDbParam& param);

    /// Support for fetching the sequence length by batch
    virtual void GetBlobs(TTSE_LockSets& tse_sets);
    /// Support for fetching the sequence chunks by batch
    virtual void GetChunks(const TChunkSet& chunks);

    /// For a given TBlobId, get the TTSE_Lock.
    ///
    /// If the provided TBlobId is known to this code, the
    /// corresponding TTSE_Lock data will be fetched and returned.
    /// Otherwise, an empty valued TTSE_Lock is returned.
    ///
    /// @param blob_id
    ///   Indicates which data to get.
    /// @return
    ///   The returned data.
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);
private:
    typedef CParamLoaderMaker<CRemoteBlastDbDataLoader, SBlastDbParam> TMaker;
    friend class CParamLoaderMaker<CRemoteBlastDbDataLoader, SBlastDbParam>;
    /// Parametrized constructor
    /// @param loader_name name of this data loader [in]
    /// @param param parameters to initialize this data loader [in]
    CRemoteBlastDbDataLoader(const string& loader_name, 
                             const SBlastDbParam& param);

    /// Prevent automatic copy constructor generation
    CRemoteBlastDbDataLoader(const CRemoteBlastDbDataLoader &);
    
    /// Prevent automatic assignment operator generation
    CRemoteBlastDbDataLoader & operator=(const CRemoteBlastDbDataLoader &);
};

END_SCOPE(objects)


extern const string kDataLoader_RmtBlastDb_DriverName;

extern "C"
{

void NCBI_EntryPoint_DataLoader_RmtBlastDb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

void NCBI_EntryPoint_xloader_blastdb_rmt(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___BDBLOADER_RMT__HPP */
