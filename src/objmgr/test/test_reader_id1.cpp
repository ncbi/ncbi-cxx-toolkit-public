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

#include <iostream>
#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <objects/objmgr1/reader_id1.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_core_cxx.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;

int test_id1_calls()
{
  STimeout tmout;
  CSeq_id seqId;
  
  tmout.sec = 20;
  tmout.usec = 0;
  seqId.SetGi(5);
  
  
  for(int k=0;k<100;k++)
    {
      int number = 0;
      string dbname;
      int lgi = 0;
      
      cout << "ID1: " << k << endl;
      
      auto_ptr<CConn_ServiceStream> m_ID1_Server;
      CConn_ServiceStream *server;
      
      m_ID1_Server.reset(new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout));
      server = m_ID1_Server.get();
    
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
      int gi = id1_reply.GetGotgi();
      _VERIFY(gi==5);

      //cout << "got gi: " << gi << endl;
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
          iterate(CSeq_hist_rec::TIds, it2, it->GetIds())
            {
              if((*it2)->IsGi()) lgi = (*it2)->GetGi();
              else if((*it2)->IsGeneral())
                {
                  dbname = (*it2)->GetGeneral().GetDb();
                  const CObject_id& tag = (*it2)->GetGeneral().GetTag();
                  if(tag.IsStr()) number = NStr::StringToInt(tag.GetStr());
                  else            number = (tag.GetId());
                }
            }
          //cout << "HIST: " << lgi << "," << dbname << "," << number << endl;
          if(gi==lgi) break;
        }
      
      //cout << "BLOB: " << endl;
      m_ID1_Server.reset(new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout));
      server = m_ID1_Server.get();
      
      {//aaaaa
        CRef<CID1server_maxcomplex> params(new CID1server_maxcomplex);
        params->SetGi(lgi);
        params->SetEnt(number);
        params->SetSat(dbname);
        
        CID1server_request id1_request;
        id1_request.SetGetsefromgi(*params);
        CObjectOStreamAsnBinary server_output(*server);
        //cout << "BLOB: sending request" << endl;
        server_output << id1_request;
        server_output.Flush();
        
      }
      //cout << "BLOB: request sent " << endl;
      CObjectIStream& server_input1 = *CObjectIStream::Open(eSerial_AsnBinary, *server, false);
      server_input1 >> id1_reply ;
      {
        CRef<CSeq_entry> se;
        if(id1_reply.IsGotseqentry())
          se = &id1_reply.GetGotseqentry();
        else if(id1_reply.IsGotdeadseqentry())
          se = &id1_reply.GetGotdeadseqentry();
      }
      //cout << "BLOB:(" << gi << ") OK" << endl;
    }
  return 1;
}

int main()
{
  //export CONN_DEBUG_PRINTOUT=data
  //CORE_SetLOG(LOG_cxx2c());
  //SetDiagTrace(eDT_Enable);
  //SetDiagPostLevel(eDiag_Info);
  //SetDiagPostFlag(eDPF_All);

  //test_id1_calls();
  
  CSeq_id seqId;
  seqId.SetGi(5);
  //seqId.SetEmbl().SetAccession("X66994");
  //seqId.SetEmbl().SetVersion(1);
  

  CId1Reader reader;
  for(int k=0;k<10;)
    {
      for(CIStream srs(reader.SeqrefStreamBuf(seqId)); ! srs.Eof(); )
        {
          delete reader.RetrieveSeqref(srs);
          cout << "SR: " << k++ << endl;
        }
    }
/*
  CId1Reader reader;
  CSeqref *seqRef = 0;
  for(CIStream srs(reader.SeqrefStreamBuf(seqId)); ! srs.Eof(); )
    {
      if(!seqRef) seqRef = reader.RetrieveSeqref(srs); 
      delete reader.RetrieveSeqref(srs); 
    }
*/
  
  CBlobClass cl;
  for(int k=0;k<1200;k++)
    {
       CId1Reader reader;
       CSeqref *seqRef = 0;
       for(CIStream srs(reader.SeqrefStreamBuf(seqId)); ! srs.Eof(); )
       {
         if(!seqRef) seqRef = reader.RetrieveSeqref(srs);
         delete reader.RetrieveSeqref(srs);
       }

       cout << "K: " << k << endl;
       streambuf *x = seqRef->BlobStreamBuf(0, 0, cl);
       for(CIStream bs(x); ! bs.Eof(); )
         {
           CBlob *blob = seqRef->RetrieveBlob(bs);
           //CObjectOStreamAsn oos(cout);
           //oos << *blob->Seq_entry();
           //cout << endl;
           
           CRef<CSeq_entry> se=blob->Seq_entry();
           delete blob;
         }
       delete x ;
       delete seqRef;
    }
  return 0;
}

/*
* $Log$
* Revision 1.5  2002/03/26 17:17:04  kimelman
* reader stream fixes
*
* Revision 1.4  2002/03/25 17:49:13  kimelman
* ID1 failure handling
*
* Revision 1.3  2002/03/21 19:14:55  kimelman
* GB related bugfixes
*
* Revision 1.2  2002/01/23 21:59:34  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.1  2002/01/11 19:06:27  gouriano
* restructured objmgr
*
* Revision 1.3  2001/12/10 20:08:02  butanaev
* Code cleanup.
*
* Revision 1.2  2001/12/07 21:25:00  butanaev
* Interface development, code beautyfication.
*
* Revision 1.1  2001/12/07 16:43:35  butanaev
* Switching to new reader interfaces.
*
* Revision 1.6  2001/12/06 20:37:05  butanaev
* Fixed timeout problem.
*
* Revision 1.5  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.4  2001/12/06 14:35:22  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
* Revision 1.3  2001/11/29 21:24:03  butanaev
* Classes working with PUBSEQ transferred to separate lib. Code cleanup.
* Test app redesigned.
*
* Revision 1.2  2001/10/11 15:47:34  butanaev
* Loader from ID1 implemented.
*
* Revision 1.1  2001/09/10 20:03:04  butanaev
* Initial checkin.
*/

