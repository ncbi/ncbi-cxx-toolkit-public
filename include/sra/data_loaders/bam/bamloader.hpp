#ifndef OBJTOOLS_DATA_LOADERS_BAM___BAMLOADER__HPP
#define OBJTOOLS_DATA_LOADERS_BAM___BAMLOADER__HPP

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
 * File Description: BAM file data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

#include <objtools/readers/iidmapper.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CBAMDataLoader_Impl;

//
// class CBAMDataLoader is used to retrieve consensus sequences from
// the BAM server
//

class NCBI_XLOADER_BAM_EXPORT CBAMDataLoader : public CDataLoader
{
public:

    struct SBamFileName
    {
        SBamFileName(const string& bam_name = kEmptyStr,
                     const string& index_name = kEmptyStr)
            : m_BamName(bam_name),
              m_IndexName(index_name)
            {
            }

        string m_BamName;
        string m_IndexName;
    };
    struct SLoaderParams
    {
        string          m_DirPath;
        vector<SBamFileName> m_BamFiles;
        AutoPtr<IIdMapper> m_IdMapper;
    };


    typedef SRegisterLoaderInfo<CBAMDataLoader> TRegisterLoaderInfo;
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
        const string& srz_acc,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dir_path,
        const string& bam_name,
        const string& index_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dir_path,
        const vector<SBamFileName>& bam_files,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const string& srz_acc);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const string& bam_name,
                                        const string& index_name);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const vector<SBamFileName>& bam_files);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobId GetBlobIdFromString(const string& str) const;

    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    virtual void GetChunk(TChunk chunk);
    virtual void GetChunks(const TChunkSet& chunks);

    typedef vector<CAnnotName> TAnnotNames;
    TAnnotNames GetPossibleAnnotNames(void) const;

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    virtual SAccVerFound GetAccVerFound(const CSeq_id_Handle& idh);
    virtual SGiFound GetGiFound(const CSeq_id_Handle& idh);
    virtual string GetLabel(const CSeq_id_Handle& idh);
    virtual int GetTaxId(const CSeq_id_Handle& idh);

    static bool GetPileupGraphsParamDefault(void);
    static void SetPileupGraphsParamDefault(bool param);

private:
    typedef CParamLoaderMaker<CBAMDataLoader, SLoaderParams> TMaker;
    friend class CParamLoaderMaker<CBAMDataLoader, SLoaderParams>;

    // default constructor
    CBAMDataLoader(void);
    // parametrized constructor
    CBAMDataLoader(const string& loader_name, const SLoaderParams& params);
    ~CBAMDataLoader(void);

    static string GetLoaderNameFromArgs(const SLoaderParams& params);

    CRef<CBAMDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_BAM_EXPORT const string kDataLoader_Bam_DriverName;

extern "C"
{

NCBI_XLOADER_BAM_EXPORT
void NCBI_EntryPoint_DataLoader_Bam(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_BAM_EXPORT
void NCBI_EntryPoint_xloader_bam(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_BAM___BAMLOADER__HPP
