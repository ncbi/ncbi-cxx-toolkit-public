#ifndef READER__HPP_INCLUDED
#define READER__HPP_INCLUDED
/* */

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
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CStreamable
{
public:
  virtual ~CStreamable() {}
  virtual void Save(ostream &) const = 0;
  virtual void Restore(istream &) = 0;
};

ostream & operator << (ostream &os, const CStreamable &obj);
istream & operator >> (istream &is, CStreamable &obj);

class CIntStreamable : public CStreamable
{
public:
  typedef unsigned long TInt;
  
  CIntStreamable(TInt value = 0) : m_Value(value) {}
  virtual void Save(ostream &) const;
  virtual void Restore(istream &);
  TInt      &Value()       { return m_Value; }
  const TInt Value() const { return m_Value; }
  
private:
  TInt m_Value;
};

class CStringStreamable : public CStreamable
{
public:
  CStringStreamable(const string &value = "") : m_Value(value) {}
  virtual void Save(ostream &) const;
  virtual void Restore(istream &);
  string       &Value()       { return m_Value; }
  const string &Value() const { return m_Value; }

private:
  string m_Value;
};

class CIStream : public istream
{
public:
  CIStream(streambuf *sb) : istream(sb), m_sb(sb) { unsetf(ios_base::skipws); }
  ~CIStream() { delete m_sb; }

  bool Eof();
  static size_t CIStream::Read(istream &is, char* buffer, size_t bufferLength);

  streambuf *m_sb;
};

class CBlobDescr : public CStringStreamable
{};

class CBlobClass : public CIntStreamable
{};

class CSeqrefFlag : public CIntStreamable
{};

class CBlob : public CStreamable
{
public:
  virtual void Save(ostream &) const;
  virtual void Restore(istream &);
  virtual CSeq_entry *Seq_entry() { return NULL; }
  string &Descr() { return m_Descr.Value(); }
  CBlobClass::TInt &Class() { return m_Class.Value(); }

protected:
  CBlob(istream &is) : m_IStream(is) {}
  istream &m_IStream;
private:
  CBlobClass m_Class;
  CBlobDescr m_Descr;
};

class CSeqref : public CStreamable
{
public:
  
  virtual void       Save(ostream &os) const { m_Flag.Save(os); }
  virtual void       Restore(istream &is) { m_Flag.Restore(is); }
  virtual streambuf *BlobStreamBuf(int start, int stop, const CBlobClass &cl) = 0;
  virtual CBlob     *RetrieveBlob(istream &is) = 0;
  virtual CSeqref*   Dup() const = 0;
  virtual char*      print(char*,int)    const = 0;
  virtual char*      printTSE(char*,int) const = 0;

  enum EMatchLevel {
    eContext,
    eTSE    ,
    eSeq
  };
  virtual int Compare(const CSeqref &seqRef,EMatchLevel ml=eSeq) const = 0;
  CSeqrefFlag::TInt &Flag() { return m_Flag.Value(); }
private:
  CSeqrefFlag m_Flag;
};

class CReader
{
public:
  
  virtual ~CReader() {}
  virtual streambuf *SeqrefStreamBuf(const CSeq_id &seqId) = 0;
  virtual CSeqref *RetrieveSeqref(istream &is) = 0;

  // return the level of reasonable parallelism
  // 1 - non MTsafe; 0 - no synchronization required,
  // n - at most n connection is advised/supported
  virtual int  ParalellLevel(void) const;

  // returns the time in secons when already retrived data
  // could become obsolete by fresher version 
  // -1 - never
  virtual CIntStreamable::TInt GetConst(string &const_name) const;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
* $Log$
* Revision 1.9  2002/03/26 20:07:05  coremake
* fake change
*
* Revision 1.8  2002/03/26 19:43:01  butanaev
* Fixed compilation for g++.
*
* Revision 1.7  2002/03/26 18:48:30  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.6  2002/03/26 17:16:59  kimelman
* reader stream fixes
*
* Revision 1.5  2002/03/22 18:49:23  kimelman
* stream fix: WS skipping in binary stream
*
* Revision 1.4  2002/03/21 19:14:52  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 01:34:50  kimelman
* gbloader related bugfixes
*
* Revision 1.2  2002/03/20 04:50:35  kimelman
* GB loader added
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
* Revision 1.5  2001/12/13 00:20:55  kimelman
* bugfixes:
*
* Revision 1.4  2001/12/12 21:42:27  kimelman
* a) int -> unsigned long
* b) bool Compare -> int Compare
*
* Revision 1.3  2001/12/10 20:07:24  butanaev
* Code cleanup.
*
* Revision 1.2  2001/12/07 21:25:19  butanaev
* Interface development, code beautyfication.
*
* Revision 1.1  2001/12/07 16:11:44  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/
