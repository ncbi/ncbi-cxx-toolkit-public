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

#include <objects/objmgr/reader_pubseq.hpp>
#include <objects/objmgr/impl/seqref_pubseq.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>


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


char* CPubseqSeqref::print(char *s,int size) const
{
    CNcbiOstrstream ostr(s,size);
    ostr << "SeqRef(" << Sat() << "," << SatKey () << "," << Gi() << ")" ;
    s[ostr.pcount()]=0;
    return s;
}


char* CPubseqSeqref::printTSE(char *s,int size) const
{
    CNcbiOstrstream ostr(s,size);
    ostr << "TSE(" << Sat() << "," << SatKey () << ")" ;
    s[ostr.pcount()]=0;
    return s;
}


int CPubseqSeqref::Compare(const CSeqref& seqRef, EMatchLevel ml) const
{
    const CPubseqSeqref *p = dynamic_cast<const CPubseqSeqref*>(& seqRef);
    if(p==0) {
        throw runtime_error("Attempt to compare seqrefs from different sources");
    }
    if(ml==eContext) return 0;

    //cout << "Compare" ; print(); cout << " vs "; p->print(); cout << endl;
    
    if(Sat() < p->Sat())  return -1;
    if(Sat() > p->Sat())  return 1;
    // Sat() == p->Sat()
    if(SatKey() < p->SatKey())  return -1;
    if(SatKey() > p->SatKey())  return 1;
    // blob == p->blob
    //cout << "Same TSE" << endl;
    if(ml==eTSE) return 0;
    if(Gi() < p->Gi())  return -1;
    if(Gi() > p->Gi())  return 1;
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
        FDBAPI_CreateContext createContextFunc = drvMgr.GetDriver("ctlib",&errmsg);
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
    return m_Context->Connect(m_Server, m_User, m_Password, 0);
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
    try {
        return x_RetrieveSeqrefs(sr, seqId, conn);
    }
    catch ( const CDB_Connection& ) {
        Reconnect(conn);
        throw;
    }
}


bool CPubseqReader::x_RetrieveSeqrefs(TSeqrefs& sr,
                                      const CSeq_id& seqId,
                                      TConn con)
{
    int gi = 0;

    if(seqId.IsGi())
        gi = seqId.GetGi();
    else {
        CNcbiOstrstream oss;
        {
            CObjectOStreamAsn ooss(oss);
            ooss << seqId;
        }
        CDB_VarChar asnIn(static_cast<string>(CNcbiOstrstreamToString(oss)));
        
        auto_ptr<CDB_RPCCmd> cmd(m_Pool[con]->RPC("id_gi_by_seqid_asn", 1));
        cmd->SetParam("@asnin", &asnIn);
        cmd->Send();
        CDB_Int giFound;
        
        while(cmd->HasMoreResults()) {
            auto_ptr<CDB_Result> result(cmd->Result());
            if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
                continue;
            
            while(result->Fetch()) {
                for(unsigned pos = 0; pos < result->NofItems(); ++pos) {
                    string name = result->ItemName(pos);
                    if (name == "gi")
                        result->GetItem(&giFound);
                    else
                        result->SkipItem();
                }
            }
            
            gi = giFound.Value();
            //LOG_POST(setw(3) << CThread::GetSelf() << ":: " << "id_gi_by_seqid_asn => gi("<<gi << ")");
        }
    }
    
    if (gi == 0)
        return true; // no data?

    {
        auto_ptr<CDB_RPCCmd> cmd(m_Pool[con]->RPC("id_gi_class", 1));
        CDB_Int giIn(gi);
        cmd->SetParam("@gi", &giIn);
        cmd->Send();
        CDB_Int giOut(gi);
        CDB_Int satOut;
        CDB_Int satKeyOut;
        
        while(cmd->HasMoreResults()) {
            auto_ptr<CDB_Result> result(cmd->Result());
            if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
                continue;
            
            if(result->Fetch()) {
                result->GetItem(&satOut);
                result->SkipItem();
                result->GetItem(&satKeyOut);

                sr.push_back(CRef<CSeqref>(new CPubseqSeqref
                                           (*this,
                                            giOut.Value(),
                                            satOut.Value(),
                                            satKeyOut.Value())));
            }
            while(result->Fetch()) {
                // do nothing
            }
        }
    }
  
    return true;
}


CPubseqSeqref::CPubseqSeqref(CPubseqReader& reader,
                             int gi, int sat, int satkey)
    : m_Reader(reader), m_Gi(gi), m_Sat(sat), m_SatKey(satkey)
{
    Flag() = 0;
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
      m_Cmd(seqId.x_GetConn(conn)->RPC("id_get_asn", 5))
{
    try {
        CDB_Int giIn(seqId.Gi());
        CDB_SmallInt satIn(seqId.Sat());
        CDB_Int satKeyIn(seqId.SatKey());
        CDB_Int z(0);
        CDB_Int o(1);

        m_Cmd->SetParam("@gi", &giIn);
        m_Cmd->SetParam("@sat_key", &satKeyIn);
        m_Cmd->SetParam("@sat", &satIn);
        m_Cmd->SetParam("@maxplex", &z);
        m_Cmd->SetParam("@outfmt", &z);
        m_Cmd->SetParam("@ext_feat", &o);
        m_Cmd->Send();
    }
    catch ( ... ) {
        x_Reconnect();
        throw;
    }
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
    try {
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
                    const string name = m_Result->ItemName(pos);
                    _TRACE("next item: " << name);
                    if(name == "asn1") {
                        m_Blob.Reset(new CPubseqBlob(*this,
                                                     classOut.Value(),
                                                     descrOut.Value()));
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
    catch ( ... ) {
        x_Reconnect();
        throw;
    }
}


CPubseqBlob::CPubseqBlob(CPubseqBlobSource& source,
                         int cls, const string& descr)
    : m_Source(source)
{
    Class() = cls;
    Descr() = descr;
}


CPubseqBlob::~CPubseqBlob(void)
{
}


CSeq_entry *CPubseqBlob::Seq_entry()
{
    try {
        CResultByteSource src(m_Source.m_Result.get());
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Create(eSerial_AsnBinary, src));

        m_Seq_entry = new CSeq_entry;

        *in >> *m_Seq_entry;
    }
    catch ( ... ) {
        m_Source.x_Reconnect();
        throw;
    }

    return m_Seq_entry;
}


CResultByteSource::CResultByteSource(CDB_Result* result)
    : m_Result(result)
{
}


CResultByteSource::~CResultByteSource()
{
}



CRef<CByteSourceReader> CResultByteSource::Open(void)
{
    return CRef<CByteSourceReader>(new CResultByteSourceReader(m_Result));
}


CResultByteSourceReader::CResultByteSourceReader(CDB_Result* result)
    : m_Result(result)
{
}


CResultByteSourceReader::~CResultByteSourceReader()
{
}


size_t CResultByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    return m_Result->ReadItem(buffer, bufferLength);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
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
