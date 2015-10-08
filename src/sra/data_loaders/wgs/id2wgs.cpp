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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Processor of ID2 requests for WGS data
 *
 */

#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/id2wgs.hpp>
#include <sra/data_loaders/wgs/impl/id2wgs_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/id2processor_interface.hpp>
#include <sra/data_loaders/wgs/id2wgs_entry.hpp>
#include <sra/data_loaders/wgs/id2wgs_params.h>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);


/////////////////////////////////////////////////////////////////////////////
// CID2WGSProcessor
/////////////////////////////////////////////////////////////////////////////


CID2WGSProcessor::CID2WGSProcessor(void)
    : m_Impl(new CID2WGSProcessor_Impl)
{
}


CID2WGSProcessor::CID2WGSProcessor(const CConfig::TParamTree* params,
                                   const string& driver_name)
    : m_Impl(new CID2WGSProcessor_Impl(params, driver_name))
{
}


CID2WGSProcessor::~CID2WGSProcessor(void)
{
}


bool CID2WGSProcessor::ProcessRequest(TReplies& replies,
                                      CID2_Request& request)
{
    return m_Impl->ProcessRequest(replies, request);
}


CID2WGSProcessor::TReplies
CID2WGSProcessor::ProcessSomeRequests(CID2_Request_Packet& packet)
{
    return m_Impl->ProcessSomeRequests(packet);
}


END_NAMESPACE(objects);

void ID2Processors_Register_WGS(void)
{
    RegisterEntryPoint<objects::CID2Processor>(NCBI_EntryPoint_id2proc_wgs);
}


/// Class factory for ID1 reader
///
/// @internal
///
class CID2WGSProcessorCF : 
    public CSimpleClassFactoryImpl<objects::CID2Processor,
                                   objects::CID2WGSProcessor>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CID2Processor,
                                    objects::CID2WGSProcessor> TParent;
public:
    CID2WGSProcessorCF()
        : TParent(NCBI_ID2PROC_WGS_DRIVER_NAME, 0)
        {
        }

    objects::CID2Processor* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CID2Processor),
                   const TPluginManagerParamTree* params = 0) const
    {
        objects::CID2Processor* drv = 0;
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CID2Processor)) 
                            != CVersionInfo::eNonCompatible) {
            drv = new objects::CID2WGSProcessor(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_id2proc_wgs(
     CPluginManager<objects::CID2Processor>::TDriverInfoList&   info_list,
     CPluginManager<objects::CID2Processor>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CID2WGSProcessorCF>::NCBI_EntryPointImpl(info_list, method);
}


END_NCBI_NAMESPACE;
