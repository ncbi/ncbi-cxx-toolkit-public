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

#include <objmgr/reader_pubseq.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/seqref_pubseq.hpp>
#include <objmgr/impl/reader_zlib.hpp>
#include <objmgr/impl/reader_snp.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <corelib/ncbicntr.hpp>

#include <serial/objostrasn.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
static CAtomicCounter s_pubseq_readers;
#endif

CPubseqReader::CPubseqReader(TConn noConn,
                             const string& server,
                             const string& user,
                             const string& pswd)
    : m_Server(server) , m_User(user), m_Password(pswd), m_Context(NULL),
      m_NoMoreConnections(false)
{
    noConn=1; // limit number of simultaneous connections to one
#if !defined(HAVE_SYBASE_REENTRANT)
    noConn=1;
#endif
#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        THROW1_TRACE(runtime_error,
                     "CPubseqReader: "
                     "Attempt to open multiple pubseq_readers without MT-safe DB library");
    }
#endif
    try {
        SetParallelLevel(noConn);
    }
    catch ( ... ) {
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
    conn = conn % m_Pool.size();
    CDB_Connection* ret = m_Pool[conn];
    if ( !ret ) {
        ret = x_NewConnection();

        if ( !ret ) {
            NCBI_THROW(CLoaderException, eConnectionFailed,
                       "too many connections failed: probably server is dead");
        }

        m_Pool[conn] = ret;
    }
    return ret;
}


void CPubseqReader::Reconnect(TConn conn)
{
    LOG_POST("Reconnect");
    conn = conn % m_Pool.size();
    delete m_Pool[conn];
    m_Pool[conn] = 0;
}


CDB_Connection *CPubseqReader::x_NewConnection(void)
{
    for ( int i = 0; !m_NoMoreConnections && i < 3; ++i ) {
        if ( m_Context.get() == NULL ) {
            C_DriverMgr drvMgr;
            //DBAPI_RegisterDriver_CTLIB(drvMgr);
            //DBAPI_RegisterDriver_DBLIB(drvMgr);
            string errmsg;
            FDBAPI_CreateContext createContextFunc =
                drvMgr.GetDriver("ctlib",&errmsg);
            if ( !createContextFunc ) {
                LOG_POST(errmsg);
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
                m_NoMoreConnections = true;
                NCBI_THROW(CLoaderException, eConnectionFailed,
                           "Neither ctlib nor dblib are available");
#else
                createContextFunc = drvMgr.GetDriver("dblib",&errmsg);
                if ( !createContextFunc ) {
                    LOG_POST(errmsg);
                    m_NoMoreConnections = true;
                    NCBI_THROW(CLoaderException, eConnectionFailed,
                               "Neither ctlib nor dblib are available");
                }
#endif
            }
            map<string,string> args;
            args["packet"]="3584"; // 7*512
            m_Context.reset((*createContextFunc)(&args));
            //m_Context.reset((*createContextFunc)(0));
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


void CPubseqReader::RetrieveSeqrefs(TSeqrefs& sr,
                                    const CSeq_id& seqId,
                                    TConn conn)
{
    x_RetrieveSeqrefs(sr, seqId, x_GetConnection(conn));
}


void CPubseqReader::x_RetrieveSeqrefs(TSeqrefs& srs,
                                      const CSeq_id& seqId,
                                      CDB_Connection* conn)
{
    int gi;
    if ( seqId.IsGi() ) {
        gi = seqId.GetGi();
    }
    else {
        gi = x_ResolveSeq_id_to_gi(seqId, conn);
    }

    if ( gi ) {
        x_RetrieveSeqrefs(srs, gi, conn);
    }
}


void CPubseqReader::x_RetrieveSeqrefs(TSeqrefs& srs,
                                      int gi,
                                      CDB_Connection* conn)
{
    auto_ptr<CDB_RPCCmd> cmd(conn->RPC("id_gi_class", 1));
    {{
        CDB_Int giIn(gi);
        cmd->SetParam("@gi", &giIn);
        cmd->Send();
    }}

    while(cmd->HasMoreResults()) {
        auto_ptr<CDB_Result> result(cmd->Result());
        if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
            continue;
            
        while(result->Fetch()) {
            CDB_Int sat;
            CDB_Int satKey;
            CDB_Int extFeat;
            
            _TRACE("next fetch: " << result->NofItems() << " items");
            for ( unsigned pos = 0; pos < result->NofItems(); ++pos ) {
                const string& name = result->ItemName(pos);
                _TRACE("next item: " << name);
                if (name == "sat" ) {
                    result->GetItem(&sat);
                }
                else if(name == "sat_key") {
                    result->GetItem(&satKey);
                }
                else if(name == "ext_feat") {
                    result->GetItem(&extFeat);
                }
                else {
                    result->SkipItem();
                }
            }

            _ASSERT(sat.Value() != kSNP_Sat);
            srs.push_back(Ref(new CSeqref(gi, sat.Value(), satKey.Value())));
            if ( TrySNPSplit() ) {
                if ( extFeat.IsNULL() ) {
                    AddSNPSeqref(srs, gi, CSeqref::fPossible);
                }
                else if ( extFeat.Value() != 0 ) {
                    AddSNPSeqref(srs, gi);
                }
            }
        }
    }
}


int CPubseqReader::x_ResolveSeq_id_to_gi(const CSeq_id& seqId,
                                         CDB_Connection* conn)
{
    // note: this was
    //CDB_VarChar asnIn(static_cast<string>(CNcbiOstrstreamToString(oss)));
    // but MSVC doesn't like this.  This is the only version that
    // will compile:
    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        {{
            CObjectOStreamAsn ooss(oss);
            ooss << seqId;
        }}
        asnIn = CNcbiOstrstreamToString(oss);
    }}
            
    auto_ptr<CDB_RPCCmd> cmd(conn->RPC("id_gi_by_seqid_asn", 1));
    cmd->SetParam("@asnin", &asnIn);
    cmd->Send();

    while(cmd->HasMoreResults()) {
        auto_ptr<CDB_Result> result(cmd->Result());
        if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
            continue;
            
        while(result->Fetch()) {
            for(unsigned pos = 0; pos < result->NofItems(); ++pos) {
                const string& name = result->ItemName(pos);
                if (name == "gi") {
                    CDB_Int giFound;
                    result->GetItem(&giFound);
                    return giFound.Value();
                }
                else {
                    result->SkipItem();
                }
            }
        }
    }
    return 0;
}


CRef<CTSE_Info> CPubseqReader::GetMainBlob(const CSeqref& seqref, TConn connid)
{
    CDB_Connection* conn = x_GetConnection(connid);
    auto_ptr<CDB_RPCCmd> cmd(x_SendRequest(seqref, conn, false));
    auto_ptr<CDB_Result> result(x_ReceiveData(*cmd));
    return x_ReceiveMainBlob(*result);
}


CRef<CSeq_annot_SNP_Info> CPubseqReader::GetSNPAnnot(const CSeqref& seqref,
                                                     TConn connid)
{
    CDB_Connection* conn = x_GetConnection(connid);
    auto_ptr<CDB_RPCCmd> cmd(x_SendRequest(seqref, conn, true));
    auto_ptr<CDB_Result> result(x_ReceiveData(*cmd));
    return x_ReceiveSNPAnnot(*result);
}


CDB_RPCCmd* CPubseqReader::x_SendRequest(const CSeqref& seqref,
                                         CDB_Connection* conn,
                                         bool is_snp)
{
    auto_ptr<CDB_RPCCmd> cmd(conn->RPC("id_get_asn", 4));
    CDB_Int giIn(seqref.GetGi());
    CDB_SmallInt satIn(seqref.GetSat());
    CDB_Int satKeyIn(seqref.GetSatKey());
    bool is_external = is_snp;
    bool load_external = is_external || !TrySNPSplit();
    CDB_Int ext_feat(load_external);

    cmd->SetParam("@gi", &giIn);
    cmd->SetParam("@sat_key", &satKeyIn);
    cmd->SetParam("@sat", &satIn);
    cmd->SetParam("@ext_feat", &ext_feat);
    cmd->Send();
    return cmd.release();
}


CDB_Result* CPubseqReader::x_ReceiveData(CDB_RPCCmd& cmd)
{
    // new row
    CDB_VarChar descrOut("-");
    CDB_Int classOut(0);
    CDB_Int confidential(0),withdrawn(0);
    
    while( cmd.HasMoreResults() ) {
        _TRACE("next result");
        if ( cmd.HasFailed() && confidential.Value()>0 ||
             withdrawn.Value()>0 ) {
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
                if ( name == "asn1" ) {
                    return result.release();
                }
                else if ( name == "confidential" ) {
                    result->GetItem(&confidential);
                }
                else if ( name == "override" ) {
                    result->GetItem(&withdrawn);
                }
                else {
                    result->SkipItem();
                }
            }
        }
    }
    if ( confidential.Value()>0 || withdrawn.Value()>0 ) {
        NCBI_THROW(CLoaderException, ePrivateData, "gi is private");
    }
    NCBI_THROW(CLoaderException, eNoData, "no data");
}


CRef<CTSE_Info> CPubseqReader::x_ReceiveMainBlob(CDB_Result& result)
{
    CRef<CSeq_entry> seq_entry(new CSeq_entry);

    CResultBtSrcRdr reader(&result);
    CObjectIStreamAsnBinary in(reader);
    
    CReader::SetSeqEntryReadHooks(in);
    in >> *seq_entry;

    return Ref(new CTSE_Info(*seq_entry));
}


CRef<CSeq_annot_SNP_Info> CPubseqReader::x_ReceiveSNPAnnot(CDB_Result& result)
{
    CRef<CSeq_annot_SNP_Info> snp_annot_info(new CSeq_annot_SNP_Info);
    
    {{
        CResultBtSrcRdr src(&result);
        CResultZBtSrcRdr src2(&src);
        
        CObjectIStreamAsnBinary in(src2);
        
        snp_annot_info->Read(in);
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
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.38  2003/10/21 14:27:35  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.37  2003/10/14 21:06:25  vasilche
* Fixed compression statistics.
* Disabled caching of SNP blobs.
*
* Revision 1.36  2003/09/30 16:22:02  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.35  2003/08/27 14:25:22  vasilche
* Simplified CCmpTSE class.
*
* Revision 1.34  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.33  2003/07/24 20:35:16  vasilche
* Fix includes.
*
* Revision 1.32  2003/07/24 19:28:09  vasilche
* Implemented SNP split for ID1 loader.
*
* Revision 1.31  2003/07/18 20:27:54  vasilche
* Check if reference counting is working before trying ot use it.
*
* Revision 1.30  2003/07/17 22:23:27  vasilche
* Fixed unsigned <-> size_t argument.
*
* Revision 1.29  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.28  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.27  2003/05/13 20:14:40  vasilche
* Catching exceptions and reconnection were moved from readers to genbank loader.
*
* Revision 1.26  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.25  2003/04/15 16:33:29  dicuccio
* Minor logic reorganization to appease MSVC - wasn't dealing well with a verbose
* cast
*
* Revision 1.24  2003/04/15 15:30:15  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.23  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.22  2003/04/02 03:57:09  kimelman
* packet size tuning
*
* Revision 1.21  2003/03/03 20:34:51  vasilche
* Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
* Avoid using _REENTRANT macro - use NCBI_THREADS instead.
*
* Revision 1.20  2003/03/01 22:26:56  kimelman
* performance fixes
*
* Revision 1.19  2003/01/18 08:42:25  kimelman
* 1.added SNP retrieval per M.DiCuccio request; 2.avoid gi relookup
*
* Revision 1.18  2002/12/30 23:36:22  vakatov
* CPubseqStreamBuf::underflow() -- strstream::freeze(false) to avoid mem.leak
*
* Revision 1.17  2002/12/26 20:53:41  dicuccio
* Minor tweaks to relieve compiler warnings in MSVC
*
* Revision 1.16  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.15  2002/07/22 22:55:55  kimelman
* bugfixes: a) null termination added for text seqid strstream.
*           b) processing of failed request.
*
* Revision 1.14  2002/07/10 16:49:59  grichenk
* Removed CRef<CSeq_entry>, use pointer instead
*
* Revision 1.13  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.12  2002/05/09 21:40:59  kimelman
* MT tuning
*
* Revision 1.11  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.10  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.9  2002/04/25 20:54:07  kimelman
* noise off
*
* Revision 1.8  2002/04/12 22:56:23  kimelman
* processing of ctlib fail
*
* Revision 1.7  2002/04/12 14:52:34  butanaev
* Typos fixed, code cleanup.
*
* Revision 1.6  2002/04/11 20:03:26  kimelman
* switch to pubseq
*
* Revision 1.5  2002/04/11 17:59:36  butanaev
* Typo fixed.
*
* Revision 1.4  2002/04/11 17:47:17  butanaev
* Switched to using dbapi driver manager.
*
* Revision 1.3  2002/04/10 22:47:56  kimelman
* added pubseq_reader as default one
*
* Revision 1.2  2002/04/09 16:10:57  ucko
* Split CStrStreamBuf out into a common location.
*
* Revision 1.1  2002/04/08 20:52:27  butanaev
* Added PUBSEQ reader.
*
* Revision 1.8  2002/04/03 18:37:33  butanaev
* Replaced DBLink with dbapi.
*
* Revision 1.7  2002/01/10 16:24:26  butanaev
* Fixed possible memory leaks.
*
* Revision 1.6  2001/12/21 15:01:49  butanaev
* Removed debug code, bug fixes.
*
* Revision 1.5  2001/12/19 19:42:13  butanaev
* Implemented construction of PUBSEQ blob stream, CPubseqReader family  interfaces.
*
* Revision 1.4  2001/12/14 20:48:07  butanaev
* Implemented fetching Seqrefs from PUBSEQ.
*
* Revision 1.3  2001/12/13 17:50:34  butanaev
* Adjusted for new interface changes.
*
* Revision 1.2  2001/12/12 22:08:58  butanaev
* Removed duplicate code, created frame for CPubseq* implementation.
*
*/
