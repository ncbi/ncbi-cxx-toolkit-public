#ifndef OBJTOOLS_DATA_LOADERS_CSRA___CSRALOADER__HPP
#define OBJTOOLS_DATA_LOADERS_CSRA___CSRALOADER__HPP

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
 * File Description: CSRA file data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>
#include <objtools/readers/iidmapper.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CCSRADataLoader_Impl;

//
// class CCSRADataLoader is used to retrieve alignments and short sequences
// from CSRA-DB
//

class NCBI_XLOADER_CSRA_EXPORT CCSRADataLoader : public CDataLoader
{
public:

    struct NCBI_XLOADER_CSRA_EXPORT SLoaderParams
    {
        enum {
            // -1 means from config
            kMinMapQuality_config = -1,
            kPileupGraphs_config = -1,
            kQualityGraphs_config = -1,
            kSpotGroups_config = -1,
            kPathInId_config = -1
        };

        SLoaderParams(void)
            : m_MinMapQuality(kMinMapQuality_config),
              m_PileupGraphs(kPileupGraphs_config),
              m_QualityGraphs(kQualityGraphs_config),
              m_SpotGroups(kSpotGroups_config),
              m_PathInId(kPathInId_config)
            {
            }

        string          m_DirPath;
        vector<string>  m_CSRAFiles;
        AutoPtr<IIdMapper> m_IdMapper;
        string          m_AnnotName;
        int             m_MinMapQuality;
        int             m_PileupGraphs; // bool value or kPileupGraphs_config
        int             m_QualityGraphs; // bool value or kQualityGraphs_config
        int             m_SpotGroups;
        int             m_PathInId; // bool value or kPathInId_config

        string GetLoaderName(void) const;

        int GetEffectiveMinMapQuality(void) const;
        bool GetEffectivePileupGraphs(void) const;
        bool GetEffectiveQualityGraphs(void) const;
        int GetEffectiveSpotGroups(void) const;
    };


    typedef SRegisterLoaderInfo<CCSRADataLoader> TRegisterLoaderInfo;
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
        const string& csra_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dir_path,
        const vector<string>& csra_files,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const string& srz_acc);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const string& csra_name);
    static string GetLoaderNameFromArgs(const string& dir_path,
                                        const vector<string>& csra_files);

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
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    virtual STypeFound GetSequenceTypeFound(const CSeq_id_Handle& idh);

    static bool GetPileupGraphsParamDefault(void);
    static void SetPileupGraphsParamDefault(bool param);
    static bool GetQualityGraphsParamDefault(void);
    static void SetQualityGraphsParamDefault(bool param);
    static int GetMinMapQualityParamDefault(void);
    static void SetMinMapQualityParamDefault(int param);
    static int GetSpotGroupsParamDefault(void);
    static void SetSpotGroupsParamDefault(int param);

private:
    typedef CParamLoaderMaker<CCSRADataLoader, SLoaderParams> TMaker;
    friend class CParamLoaderMaker<CCSRADataLoader, SLoaderParams>;

    // default constructor
    CCSRADataLoader(void);
    // parametrized constructor
    CCSRADataLoader(const string& loader_name, const SLoaderParams& params);
    ~CCSRADataLoader(void);

    static string GetLoaderNameFromArgs(const SLoaderParams& params);

    CRef<CCSRADataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_CSRA_EXPORT const char kDataLoader_CSRA_DriverName[];

extern "C"
{

NCBI_XLOADER_CSRA_EXPORT
void NCBI_EntryPoint_DataLoader_CSRA(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_CSRA_EXPORT
void NCBI_EntryPoint_xloader_csra(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_CSRA___CSRALOADER__HPP
