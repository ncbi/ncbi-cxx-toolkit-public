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
* ===========================================================================
*
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Data reader from Pubseq_OS
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_config_value.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/seqref_pubseq.hpp>
#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <corelib/ncbicntr.hpp>
#include <corelib/plugin_manager_impl.hpp>

#include <serial/objostrasn.hpp>
#include <serial/iterator.hpp>

#include <util/compress/reader_zlib.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
static CAtomicCounter s_pubseq_readers;
#endif

#ifdef _DEBUG
static int GetDebugLevel(void)
{
    static int var = GetConfigInt("GENBANK", "PUBSEQOS_DEBUG");
    return var;
}
#else
# define GetDebugLevel() (0)
#endif


CPubseqReader::CPubseqReader(TConn noConn,
                             const string& server,
                             const string& user,
                             const string& pswd)
    : m_Server(server) , m_User(user), m_Password(pswd), m_Context(0),
      m_NoMoreConnections(false)
{
#if defined(NCBI_NO_THREADS) || !defined(HAVE_SYBASE_REENTRANT)
    noConn=1;
#else
    //noConn=1; // limit number of simultaneous connections to one
#endif
#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif
    try {
        SetParallelLevel(noConn);
    }
    catch ( ... ) {
        // close all connections before exiting
        SetParallelLevel(0);
        throw;
    }
}


CPubseqReader::~CPubseqReader()
{
    SetParallelLevel(0);

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    s_pubseq_readers.Add(-1);
#endif
}


CReader::TConn CPubseqReader::GetParallelLevel(void) const
{
    return m_Pool.size();
}


void CPubseqReader::SetParallelLevel(TConn size)
{
    size_t oldSize = m_Pool.size();
    for(size_t i = size; i < oldSize; ++i) {
        delete m_Pool[i];
        m_Pool[i] = 0;
    }

    m_Pool.resize(size);

    for(size_t i = oldSize; i < min(1u, size); ++i) {
        m_Pool[i] = x_NewConnection();
    }
}


CDB_Connection* CPubseqReader::x_GetConnection(TConn conn)
{
    _ASSERT(conn < m_Pool.size());
    CDB_Connection* ret = m_Pool[conn];
    if ( !ret ) {
        ret = x_NewConnection();

        if ( !ret ) {
            NCBI_THROW(CLoaderException, eNoConnection,
                       "too many connections failed: probably server is dead");
        }

        m_Pool[conn] = ret;
    }
    return ret;
}


void CPubseqReader::Reconnect(TConn conn)
{
    _ASSERT(conn < m_Pool.size());
    delete m_Pool[conn];
    m_Pool[conn] = 0;
}


CDB_Connection *CPubseqReader::x_NewConnection(void)
{
    for ( int i = 0; !m_NoMoreConnections && i < 3; ++i ) {
        if ( !m_Context ) {
            C_DriverMgr drvMgr;
            map<string, string> args;
            args["packet"] = "3584"; // 7*512
            //DBAPI_RegisterDriver_CTLIB(drvMgr);
            //DBAPI_RegisterDriver_DBLIB(drvMgr);
            string errmsg;
            m_Context = drvMgr.GetDriverContext("ctlib", &errmsg, &args);
            if ( !m_Context ) {
                LOG_POST(errmsg);
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
                m_NoMoreConnections = true;
                NCBI_THROW(CLoaderException, eNoConnection,
                           "Cannot create dbapi context");
#else
                m_Context = drvMgr.GetDriverContext("dblib", &errmsg, &args);
                if ( !m_Context ) {
                    LOG_POST(errmsg);
                    m_NoMoreConnections = true;
                    NCBI_THROW(CLoaderException, eNoConnection,
                               "Cannot create dbapi context");
                }
#endif
            }
        }
        try {
            auto_ptr<CDB_Connection> conn(m_Context->Connect(m_Server,
                                                             m_User,
                                                             m_Password,
                                                             0));
            if ( conn.get() ) {
                auto_ptr<CDB_LangCmd> cmd(conn->LangCmd("set blob_stream on"));
                if ( cmd.get() ) {
                    cmd->Send();
                }
            }
            return conn.release();
        }
        catch ( CException& exc ) {
            ERR_POST("CPubseqReader::x_NewConnection: "
                     "cannot connect: " << exc.what());
        }
    }
    m_NoMoreConnections = true;
    return 0;
}


void CPubseqReader::ResolveSeq_id(CReaderRequestResult& /*result*/,
                                  CLoadLockBlob_ids& ids,
                                  const CSeq_id& id,
                                  TConn conn)
{
    _TRACE("ResolveSeq_id: " << id.AsFastaString());
    // note: this was
    //CDB_VarChar asnIn(static_cast<string>(CNcbiOstrstreamToString(oss)));
    // but MSVC doesn't like this.  This is the only version that
    // will compile:
    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        {{
            CObjectOStreamAsn ooss(oss);
            ooss << id;
        }}
        asnIn = CNcbiOstrstreamToString(oss);
    }}

    CDB_Connection* db_conn = x_GetConnection(conn);
    auto_ptr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_by_seqid_asn", 1));
    cmd->SetParam("@asnin", &asnIn);
    cmd->Send();

    while(cmd->HasMoreResults()) {
        auto_ptr<CDB_Result> result(cmd->Result());
        if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
            continue;
            
        while(result->Fetch()) {
            CDB_Int giGot;
            CDB_Int satGot;
            CDB_Int satKeyGot;
            CDB_Int extFeatGot;
            
            _TRACE("next fetch: " << result->NofItems() << " items");
            for ( unsigned pos = 0; pos < result->NofItems(); ++pos ) {
                const string& name = result->ItemName(pos);
                _TRACE("next item: " << name);
                if (name == "gi") {
                    result->GetItem(&giGot);
                    _TRACE("gi: "<<giGot.Value());
                }
                else if (name == "sat" ) {
                    result->GetItem(&satGot);
                    _TRACE("sat: "<<satGot.Value());
                }
                else if(name == "sat_key") {
                    result->GetItem(&satKeyGot);
                    _TRACE("sat_key: "<<satKeyGot.Value());
                }
                else if(name == "extra_feat" || name == "ext_feat") {
                    result->GetItem(&extFeatGot);
#ifdef _DEBUG
                    if ( extFeatGot.IsNULL() ) {
                        _TRACE("ext_feat = NULL");
                    }
                    else {
                        _TRACE("ext_feat = "<<extFeatGot.Value());
                    }
#endif
                }
                else {
                    result->SkipItem();
                }
            }

            _ASSERT(satGot.Value() != eSat_SNP);
            int gi = giGot.Value();
            int sat = satGot.Value();
            int sat_key = satKeyGot.Value();

            if ( GetDebugLevel() >= 5 ) {
                NcbiCout << "CPubseqReader::ResolveSeq_id"
                    "(" << asnIn.Value() << ")"
                    " gi=" << gi <<
                    " sat=" << sat <<
                    " satkey=" << sat_key <<
                    " extfeat=";
                if ( extFeatGot.IsNULL() ) {
                    NcbiCout << "NULL";
                }
                else {
                    NcbiCout << extFeatGot.Value();
                }
                NcbiCout << NcbiEndl;
            }

            if ( TrySNPSplit() && sat != eSat_ANNOT ) {
                {{
                    // main blob
                    CBlob_id blob_id;
                    blob_id.SetSat(sat);
                    blob_id.SetSatKey(sat_key);
                    ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                }}
                if ( !extFeatGot.IsNULL() ) {
                    int ext_feat = extFeatGot.Value();
                    while ( ext_feat ) {
                        int bit = ext_feat & ~(ext_feat-1);
                        ext_feat -= bit;
#ifdef GENBANK_USE_SNP_SATELLITE_15
                        if ( bit == eSubSat_SNP ) {
                            AddSNPBlob_id(ids, gi);
                            continue;
                        }
#endif
                        CBlob_id blob_id;
                        blob_id.SetSat(eSat_ANNOT);
                        blob_id.SetSatKey(gi);
                        blob_id.SetSubSat(bit);
                        ids.AddBlob_id(blob_id, fBlobHasExtAnnot);
                    }
                }
            }
            else {
                // whole blob
                CBlob_id blob_id;
                blob_id.SetSat(sat);
                blob_id.SetSatKey(sat_key);
                if ( !extFeatGot.IsNULL() ) {
                    blob_id.SetSubSat(extFeatGot.Value());
                }
                ids.AddBlob_id(blob_id, fBlobHasAllLocal);
            }
        }
    }
}


void CPubseqReader::ResolveSeq_id(CReaderRequestResult& /*result*/,
                                  CLoadLockSeq_ids& ids,
                                  const CSeq_id& id,
                                  TConn conn)
{
    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        {{
            CObjectOStreamAsn ooss(oss);
            ooss << id;
        }}
        asnIn = CNcbiOstrstreamToString(oss);
    }}

    CDB_Connection* db_conn = x_GetConnection(conn);

    CDB_Int gi;
    if ( id.IsGi() ) {
        gi = id.GetGi();
    }
    else {
        // Get gi by seq-id
        _TRACE("ResolveSeq_id to gi: " << id.AsFastaString());
        auto_ptr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_by_seqid_asn", 1));
        cmd->SetParam("@asnin", &asnIn);
        cmd->Send();

        while(cmd->HasMoreResults()) {
            auto_ptr<CDB_Result> result(cmd->Result());
            if (result.get() == 0  ||  result->ResultType() != eDB_RowResult) {
                continue;
            }

            while(result->Fetch()) {
                _TRACE("next fetch: " << result->NofItems() << " items");
                for ( unsigned pos = 0; pos < result->NofItems(); ++pos ) {
                    const string& name = result->ItemName(pos);
                    _TRACE("next item: " << name);
                    if (name == "gi") {
                        result->GetItem(&gi);
                    }
                    else {
                        result->SkipItem();
                    }
                }
            }
        }
    }

    if ( gi.Value() != 0 ) {
        _TRACE("ResolveGi to Seq-ids: " << gi.Value());
        auto_ptr<CDB_RPCCmd> cmd(db_conn->RPC("id_seqid4gi", 2));
        CDB_TinyInt bin = 1;
        cmd->SetParam("@gi", &gi);
        cmd->SetParam("@bin", &bin);
        cmd->Send();

        while(cmd->HasMoreResults()) {
            auto_ptr<CDB_Result> result(cmd->Result());
            if (result.get() == 0  ||  result->ResultType() != eDB_RowResult) {
                continue;
            }

            while(result->Fetch()) {
                _TRACE("next fetch: " << result->NofItems() << " items");
                for ( unsigned pos = 0; pos < result->NofItems(); ++pos ) {
                    const string& name = result->ItemName(pos);
                    _TRACE("next item: " << name);
                    if ( name == "seqid" ) {
                        CResultBtSrcRdr reader(result.get());
                        CObjectIStreamAsnBinary in;
                        in.Open(reader);
                        CSeq_id id;
                        while (true) {
                            try {
                                in >> id;
                                ids.AddSeq_id(id);
                            }
                            catch (...) {
                                break;
                            }
                        }
                    }
                    else {
                        result->SkipItem();
                    }
                }
            }
        }
    }
}


CReader::TBlobVersion
CPubseqReader::GetVersion(const CBlob_id& blob_id, TConn conn)
{
    return 0;
}


void CPubseqReader::GetTSEBlob(CTSE_Info& tse_info,
                               const CBlob_id& blob_id,
                               TConn conn)
{
    CDB_Connection* db_conn = x_GetConnection(conn);
    auto_ptr<CDB_RPCCmd> cmd(x_SendRequest(blob_id, db_conn));
    auto_ptr<CDB_Result> result(x_ReceiveData(&tse_info, *cmd));
    if ( result.get() ) {
        x_ReceiveMainBlob(tse_info, blob_id, *result);
    }
}


CRef<CSeq_annot_SNP_Info> CPubseqReader::GetSNPAnnot(const CBlob_id& blob_id,
                                                     TConn conn)
{
    CRef<CSeq_annot_SNP_Info> ret;
    CDB_Connection* db_conn = x_GetConnection(conn);
    auto_ptr<CDB_RPCCmd> cmd(x_SendRequest(blob_id, db_conn));
    auto_ptr<CDB_Result> result(x_ReceiveData(0, *cmd));
    if ( result.get() ) {
        ret = x_ReceiveSNPAnnot(*result);
    }
    return ret;
}


CDB_RPCCmd* CPubseqReader::x_SendRequest(const CBlob_id& blob_id,
                                         CDB_Connection* db_conn)
{
    auto_ptr<CDB_RPCCmd> cmd(db_conn->RPC("id_get_asn", 4));
    CDB_SmallInt satIn(blob_id.GetSat());
    CDB_Int satKeyIn(blob_id.GetSatKey());
    CDB_Int ext_feat(blob_id.GetSubSat());

    _TRACE("x_SendRequest: "<<blob_id.ToString());

    //CDB_Int giIn(seqref.GetGi());
    //cmd->SetParam("@gi", &giIn);
    cmd->SetParam("@sat_key", &satKeyIn);
    cmd->SetParam("@sat", &satIn);
    cmd->SetParam("@ext_feat", &ext_feat);
    cmd->Send();
    return cmd.release();
}


CDB_Result* CPubseqReader::x_ReceiveData(CTSE_Info* tse_info, CDB_RPCCmd& cmd)
{
    enum {
        kState_dead = 125
    };

    // new row
    while( cmd.HasMoreResults() ) {
        _TRACE("next result");
        if ( cmd.HasFailed() ) {
            break;
        }
        
        auto_ptr<CDB_Result> result(cmd.Result());
        if ( !result.get() || result->ResultType() != eDB_RowResult ) {
            continue;
        }
        
        while ( result->Fetch() ) {
            _TRACE("next fetch: " << result->NofItems() << " items");
            for ( unsigned pos = 0; pos < result->NofItems(); ++pos ) {
                const string& name = result->ItemName(pos);
                _TRACE("next item: " << name);
                if ( name == "confidential" ) {
                    CDB_Int v;
                    result->GetItem(&v);
                    _TRACE("confidential: "<<v.Value());
                    if ( v.Value() ) {
                        tse_info->SetBlobState(
                            CBioseq_Handle::fState_confidential);
                    }
                }
                else if ( name == "suppress" ) {
                    CDB_Int v;
                    result->GetItem(&v);
                    _TRACE("suppress: "<<v.Value());
                    if ( v.Value() ) {
                        tse_info->SetBlobState(
                            (v.Value() & 4)
                            ? CBioseq_Handle::fState_suppress_temp
                            : CBioseq_Handle::fState_suppress_perm);
                    }
                }
                else if ( name == "override" ) {
                    CDB_Int v;
                    result->GetItem(&v);
                    _TRACE("withdrawn: "<<v.Value());
                    if ( v.Value() ) {
                        tse_info->SetBlobState(
                            CBioseq_Handle::fState_withdrawn);
                    }
                }
                else if ( name == "last_touched_m" ) {
                    CDB_Int v;
                    result->GetItem(&v);
                    _TRACE("version: " << v.Value());
                    tse_info->SetBlobVersion(v.Value());
                }
                else if ( name == "state" ) {
                    CDB_Int v;
                    result->GetItem(&v);
                    _TRACE("state: "<<v.Value());
                    if ( v.Value() == kState_dead ) {
                        tse_info->SetBlobState(CBioseq_Handle::fState_dead);
                    }
                }
                else if ( name == "asn1" ) {
                    return result.release();
                }
                else {
#ifdef _DEBUG
                    AutoPtr<CDB_Object> item(result->GetItem(0));
                    _TRACE("item type: " << item->GetType());
                    switch ( item->GetType() ) {
                    case eDB_Int:
                    case eDB_SmallInt:
                    case eDB_TinyInt:
                    {
                        CDB_Int v;
                        v.AssignValue(*item);
                        _TRACE("item value: " << v.Value());
                        break;
                    }
                    case eDB_VarChar:
                    {
                        CDB_VarChar v;
                        v.AssignValue(*item);
                        _TRACE("item value: " << v.Value());
                        break;
                    }
                    default:
                        break;
                    }
#else
                    result->SkipItem();
#endif
                }
            }
        }
    }
    // no data
    tse_info->SetBlobState(CBioseq_Handle::fState_no_data);
    return 0;
}


void CPubseqReader::x_ReceiveMainBlob(CTSE_Info& tse_info,
                                      const CBlob_id& blob_id,
                                      CDB_Result& result)
{
    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
    CRef<CSeq_entry> seq_entry(new CSeq_entry);

    CResultBtSrcRdr reader(&result);
    CObjectIStreamAsnBinary in(reader);
    
#ifdef GENBANK_USE_SNP_SATELLITE_15
    CReader::SetSeqEntryReadHooks(in);
    in >> *seq_entry;
#else
    if ( blob_id.GetSubSat() == eSubSat_SNP ) {
        CSeq_annot_SNP_Info_Reader::Parse(in, Begin(*seq_entry), snps);
    }
    else {
        CReader::SetSeqEntryReadHooks(in);
        in >> *seq_entry;
    }
#endif

    if ( GetDebugLevel() >= 8 ) {
        NcbiCout << MSerial_AsnText << *seq_entry;
    }

    tse_info.SetSeq_entry(*seq_entry, snps);
}


CRef<CSeq_annot_SNP_Info> CPubseqReader::x_ReceiveSNPAnnot(CDB_Result& result)
{
    CRef<CSeq_annot_SNP_Info> snp_annot_info;
    
    {{
        CResultBtSrcRdr src(&result);
        CNlmZipBtRdr src2(&src);
        
        CObjectIStreamAsnBinary in(src2);
        
        snp_annot_info = CSeq_annot_SNP_Info_Reader::ParseAnnot(in);
    }}
    
    return snp_annot_info;
}


CResultBtSrcRdr::CResultBtSrcRdr(CDB_Result* result)
    : m_Result(result)
{
}


CResultBtSrcRdr::~CResultBtSrcRdr()
{
}


size_t CResultBtSrcRdr::Read(char* buffer, size_t bufferLength)
{
    size_t ret;
    while ( (ret = m_Result->ReadItem(buffer, bufferLength)) == 0 ) {
        if ( !m_Result->Fetch() )
            break;
    }
    return ret;
}


END_SCOPE(objects)

void GenBankReaders_Register_Pubseq(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_ReaderPubseqos);
}


const string kPubseqReaderDriverName("pubseqos");


/// Class factory for Pubseq reader
///
/// @internal
///
class CPubseqReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader, objects::CPubseqReader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader, 
                                    objects::CPubseqReader> TParent;
public:

    CPubseqReaderCF() : TParent(kPubseqReaderDriverName, 0)
    {
    }

    ~CPubseqReaderCF()
    {
    }
};


void NCBI_EntryPoint_ReaderPubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CPubseqReaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}


END_NCBI_SCOPE
