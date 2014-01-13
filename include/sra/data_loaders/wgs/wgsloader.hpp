#ifndef OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER__HPP
#define OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER__HPP

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
 * File Description: WGS file data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CWGSDataLoader_Impl;

//
// class CWGSDataLoader is used to retrieve alignments and short sequences
// from WGS-DB
//

class NCBI_XLOADER_WGS_EXPORT CWGSDataLoader : public CDataLoader
{
public:

    struct SLoaderParams
    {
        SLoaderParams(void)
            {
            }
        string m_WGSVolPath; // search path for WGS files, may be empty
        vector<string>  m_WGSFiles;
    };


    typedef SRegisterLoaderInfo<CWGSDataLoader> TRegisterLoaderInfo;
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
        const string& dir_path,
        const vector<string>& wgs_files,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const SLoaderParams& params);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const vector<string>& wgs_files);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobId GetBlobIdFromString(const string& str) const;

    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                                 const SAnnotSelector* sel);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CBioseq_Info& bioseq,
                                                 const SAnnotSelector* sel);
    virtual TTSE_LockSet GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                               const SAnnotSelector* sel);

    virtual void GetChunk(TChunk chunk);
    virtual void GetChunks(const TChunkSet& chunks);

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    virtual CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    virtual int GetGi(const CSeq_id_Handle& idh);
    virtual int GetTaxId(const CSeq_id_Handle& idh);
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    virtual CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);

private:
    typedef CParamLoaderMaker<CWGSDataLoader, SLoaderParams> TMaker;
    friend class CParamLoaderMaker<CWGSDataLoader, SLoaderParams>;

    // default constructor
    CWGSDataLoader(void);
    // parametrized constructor
    CWGSDataLoader(const string& loader_name, const SLoaderParams& params);
    ~CWGSDataLoader(void);

    CRef<CWGSDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_WGS_EXPORT const char kDataLoader_WGS_DriverName[];

extern "C"
{

NCBI_XLOADER_WGS_EXPORT
void NCBI_EntryPoint_DataLoader_WGS(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_WGS_EXPORT
void NCBI_EntryPoint_xloader_wgs(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER__HPP
