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
#include <objmgr/impl/seqref_pubseq.hpp>
#include <objmgr/impl/pack_string.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/iterator.hpp>

#include <util/compress/zlib.hpp>

#include <vector>
#include <memory>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDB_Connection* CPubseqSeqref::x_GetConn(TConn conn) const
{
    return m_Reader.GetConnection(conn);
}


void CPubseqSeqref::x_Reconnect(TConn conn) const
{
    m_Reader.Reconnect(conn);
}


const string CPubseqSeqref::print(void) const
{
    CNcbiOstrstream ostr;
    ostr << "SeqRef(" << Sat() << "," << SatKey () << "," << Gi() << ")" ;
    return CNcbiOstrstreamToString(ostr);
}


const string CPubseqSeqref::printTSE(void) const
{
    CNcbiOstrstream ostr;
    ostr << "TSE(" << Sat() << "," << SatKey () << ")" ;
    return CNcbiOstrstreamToString(ostr);
}


int CPubseqSeqref::Compare(const CSeqref& seqRef, EMatchLevel ml) const
{
    const CPubseqSeqref *p = dynamic_cast<const CPubseqSeqref*>(& seqRef);
    if( !p ) {
        THROW1_TRACE(runtime_error,
                     "Attempt to compare seqrefs from different sources");
    }
    if ( ml == eContext )
        return 0;

    //cout << "Compare" ; print(); cout << " vs "; p->print(); cout << endl;
    
    if ( Sat() < p->Sat() )
        return -1;
    if ( Sat() > p->Sat() )
        return 1;
    // Sat() == p->Sat()

    if ( SatKey() < p->SatKey() )
        return -1;
    if ( SatKey() > p->SatKey() )
        return 1;
    // blob == p->blob

    //cout << "Same TSE" << endl;

    if ( ml==eTSE )
        return 0;
    if ( Gi() < p->Gi() )
        return -1;
    if ( Gi() > p->Gi() )
        return 1;
    //cout << "Same GI" << endl;

    return 0;
}


#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
DEFINE_STATIC_MUTEX(s_readers_mutex);
static int    s_pubseq_readers=0;
#endif

CPubseqReader::CPubseqReader(TConn noConn,
                             const string& server,
                             const string& user,
                             const string& pswd)
    : m_Server(server) , m_User(user), m_Password(pswd), m_Context(NULL)
{
#if !defined(HAVE_SYBASE_REENTRANT)
    noConn=1;
#endif
#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    {{
        CMutexGuard g(s_readers_mutex);
        if(s_pubseq_readers>0)
            throw runtime_error("Attempt to open multiple pubseq_readers without MT-safe DB library");
        s_pubseq_readers++;
    }}
#endif
    SetParallelLevel(noConn);
    // LOG_POST("opened " << m_Pool.size() << " new connections");
}

CPubseqReader::~CPubseqReader()
{
    // LOG_POST("closed " << m_Pool.size() << " connections");
    SetParallelLevel(0);

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    {{
        CMutexGuard g(s_readers_mutex);
        s_pubseq_readers--;
    }}
#endif
}

CReader::TConn CPubseqReader::GetParallelLevel(void) const
{
    return m_Pool.size();
}

void CPubseqReader::SetParallelLevel(TConn size)
{
    size_t oldSize = m_Pool.size();
    for(size_t i = size; i < oldSize; ++i)
        delete m_Pool[i];

    m_Pool.resize(size);

    for(size_t i = oldSize; i < size; ++i)
        m_Pool[i] = NewConn();
}

CDB_Connection *CPubseqReader::NewConn()
{
    if ( m_Context.get() == NULL ) {
        C_DriverMgr drvMgr;
        string errmsg;
        FDBAPI_CreateContext createContextFunc =
            drvMgr.GetDriver("ctlib",&errmsg);
        if ( !createContextFunc ) {
            LOG_POST(errmsg);
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
            throw runtime_error("No ctlib available");
#else
            createContextFunc = drvMgr.GetDriver("dblib",&errmsg);
            if ( !createContextFunc ) {
                LOG_POST(errmsg);
                throw runtime_error("Neither ctlib nor dblib are available");
            }
#endif
        }
        map<string,string> args;
        args["packet"]="3584"; // 7*512
        m_Context.reset((*createContextFunc)(&args));
        //m_Context.reset((*createContextFunc)(0));
    }
    auto_ptr<CDB_Connection> conn(
        m_Context->Connect(m_Server, m_User, m_Password, 0));
    if ( conn.get() ) {
        auto_ptr<CDB_LangCmd> cmd(conn->LangCmd("set blob_stream on"));
        if ( cmd.get() ) {
            cmd->Send();
        }
    }
    return conn.release();
}


CDB_Connection* CPubseqReader::GetConnection(TConn conn)
{
    return m_Pool[conn % m_Pool.size()];
}


void CPubseqReader::Reconnect(TConn conn)
{
    LOG_POST("Reconnect");
    conn = conn % m_Pool.size();
    delete m_Pool[conn];
    m_Pool[conn] = NewConn();
}


bool CPubseqReader::RetrieveSeqrefs(TSeqrefs& sr,
                                    const CSeq_id& seqId,
                                    TConn conn)
{
    return x_RetrieveSeqrefs(sr, seqId, conn);
}


bool CPubseqReader::x_RetrieveSeqrefs(TSeqrefs& srs,
                                      const CSeq_id& seqId,
                                      TConn con)
{
    int gi = 0;

    if ( seqId.IsGi() ) {
        gi = seqId.GetGi();
    }
    else {

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
            
        auto_ptr<CDB_RPCCmd> cmd(m_Pool[con]->RPC("id_gi_by_seqid_asn", 1));
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
                        gi = giFound.Value();
                    }
                    else {
                        result->SkipItem();
                    }
                }
            }
            
            //LOG_POST(setw(3) << CThread::GetSelf() << ":: " <<
            // "id_gi_by_seqid_asn => gi("<<gi << ")");
        }
    }
    
    if (gi == 0)
        return true; // no data?

    auto_ptr<CDB_RPCCmd> cmd(m_Pool[con]->RPC("id_gi_class", 1));
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

            CRef<CSeqref> sr;
            sr.Reset(new CPubseqSeqref(*this,
                                       gi,
                                       sat.Value(),
                                       satKey.Value()));
            srs.push_back(sr);
            if ( extFeat.IsNULL() || extFeat.Value() != 0 ) {
                sr.Reset(new CPubseqSeqref(*this, gi, 15, gi));
                sr->SetFlags(extFeat.IsNULL()?
                             CSeqref::fHasExternal | CSeqref::fPossible:
                             CSeqref::fHasExternal);
                srs.push_back(sr);
            }
        }
    }
  
    return true;
}


CPubseqSeqref::CPubseqSeqref(CPubseqReader& reader,
                             int gi, int sat, int satkey)
    : m_Reader(reader), m_Gi(gi), m_Sat(sat), m_SatKey(satkey)
{
}


CPubseqSeqref::~CPubseqSeqref(void)
{
}


CBlobSource* CPubseqSeqref::GetBlobSource(TPos , TPos ,
                                          TBlobClass ,
                                          TConn conn) const
{
    return new CPubseqBlobSource(*this, conn);
}


CPubseqBlobSource::CPubseqBlobSource(const CPubseqSeqref& seqId, TConn conn)
    : m_Seqref(seqId), m_Conn(conn),
      m_Cmd(seqId.x_GetConn(conn)->RPC("id_get_asn", 3))
{
    CDB_Int giIn(seqId.Gi());
    CDB_SmallInt satIn(seqId.Sat());
    CDB_Int satKeyIn(seqId.SatKey());

    m_Cmd->SetParam("@gi", &giIn);
    m_Cmd->SetParam("@sat_key", &satKeyIn);
    m_Cmd->SetParam("@sat", &satIn);
    m_Cmd->Send();
}


CPubseqBlobSource::~CPubseqBlobSource(void)
{
}


void CPubseqBlobSource::x_Reconnect(void) const
{
    m_Seqref.x_Reconnect(m_Conn);
}


bool CPubseqBlobSource::HaveMoreBlobs(void)
{
    if ( !m_Blob ) {
        x_GetNextBlob();
    }
    return m_Blob;
}


CBlob* CPubseqBlobSource::RetrieveBlob(void)
{
    return m_Blob.Release();
}


void CPubseqBlobSource::x_GetNextBlob(void)
{
    // new row
    CDB_VarChar descrOut("-");
    CDB_Int classOut(0);
    CDB_Int confidential(0),withdrawn(0);
    
    while(m_Cmd->HasMoreResults()) {
        _TRACE("next result");
        if ( m_Cmd->HasFailed() && confidential.Value()>0 ||
             withdrawn.Value()>0 ) {
            LOG_POST("GI(" << m_Seqref.Gi() <<") is private");
            return;
        }
            
        m_Result.reset(m_Cmd->Result());
        if (m_Result.get() == 0 || m_Result->ResultType() != eDB_RowResult)
            continue;
            
        while(m_Result->Fetch()) {
            _TRACE("next fetch: " << m_Result->NofItems() << " items");
            for ( unsigned pos = 0; pos < m_Result->NofItems(); ++pos ) {
                const string& name = m_Result->ItemName(pos);
                _TRACE("next item: " << name);
                if(name == "asn1") {
                    m_Blob.Reset(new CPubseqBlob(*this,
                                                 classOut.Value(),
                                                 descrOut.Value(),
                                                 m_Seqref.Sat() == 15));
                    return;
                }
                else if(name == "confidential") {
                    m_Result->GetItem(&confidential);
                }
                else if(name == "override") {
                    m_Result->GetItem(&withdrawn);
                }
                else {
                    m_Result->SkipItem();
                }
            }
        }
    }
    if(confidential.Value()>0 || withdrawn.Value()>0) {
        LOG_POST("GI(" << m_Seqref.Gi() <<") is private");
        return;
    }
}


CPubseqBlob::CPubseqBlob(CPubseqBlobSource& source,
                         int cls, const string& descr,
                         bool snp)
    : m_Source(source), m_SNP(snp)
{
    Class() = cls;
    Descr() = descr;
}


CPubseqBlob::~CPubseqBlob(void)
{
}


CSeq_entry *CPubseqBlob::Seq_entry()
{
    if ( m_SNP ) {
        CResultZBtSrc src(m_Source.m_Result.get());
        
        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Create(eSerial_AsnBinary, src));

        if ( true ) {
            CObjectTypeInfo type = CType<CGb_qual>();
            type.FindMember("qual").SetLocalReadHook(*in, new CPackStringClassHook);
            type.FindMember("val").SetLocalReadHook(*in, new CPackStringClassHook(1));
            type = CObjectTypeInfo(CType<CImp_feat>());
            type.FindMember("key").SetLocalReadHook(*in, new CPackStringClassHook(32, 128));

            type = CObjectTypeInfo(CType<CObject_id>());
            type.FindVariant("str").SetLocalReadHook(*in, new CPackStringChoiceHook);

            type = CObjectTypeInfo(CType<CDbtag>());
            type.FindMember("db").SetLocalReadHook(*in, new CPackStringClassHook);

            type = CObjectTypeInfo(CType<CSeq_feat>());
            type.FindMember("comment").SetLocalReadHook(*in, new CPackStringClassHook);
        }
        
        CRef<CSeq_annot> annot(new CSeq_annot);

        *in >> *annot;

        in.reset();

        if ( false ) {
            if ( !annot->ReferencedOnlyOnce() )
                ERR_POST("other reference to annot");
            //annot.Release();
            THROW1_TRACE(runtime_error, "abort");
        }

        if ( false ) {
            set<string> qual, val;
            size_t count = 0;
            for( CTypeConstIterator<CGb_qual> i(ConstBegin(*annot)); i; ++i ) {
                ++count;
                {{
                    const string& s = i->GetQual();
                    set<string>& ss = qual;
                    if ( s.data() != ss.insert(s).first->data() ) {
                        THROW1_TRACE(runtime_error,
                                     "bad ref counting");
                    }
                }}
                {{
                    const string& s = i->GetVal();
                    set<string>& ss = val;
                    if ( val.size() <= 1 &&
                         s.data() != ss.insert(s).first->data() ) {
                        THROW1_TRACE(runtime_error,
                                     "bad ref counting");
                    }
                }}
            }
            NcbiCout << "annot stat: Gb-qual count: " << count <<
                " qual count: " << qual.size() <<
                " val count: " << val.size() << NcbiEndl;
        }

        m_Seq_entry.Reset(new CSeq_entry);
        m_Seq_entry->SetSet().SetSeq_set(); // it's not optional
        m_Seq_entry->SetSet().SetAnnot().push_back(annot);
    }
    else {
        CResultBtSrc src(m_Source.m_Result.get());

        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Create(eSerial_AsnBinary, src));
        
        m_Seq_entry.Reset(new CSeq_entry);
        
        *in >> *m_Seq_entry;
    }

    return m_Seq_entry;
}


CResultBtSrc::CResultBtSrc(CDB_Result* result)
    : m_Result(result)
{
}


CResultBtSrc::~CResultBtSrc()
{
}



CRef<CByteSourceReader> CResultBtSrc::Open(void)
{
    return CRef<CByteSourceReader>(new CResultBtSrcRdr(m_Result));
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


CResultZBtSrc::CResultZBtSrc(CDB_Result* result)
    : m_Result(result)
{
}


CResultZBtSrc::~CResultZBtSrc()
{
}



CRef<CByteSourceReader> CResultZBtSrc::Open(void)
{
    return CRef<CByteSourceReader>(new CResultZBtSrcRdr(m_Result));
}


class NCBI_XOBJMGR_EXPORT CResultZBtSrcX
{
public:
    CResultZBtSrcX(CResultZBtSrcRdr& reader);
    ~CResultZBtSrcX(void);

    size_t Read(char* buffer, size_t bufferLength);
    void ReadLength(void);

    enum {
        kMax_UncomprSize = 128*1024,
        kMax_ComprSize = 128*1024
    };

private:
    CResultZBtSrcRdr& m_Reader;
    vector<char>      m_Buffer;
    size_t            m_BufferPos;
    size_t            m_BufferEnd;
    CZipCompression   m_Decompressor;
    vector<char>      m_Compressed;
};


CResultZBtSrcRdr::CResultZBtSrcRdr(CDB_Result* result)
    : m_Result(result), m_Type(eType_unknown)
{
}


CResultZBtSrcRdr::~CResultZBtSrcRdr()
{
}


size_t CResultZBtSrcRdr::Read(char* buffer, size_t bufferLength)
{
    EType type = m_Type;
    if ( type == eType_plain ) {
        return x_Read(buffer, bufferLength);
    }

    if ( type == eType_unknown ) {
        if ( bufferLength < 4 ) {
            THROW1_TRACE(runtime_error,
                         "CResultZBtSrcRdr: "
                         "too small buffer to determine compression type");
        }
        size_t cnt = x_Read(buffer, 4);
        if ( cnt != 4 ) {
            // too few bytes - assume non "ZIP"
            _TRACE("CResultZBtSrcRdr: non-ZIP: " << cnt);
            m_Type = eType_plain;
            return cnt;
        }
        if ( memcmp(buffer, "ZIP", cnt) != 0 ) {
            // non "ZIP"
            cnt += x_Read(buffer+4, bufferLength-4);
            _TRACE("CResultZBtSrcRdr: non-ZIP: " << cnt);
            m_Type = eType_plain;
            return cnt;
        }
        m_Type = eType_zlib;

        m_Decompressor.reset(new CResultZBtSrcX(*this));
        //m_Decompressor->ReadLength();
    }

    return m_Decompressor->Read(buffer, bufferLength);
}


size_t CResultZBtSrcRdr::x_Read(char* buffer, size_t bufferLength)
{
    size_t ret = 0;
    while ( bufferLength > 0 ) {
        size_t cnt = m_Result->ReadItem(buffer, bufferLength);
        if ( cnt == 0 ) {
            if ( !m_Result->Fetch() )
                break;
        }
        else {
#if 0 // && _DEBUG
            NcbiCout << "Data:";
            for ( size_t i = 0; i < cnt; ++i ) {
                NcbiCout << ' ' <<
                    "0123456789abcdef"[(buffer[i]>>4)&15] << 
                    "0123456789abcdef"[(buffer[i])&15];
            }
            NcbiCout << NcbiEndl;
#endif
            bufferLength -= cnt;
            buffer += cnt;
            ret += cnt;
        }
    }
    return ret;
}


CResultZBtSrcX::CResultZBtSrcX(CResultZBtSrcRdr& reader)
    : m_Reader(reader), m_BufferPos(0), m_BufferEnd(0)
{
}


CResultZBtSrcX::~CResultZBtSrcX(void)
{
}


void CResultZBtSrcX::ReadLength(void)
{
    char header[8];
    if ( m_Reader.x_Read(header, 8) != 8 ) {
        THROW1_TRACE(runtime_error,
                     "CResultZBtSrcX: "
                     "too few header bytes");
    }
    unsigned int compr_size = 0;
    for ( size_t i = 0; i < 4; ++i ) {
        compr_size = (compr_size<<8) | (unsigned char)header[i];
    }
    unsigned int uncompr_size = 0;
    for ( size_t i = 4; i < 8; ++i ) {
        uncompr_size = (uncompr_size<<8) | (unsigned char)header[i];
    }

    if ( compr_size > kMax_ComprSize ) {
        THROW1_TRACE(runtime_error,
                     "CResultZBtSrcX: "
                     "compressed size is too large");
    }
    if ( uncompr_size > kMax_UncomprSize ) {
        THROW1_TRACE(runtime_error,
                     "CResultZBtSrcX: "
                     "uncompressed size is too large");
    }
    m_Compressed.reserve(compr_size);
    if ( m_Reader.x_Read(&m_Compressed[0], compr_size) != compr_size ) {
        THROW1_TRACE(runtime_error,
                     "CResultZBtSrcX: "
                     "compressed data is not complete");
    }
    m_BufferPos = kMax_UInt;
    m_Buffer.reserve(uncompr_size);
    if ( !m_Decompressor.DecompressBuffer(&m_Compressed[0], compr_size,
                                          &m_Buffer[0], uncompr_size,
                                          &uncompr_size) ) {
        THROW1_TRACE(runtime_error,
                     "CResultZBtSrcX: "
                     "decompression failed");
    }
    m_BufferEnd = uncompr_size;
    m_BufferPos = 0;
}


size_t CResultZBtSrcX::Read(char* buffer, size_t bufferLength)
{
    while ( m_BufferPos == m_BufferEnd ) {
        ReadLength();
    }
    size_t cnt = min(bufferLength, m_BufferEnd-m_BufferPos);
    memcpy(buffer, &m_Buffer[m_BufferPos], cnt);
    m_BufferPos += cnt;
    return cnt;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
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
