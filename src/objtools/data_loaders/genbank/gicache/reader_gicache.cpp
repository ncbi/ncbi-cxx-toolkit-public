/*  $Id$
 * ===========================================================================
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
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Data reader from gicache
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>

#include <objtools/data_loaders/genbank/gicache/reader_gicache.hpp>
#include <objtools/data_loaders/genbank/gicache/reader_gicache_entry.hpp>
#include <objtools/data_loaders/genbank/gicache/reader_gicache_params.h>
#include <objtools/data_loaders/genbank/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

//#define USE_GICACHE

#ifdef USE_GICACHE
# include "gicache.h"
#else
# define DEFAULT_GI_CACHE_PATH ""
#endif

#define NCBI_USE_ERRCODE_X   Objtools_Rd_GICache

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CGICacheReader::CGICacheReader(void)
    : m_Path(DEFAULT_GI_CACHE_PATH)
{
    SetMaximumConnections(1);
    x_Initialize();
}


CGICacheReader::CGICacheReader(const TPluginManagerParamTree* params,
                               const string& driver_name)
{
    CConfig conf(params);
    m_Path = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_GICACHE_PARAM_PATH_NAME,
        CConfig::eErr_NoThrow,
        DEFAULT_GI_CACHE_PATH);
    x_Initialize();
}


CGICacheReader::~CGICacheReader()
{
#ifdef USE_GICACHE
    CMutexGuard guard(m_Mutex);
    GICache_ReadEnd();
#endif
}


#ifdef USE_GICACHE
static
void s_LogFunc(int severity, char* msg)
{
    /*
      Severity values from gicache.c
      #define SEV_NONE 0 
      #define SEV_INFO 1
      #define SEV_WARNING 2
      #define SEV_ERROR 3
      #define SEV_REJECT 4 
      #define SEV_FATAL 5
     */
    static const EDiagSev sev[] = {
        eDiag_Info,
        eDiag_Info,
        eDiag_Warning,
        eDiag_Error,
        eDiag_Critical,
        eDiag_Fatal
    };
    ERR_POST(Severity(sev[min(size_t(severity), ArraySize(sev)-1)]) << msg);
}
#endif


void CGICacheReader::x_Initialize(void)
{
    ERR_POST_ONCE("This app is using deprecated OM++ reader gicache. Please remove it from your configuration.");
#ifdef USE_GICACHE
    string index = m_Path;
    if ( CFile(index).IsDir() ) {
        const char* file;
#if defined(DEFAULT_64BIT_SUFFIX)  &&  SIZEOF_VOIDP > 4
        file = DEFAULT_GI_CACHE_PREFIX DEFAULT_64BIT_SUFFIX;
#else
        file = DEFAULT_GI_CACHE_PREFIX;
#endif
        index = CFile::MakePath(index, file);
    }
    CMutexGuard guard(m_Mutex);
    GICache_SetLogEx(s_LogFunc);
    GICache_ReadData(index.c_str());
#endif
}


void CGICacheReader::x_AddConnectionSlot(TConn /*conn*/)
{
}


void CGICacheReader::x_RemoveConnectionSlot(TConn /*conn*/)
{
}


void CGICacheReader::x_DisconnectAtSlot(TConn /*conn*/, bool /*failed*/)
{
}


void CGICacheReader::x_ConnectAtSlot(TConn /*conn*/)
{
}


int CGICacheReader::GetRetryCount(void) const
{
    return 1;
}


bool CGICacheReader::MayBeSkippedOnErrors(void) const
{
    return true;
}


int CGICacheReader::GetMaximumConnectionsLimit(void) const
{
    return 1;
}


bool CGICacheReader::LoadSeq_idAccVer(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( seq_id.IsGi() ) {
        CLoadLockAcc ids(result, seq_id);
#ifdef USE_GICACHE
        char buffer[256];
        int got;
        {{
            CMutexGuard guard(m_Mutex);
            got = GICache_GetAccession(GI_TO(TIntId, seq_id.GetGi()), buffer, sizeof(buffer));
        }}
        if ( got ) {
            if ( buffer[0] ) {
                try {
                    TSequenceAcc acc;
                    acc.acc_ver = CSeq_id_Handle::GetHandle(buffer);
                    acc.sequence_found = true;
                    ids.SetLoadedAccVer(acc);
                    return true;
                }
                catch ( CException& /*ignored*/ ) {
                    ERR_POST("Bad accver for gi "<<seq_id.GetGi()<<
                             ": \""<<buffer<<"\"");
                }
            }
        }
#endif
    }
    // if any problem occurs -> fall back to regular reader
    return false;
}


bool CGICacheReader::LoadStringSeq_ids(CReaderRequestResult& /*result*/,
                                       const string& /*seq_id*/)
{
    return false;
}


bool CGICacheReader::LoadSeq_idSeq_ids(CReaderRequestResult& /*result*/,
                                       const CSeq_id_Handle& /*seq_id*/)
{
    return false;
}


bool CGICacheReader::LoadBlobState(CReaderRequestResult& /*result*/,
                                   const TBlobId& /*blob_id*/)
{
    return false;
}


bool CGICacheReader::LoadBlobVersion(CReaderRequestResult& /*result*/,
                                     const TBlobId& /*blob_id*/)
{
    return false;
}


bool CGICacheReader::LoadBlob(CReaderRequestResult& /*result*/,
                              const CBlob_id& /*blob_id*/)
{
    return false;
}


END_SCOPE(objects)

void GenBankReaders_Register_GICache(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_GICacheReader);
}


/// Class factory for ID1 reader
///
/// @internal
///
class CGICacheReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader, objects::CGICacheReader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader,
                                    objects::CGICacheReader> TParent;
public:
    CGICacheReaderCF()
        : TParent(NCBI_GBLOADER_READER_GICACHE_DRIVER_NAME, 0) {}
    ~CGICacheReaderCF() {}

    objects::CReader* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CReader),
                   const TPluginManagerParamTree* params = 0) const
    {
        objects::CReader* drv = 0;
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CReader)) 
                            != CVersionInfo::eNonCompatible) {
            drv = new objects::CGICacheReader(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_GICacheReader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CGICacheReaderCF>::NCBI_EntryPointImpl(info_list,
                                                               method);
}


void NCBI_EntryPoint_xreader_gicache(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_GICacheReader(info_list, method);
}


END_NCBI_SCOPE

