#ifndef OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER__HPP
#define OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER__HPP

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
 * File Description: SNP file data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSNPDataLoader_Impl;

//
// class CSNPDataLoader is used to retrieve alignments and short sequences
// from SNP-DB
//

class NCBI_XLOADER_SNP_EXPORT CSNPDataLoader : public CDataLoader
{
public:
    typedef vector<string> TVDBFiles;
    struct NCBI_XLOADER_SNP_EXPORT SLoaderParams
    {
        string GetLoaderName(void) const;

        string m_DirPath;
        TVDBFiles m_VDBFiles;
        string          m_AnnotName;
    };


    typedef SRegisterLoaderInfo<CSNPDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const SLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
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
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dir_path,
        const string& file,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dir_path,
        const vector<string>& files,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const SLoaderParams& params);
    static string GetLoaderNameFromArgs(const string& vdb_file);
    static string GetLoaderNameFromArgs(const TVDBFiles& vdb_files);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const string& vdb_file);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const TVDBFiles& vdb_files);

    typedef vector<CAnnotName> TAnnotNames;
    TAnnotNames GetPossibleAnnotNames(void) const;

    virtual TBlobId GetBlobIdFromString(const string& str) const;

    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    virtual TTSE_LockSet GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                               const SAnnotSelector* sel);
    virtual void GetChunk(TChunk chunk);
    virtual void GetChunks(const TChunkSet& chunks);

private:
    typedef CParamLoaderMaker<CSNPDataLoader, SLoaderParams> TMaker;
    friend class CParamLoaderMaker<CSNPDataLoader, SLoaderParams>;

    // default constructor
    CSNPDataLoader(void);
    // parametrized constructor
    CSNPDataLoader(const string& loader_name,
                   const SLoaderParams& params);
    ~CSNPDataLoader(void);

    CRef<CSNPDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_SNP_EXPORT const char kDataLoader_SNP_DriverName[];

NCBI_XLOADER_SNP_EXPORT void DataLoaders_Register_SNP(void);

extern "C"
{

NCBI_XLOADER_SNP_EXPORT
void NCBI_EntryPoint_xloader_snp(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER__HPP
