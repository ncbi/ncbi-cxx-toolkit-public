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

#include <corelib/ncbistre.hpp>
#include <objects/objmgr1/reader_id1.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

streambuf *CId1Reader::SeqrefStreamBuf(const CSeq_id &seqId)
{
  int gi=0;
  STimeout tmout;
  tmout.sec = 20;
  tmout.usec = 0;
  auto_ptr<CConn_ServiceStream> m_ID1_Server;
  
  m_ID1_Server.reset(new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout));

  CConn_ServiceStream *server = m_ID1_Server.get();

  {
    CID1server_request id1_request;
    id1_request.SetGetgi(seqId);
    CObjectOStreamAsnBinary server_output(*server);
    server_output << id1_request;
    server_output.Flush();
  }

  CObjectIStream& server_input = *CObjectIStream::Open(eSerial_AsnBinary, *server, false);
  CID1server_back id1_reply;

  server_input >> id1_reply;
  gi = id1_reply.GetGotgi();

  if(gi==0)
    return m_Stream.rdbuf();

  {
    CID1server_request id1_request1;
    id1_request1.SetGetgirev(gi);
    CObjectOStreamAsnBinary server_output(*server);
    server_output << id1_request1;
    server_output.Flush();
  }
  server_input >> id1_reply;
  
  for(CTypeConstIterator<CSeq_hist_rec> it = ConstBegin(id1_reply); it;  ++it)
    {
      int number = 0;
      string dbname;
      int lgi = 0;
      
      iterate(CSeq_hist_rec::TIds, it2, it->GetIds())
        {
          if((*it2)->IsGi())
            {
              lgi = (*it2)->GetGi();
            }
          else if((*it2)->IsGeneral())
            {
              dbname = (*it2)->GetGeneral().GetDb();
              const CObject_id& tag = (*it2)->GetGeneral().GetTag();
              if(tag.IsStr())
                {
                  number = NStr::StringToInt(tag.GetStr());
                }
              else
                {
                  number = (tag.GetId());
                }
            }
        }
      if(gi!=lgi) continue;
      CId1Seqref id1Seqref;
      id1Seqref.Gi() = gi;
      id1Seqref.Sat() = dbname;
      id1Seqref.SatKey() = number;
      id1Seqref.Flag() = 0;
      try {
        m_Stream << id1Seqref;
      } catch ( exception e ) {
        
        LOG_POST( "TROUBLE: reader_id1:m_stream:: write failed for (" <<
                  dbname << ":" << number << " - " << gi << ") :: " << e.what() );
        throw runtime_error("reader_id1 - internal buffer overflow") ;
      }
      break; // mk - get only the first one
    }
   return m_Stream.rdbuf();
}

void CId1Seqref::Save(ostream &os) const
{
  CSeqref::Save(os);
  os << m_Gi << m_Sat << m_SatKey;
}

void CId1Seqref::Restore(istream &is)
{
  CSeqref::Restore(is);
  is >> m_Gi >> m_Sat >> m_SatKey;
}

CSeqref* CId1Seqref::Dup() const
{
  return new CId1Seqref(*this);
}

CSeqref *CId1Reader::RetrieveSeqref(istream &is)
{
  CId1Seqref *id1Seqref = new CId1Seqref;
  is >> *id1Seqref;
  return id1Seqref;
}

struct CId1StreamBuf : public streambuf
{
  CId1StreamBuf(CId1Seqref &id1Seqref);
  CT_INT_TYPE underflow();
  void Close() { m_Server = 0; }

  CId1Seqref &m_Id1Seqref;
  char buffer[1024];
  CConn_ServiceStream *m_Server;
  auto_ptr<CConn_ServiceStream> m_ID1_Server;
};

CId1StreamBuf::CId1StreamBuf(CId1Seqref &id1Seqref) : m_Id1Seqref(id1Seqref)
{
  CRef<CID1server_maxcomplex> params(new CID1server_maxcomplex);
  params->SetGi(m_Id1Seqref.Gi());
  params->SetEnt(m_Id1Seqref.SatKey());
  params->SetSat(m_Id1Seqref.Sat());
  
  CID1server_request id1_request;
  id1_request.SetGetsefromgi(*params);
  
  STimeout tmout;
  tmout.sec = 2;
  tmout.usec = 0;
  m_ID1_Server.reset(new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout));
  
  m_Server = m_ID1_Server.get();
  {
    CObjectOStreamAsnBinary server_output(*m_Server);
    server_output << id1_request;
    server_output.Flush();
  }
}

CT_INT_TYPE CId1StreamBuf::underflow()
{
  if(! m_Server)
    return CT_EOF;

  int n = CIStream::Read(*m_Server, buffer, sizeof(buffer));
  setg(buffer, buffer, buffer + n);
  return n == 0 ? CT_EOF : buffer[0];
}

streambuf *CId1Seqref::BlobStreamBuf(int, int, const CBlobClass &)
{
  return new CId1StreamBuf(*this);
}

CSeq_entry *CId1Blob::Seq_entry()
{
  CObjectIStream *m_ObjStream = CObjectIStream::Open(eSerial_AsnBinary, m_IStream, false);
  CID1server_back id1_reply;
  bool clenup=false;
  try
    {
      *m_ObjStream >> id1_reply;
      static_cast<CId1StreamBuf *>(m_IStream.rdbuf())->Close();
    }
  catch ( exception e)
    {
      LOG_POST( "TROUBLE: reader_id1: can not read Seqentry from reply: " << e.what() );
      clenup=true;
    }
  if(id1_reply.IsGotseqentry())
    m_Seq_entry = &id1_reply.GetGotseqentry();
  else if(id1_reply.IsGotdeadseqentry())
    m_Seq_entry = &id1_reply.GetGotdeadseqentry();
  if(clenup && m_Seq_entry)
    {
      delete m_Seq_entry;
      m_Seq_entry=0;
    }
  
  return m_Seq_entry;
}

CBlob *CId1Seqref::RetrieveBlob(istream &is)
{
  return new CId1Blob(is);
}

void CId1Blob::Save(ostream &os) const
{
  CBlob::Save(os);
}

void CId1Blob::Restore(istream &is)
{
  CBlob::Restore(is);
}

char* CId1Seqref::print(char *s,int size) const
{
  CNcbiOstrstream ostr(s,size);
  ostr << "SeqRef(" << Sat() << "," << SatKey () << "," << Gi() << ")" ;
  s[ostr.pcount()]=0;
  return s;
}

char* CId1Seqref::printTSE(char *s,int size) const
{
  CNcbiOstrstream ostr(s,size);
  ostr << "TSE(" << Sat() << "," << SatKey () << ")" ;
  s[ostr.pcount()]=0;
  return s;
}

int CId1Seqref::Compare(const CSeqref &seqRef,EMatchLevel ml) const
{
  const CId1Seqref *p = dynamic_cast<const CId1Seqref*>(& seqRef);
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

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.8  2002/03/25 17:49:13  kimelman
* ID1 failure handling
*
* Revision 1.7  2002/03/25 15:44:47  kimelman
* proper logging and exception handling
*
* Revision 1.6  2002/03/22 21:50:21  kimelman
* bugfix: avoid history rtequest for nonexistent sequence
*
* Revision 1.5  2002/03/21 19:14:54  kimelman
* GB related bugfixes
*
* Revision 1.4  2002/03/21 01:34:55  kimelman
* gbloader related bugfixes
*
* Revision 1.3  2002/03/20 04:50:13  kimelman
* GB loader added
*
* Revision 1.2  2002/01/16 18:56:28  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
* Revision 1.7  2001/12/17 21:38:24  butanaev
* Code cleanup.
*
* Revision 1.6  2001/12/13 00:19:25  kimelman
* bugfixes:
*
* Revision 1.5  2001/12/12 21:46:40  kimelman
* Compare interface fix
*
* Revision 1.4  2001/12/10 20:08:02  butanaev
* Code cleanup.
*
* Revision 1.3  2001/12/07 21:24:59  butanaev
* Interface development, code beautyfication.
*
* Revision 1.2  2001/12/07 16:43:58  butanaev
* Fixed includes.
*
* Revision 1.1  2001/12/07 16:10:23  butanaev
* Switching to new reader interfaces.
*
* Revision 1.3  2001/12/06 20:37:05  butanaev
* Fixed timeout problem.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/
