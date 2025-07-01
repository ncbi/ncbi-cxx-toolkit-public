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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_url.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/annot_selector.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>
#include <objtools/data_loaders/genbank/impl/psg_evloop.hpp>
#include <objtools/data_loaders/genbank/impl/psg_processors.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cache.hpp>
#include <objtools/data_loaders/genbank/gbloader_params.h>
#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <util/thread_pool.hpp>
#include <sstream>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)

using namespace psgl;

const int kDefaultCacheLifespanSeconds = 2*3600;
const size_t kDefaultMaxCacheSize = 10000;
const unsigned int kDefaultRetryCount = 4;
const unsigned int kDefaultBulkRetryCount = 8;

#define DEFAULT_WAIT_TIME 1
#define DEFAULT_WAIT_TIME_MULTIPLIER 1.5
#define DEFAULT_WAIT_TIME_INCREMENT 1
#define DEFAULT_WAIT_TIME_MAX 30

static CIncreasingTime::SAllParams s_WaitTimeParams = {
    {
        "wait_time",
        0,
        DEFAULT_WAIT_TIME
    },
    {
        "wait_time_max",
        0,
        DEFAULT_WAIT_TIME_MAX
    },
    {
        "wait_time_multiplier",
        0,
        DEFAULT_WAIT_TIME_MULTIPLIER
    },
    {
        "wait_time_increment",
        0,
        DEFAULT_WAIT_TIME_INCREMENT
    }
};


class CPSGDataLoader_Impl::CPSG_PrefetchCDD_Task : public CObject
{
public:
    CPSG_PrefetchCDD_Task(CPSGDataLoader_Impl& loader)
        : m_Loader(loader),
          m_CancelRequested(false),
          m_AddRequestSemaphore(0, kMax_UInt),
          m_QueueGuard(*loader.m_ThreadPool, *loader.m_Queue),
          m_ReadResultsThread(&CPSG_PrefetchCDD_Task::ReadResults, this)
    {
    }
    ~CPSG_PrefetchCDD_Task()
    {
        Stop();
    }

    void ReadResults();

    void AddRequest(const CDataLoader::TIds& ids);

    void RequestToCancel()
    {
        m_CancelRequested.store(true, memory_order_release);
    }
    
    bool IsCancelRequested()
    {
        return m_CancelRequested.load(memory_order_acquire);
    }

    void Stop()
    {
        RequestToCancel();
        m_QueueGuard.CancelAll(); // wake up from GetNextResult()
        m_AddRequestSemaphore.Post(); // wake up from waiting for new requests
        m_ReadResultsThread.join();
    }

private:
    CPSGDataLoader_Impl& m_Loader;
    atomic<bool> m_CancelRequested;
    CSemaphore m_AddRequestSemaphore;
    CPSGL_QueueGuard m_QueueGuard;
    thread m_ReadResultsThread;
};


void CPSGDataLoader_Impl::CPSG_PrefetchCDD_Task::ReadResults()
{
    while ( !IsCancelRequested() ) {
        if ( auto result = m_QueueGuard.GetNextResult() ) {
            if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
                if ( auto processor = result.GetProcessor<CPSGL_CDDAnnot_Processor>() ) {
                    // execution or processing are failed somehow
                    LOG_POST(Warning<<"CPSGDataLoader: Failed to load CDDs info for " << processor->m_CDDIds);
                }
            }
        }
        else {
            // no active requests, wait for new requests added
            m_AddRequestSemaphore.Wait();
        }
    }
}


void CPSGDataLoader_Impl::CPSG_PrefetchCDD_Task::AddRequest(const CDataLoader::TIds& ids)
{
    if ( IsCancelRequested() ) {
        return;
    }
    SCDDIds cdd_ids = x_GetCDDIds(ids);
    if ( !cdd_ids ) {
        // not a protein or no suitable ids
        return;
    }
    string blob_id = x_MakeLocalCDDEntryId(cdd_ids);
    if ( m_Loader.m_CDDInfoCache->Find(blob_id) ) {
        // known for no CDDs
        return;
    }
    auto cached = m_Loader.m_AnnotCache->Find(make_pair(kCDDAnnotName, ids));
    if ( cached ) {
        return;
    }
    CPSG_BioIds bio_ids;
    for (auto& id : ids) {
        bio_ids.push_back(CPSG_BioId(id));
    }
    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names({kCDDAnnotName});
    auto request = make_shared<CPSG_Request_NamedAnnotInfo>(std::move(bio_ids), annot_names);
    // TODO: do we need full CDD entry in prefetch?
    //request->IncludeData(CPSG_Request_Biodata::eWholeTSE);
    request->IncludeData(CPSG_Request_Biodata::eNoTSE);
    CRef<CPSGL_CDDAnnot_Processor> processor
        (new CPSGL_CDDAnnot_Processor(cdd_ids,
                                      ids,
                                      nullptr,
                                      m_Loader.m_AnnotCache.get(),
                                      m_Loader.m_CDDInfoCache.get(),
                                      m_Loader.m_BlobMap.get()));
    m_QueueGuard.AddRequest(request, processor);
    m_AddRequestSemaphore.Post();
}


/////////////////////////////////////////////////////////////////////////////
// CPSGDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////

#define NCBI_PSGLOADER_NAME "psg_loader"
#define NCBI_PSGLOADER_SERVICE_NAME "service_name"
#define NCBI_PSGLOADER_NOSPLIT "no_split"
#define NCBI_PSGLOADER_WHOLE_TSE "whole_tse"
#define NCBI_PSGLOADER_WHOLE_TSE_BULK "whole_tse_bulk"
#define NCBI_PSGLOADER_ADD_WGS_MASTER "add_wgs_master"
//#define NCBI_PSGLOADER_RETRY_COUNT "retry_count"

NCBI_PARAM_DECL(string, PSG_LOADER, SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, PSG_LOADER, SERVICE_NAME, "PSG2",
                  eParam_NoThread, PSG_LOADER_SERVICE_NAME);
typedef NCBI_PARAM_TYPE(PSG_LOADER, SERVICE_NAME) TPSG_ServiceName;

NCBI_PARAM_DECL(bool, PSG_LOADER, WHOLE_TSE);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, WHOLE_TSE, false,
                  eParam_NoThread, PSG_LOADER_WHOLE_TSE);
typedef NCBI_PARAM_TYPE(PSG_LOADER, WHOLE_TSE) TPSG_WholeTSE;

NCBI_PARAM_DECL(bool, PSG_LOADER, WHOLE_TSE_BULK);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, WHOLE_TSE_BULK, false,
                  eParam_NoThread, PSG_LOADER_WHOLE_TSE_BULK);
typedef NCBI_PARAM_TYPE(PSG_LOADER, WHOLE_TSE_BULK) TPSG_WholeTSEBulk;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, MAX_POOL_THREADS);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, MAX_POOL_THREADS, 10,
                  eParam_NoThread, PSG_LOADER_MAX_POOL_THREADS);
typedef NCBI_PARAM_TYPE(PSG_LOADER, MAX_POOL_THREADS) TPSG_MaxPoolThreads;

NCBI_PARAM_DECL(bool, PSG_LOADER, PREFETCH_CDD);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, PREFETCH_CDD, false,
                  eParam_NoThread, PSG_LOADER_PREFETCH_CDD);
typedef NCBI_PARAM_TYPE(PSG_LOADER, PREFETCH_CDD) TPSG_PrefetchCDD;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, RETRY_COUNT);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, RETRY_COUNT, kDefaultRetryCount,
                  eParam_NoThread, PSG_LOADER_RETRY_COUNT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, RETRY_COUNT) TPSG_RetryCount;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, BULK_RETRY_COUNT);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, BULK_RETRY_COUNT, kDefaultBulkRetryCount,
                  eParam_NoThread, PSG_LOADER_BULK_RETRY_COUNT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, BULK_RETRY_COUNT) TPSG_BulkRetryCount;

NCBI_PARAM_DECL(bool, PSG_LOADER, IPG_TAX_ID);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, IPG_TAX_ID, false, eParam_NoThread, PSG_LOADER_IPG_TAX_ID);
typedef NCBI_PARAM_TYPE(PSG_LOADER, IPG_TAX_ID) TPSG_IpgTaxIdEnabled;


template<class TParamType>
static void s_ConvertParamValue(TParamType& value, const string& str)
{
    value = NStr::StringToNumeric<TParamType>(str);
}


template<>
void s_ConvertParamValue<bool>(bool& value, const string& str)
{
    value = NStr::StringToBool(str);
}


static const TPluginManagerParamTree* s_FindSubNode(const TPluginManagerParamTree* params,
                                                    const string& name)
{
    return params? params->FindSubNode(name): 0;
}


template<class TParamDescription>
static typename TParamDescription::TValueType s_GetParamValue(const TPluginManagerParamTree* config)
{
    typedef CParam<TParamDescription> TParam;
    typename TParam::TValueType value = TParam::GetDefault();
    CParamBase::EParamSource source = CParamBase::eSource_NotSet;
    TParam::GetState(0, &source);
    if ( config && (source == CParamBase::eSource_NotSet ||
                    source == CParamBase::eSource_Default ||
                    source == CParamBase::eSource_Config) ) {
        if ( const TPluginManagerParamTree* node = s_FindSubNode(config, TParamDescription::sm_ParamDescription.name) ) {
            s_ConvertParamValue<typename TParam::TValueType>(value, node->GetValue().value);
        }
    }
    return value;
}


CPSGDataLoader_Impl::CPSGDataLoader_Impl(const CGBLoaderParams& params)
    : m_ThreadPool(new CThreadPool(kMax_UInt, TPSG_MaxPoolThreads::GetDefault())),
      m_WaitTime(s_WaitTimeParams)
{
    unique_ptr<CPSGDataLoader::TParamTree> app_params;
    const CPSGDataLoader::TParamTree* psg_params = 0;
    if (params.GetParamTree()) {
        psg_params = CPSGDataLoader::GetParamsSubnode(params.GetParamTree(), NCBI_PSGLOADER_NAME);
    }
    else {
        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        if (app) {
            app_params.reset(CConfig::ConvertRegToTree(app->GetConfig()));
            psg_params = CPSGDataLoader::GetParamsSubnode(app_params.get(), NCBI_PSGLOADER_NAME);
        }
    }

    string service_name = params.GetPSGServiceName();
    if (service_name.empty() && psg_params) {
        service_name = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_SERVICE_NAME);
    }
    if (service_name.empty()) {
        service_name = TPSG_ServiceName::GetDefault();
    }

    bool no_split = params.GetPSGNoSplit();
    if (psg_params) {
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_NOSPLIT);
            if (!value.empty()) {
                no_split = NStr::StringToBool(value);
            }
        }
        catch (CException&) {
        }
    }
    if ( no_split ) {
        m_TSERequestMode = CPSG_Request_Biodata::eOrigTSE;
        m_TSERequestModeBulk = CPSG_Request_Biodata::eOrigTSE;
    }
    else {
        m_TSERequestMode = (s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, WHOLE_TSE)>(psg_params)?
                            CPSG_Request_Biodata::eWholeTSE:
                            CPSG_Request_Biodata::eSmartTSE);
        m_TSERequestModeBulk = (s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, WHOLE_TSE_BULK)>(psg_params)?
                                CPSG_Request_Biodata::eWholeTSE:
                                CPSG_Request_Biodata::eSmartTSE);
    }
    
    m_AddWGSMasterDescr = true;
    if ( psg_params ) {
        string param = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_ADD_WGS_MASTER);
        if ( !param.empty() ) {
            try {
                m_AddWGSMasterDescr = NStr::StringToBool(param);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW_FMT(exc, CLoaderException, eBadConfig,
                                 "Bad value of parameter "
                                 NCBI_PSGLOADER_ADD_WGS_MASTER
                                 ": \""<<param<<"\"");
            }
        }
    }

    m_CacheLifespan = kDefaultCacheLifespanSeconds;
    size_t cache_max_size = kDefaultMaxCacheSize;
    if (psg_params) {
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_GBLOADER_PARAM_ID_EXPIRATION_TIMEOUT);
            if (!value.empty()) {
                m_CacheLifespan = NStr::StringToNumeric<int>(value);
            }
        }
        catch (CException&) {
        }
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_GBLOADER_PARAM_ID_GC_SIZE);
            if (!value.empty()) {
                cache_max_size = NStr::StringToNumeric<size_t>(value);
            }
        }
        catch (CException&) {
        }
    }

    m_RetryCount =
        s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, RETRY_COUNT)>(psg_params);
    m_BulkRetryCount =
        s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, BULK_RETRY_COUNT)>(psg_params);
    if ( psg_params ) {
        CConfig conf(psg_params);
        m_WaitTime.Init(conf, NCBI_PSGLOADER_NAME, s_WaitTimeParams);
    }

    m_BioseqCache.reset(new CPSGBioseqCache(m_CacheLifespan, cache_max_size));
    m_AnnotCache.reset(new CPSGAnnotCache(m_CacheLifespan, cache_max_size));
    m_BlobMap.reset(new CPSGBlobMap(m_CacheLifespan, cache_max_size));

    if ( !params.GetWebCookie().empty() ) {
        m_RequestContext = new CRequestContext();
        m_RequestContext->SetProperty("auth_token", params.GetWebCookie());
    }
    {{
        m_Queue = new CPSGL_Queue(service_name);
        m_Queue->GetPSG_Queue().SetRequestFlags(params.HasHUPIncluded()? CPSG_Request::fIncludeHUP: CPSG_Request::fExcludeHUP);
        if ( m_RequestContext ) {
            m_Queue->SetRequestContext(m_RequestContext);
        }
    }}

    m_CDDInfoCache.reset(new CPSGCDDInfoCache(m_CacheLifespan, cache_max_size));
    if (TPSG_PrefetchCDD::GetDefault()) {
        m_CDDPrefetchTask.Reset(new CPSG_PrefetchCDD_Task(*this));
    }

    if (TPSG_IpgTaxIdEnabled::GetDefault()) {
        m_IpgTaxIdMap.reset(new CPSGIpgTaxIdMap(m_CacheLifespan, cache_max_size));
    }

    CUrlArgs args;
    if (params.IsSetEnableSNP()) {
        args.AddValue(params.GetEnableSNP() ? "enable_processor" : "disable_processor", "snp");
    }
    if (params.IsSetEnableWGS()) {
        args.AddValue(params.GetEnableWGS() ? "enable_processor" : "disable_processor", "wgs");
    }
    if (params.IsSetEnableCDD()) {
        args.AddValue(params.GetEnableCDD() ? "enable_processor" : "disable_processor", "cdd");
    }
    static size_t s_PSGLoaderInstanceCounter = 0;
    size_t current_instance_number = ++s_PSGLoaderInstanceCounter;
    if ( current_instance_number > 1 ) {
        string base_client_id = GetDiagContext().GetStringUID();
        string new_client_id = base_client_id+'.'+NStr::NumericToString(current_instance_number);
        args.AddValue("client_id", new_client_id);
    }
    if (!args.GetArgs().empty()) {
        m_Queue->GetPSG_Queue().SetUserArgs(SPSG_UserArgs(args));
    }
}


CPSGDataLoader_Impl::~CPSGDataLoader_Impl(void)
{
    m_CDDPrefetchTask.Reset();
    // Make sure thread pool is destroyed before any tasks (e.g. CDD prefetch)
    // and stops them all before the loader is destroyed.
    m_ThreadPool.reset();
}


static bool CannotProcess(const CSeq_id_Handle& sih)
{
    if ( !sih ) {
        return true;
    }
    if ( sih.Which() == CSeq_id::e_Local ) {
        return true;
    }
    if ( sih.Which() == CSeq_id::e_General ) {
        if ( NStr::EqualNocase(sih.GetSeqId()->GetGeneral().GetDb(), "SRA") ) {
            // SRA is good
            return false;
        }
        if ( NStr::StartsWith(sih.GetSeqId()->GetGeneral().GetDb(), "WGS:", NStr::eNocase) ) {
            // WGS is good
            return false;
        }
        // other general ids are good too(?)
        return false;
    }
    return false;
}


template<class Call>
typename std::invoke_result<Call>::type
CPSGDataLoader_Impl::CallWithRetry(Call&& call,
                                   const char* name,
                                   int retry_count)
{
    if ( retry_count == 0 ) {
        retry_count = m_RetryCount;
    }
    for ( int t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CBlobStateException& ) {
            // no retry
            throw;
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.eConnectionFailed ||
                 exc.GetErrCode() == exc.eLoaderFailed ) {
                // can retry
                LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc);
            }
            else {
                // no retry
                throw;
            }
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception");
        }
        if ( t >= 2 ) {
            double wait_sec = m_WaitTime.GetTime(t-2);
            LOG_POST(Warning<<"CPSGDataLoader: waiting "<<wait_sec<<"s before retry");
            SleepMilliSec(Uint4(wait_sec*1000));
        }
    }
    return call();
}


void CPSGDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetIdsOnce, this,
                       cref(idh), ref(ids)),
                  "GetIds");
}


void CPSGDataLoader_Impl::GetIdsOnce(const CSeq_id_Handle& idh, TIds& ids)
{
    if ( CannotProcess(idh) ) {
        return;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    if (!seq_info) return;

    ITERATE(SPsgBioseqInfo::TIds, it, seq_info->ids) {
        ids.push_back(*it);
    }
}


CDataLoader::SGiFound
CPSGDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetGiOnce, this,
                              cref(idh)),
                         "GetGi");
}


CDataLoader::SGiFound
CPSGDataLoader_Impl::GetGiOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::SGiFound();
    }
    CDataLoader::SGiFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info) {
        ret.sequence_found = true;
        if ( seq_info->gi != ZERO_GI ) {
            ret.gi = seq_info->gi;
        }
    }
    return ret;
}


CDataLoader::SAccVerFound
CPSGDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetAccVerOnce, this,
                              cref(idh)),
                         "GetAccVer");
}


CDataLoader::SAccVerFound
CPSGDataLoader_Impl::GetAccVerOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::SAccVerFound();
    }
    CDataLoader::SAccVerFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info) {
        ret.sequence_found = true;
        if ( seq_info->canonical.IsAccVer() ) {
            ret.acc_ver = seq_info->canonical;
        }
    }
    return ret;
}


TTaxId CPSGDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetTaxIdOnce, this,
                              cref(idh)),
                         "GetTaxId");
}


TTaxId CPSGDataLoader_Impl::GetTaxIdOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return INVALID_TAX_ID;
    }
    auto tax_id = x_GetIpgTaxId(idh);
    if (tax_id != INVALID_TAX_ID) {
        return tax_id;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    if ( !seq_info ) {
        _TRACE("no seq_info for "<<idh);
    }
    else if ( seq_info->tax_id <= 0 ) {
        _TRACE("bad tax_id for "<<idh<<" : "<<seq_info->tax_id);
    }
    return seq_info ? seq_info->tax_id : INVALID_TAX_ID;
}


void CPSGDataLoader_Impl::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetTaxIdsOnce, this,
                              cref(ids), ref(loaded), ref(ret)), "GetTaxId");
}


void CPSGDataLoader_Impl::GetTaxIdsOnce(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    auto [ load_count1, fail_count1 ] = x_GetIpgTaxIds(ids, loaded, ret);
    auto fail_count = fail_count1;
    if ( load_count1 != ids.size() ) {
        vector<shared_ptr<SPsgBioseqInfo>> infos;
        infos.resize(ret.size());
        auto [ load_count2, fail_count2 ] = x_GetBulkBioseqInfo(ids, loaded, infos);
        if ( load_count2 ) {
            // have loaded infos
            for (size_t i = 0; i < infos.size(); ++i) {
                if (loaded[i] || !infos[i].get()) {
                    continue;
                }
                ret[i] = infos[i]->tax_id;
                loaded[i] = true;
            }
        }
        fail_count += fail_count2;
    }
    if ( fail_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<fail_count<<" seq-ids in bulk request");
    }
}


TSeqPos CPSGDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceLengthOnce, this,
                              cref(idh)),
                         "GetSequenceLength");
}


TSeqPos CPSGDataLoader_Impl::GetSequenceLengthOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return kInvalidSeqPos;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    return (seq_info && seq_info->length > 0) ? TSeqPos(seq_info->length) : kInvalidSeqPos;
}


CDataLoader::SHashFound
CPSGDataLoader_Impl::GetSequenceHash(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceHashOnce, this,
                              cref(idh)),
                         "GetSequenceHash");
}


CDataLoader::SHashFound
CPSGDataLoader_Impl::GetSequenceHashOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::SHashFound();
    }
    CDataLoader::SHashFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info) {
        ret.sequence_found = true;
        if ( seq_info->hash ) {
            ret.hash_known = true;
            ret.hash = seq_info->hash;
        }
    }
    return ret;
}


CDataLoader::STypeFound
CPSGDataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceTypeOnce, this,
                              cref(idh)),
                         "GetSequenceType");
}


CDataLoader::STypeFound
CPSGDataLoader_Impl::GetSequenceTypeOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::STypeFound();
    }
    CDataLoader::STypeFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info && seq_info->molecule_type != CSeq_inst::eMol_not_set) {
        ret.sequence_found = true;
        ret.type = seq_info->molecule_type;
    }
    return ret;
}


int CPSGDataLoader_Impl::GetSequenceState(CDataSource* data_source, const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceStateOnce, this,
                              data_source, cref(idh)),
                         "GetSequenceState");
}


int CPSGDataLoader_Impl::GetSequenceStateOnce(CDataSource* data_source, const CSeq_id_Handle& idh)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
                           CBioseq_Handle::fState_no_data);
    if ( CannotProcess(idh) ) {
        return kNotFound;
    }
    auto info = x_GetBioseqAndBlobInfo(data_source, idh);
    if ( !info.first ) {
        return kNotFound;
    }
    CBioseq_Handle::TBioseqStateFlags state = info.first->GetBioseqStateFlags();
    if ( info.second ) {
        state |= info.second->blob_state_flags;
        if (!(info.first->GetBioseqStateFlags() & CBioseq_Handle::fState_dead) &&
            !(info.first->GetChainStateFlags() & CBioseq_Handle::fState_dead)) {
            state &= ~CBioseq_Handle::fState_dead;
        }
    }
    return state;
}


CDataLoader::TTSE_LockSet
CPSGDataLoader_Impl::GetRecords(CDataSource* data_source,
                                const CSeq_id_Handle& idh,
                                CDataLoader::EChoice choice)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetRecordsOnce, this,
                              data_source, cref(idh), choice),
                         "GetRecords");
}


struct SErrorFormatter
{
    explicit
    SErrorFormatter(const CPSGL_Processor* processor)
        : processor(processor)
    {
    }

    const CPSGL_Processor* processor;
};
static ostream& operator<<(ostream& out, SErrorFormatter formatter)
{
    if ( auto processor = formatter.processor ) {
        out << ": " << processor->Descr() << ':';
        for ( auto& error : processor->GetErrors() ) {
            out << ' ' << error << '\n';
        }
    }
    return out;
}


static auto FormatError(const CPSGL_Processor* processor)
{
    return SErrorFormatter(processor);
}


CDataLoader::TTSE_LockSet
CPSGDataLoader_Impl::GetRecordsOnce(CDataSource* data_source,
                                    const CSeq_id_Handle& idh,
                                    CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    if (choice == CDataLoader::eOrphanAnnot) {
        // PSG loader doesn't provide orphan annotations
        return locks;
    }
    if ( CannotProcess(idh) ) {
        return locks;
    }

    CPSG_Request_Biodata::EIncludeData inc_data = CPSG_Request_Biodata::eNoTSE;
    if (data_source) {
        inc_data = m_TSERequestMode;
    }
    
    // we may already know blob_id
    auto bioseq_info = m_BioseqCache->Find(idh);
    if ( bioseq_info && bioseq_info->KnowsBlobId() ) {
        // blob id is known
        if ( !bioseq_info->HasBlobId() ) {
            // no blob
            return locks;
        }
        locks.insert(GetBlobByIdOnce(data_source, *bioseq_info->GetDLBlobId()));
        return locks;
    }
    
    // get request with resolution
    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
    {{
        CPSG_BioId bio_id(idh);
        auto request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        if ( data_source ) {
            CDataSource::TLoadedBlob_ids loaded_blob_ids;
            data_source->GetLoadedBlob_ids(idh, CDataSource::fKnown_bioseqs, loaded_blob_ids);
            ITERATE(CDataSource::TLoadedBlob_ids, loaded_blob_id, loaded_blob_ids) {
                const CPsgBlobId* pbid = dynamic_cast<const CPsgBlobId*>(&**loaded_blob_id);
                if (!pbid) continue;
                request->ExcludeTSE(CPSG_BlobId(pbid->ToPsgId()));
            }
        }
        request->IncludeData(inc_data);
        CRef<CPSGL_Get_Processor> processor
            (new CPSGL_Get_Processor(idh,
                                     data_source,
                                     m_BioseqCache.get(),
                                     m_BlobMap.get(),
                                     m_AddWGSMasterDescr));
        
        queue.AddRequest(request, processor);
    }}
    
    auto result = queue.GetNextResult();
    _ASSERT(result);
    if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to get bioseq info for "<<idh<<
                       FormatError(result.GetProcessor()));
    }
    
    auto processor = result.GetProcessor<CPSGL_Get_Processor>();
    _ASSERT(processor);
    
    if ( auto tse_lock = processor->GetTSE_Lock() ) {
        locks.insert(tse_lock);
    }
    else if ( !processor->HasBlob_id() ) {
        // no blob
        return locks;
    }
    else if ( processor->GotForbidden() ) {
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+idh.AsString(),
                    processor->GetForbiddenBlobState());
    }
    else {
        // we can get only blob id without blob
        locks.insert(GetBlobByIdOnce(data_source, *processor->GetDLBlobId()));
    }
    if ( m_CDDPrefetchTask ) {
        if ( auto bioseq_info = m_BioseqCache->Find(idh) ) {
            auto cdd_ids = x_GetCDDIds(bioseq_info->ids);
            if (cdd_ids && !m_CDDInfoCache->Find(x_MakeLocalCDDEntryId(cdd_ids))) {
                m_CDDPrefetchTask->AddRequest(bioseq_info->ids);
            }
        }
    }
    return locks;
}


CConstRef<CPsgBlobId> CPSGDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobIdOnce, this,
                              cref(idh)),
                         "GetBlobId");
}


CConstRef<CPsgBlobId> CPSGDataLoader_Impl::GetBlobIdOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return null;
    }
    string blob_id;
    auto seq_info = m_BioseqCache->Find(idh);
    if ( seq_info && seq_info->KnowsBlobId() ) {
        // blob id is known
    }
    else {
        // ask for blob id
        CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
        {{
            CPSG_BioId bio_id(idh);
            auto request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
            request->IncludeData(CPSG_Request_Biodata::eNoTSE);
            CRef<CPSGL_Get_Processor> processor
                (new CPSGL_Get_Processor(idh,
                                         nullptr,
                                         m_BioseqCache.get(),
                                         m_BlobMap.get(),
                                         m_AddWGSMasterDescr));
            queue.AddRequest(request, processor);
        }}
        auto result = queue.GetNextResult();
        _ASSERT(result);
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("Failed to get blob id for " << idh);
            NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                           "CPSGDataLoader::GetBlobId("<<idh<<") failed"<<
                           FormatError(result.GetProcessor()));
        }
        
        auto processor = result.GetProcessor<CPSGL_Get_Processor>();
        _ASSERT(processor);
        if ( processor->GetBioseqInfoStatus() == EPSG_Status::eError ) {
            _TRACE("Failed to get blob id for " << idh);
            NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                           "CPSGDataLoader::GetBlobId() failed"<<
                           FormatError(result.GetProcessor()));
        }
        seq_info = processor->GetBioseqInfoResult();
    }
    if ( !seq_info->HasBlobId() ) {
        // no blob
        return null;
    }
    return seq_info->GetDLBlobId();
}


void CPSGDataLoader_Impl::SetGetBlobByIdShouldFail(bool value)
{
    s_GetBlobByIdShouldFail = value;
}


bool CPSGDataLoader_Impl::GetGetBlobByIdShouldFail()
{
    return s_GetBlobByIdShouldFail;
}


CTSE_Lock CPSGDataLoader_Impl::GetBlobById(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobByIdOnce, this,
                              data_source, cref(blob_id)),
                         "GetBlobById",
                         GetGetBlobByIdShouldFail()? 1: 0);
}


CTSE_Lock CPSGDataLoader_Impl::GetBlobByIdOnce(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    if (!data_source) return CTSE_Lock();

    if ( GetGetBlobByIdShouldFail() ) {
        _TRACE("GetBlobById("<<blob_id.ToPsgId()<<") should fail");
    }
    CTSE_LoadLock load_lock;
    {{
        CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(&blob_id);
        load_lock = data_source->GetTSE_LoadLockIfLoaded(dl_blob_id);
        if ( load_lock && load_lock.IsLoaded() ) {
            _TRACE("GetBlobById() already loaded " << blob_id.ToPsgId());
            return load_lock;
        }
    }}
    
    CTSE_Lock ret;
    if ( SCDDIds cdd_ids = x_ParseLocalCDDEntryId(blob_id) ) {
        if ( s_GetDebugLevel() >= 5 ) {
            LOG_POST(Info<<"PSG loader: Re-loading CDD blob: " << blob_id.ToString());
        }
        ret = x_CreateLocalCDDEntry(data_source, cdd_ids);
    }
    else {
        CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
        {{
            CPSG_BlobId bid(blob_id.ToPsgId());
            auto request = make_shared<CPSG_Request_Blob>(bid);
            request->IncludeData(m_TSERequestMode);
            CRef<CPSGL_GetBlob_Processor> processor
                (new CPSGL_GetBlob_Processor(blob_id,
                                             data_source,
                                             m_BlobMap.get(),
                                             m_AddWGSMasterDescr));
            queue.AddRequest(request, processor);
        }}
        auto result = queue.GetNextResult();
        _ASSERT(result);
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                           "failed to get blob "<<blob_id.ToPsgId()<<
                           FormatError(result.GetProcessor()));
        }

        auto processor = result.GetProcessor<CPSGL_GetBlob_Processor>();
        _ASSERT(processor);
        ret = processor->GetTSE_Lock();
        if ( !ret && (processor->GotForbidden() || processor->GotUnauthorized()) ) {
            CBioseq_Handle::TBioseqStateFlags state = CBioseq_Handle::fState_no_data;
            if ( processor->GotUnauthorized() ) {
                state |= CBioseq_Handle::fState_confidential;
            }
            else {
                state |= CBioseq_Handle::fState_withdrawn;
            }
            state |= processor->GetBlobInfoState(blob_id.ToPsgId());
            NCBI_THROW2(CBlobStateException, eBlobStateError,
                        "blob state error for "+blob_id.ToPsgId(), state);
        }
    }
    if (!ret) {
        _TRACE("Failed to load blob for " << blob_id.ToPsgId()<<" @ "<<CStackTrace());
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CPSGDataLoader::GetBlobById("+blob_id.ToPsgId()+") failed");
    }
    return ret;
}


void CPSGDataLoader_Impl::GetBlobs(CDataSource* data_source, TTSE_LockSets& tse_sets)
{
    TLoadedSeqIds loaded;
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobsOnce, this,
                       data_source, ref(loaded), ref(tse_sets)),
                  "GetBlobs",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetBlobsOnce(CDataSource* data_source,
                                       TLoadedSeqIds& loaded,
                                       TTSE_LockSets& tse_sets)
{
    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    enum ERequestType {
        eRequestGet,
        eRequestGetBlob
    };
    ITERATE(TTSE_LockSets, tse_set, tse_sets) {
        const CSeq_id_Handle& idh = tse_set->first;
        if ( loaded.count(idh) ) {
            continue;
        }
        CPSG_BioId bio_id(idh);
        auto request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        CPSG_Request_Biodata::EIncludeData inc_data = CPSG_Request_Biodata::eNoTSE;
        if ( data_source ) {
            inc_data = m_TSERequestModeBulk;
            CDataSource::TLoadedBlob_ids loaded_blob_ids;
            data_source->GetLoadedBlob_ids(idh, CDataSource::fKnown_bioseqs, loaded_blob_ids);
            ITERATE(CDataSource::TLoadedBlob_ids, loaded_blob_id, loaded_blob_ids) {
                const CPsgBlobId* pbid = dynamic_cast<const CPsgBlobId*>(&**loaded_blob_id);
                if (!pbid) continue;
                request->ExcludeTSE(CPSG_BlobId(pbid->ToPsgId()));
            }
        }
        request->IncludeData(inc_data);
        CRef<CPSGL_Get_Processor> processor
            (new CPSGL_Get_Processor(idh,
                                     data_source,
                                     m_BioseqCache.get(),
                                     m_BlobMap.get(),
                                     m_AddWGSMasterDescr));
        queue.AddRequest(request, processor, eRequestGet);
    }
    size_t failed_count = 0;
    // Waiting for skipped blobs can block all pool threads. To prevent this postpone
    // waiting until all other tasks are completed.

    // we need to remember the reason for reqursive getblob requests
    typedef vector<CSeq_id_Handle> TRequests;
    map<CRef<CPSGL_GetBlob_Processor>, TRequests> getblob2reqs;
    CRef<CPSGL_Processor> first_failed_processor;
    while ( auto result = queue.GetNextResult() ) {
        if ( result.GetIndex() == eRequestGet ) {
            auto processor = result.GetProcessor<CPSGL_Get_Processor>();
            const CSeq_id_Handle& idh = processor->GetSeq_id();
            if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
                NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                               "failed to get bioseq info for "<<idh<<
                               FormatError(result.GetProcessor()));
            }
            
            if ( auto tse_lock = processor->GetTSE_Lock() ) {
                tse_sets[idh].insert(tse_lock);
                loaded.insert(idh);
            }
            else if ( !processor->HasBlob_id() ) {
                // no blob
                loaded.insert(idh);
            }
            else {
                // we can get only blob id without blob
                // retry with explcit blob id
                if ( s_GetDebugLevel() >= 5 ) {
                    LOG_POST(Info<<"PSG loader: Re-loading blob: " << processor->GetPSGBlobId()<<" for "<<idh);
                }
                CPSG_BlobId blob_id(processor->GetPSGBlobId());
                auto request = make_shared<CPSG_Request_Blob>(std::move(blob_id));
                request->IncludeData(m_TSERequestMode);
                CRef<CPSGL_GetBlob_Processor> processor2
                    (new CPSGL_GetBlob_Processor(*processor->GetDLBlobId(),
                                                 data_source,
                                                 m_BlobMap.get(),
                                                 m_AddWGSMasterDescr));
                getblob2reqs[processor2].push_back(idh);
                queue.AddRequest(request, processor2, eRequestGetBlob);
            }
        }
        else { // eRequestGetBlob
            auto processor = result.GetProcessor<CPSGL_GetBlob_Processor>();
            TRequests reqs = std::move(getblob2reqs[processor]);
            getblob2reqs.erase(processor);
            if ( result.GetStatus() != CThreadPool_Task::eCompleted ||
                 !processor->GetTSE_Lock() ) {
                NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                               "failed to get blob "<<processor->GetBlob_id()<<
                               " for "<<reqs.front()<<
                               FormatError(result.GetProcessor()));
            }
            
            auto tse_lock = processor->GetTSE_Lock();
            for ( auto& req : reqs ) {
                tse_sets[req].insert(tse_lock);
                loaded.insert(req);
            }
        }
    }
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" blobs"<<
                       FormatError(first_failed_processor));
    }
}


void CPSGDataLoader_Impl::GetCDDAnnots(CDataSource* data_source,
                                       const TSeqIdSets& id_sets,
                                       TLoaded& loaded,
                                       TCDD_Locks& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetCDDAnnotsOnce, this,
                       data_source, id_sets, ref(loaded), ref(ret)),
                  "GetCDDAnnots",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::LoadChunk(CDataSource* data_source,
                                    CTSE_Chunk_Info& chunk_info)
{
    CDataLoader::TChunkSet chunks;
    chunks.push_back(Ref(&chunk_info));
    LoadChunks(data_source, chunks);
}


bool CPSGDataLoader_Impl::x_ReadCDDChunk(CDataSource* data_source,
                                         CDataLoader::TChunk chunk,
                                         const CPSG_BlobInfo& blob_info,
                                         const CPSG_BlobData& blob_data)
{
    _ASSERT(chunk->GetChunkId() == kDelayedMain_ChunkId);
    _DEBUG_ARG(const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk->GetBlobId()));
    _ASSERT(x_IsLocalCDDEntryId(blob_id));
    _ASSERT(!chunk->IsLoaded());
    
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(chunk->GetBlobId());
    if ( !load_lock ||
         !load_lock.IsLoaded() ||
         !load_lock->x_NeedsDelayedMainChunk() ) {
        _TRACE("Cannot make CDD entry because of wrong TSE state id="<<blob_id.ToString());
        return false;
    }
    
    unique_ptr<CObjectIStream> in(GetBlobDataStream(blob_info, blob_data));
    if (!in.get()) {
        _TRACE("Failed to open blob data stream for blob-id " << blob_id.ToString());
        return false;
    }

    CRef<CSeq_entry> entry(new CSeq_entry);
    *in >> *entry;
    if ( s_GetDebugLevel() >= 8 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<load_lock->GetBlobId().ToString()<<" "<<
                 MSerial_AsnText<<*entry);
    }
    if ( s_GetDebugLevel() >= 6 ) {
        set<CSeq_id_Handle> annot_ids;
        for ( CTypeConstIterator<CSeq_id> it = ConstBegin(*entry); it; ++it ) {
            annot_ids.insert(CSeq_id_Handle::GetHandle(*it));
        }
        for ( auto& id : annot_ids ) {
            LOG_POST(Info<<"CPSGDataLoader: CDD actual id "<<MSerial_AsnText<<*id.GetSeqId());
        }
    }
    load_lock->SetSeq_entry(*entry);
    chunk->SetLoaded();
    return true;
}


void CPSGDataLoader_Impl::LoadChunks(CDataSource* data_source,
                                     const CDataLoader::TChunkSet& chunks)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::LoadChunksOnce, this,
                       data_source, cref(chunks)),
                  "LoadChunks");
}


void CPSGDataLoader_Impl::LoadChunksOnce(CDataSource* data_source,
                                         const CDataLoader::TChunkSet& chunks)
{
    if (chunks.empty()) return;

    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    enum ERequestType {
        eRequestChunk,      // plain chunk
        eRequestDelayedTSE, // delayed main chunk - full TSE
        eRequestCDD         // CDD with yet unknown blob_id, may require eRequestDelayedTSE later
    };
    for ( auto& it : chunks) {
        CTSE_Chunk_Info& chunk = it.GetNCObject();
        if ( chunk.IsLoaded() ) {
            continue;
        }
        if ( chunk.GetChunkId() == kMasterWGS_ChunkId ) {
            CWGSMasterSupport::LoadWGSMaster(data_source->GetDataLoader(), it);
            continue;
        }
        if ( chunk.GetChunkId() == kDelayedMain_ChunkId ) {
            const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId());
            if ( SCDDIds cdd_ids = x_ParseLocalCDDEntryId(blob_id) ) {
                if (m_CDDInfoCache->Find(blob_id.ToPsgId())) {
                    x_CreateEmptyLocalCDDEntry(data_source, &chunk);
                    continue;
                }
                CPSG_BioId bio_id = x_LocalCDDEntryIdToBioId(cdd_ids); // back to Seq id
                CPSG_Request_NamedAnnotInfo::TAnnotNames names = { kCDDAnnotName };
                _ASSERT(bio_id.GetId().find('|') == NPOS);
                auto request = make_shared<CPSG_Request_NamedAnnotInfo>(bio_id, names);
                request->IncludeData(m_TSERequestMode);
                CPSGL_NA_Processor::TSeq_ids ids;
                ids.push_back(cdd_ids.gi);
                if ( cdd_ids.acc_ver ) {
                    ids.push_back(cdd_ids.acc_ver);
                }
                CRef<CPSGL_LocalCDDBlob_Processor> processor
                    (new CPSGL_LocalCDDBlob_Processor(chunk,
                                                      cdd_ids,
                                                      data_source,
                                                      m_BlobMap.get(),
                                                      m_AddWGSMasterDescr));
                queue.AddRequest(request, processor, eRequestCDD);
            }
            else {
                auto request = make_shared<CPSG_Request_Blob>(blob_id.ToPsgId());
                request->IncludeData(m_TSERequestMode);
                CRef<CPSGL_GetBlob_Processor> processor
                    (new CPSGL_GetBlob_Processor(blob_id,
                                                 data_source,
                                                 m_BlobMap.get(),
                                                 m_AddWGSMasterDescr));
                processor->SetLockedDelayedChunkInfo(blob_id.ToPsgId(), chunk);
                queue.AddRequest(request, processor, eRequestDelayedTSE);
            }
        }
        else {
            const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId());
            _ASSERT(!blob_id.GetId2Info().empty());
            auto request = make_shared<CPSG_Request_Chunk>(CPSG_ChunkId(chunk.GetChunkId(),
                                                                        blob_id.GetId2Info()));
            CRef<CPSGL_GetChunk_Processor> processor
                (new CPSGL_GetChunk_Processor(chunk,
                                              data_source,
                                              m_BlobMap.get(),
                                              m_AddWGSMasterDescr));
            queue.AddRequest(request, processor, eRequestChunk);
        }
    }
    size_t failed_count = 0;
    CRef<CPSGL_Processor> first_failed_processor;
    while ( auto result = queue.GetNextResult() ) {
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("CPSGDataLoader::LoadChunks(): failed to process request "<<
                   result.GetProcessor<CPSGL_Blob_Processor>()->Descr());
            ++failed_count;
            if ( !first_failed_processor ) {
                first_failed_processor = result.GetProcessor();
            }
            continue;
        }
    }
    // check if all chunks are loaded
    ITERATE ( CDataLoader::TChunkSet, it, chunks ) {
        const CTSE_Chunk_Info & chunk = **it;
        if ( !chunk.IsLoaded() ) {
            _TRACE("CPSGDataLoader::LoadChunks(): failed to load chunk "<<
                   chunk.GetChunkId()<<" of "<<chunk.GetBlobId()->ToString());
            ++failed_count;
        }
    }
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" chunks"<<
                       FormatError(first_failed_processor));
    }
}


static
pair<CRef<CTSE_Chunk_Info>, string>
s_CreateNAChunk(const CPSG_NamedAnnotInfo& psg_annot_info)
{
    pair<CRef<CTSE_Chunk_Info>, string> ret;
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    unsigned main_count = 0;
    unsigned zoom_count = 0;
    // detailed annot info
    set<string> names;
    for ( auto& annot_info_ref : psg_annot_info.GetId2AnnotInfoList() ) {
        if ( s_GetDebugLevel() >= 8 ) {
            LOG_POST(Info<<"PSG loader: "<<psg_annot_info.GetBlobId().GetId()<<" NA info "
                     <<MSerial_AsnText<<*annot_info_ref);
        }
        const CID2S_Seq_annot_Info& annot_info = *annot_info_ref;
        // create special external annotations blob
        CAnnotName name(annot_info.GetName());
        if ( name.IsNamed() && !ExtractZoomLevel(name.GetName(), 0, 0) ) {
            //setter.GetTSE_LoadLock()->SetName(name);
            names.insert(name.GetName());
            ++main_count;
        }
        else {
            ++zoom_count;
        }
            
        vector<SAnnotTypeSelector> types;
        if ( annot_info.IsSetAlign() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Align));
        }
        if ( annot_info.IsSetGraph() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph));
        }
        if ( annot_info.IsSetFeat() ) {
            for ( auto feat_type_info_iter : annot_info.GetFeat() ) {
                const CID2S_Feat_type_Info& finfo = *feat_type_info_iter;
                int feat_type = finfo.GetType();
                if ( feat_type == 0 ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeq_annot::C_Data::e_Seq_table));
                }
                else if ( !finfo.IsSetSubtypes() ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeqFeatData::E_Choice(feat_type)));
                }
                else {
                    for ( auto feat_subtype : finfo.GetSubtypes() ) {
                        types.push_back(SAnnotTypeSelector
                                        (CSeqFeatData::ESubtype(feat_subtype)));
                    }
                }
            }
        }
            
        CTSE_Chunk_Info::TLocationSet loc;
        CSplitParser::x_ParseLocation(loc, annot_info.GetSeq_loc());
            
        ITERATE ( vector<SAnnotTypeSelector>, it, types ) {
            chunk->x_AddAnnotType(name, *it, loc);
        }
    }
    if ( names.size() == 1 ) {
        ret.second = *names.begin();
    }
    if ( s_GetDebugLevel() >= 5 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<psg_annot_info.GetBlobId().GetId()<<
                 " annots: "<<ret.second<<" "<<main_count<<"+"<<zoom_count);
    }
    if ( !names.empty() ) {
        ret.first = chunk;
    }
    return ret;
}


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNA(
    CDataSource* data_source,
    const TIds& ids,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetAnnotRecordsNAOnce, this,
                              data_source, cref(ids), sel, processed_nas),
                         "GetAnnotRecordsNA");
}


bool CPSGDataLoader_Impl::x_CheckAnnotCache(
    const string& name,
    const TIds& ids,
    CDataSource* data_source,
    CDataLoader::TProcessedNAs* processed_nas,
    CDataLoader::TTSE_LockSet& locks)
{
    auto cached = m_AnnotCache->Find(make_pair(name, ids));
    if (cached) {
        for (auto& info : cached->infos) {
            CDataLoader::SetProcessedNA(name, processed_nas);
            auto chunk_info = s_CreateNAChunk(*info);
            CRef<CPsgBlobId> blob_id(new CPsgBlobId(info->GetBlobId().GetId()));
            blob_id->SetTSEName(name);
            CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(CBlobIdKey(blob_id));
            if ( load_lock ) {
                if ( !load_lock.IsLoaded() ) {
                    load_lock->SetName(cached->name);
                    load_lock->GetSplitInfo().AddChunk(*chunk_info.first);
                    _ASSERT(load_lock->x_NeedsDelayedMainChunk());
                    load_lock.SetLoaded();
                }
                locks.insert(load_lock);
            }
        }
        return true;
    }
    return false;
}


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNAOnce(
    CDataSource* data_source,
    const TIds& ids,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    CDataLoader::TTSE_LockSet locks;
    if ( !data_source  ||  ids.empty() ) {
        return locks;
    }

    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names;
    if ( !kCreateLocalCDDEntries && !x_CheckAnnotCache(kCDDAnnotName, ids, data_source, processed_nas, locks) ) {
        annot_names.push_back(kCDDAnnotName);
    }
    auto snp_scale_limit = CSeq_id::eSNPScaleLimit_Default;
    string snp_name = "SNP"; // name used for caching SNP annots with scale-limit.
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        CPSG_BioIds bio_ids;
        for (auto& id : ids) {
            bio_ids.push_back(CPSG_BioId(id));
        }
        const SAnnotSelector::TNamedAnnotAccessions& accs = sel->GetNamedAnnotAccessions();
        ITERATE(SAnnotSelector::TNamedAnnotAccessions, it, accs) {
            if ( kCreateLocalCDDEntries && NStr::EqualNocase(it->first, kCDDAnnotName) ) {
                // CDDs are added as external annotations
                continue;
            }
            string name = it->first;
            if (name == "SNP") {
                snp_scale_limit = sel->GetSNPScaleLimit();
                if (snp_scale_limit == CSeq_id::eSNPScaleLimit_Default) {
                    snp_scale_limit = CPSGDataLoader::GetSNP_Scale_Limit();
                }
                if (snp_scale_limit != CSeq_id::eSNPScaleLimit_Default) {
                    snp_name = "SNP::" + NStr::NumericToString((int)snp_scale_limit);
                    name = snp_name;
                }
            }
            if ( !x_CheckAnnotCache(name, ids, data_source, processed_nas, locks) ) {
                annot_names.push_back(it->first);
            }
        }

        if ( !annot_names.empty() ) {
            enum ERequestType {
                eRequestNA,
                eRequestGetBlob
            };

            CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
            {{
                auto request = make_shared<CPSG_Request_NamedAnnotInfo>(std::move(bio_ids), annot_names);
                request->SetSNPScaleLimit(snp_scale_limit);
                CRef<CPSGL_NA_Processor> processor
                    (new CPSGL_NA_Processor(ids,
                                            data_source,
                                            m_BlobMap.get(),
                                            m_AddWGSMasterDescr));
                queue.AddRequest(request, processor, eRequestNA);
            }}

            // we need to remember the reason for recursive getblob requests
            typedef vector<string> TRequests;
            map<CRef<CPSGL_GetBlob_Processor>, TRequests> getblob2reqs;
            while ( auto result = queue.GetNextResult() ) {
                if ( result.GetIndex() == eRequestNA ) {
                    auto processor = result.GetProcessor<CPSGL_NA_Processor>();
                    if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
                        _TRACE("Failed to load annotations for "<<ids.front());
                        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                                       "CPSGDataLoader::GetAnnotRecordsNA("<<ids.front()<<") failed"<<
                                       FormatError(result.GetProcessor()));
                    }
                    for ( auto& r : processor->GetResults() ) {
                        if ( r.m_TSE_Lock ) {
                            CDataLoader::SetProcessedNA(r.m_NA, processed_nas);
                            locks.insert(r.m_TSE_Lock);
                        }
                        else {
                            CPSG_BlobId blob_id(r.m_Blob_id);
                            auto request = make_shared<CPSG_Request_Blob>(std::move(blob_id));
                            request->IncludeData(m_TSERequestMode);
                            CRef<CPsgBlobId> dl_blob_id(new CPsgBlobId(r.m_Blob_id));
                            dl_blob_id->SetTSEName(r.m_NA);
                            CRef<CPSGL_GetBlob_Processor> processor2
                                (new CPSGL_GetBlob_Processor(*dl_blob_id,
                                                             data_source,
                                                             m_BlobMap.get(),
                                                             m_AddWGSMasterDescr));
                            getblob2reqs[processor2].push_back(r.m_NA);
                            queue.AddRequest(request, processor2, eRequestGetBlob);
                        }
                    }
                }
                else {
                    auto processor = result.GetProcessor<CPSGL_GetBlob_Processor>();
                    TRequests reqs = std::move(getblob2reqs[processor]);
                    getblob2reqs.erase(processor);
                    if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
                        _TRACE("Failed to load annotations for "<<ids.front());
                        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                                       "CPSGDataLoader::GetAnnotRecordsNA("<<ids.front()<<") failed"<<
                                       FormatError(result.GetProcessor()));
                    }
                    for ( auto& name : reqs ) {
                        CDataLoader::SetProcessedNA(name, processed_nas);
                    }
                    locks.insert(processor->GetTSE_Lock());
                }
            }
        }
    }
    if ( kCreateLocalCDDEntries ) {
        if ( SCDDIds cdd_ids = x_GetCDDIds(ids) ) {
            if ( auto tse_lock = x_CreateLocalCDDEntry(data_source, cdd_ids) ) {
                locks.insert(tse_lock);
            }
        }
    }
    return locks;
}


void CPSGDataLoader_Impl::GetCDDAnnotsOnce(CDataSource* data_source,
                                           const TSeqIdSets& id_sets,
                                           TLoaded& loaded,
                                           TCDD_Locks& ret)
{
    if (id_sets.empty()) return;
    _ASSERT(id_sets.size() == loaded.size());
    _ASSERT(id_sets.size() == ret.size());

    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names{kCDDAnnotName};

    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    vector<SCDDIds> cdd_ids(id_sets.size());
    for (size_t i = 0; i < id_sets.size(); ++i) {
        if ( loaded[i] ) {
            // already loaded
            continue;
        }
        const TIds& ids = id_sets[i];
        if ( ids.empty() ) {
            // no ids -> no CDDs
            continue;
        }
        cdd_ids[i] = x_GetCDDIds(ids);
        if ( !cdd_ids[i] ) {
            // not a protein or no suitable ids
            loaded[i] = true;
            continue;
        }
        // Skip if it's known that the bioseq has no CDDs.
        if (m_CDDInfoCache->Find(x_MakeLocalCDDEntryId(cdd_ids[i]))) {
            // known to have no CDDs
            loaded[i] = true;
            continue;
        }
        // Check if there's a loaded CDD blob.
        CTSE_LoadLock cdd_tse;
        for (auto& id : ids) {
            CDataSource::TLoadedBlob_ids blob_ids;
            data_source->GetLoadedBlob_ids(id, CDataSource::fLoaded_bioseq_annots, blob_ids);
            for (auto& bid : blob_ids) {
                if (x_IsLocalCDDEntryId(dynamic_cast<const CPsgBlobId&>(*bid))) {
                    cdd_tse = data_source->GetTSE_LoadLockIfLoaded(bid);
                    break;
                }
            }
            if (cdd_tse) {
                break;
            }
        }
        if (cdd_tse) {
            // found CDD tse in OM
            if ( !x_IsEmptyCDD(*cdd_tse) ) {
                ret[i] = cdd_tse;
            }
            loaded[i] = true;
            continue;
        }
        // lookup in annot cache
        CDataLoader::TTSE_LockSet locks;
        if ( x_CheckAnnotCache(kCDDAnnotName, ids, data_source, nullptr, locks) ) {
            _ASSERT(locks.size() == 1);
            const CTSE_Lock& tse = *locks.begin();
            // check if delayed TSE chunk is loaded
            if ( tse->x_NeedsDelayedMainChunk() ) {
                // not loaded yet, need to request CDDs
            }
            else {
                // found 
                if ( !x_IsEmptyCDD(*tse) ) {
                    ret[i] = tse;
                }
                loaded[i] = true;
                continue;
            }
        }

        // create and start request
        CPSG_BioIds bio_ids;
        for (auto& id : ids) {
            bio_ids.push_back(CPSG_BioId(id));
        }
        auto request = make_shared<CPSG_Request_NamedAnnotInfo>(std::move(bio_ids), annot_names);
        request->IncludeData(CPSG_Request_Biodata::eWholeTSE);
        CRef<CPSGL_CDDAnnot_Processor> processor
            (new CPSGL_CDDAnnot_Processor(cdd_ids[i],
                                          id_sets[i],
                                          data_source,
                                          m_AnnotCache.get(),
                                          m_CDDInfoCache.get(),
                                          m_BlobMap.get()));
        queue.AddRequest(request, processor, i);
    }

    size_t failed_count = 0;
    CRef<CPSGL_Processor> first_failed_processor;
    while ( auto result = queue.GetNextResult() ) {
        size_t i = result.GetIndex();
        _ASSERT(i < cdd_ids.size() && cdd_ids[i]);
        
        CRef<CPSGL_CDDAnnot_Processor> processor = result.GetProcessor<CPSGL_CDDAnnot_Processor>();
        _ASSERT(processor);
        
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            // execution or processing are failed somehow
            _TRACE("Failed to load CDDs for " << cdd_ids[i]);
            ++failed_count;
            if ( !first_failed_processor ) {
                first_failed_processor = result.GetProcessor();
            }
            continue;
        }
        ret[i] = processor->m_TSE_Lock;
        loaded[i] = true;
    }
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" CDD annots in bulk request"<<
                       FormatError(first_failed_processor));
    }
}


void CPSGDataLoader_Impl::DropTSE(const CPsgBlobId& /*blob_id*/)
{
}


void CPSGDataLoader_Impl::GetBulkIds(const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetBulkIdsOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetBulkIds",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetBulkIdsOnce(const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ITERATE(SPsgBioseqInfo::TIds, it, infos[i]->ids) {
                ret[i].push_back(*it);
            }
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" seq-ids in bulk request");
    }
}


void CPSGDataLoader_Impl::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetAccVersOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetAccVers",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetAccVersOnce(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            CSeq_id_Handle idh = infos[i]->canonical;
            if (idh.IsAccVer()) {
                ret[i] = idh;
            }
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" acc.ver in bulk request");
    }
}


void CPSGDataLoader_Impl::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetGisOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetGis",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetGisOnce(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->gi;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" acc.ver in bulk request");
    }
}


void CPSGDataLoader_Impl::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetLabelsOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetLabels",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetLabelsOnce(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = objects::GetLabel(infos[i]->ids);
            if (!ret[i].empty()) loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" labels in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceLengths(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceLengthsOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetSequenceLengths",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceLengthsOnce(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            auto length = infos[i]->length;
            ret[i] = length > 0? TSeqPos(length): kInvalidSeqPos;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence lengths in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceTypes(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceTypesOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetSequenceTypes",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceTypesOnce(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->molecule_type;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence types in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceStates(CDataSource* data_source, const TIds& ids, TLoaded& loaded, TSequenceStates& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceStatesOnce, this,
                       data_source, cref(ids), ref(loaded), ref(ret)),
                  "GetSequenceStates",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceStatesOnce(CDataSource* data_source, const TIds& ids, TLoaded& loaded, TSequenceStates& ret)
{
    TBioseqAndBlobInfos infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqAndBlobInfo(data_source, ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || (!infos[i].first.get() || !infos[i].second.get())) continue;
            auto bioseq_info = infos[i].first;
            auto blob_info = infos[i].second;
            ret[i] = bioseq_info->GetBioseqStateFlags();
            ret[i] |= blob_info->blob_state_flags;
            if (!(bioseq_info->GetBioseqStateFlags() & CBioseq_Handle::fState_dead) &&
                !(bioseq_info->GetChainStateFlags() & CBioseq_Handle::fState_dead)) {
                ret[i] &= ~CBioseq_Handle::fState_dead;
            }
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence states in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceHashes(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceHashesOnce, this,
                       cref(ids), ref(loaded), ref(ret), ref(known)),
                  "GetSequenceHashes",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceHashesOnce(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->hash;
            known[i] = infos[i]->hash != 0;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence hashes in bulk request");
    }
}


shared_ptr<SPsgBioseqInfo> CPSGDataLoader_Impl::x_GetBioseqInfo(const CSeq_id_Handle& idh)
{
    if ( shared_ptr<SPsgBioseqInfo> ret = m_BioseqCache->Find(idh) ) {
        if ( ret->tax_id <= 0 ) {
            _TRACE("bad tax_id for "<<idh<<" : "<<ret->tax_id);
        }
        return ret;
    }

    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    {{
        CPSG_BioId bio_id(idh);
        shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(std::move(bio_id));
        request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
        CRef<CPSGL_BioseqInfo_Processor> processor
            (new CPSGL_BioseqInfo_Processor(idh, m_BioseqCache.get()));
        queue.AddRequest(request, processor);
    }}

    auto result = queue.GetNextResult();
    CRef<CPSGL_BioseqInfo_Processor> processor = result.GetProcessor<CPSGL_BioseqInfo_Processor>();
    _ASSERT(processor);

    if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
        _TRACE("Failed to get bioseq info for " << idh << " @ "<<CStackTrace());
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed, "failed to get bioseq info for "<<idh);
    }
    if ( !processor->m_BioseqInfo ) {
        _TRACE("No bioseq info for " << idh);
        return nullptr;
    }
    if ( processor->m_BioseqInfoResult ) {
        return std::move(processor->m_BioseqInfoResult);
    }
    else {
        return m_BioseqCache->Add(idh, *processor->m_BioseqInfo);
    }
}


static bool s_IsIpgAccession(const CSeq_id_Handle& idh, string& acc_ver, bool& is_wp_acc)
{
    if (!idh) {
        return false;
    }
    auto seq_id = idh.GetSeqId();
    auto text_id = seq_id->GetTextseq_Id();
    if (!text_id) return false;
    CSeq_id::EAccessionInfo acc_info = idh.IdentifyAccession();
    const int kAccFlags = CSeq_id::fAcc_prot | CSeq_id::fAcc_vdb_only;
    if ((acc_info & kAccFlags) != kAccFlags) {
        return false;
    }
    if ( !text_id->IsSetAccession() || !text_id->IsSetVersion() ) {
        return false;
    }
    acc_ver = text_id->GetAccession()+'.'+NStr::NumericToString(text_id->GetVersion());
    is_wp_acc = (acc_info & (CSeq_id::eAcc_division_mask | CSeq_id::eAcc_flag_mask)) == CSeq_id::eAcc_refseq_unique_prot;
    return true;
}


TTaxId CPSGDataLoader_Impl::x_GetIpgTaxId(const CSeq_id_Handle& idh)
{
    if (!m_IpgTaxIdMap) {
        return INVALID_TAX_ID;
    }

    TTaxId cached = m_IpgTaxIdMap->Find(idh);
    if (cached != INVALID_TAX_ID) {
        return cached;
    }

    string acc_ver;
    bool is_wp_acc = false;
    if (!s_IsIpgAccession(idh, acc_ver, is_wp_acc)) {
        return INVALID_TAX_ID;
    }

    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    {{
        shared_ptr<CPSG_Request_IpgResolve> request = make_shared<CPSG_Request_IpgResolve>(acc_ver);
        CRef<CPSGL_IpgTaxId_Processor> processor(
            new CPSGL_IpgTaxId_Processor(idh, is_wp_acc, m_IpgTaxIdMap.get()));
        queue.AddRequest(request, processor);
    }}

    auto result = queue.GetNextResult();
    if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
        _TRACE("Failed to get IPG tax id for " << idh);
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Failed to get IPG tax id for "+acc_ver);
    }
    auto processor = result.GetProcessor<CPSGL_IpgTaxId_Processor>();
    _ASSERT(processor);
    return processor->m_TaxId;
}


pair<size_t, size_t> CPSGDataLoader_Impl::x_GetIpgTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    if (!m_IpgTaxIdMap) {
        return make_pair(0, 0);
    }

    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
    
    size_t load_count = 0;
    size_t fail_count = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        TTaxId cached = m_IpgTaxIdMap->Find(ids[i]);
        if ( cached != INVALID_TAX_ID ) {
            ret[i] = cached;
            loaded[i] = true;
            ++load_count;
            continue;
        }
        
        string acc_ver;
        bool is_wp_acc = false;
        if ( !s_IsIpgAccession(ids[i], acc_ver, is_wp_acc) ) {
            continue;
        }
        
        shared_ptr<CPSG_Request_IpgResolve> request = make_shared<CPSG_Request_IpgResolve>(acc_ver);
        CRef<CPSGL_IpgTaxId_Processor> processor(
            new CPSGL_IpgTaxId_Processor(ids[i], is_wp_acc, m_IpgTaxIdMap.get()));
        queue.AddRequest(request, processor, i);
    }

    while ( auto result = queue.GetNextResult() ) {
        size_t i = result.GetIndex();
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("Failed to get IPG tax id for " << ids[i]);
            ++fail_count;
            continue;
        }
        auto processor = result.GetProcessor<CPSGL_IpgTaxId_Processor>();
        _ASSERT(processor);
        auto tax_id = processor->m_TaxId;
        if ( tax_id != INVALID_TAX_ID ) {
            ret[i] = tax_id;
            loaded[i] = true;
            ++load_count;
        }
    }
    return make_pair(load_count, fail_count);
}


BEGIN_LOCAL_NAMESPACE;

enum {
    kProcessorIndex_Info,
    kProcessorIndex_BlobInfo,
    kProcessorIndex_count = 4
};

END_LOCAL_NAMESPACE;

        
CPSGDataLoader_Impl::TBioseqAndBlobInfo
CPSGDataLoader_Impl::x_GetBioseqAndBlobInfo(CDataSource* data_source,
                                            const CSeq_id_Handle& idh)
{
    TBioseqAndBlobInfo ret;
    
    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
    
    x_CreateBioseqAndBlobInfoRequests(queue, idh, ret);
    
    while ( auto result = queue.GetNextResult() ) {
        if ( !x_ProcessBioseqAndBlobInfoResult(queue, idh, ret, result) ) {
            NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                           "failed to get bioseq/blob info for "<<idh);
        }
    }
    
    if ( !x_FinalizeBioseqAndBlobInfo(ret) ) {
        if ( !ret.first || !ret.first->HasBlobId() ) {
            NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                           "failed to get bioseq/blob info for "<<idh);
        }
        ret.second = x_GetBlobInfo(data_source, ret.first->GetPSGBlobId());
    }
    
    return ret;
}


void CPSGDataLoader_Impl::x_CreateBioseqAndBlobInfoRequests(CPSGL_QueueGuard& queue,
                                                            const CSeq_id_Handle& idh,
                                                            TBioseqAndBlobInfo& ret,
                                                            size_t id_index)
{
    if ( CannotProcess(idh) ) {
        return;
    }
    shared_ptr<SPsgBioseqInfo> bioseq_info = m_BioseqCache->Find(idh);
    shared_ptr<SPsgBlobInfo> blob_info;
    if ( bioseq_info && bioseq_info->KnowsBlobId() ) {
        // blob id is known
        ret.first = bioseq_info;
        if ( !bioseq_info->HasBlobId() ) {
            // no blob - no need to ask again
            return;
        }
        blob_info = m_BlobMap->Find(bioseq_info->GetPSGBlobId());
        if ( blob_info ) {
            // blob info is cached - no need to ask again
            ret.second = blob_info;
            return;
        }
        // ask by blob_id
        CPSG_BlobId bid(bioseq_info->GetPSGBlobId());
        shared_ptr<CPSG_Request_Blob> request = make_shared<CPSG_Request_Blob>(std::move(bid));
        request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        CRef<CPSGL_BlobInfo_Processor> processor
            (new CPSGL_BlobInfo_Processor(idh, bioseq_info->GetPSGBlobId(), m_BlobMap.get()));
        queue.AddRequest(request, processor,
                         id_index*kProcessorIndex_count + kProcessorIndex_BlobInfo);
    }
    else {
        // ask for both bioseq info and blob info
        CPSG_BioId bio_id(idh);
        auto blob_request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        blob_request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        CRef<CPSGL_Info_Processor> blob_processor
            (new CPSGL_Info_Processor(idh, m_BioseqCache.get(), m_BlobMap.get()));
        queue.AddRequest(blob_request, blob_processor,
                         id_index*kProcessorIndex_count + kProcessorIndex_Info);
    }
}


bool CPSGDataLoader_Impl::x_ProcessBioseqAndBlobInfoResult(CPSGL_QueueGuard& queue,
                                                           const CSeq_id_Handle& idh,
                                                           TBioseqAndBlobInfo& ret,
                                                           const CPSGL_ResultGuard& result)
{
    auto index = result.GetIndex() % kProcessorIndex_count;
    if ( index == kProcessorIndex_Info ) {
        auto processor = result.GetProcessor<CPSGL_Info_Processor>();
        _ASSERT(processor);
        
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("Failed to get bioseq info for " << processor->GetSeq_id());
            return false;
        }
        if ( processor->m_BioseqInfoStatus == EPSG_Status::eError ) {
            // inconsistent reply
            _TRACE("Failed to load bioseq info for " << processor->GetSeq_id());
            return false;
        }
        ret.first = processor->m_BioseqInfoResult;
        if ( !ret.first ) {
            // not bioseq info
            _TRACE("No bioseq info for " << processor->GetSeq_id());
            return true;
        }
        if ( processor->m_BlobInfoStatus == EPSG_Status::eError ) {
            // inconsistent reply
            _TRACE("Failed to load blob info for " << processor->GetSeq_id());
            return false;
        }
        ret.second = processor->m_BlobInfoResult;
        if ( !ret.second ) {
            // re-try with getblob request
            CPSG_BlobId bid(ret.first->GetPSGBlobId());
            shared_ptr<CPSG_Request_Blob> request = make_shared<CPSG_Request_Blob>(std::move(bid));
            request->IncludeData(CPSG_Request_Biodata::eNoTSE);
            CRef<CPSGL_BlobInfo_Processor> processor2
                (new CPSGL_BlobInfo_Processor(processor->GetSeq_id(), ret.first->GetPSGBlobId(), m_BlobMap.get()));
            size_t id_index = result.GetIndex()/kProcessorIndex_count;
            queue.AddRequest(request, processor2,
                             id_index*kProcessorIndex_count + kProcessorIndex_BlobInfo);
            return true;
        }
        return true;
    }
    else {
        _ASSERT(index == kProcessorIndex_BlobInfo);
        auto processor = result.GetProcessor<CPSGL_BlobInfo_Processor>();
        _ASSERT(processor);
                
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("Failed to get blob info for " << processor->GetSeq_id());
            return false;
        }
        if ( processor->m_BlobInfoStatus == EPSG_Status::eError ) {
            // inconsistent reply
            _TRACE("Failed to load blob info for " << processor->GetSeq_id());
            return false;
        }
        ret.second = processor->m_BlobInfoResult;
        if ( !ret.second ) {
            _TRACE("No blob info for " << processor->GetSeq_id());
            return true;
        }
        x_AdjustBlobState(*ret.second, processor->GetSeq_id()); // TODO: in processor?
        return true;
    }
}


bool CPSGDataLoader_Impl::x_FinalizeBioseqAndBlobInfo(TBioseqAndBlobInfo& ret)
{
    if ( ret.first && ret.first->HasBlobId() && !ret.second ) {
        ret.second = m_BlobMap->Find(ret.first->GetPSGBlobId());
        if ( !ret.second ) {
            return false;
        }
    }
    return true;
}


shared_ptr<SPsgBlobInfo> CPSGDataLoader_Impl::x_GetBlobInfo(CDataSource* data_source,
                                                            const string& blob_id)
{
    // lookup cached blob info
    if ( shared_ptr<SPsgBlobInfo> ret = m_BlobMap->Find(blob_id) ) {
        return ret;
    }
    // use blob info from already loaded TSE
    if ( data_source ) {
        CDataLoader::TBlobId dl_blob_id(new CPsgBlobId(blob_id));
        auto load_lock = data_source->GetTSE_LoadLockIfLoaded(dl_blob_id);
        if ( load_lock && load_lock.IsLoaded() ) {
            auto blob_info = make_shared<SPsgBlobInfo>(*load_lock);
            m_BlobMap->Add(blob_id, blob_info);
            return blob_info;
        }
    }

    // load blob info from PSG
    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    {{
        CPSG_BlobId bid(blob_id);
        shared_ptr<CPSG_Request_Blob> request = make_shared<CPSG_Request_Blob>(std::move(bid));
        request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        CRef<CPSGL_BlobInfo_Processor> processor
            (new CPSGL_BlobInfo_Processor(blob_id, m_BlobMap.get()));
        queue.AddRequest(request, processor);
    }}

    auto result = queue.GetNextResult();
    CRef<CPSGL_BlobInfo_Processor> processor = result.GetProcessor<CPSGL_BlobInfo_Processor>();
    _ASSERT(processor);

    if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
        _TRACE("Failed to get blob info for " << blob_id << " @ "<<CStackTrace());
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed, "failed to get bioseq info for "<<blob_id);
    }
    if ( processor->m_BlobInfoStatus == EPSG_Status::eError ) {
        // inconsistent reply
        _TRACE("Failed to load blob info for " << blob_id);
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed, "failed to get blob info for "<<blob_id);
    }
    if ( !processor->m_BlobInfo ) {
        _TRACE("No blob info for " << blob_id);
        return nullptr;
    }
    auto blob_info = make_shared<SPsgBlobInfo>(*processor->m_BlobInfo);
    m_BlobMap->Add(blob_id, blob_info);
    return blob_info;
}


void CPSGDataLoader_Impl::x_AdjustBlobState(SPsgBlobInfo& blob_info, const CSeq_id_Handle idh)
{
    if (!idh) return;
    if (!(blob_info.blob_state_flags & CBioseq_Handle::fState_dead)) return;
    auto seq_info = m_BioseqCache->Find(idh);
    if (!seq_info) return;
    auto seq_state = seq_info->GetBioseqStateFlags();
    auto chain_state = seq_info->GetChainStateFlags();
    if (seq_state == CBioseq_Handle::fState_none &&
        chain_state == CBioseq_Handle::fState_none) {
        blob_info.blob_state_flags &= ~CBioseq_Handle::fState_dead;
    }
}


pair<size_t, size_t> CPSGDataLoader_Impl::x_GetBulkBioseqInfo(
    const TIds& ids,
    const TLoaded& loaded,
    TBioseqInfos& ret)
{
    pair<size_t, size_t> counts(0, 0);

    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);
    
    for (size_t i = 0; i < ids.size(); ++i) {
        const CSeq_id_Handle& id = ids[i];
        if (loaded[i]) continue;
        if ( CannotProcess(ids[i]) ) {
            continue;
        }
        ret[i] = m_BioseqCache->Find(ids[i]);
        if (ret[i]) {
            counts.first += 1;
            continue;
        }
        CPSG_BioId bio_id(ids[i]);
        shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(std::move(bio_id));
        request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
        CRef<CPSGL_BioseqInfo_Processor> processor
            (new CPSGL_BioseqInfo_Processor(id, m_BioseqCache.get()));
        queue.AddRequest(request, processor, i);
    }

    while ( auto result = queue.GetNextResult() ) {
        size_t i = result.GetIndex();
        CRef<CPSGL_BioseqInfo_Processor> processor = result.GetProcessor<CPSGL_BioseqInfo_Processor>();
        _ASSERT(processor);
        const CSeq_id_Handle& id = ids[i];
        
        if ( result.GetStatus() != CThreadPool_Task::eCompleted ) {
            // execution or processing are failed somehow
            _TRACE("Failed to load bioseq info for " << id);
            counts.second += 1;
            continue;
        }
        if ( processor->m_BioseqInfoStatus == EPSG_Status::eError ) {
            // inconsistent reply
            _TRACE("Failed to load bioseq info for " << id);
            counts.second += 1;
            continue;
        }
        if ( !processor->m_BioseqInfo ) {
            // no bioseq info
            _TRACE("No bioseq info for " << id);
            counts.first += 1;
            continue;
        }
        // all good
        _ASSERT(processor->m_BioseqInfo);
        ret[i] = m_BioseqCache->Add(id, *processor->m_BioseqInfo);
        counts.first += 1;
    }
    return counts;
}


pair<size_t, size_t> CPSGDataLoader_Impl::x_GetBulkBioseqAndBlobInfo(
    CDataSource* data_source,
    const TIds& ids,
    const TLoaded& loaded,
    TBioseqAndBlobInfos& ret)
{
    pair<size_t, size_t> counts(0, 0);
    vector<bool> errors(ids.size());
    
    CPSGL_QueueGuard queue(*m_ThreadPool, *m_Queue);

    for (size_t i = 0; i < ids.size(); ++i) {
        if ( loaded[i] ) {
            continue;
        }
        x_CreateBioseqAndBlobInfoRequests(queue, ids[i], ret[i], i);
    }

    while ( auto result = queue.GetNextResult() ) {
        size_t i = result.GetIndex()/kProcessorIndex_count;
        _ASSERT(i < ids.size());
        if ( !x_ProcessBioseqAndBlobInfoResult(queue, ids[i], ret[i], result) ) {
            errors[i] = true;
        }
    }
    
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        if ( errors[i] ) {
            counts.second += 1;
            continue;
        }
        if ( !x_FinalizeBioseqAndBlobInfo(ret[i]) ) {
            counts.second += 1;
            continue;
        }
        counts.first += 1;
    }
    return counts;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
