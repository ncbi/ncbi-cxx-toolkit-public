#ifndef OBJTOOLS_DATA_LOADERS_SRA___SRALOADER__HPP
#define OBJTOOLS_DATA_LOADERS_SRA___SRALOADER__HPP

/*  $Id $
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
 * File Description: SRA file data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CSRADataLoader_Impl;

//
// class CSRADataLoader is used to retrieve consensus sequences from
// the SRA server
//

class NCBI_XLOADER_SRA_EXPORT CSRADataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CSRADataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CSRADataLoader(void);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobId GetBlobIdFromString(const string& str) const;

    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);

private:
    typedef CSimpleLoaderMaker<CSRADataLoader> TMaker;
    friend class CSimpleLoaderMaker<CSRADataLoader>;

    CSRADataLoader(const string& loader_name);

    CRef<CSRADataLoader_Impl> m_Impl;
};


class CSraException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that  corelib can generate.
    ///
    /// These generic error conditions can occur for corelib applications.
    enum EErrCode {
        eNullPtr,       ///< Null pointer error
        eAddRefFailed,  ///< AddRef failed
        eInvalidArg,    ///< Invalid argument error
        eOtherError
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CSraException, CException);
};


END_SCOPE(objects)


extern NCBI_XLOADER_SRA_EXPORT const string kDataLoader_Sra_DriverName;

extern "C"
{

NCBI_XLOADER_SRA_EXPORT
void NCBI_EntryPoint_DataLoader_Sra(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_SRA_EXPORT
void NCBI_EntryPoint_xloader_sra(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SRA___SRALOADER__HPP
