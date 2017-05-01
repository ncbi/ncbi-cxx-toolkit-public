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
 * Authors:  Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description:
 *   Processor of ID2 requests for CDD annotations
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/impl/id2cdd_impl.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd_entry.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd_params.h>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/rwstream.hpp>
#include <serial/objostrasnb.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <objects/id2/id2processor_interface.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);


/////////////////////////////////////////////////////////////////////////////
// CID2CDDProcessor
/////////////////////////////////////////////////////////////////////////////


CID2CDDProcessor::CID2CDDProcessor(void)
    : m_Impl(new CID2CDDProcessor_Impl)
{
}


CID2CDDProcessor::CID2CDDProcessor(const CConfig::TParamTree* params,
                                   const string& driver_name)
    : m_Impl(new CID2CDDProcessor_Impl(params, driver_name))
{
}


CID2CDDProcessor::~CID2CDDProcessor(void)
{
}


bool CID2CDDProcessor::NeedToProcessReplies(void) const
{
    return true;
}


CRef<CID2ProcessorContext> CID2CDDProcessor::CreateContext(void)
{
    return null;
}


CRef<CID2ProcessorPacketContext>
CID2CDDProcessor::ProcessPacket(CID2ProcessorContext* context,
                                CID2_Request_Packet& packet,
                                TReplies& replies)
{
    return m_Impl->ProcessPacket(packet, replies);
}

    
void CID2CDDProcessor::ProcessReply(CID2ProcessorContext* context,
                                    CID2ProcessorPacketContext* packet_context,
                                    CID2_Reply& reply,
                                    TReplies& replies)
{
    m_Impl->ProcessReply(reply, replies, packet_context);
}


END_NAMESPACE(objects);

void ID2Processors_Register_CDD(void)
{
    RegisterEntryPoint<objects::CID2Processor>(NCBI_EntryPoint_id2proc_cdd);
}


/// Class factory for ID1 reader
///
/// @internal
///
class CID2CDDProcessorCF : 
    public CSimpleClassFactoryImpl<objects::CID2Processor,
                                   objects::CID2CDDProcessor>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CID2Processor,
                                    objects::CID2CDDProcessor> TParent;
public:
    CID2CDDProcessorCF()
        : TParent(NCBI_ID2PROC_CDD_DRIVER_NAME, 0)
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
            drv = new objects::CID2CDDProcessor(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_id2proc_cdd(
     CPluginManager<objects::CID2Processor>::TDriverInfoList&   info_list,
     CPluginManager<objects::CID2Processor>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CID2CDDProcessorCF>::NCBI_EntryPointImpl(info_list, method);
}


END_NCBI_NAMESPACE;
