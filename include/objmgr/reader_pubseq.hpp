#ifndef READER_PUBSEQ__HPP_INCLUDED
#define READER_PUBSEQ__HPP_INCLUDED

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
#include <serial/serial.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <objects/objmgr1/reader.hpp>

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/exception.hpp>

#ifdef HAVE_LIBSYBASE
#  include <dbapi/driver/ctlib/interfaces.hpp>
#  define DBContext CTLibContext
#else
#  define CPUBSEQREADER_NO_DB
#  define DBContext int
#endif

#include <vector>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPubseqReader;
class CPubseqBlob;


class CPubseqSeqref : public CSeqref
{
public:
  virtual void Save(ostream &os) const;
  virtual void Restore(istream &is);
  virtual streambuf *BlobStreamBuf(int start, int stop, const CBlobClass &cl, unsigned conn = 0);
  virtual CBlob *RetrieveBlob(istream &is);

  const CIntStreamable::TInt Gi() const { return m_Gi.Value(); }
  const CIntStreamable::TInt Sat() const { return m_Sat.Value(); }
  const CIntStreamable::TInt SatKey() const { return m_SatKey.Value(); }

  virtual CSeqref *Dup() const { return new CPubseqSeqref(*this); }
  virtual char *print(char*,int) const;
  virtual char *printTSE(char*,int) const;
  virtual int Compare(const CSeqref &seqRef,EMatchLevel ml=eSeq) const { return 0; }

private:
  friend class CPubseqReader;
  friend class CPubseqBlob;

  streambuf *x_BlobStreamBuf(int start, int stop, const CBlobClass &cl, unsigned conn);
  CIntStreamable m_Gi;
  CIntStreamable m_Sat;
  CIntStreamable m_SatKey;
  CPubseqReader *m_Reader;
  unsigned m_Conn;
};



class CPubseqReader : public CReader
{
public:
  CPubseqReader(unsigned = 2);
  ~CPubseqReader();
  virtual streambuf *SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn = 0);
  virtual CSeqref *RetrieveSeqref(istream &is);

  virtual int GetParalellLevel(void) const;
  virtual void SetParalellLevel(unsigned);
  virtual void Reconnect(unsigned);

private:
  friend class CPubseqSeqref;
  streambuf *x_SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn);
  CDB_Connection *NewConn();
  DBContext m_Context;
  vector<CDB_Connection *> m_Pool;
};



class CPubseqBlob : public CBlob
{
public:
  CSeq_entry *Seq_entry();

protected:
  friend class CPubseqSeqref;
  CPubseqBlob(istream &is) : CBlob(is) {}

private:

  CRef<CSeq_entry> m_Seq_entry;
  CPubseqSeqref *m_Seqref;
};



END_SCOPE(objects)
END_NCBI_SCOPE


#endif


/*
* $Log$
* Revision 1.2  2002/04/08 23:07:50  vakatov
* #include <vector>
* get rid of the "using ..." directive in the header
*
* Revision 1.1  2002/04/08 20:52:08  butanaev
* Added PUBSEQ reader.
*
* Revision 1.7  2002/04/05 20:53:29  butanaev
* Added code to detect database api availability.
*
* Revision 1.6  2002/04/03 18:37:33  butanaev
* Replaced DBLink with dbapi.
*
* Revision 1.5  2001/12/19 19:42:13  butanaev
* Implemented construction of PUBSEQ blob stream, CPubseqReader family  interfaces.
*
* Revision 1.4  2001/12/14 20:48:08  butanaev
* Implemented fetching Seqrefs from PUBSEQ.
*
* Revision 1.3  2001/12/13 17:50:34  butanaev
* Adjusted for new interface changes.
*
* Revision 1.2  2001/12/12 22:08:58  butanaev
* Removed duplicate code, created frame for CPubseq* implementation.
*
*/
