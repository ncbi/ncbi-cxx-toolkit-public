#ifndef READER_ID1__HPP_INCLUDED
#define READER_ID1__HPP_INCLUDED

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

#include <corelib/ncbiobj.hpp>
#include <objects/objmgr/reader.hpp>

#include <objects/id1/ID1server_back.hpp>
#include <objects/id1/ID1server_request.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <serial/serial.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_conn_stream.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CId1Reader;
class NCBI_XOBJMGR_EXPORT CId1Seqref : public CSeqref
{
public:
  virtual void Save(ostream &os) const;
  virtual void Restore(istream &is);
  virtual streambuf *BlobStreamBuf(int start, int stop, const CBlobClass &cl, unsigned conn = 0);
  virtual CBlob *RetrieveBlob(istream &is);
  virtual CSeqref* Dup() const;
  virtual int Compare(const CSeqref &seqRef,EMatchLevel ml=eSeq) const ;
  virtual char* print(char*,int)    const;
  virtual char* printTSE(char*,int) const;

  CIntStreamable::TInt &Gi() { return m_Gi.Value(); };
  string &Sat() { return m_Sat.Value(); };
  CIntStreamable::TInt &SatKey() { return m_SatKey.Value(); };
  
  const CIntStreamable::TInt  Gi()     const { return m_Gi.Value(); };
  const string               &Sat()    const { return m_Sat.Value(); };
  const CIntStreamable::TInt  SatKey() const { return m_SatKey.Value(); };

private:
  friend class CId1Reader;
  streambuf *x_BlobStreamBuf(int start, int stop, const CBlobClass &cl, unsigned conn);
  CIntStreamable m_Gi;
  CStringStreamable m_Sat;
  CIntStreamable m_SatKey;
  CId1Reader *m_Reader;
};

class NCBI_XOBJMGR_EXPORT CId1Blob : public CBlob
{
public:

  void Save(ostream &os) const;
  void Restore(istream &is);
  CSeq_entry *Seq_entry();

protected:
  friend class CId1Seqref;
  CId1Blob(istream &is) : CBlob(is) {}

private:
  CRef<CSeq_entry> m_Seq_entry;
};

class NCBI_XOBJMGR_EXPORT CId1Reader : public CReader
{
public:
  CId1Reader(unsigned noConn = 5);
  ~CId1Reader();
  virtual streambuf *SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn = 0);
  virtual CSeqref *RetrieveSeqref(istream &is);

  virtual size_t GetParallelLevel(void) const;
  virtual void SetParallelLevel(size_t);
  virtual void Reconnect(size_t);

protected:
  CConn_ServiceStream *NewID1Service();

private:
  friend class CId1Seqref;
  streambuf *x_SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn);
  vector<CConn_ServiceStream *> m_Pool;
};

END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.10  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.9  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/03/27 20:22:32  butanaev
* Added connection pool.
*
* Revision 1.5  2002/03/26 18:48:31  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.4  2002/03/21 19:14:52  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 01:34:51  kimelman
* gbloader related bugfixes
*
* Revision 1.2  2002/03/20 04:50:35  kimelman
* GB loader added
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
* Revision 1.6  2001/12/13 00:20:55  kimelman
* bugfixes:
*
* Revision 1.5  2001/12/12 21:43:47  kimelman
* more meaningfull Compare
*
* Revision 1.4  2001/12/10 20:07:24  butanaev
* Code cleanup.
*
* Revision 1.3  2001/12/07 21:25:19  butanaev
* Interface development, code beautyfication.
*
* Revision 1.2  2001/12/07 17:02:06  butanaev
* Fixed includes.
*
* Revision 1.1  2001/12/07 16:11:45  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/

#endif // READER_ID1__HPP_INCLUDED
