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
*/

#include <objects/objmgr1/reader_pubseq.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>

#include "strstreambuf.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void CPubseqSeqref::Save(ostream &os) const
{
  CSeqref::Save(os);
  os << m_Gi << m_Sat << m_SatKey;
}

void CPubseqSeqref::Restore(istream &is)
{
  CSeqref::Restore(is);
  is >> m_Gi >> m_Sat >> m_SatKey;
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

int CPubseqSeqref::Compare(const CSeqref &seqRef,EMatchLevel ml) const
{
  const CPubseqSeqref *p = dynamic_cast<const CPubseqSeqref*>(& seqRef);
  if(p==0) throw runtime_error("Attempt to compare seqrefs from different sources");
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

CPubseqReader::CPubseqReader(unsigned noConn,const string& server,const string& user,const string& pswd)
  : m_Server(server) , m_User(user), m_Password(pswd), m_Context(NULL)
{
  for(unsigned i = 0; i < noConn; ++i)
    m_Pool.push_back(NewConn());
}

CPubseqReader::~CPubseqReader()
{
  for(unsigned i = 0; i < m_Pool.size(); ++i)
    delete m_Pool[i];
}

int CPubseqReader::GetParalellLevel(void) const
{
  return m_Pool.size();
}

void CPubseqReader::SetParalellLevel(unsigned size)
{
  unsigned poolSize = m_Pool.size();
  for(unsigned i = size; i < poolSize; ++i)
    delete m_Pool[i];

  m_Pool.resize(size);

  for(unsigned i = poolSize; i < size; ++i)
    m_Pool[i] = NewConn();
}

CDB_Connection *CPubseqReader::NewConn()
{
  C_DriverMgr drvMgr;
  FDBAPI_CreateContext createContextFunc = drvMgr.GetDriver("ctlib");
  if(! createContextFunc)
    throw runtime_error("No ctlib available");

  if(m_Context.get() != NULL)
    m_Context.reset((*createContextFunc)(0));

  return m_Context->Connect("PUBSEQ_OS", "anyone", "allowed", 0);
}

void CPubseqReader::Reconnect(unsigned conn)
{
  conn = conn % m_Pool.size();
  delete m_Pool[conn];
  m_Pool[conn] = NewConn();
}

streambuf *CPubseqReader::SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn)
{
  try
  {
    return x_SeqrefStreamBuf(seqId, conn);
  }
  catch(const CDB_Exception &e)
  {
    Reconnect(conn);
    throw;
  }
}

streambuf *CPubseqReader::x_SeqrefStreamBuf(const CSeq_id &seqId, unsigned con)
{
  CNcbiOstrstream oss;
  CObjectOStreamAsn ooss(oss);
  ooss << seqId;
  CDB_VarChar asnIn(oss.str());
  oss.freeze(false);

  int gi = 0;
  {
    auto_ptr<CDB_RPCCmd> cmd(m_Pool[con]->RPC("id_gi_by_seqid_asn", 1));
    cmd->SetParam("@asnin", &asnIn);
    cmd->Send();
    CDB_Int giFound;

    while(cmd->HasMoreResults())
    {
      auto_ptr<CDB_Result> result(cmd->Result());
      if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
        continue;

      while(result->Fetch())
      {
        for(unsigned pos = 0; pos < result->NofItems(); ++pos)
        {
          string name = result->ItemName(pos);
          if (name == "gi")
            result->GetItem(&giFound);
          else
            result->SkipItem();
        }
      }

      gi = giFound.Value();
      cout << "gi=" << gi << "\n";
    }
  }

  auto_ptr<strstream> ss(new strstream);

  {
    auto_ptr<CDB_RPCCmd> cmd(m_Pool[con]->RPC("id_get_gi_history", 2));
    CDB_Int giIn(gi);
    CDB_TinyInt revIn(0);
    cmd->SetParam("@gi", &giIn);
    cmd->SetParam("@rev", &revIn);
    cmd->Send();
    CDB_Int giOut;
    CDB_Int satOut;
    CDB_Int satKeyOut;
    bool  done=false;

    while(cmd->HasMoreResults())
    {
      auto_ptr<CDB_Result> result(cmd->Result());
      if (result.get() == 0  ||  result->ResultType() != eDB_RowResult)
        continue;

      while(result->Fetch())
      {
        for(unsigned pos = 0; pos < result->NofItems(); ++pos)
        {
          string name = result->ItemName(pos);
          if (name == "gi")
            result->GetItem(&giOut);
          else if(name == "sat")
            result->GetItem(&satOut);
          else if(name == "sat_key")
            result->GetItem(&satKeyOut);
          else
            result->SkipItem();
        }
        if(giOut.Value() == gi && !done)
          {
            CPubseqSeqref pubseqRef;
            pubseqRef.m_Gi.Value() = giOut.Value();
            pubseqRef.m_Sat.Value() = satOut.Value();
            pubseqRef.m_SatKey.Value() = satKeyOut.Value();
            pubseqRef.m_Flag.Value() = 0;
            *ss << pubseqRef;
            done=true;
          }
      }
    }
  }

  return new CStrStreamBuf(ss.release());
}

CSeqref *CPubseqReader::RetrieveSeqref(istream &is)
{
  CPubseqSeqref *seqref = new CPubseqSeqref;
  is >> *seqref;
  seqref->m_Reader = this;
  return seqref;
}

struct CPubseqStreamBuf : public streambuf
{
  CPubseqStreamBuf(CPubseqSeqref &pubseqSeqref, CDB_Connection *conn);
  CT_INT_TYPE underflow();

  enum EStatus {eInit, eNewRow, eBlob};
  CPubseqSeqref &m_Seqref;
  EStatus m_Status;
  CDB_Connection *m_Conn;
  auto_ptr<CDB_RPCCmd> m_Cmd;
  auto_ptr<CDB_Result> m_Result;
  bool m_Eof;
  CT_CHAR_TYPE m_Buffer[1000];
};

CPubseqStreamBuf::CPubseqStreamBuf(CPubseqSeqref &pubseqSeqref, CDB_Connection *conn) :
m_Seqref(pubseqSeqref), m_Status(eInit), m_Conn(conn)
{}

CT_INT_TYPE CPubseqStreamBuf::underflow()
{
  if(m_Status == eInit)
  {
    m_Cmd.reset(m_Conn->RPC("id_get_asn", 5));

    CDB_Int giIn(m_Seqref.Gi());
    CDB_SmallInt satIn(m_Seqref.Sat());
    CDB_Int satKeyIn(m_Seqref.SatKey());
    CDB_Int z(0);

    m_Cmd->SetParam("@gi", &giIn);
    m_Cmd->SetParam("@sat_key", &satKeyIn);
    m_Cmd->SetParam("@sat", &satIn);
    m_Cmd->SetParam("@maxplex", &z);
    m_Cmd->SetParam("@outfmt", &z);
    m_Cmd->SetParam("@ext_feat", &z);
    m_Cmd->Send();

    m_Status = eNewRow;
    return underflow();
  }
  else if(m_Status == eNewRow)
  {
    CDB_VarChar descrOut("-");
    CDB_Int classOut(0);

    while(m_Cmd->HasMoreResults())
    {
      m_Result.reset(m_Cmd->Result());
      if (m_Result.get() == 0  ||  m_Result->ResultType() != eDB_RowResult)
        continue;

      while(m_Result->Fetch())
        for(unsigned pos = 0; pos < m_Result->NofItems(); ++pos)
        {
          string name = m_Result->ItemName(pos);
          if(name == "asn1")
          {
            CStringStreamable descr(descrOut.Value());
            CIntStreamable cl(classOut.Value());
            CNcbiOstrstream os;
            os << cl << descr;

            memcpy(m_Buffer, os.str(), os.pcount());
            setg(m_Buffer, m_Buffer, m_Buffer + os.pcount());

            m_Status = eBlob;
            return CT_TO_INT_TYPE(m_Buffer[0]);
          }
          else
            m_Result->SkipItem();
        }
    }
    throw runtime_error("id_get_asn: asn not found");
  }
  else if(m_Status == eBlob)
  {
    unsigned size = m_Result->ReadItem(m_Buffer, sizeof(m_Buffer));
    if(size != 0)
    {
      setg(m_Buffer, m_Buffer, m_Buffer + size);
      return CT_TO_INT_TYPE(m_Buffer[0]);
    }
  }
  return CT_EOF;
}

streambuf *CPubseqSeqref::BlobStreamBuf(int start, int stop, const CBlobClass &cl, unsigned conn)
{
  m_Conn = conn;
  return new CPubseqStreamBuf(*this, m_Reader->m_Pool[conn]);
}

CSeq_entry *CPubseqBlob::Seq_entry()
{
  CObjectIStreamAsnBinary ois(m_IStream);
  CRef<CSeq_entry> se = new CSeq_entry;
  m_Seq_entry = se;
  try
  {
    ois >> *se;
  }
  catch(const CDB_Exception &e)
  {
    m_Seqref->m_Reader->Reconnect(m_Seqref->m_Conn);
    throw;
  }
  return se;
}

CBlob *CPubseqSeqref::RetrieveBlob(istream &is)
{
  auto_ptr<CBlob> blob(new CPubseqBlob(is));
  try
  {
    is >> *blob.get();
  }
  catch(const CDB_Exception &e)
  {
    m_Reader->Reconnect(m_Conn);
    throw;
  }
  return blob.release();
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
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
