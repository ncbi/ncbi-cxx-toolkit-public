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

#include <objects/objmgr1/reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

ostream & operator << (ostream &os, const CStreamable &obj)
{
  obj.Save(os);
  return os;
}

istream & operator >> (istream &is, CStreamable &obj)
{
  obj.Restore(is);
  return is;
}

void CIntStreamable::Save(ostream &os) const
{
  for(int i=0;i<sizeof(m_Value) ; ++i)
    os << static_cast<char>((m_Value>>(8*i)) & 0xff) ;
}

void CIntStreamable::Restore(istream &is)
{
  m_Value=0;
  for(int i=0;i<sizeof(m_Value) ; ++i)
    {
      unsigned char x;
      is >> x;
      m_Value += static_cast<TInt>(x)<<(8*i);
    }
}

void CStringStreamable::Save(ostream &os) const
{
  CIntStreamable length = m_Value.length();
  os << length;
  os.write(m_Value.c_str(), m_Value.length());
}

void CStringStreamable::Restore(istream &is)
{
  CIntStreamable length;
  is >> length;
  m_Value.resize(length.Value());
  is.read((char *)m_Value.c_str(), length.Value());
}

void CBlob::Save(ostream &os) const
{
  os << m_Class << m_Descr;
}

void CBlob::Restore(istream &is)
{
  is >> m_Class >> m_Descr;
}

size_t CIStream::Read(istream &is, char* buffer, size_t bufferLength)
{
#ifdef NCBI_COMPILER_GCC
  is.read(buffer, bufferLength);
  size_t count = is.gcount();
  if (count  &&  is.eof())
    is.clear();
  return count;

#else
  size_t n = is.readsome(buffer, bufferLength);
  if(n != 0  ||  is.eof())
    return n;
  is.read(buffer, 1);
  if( !is.good() )
    return 0;
  if(bufferLength == 1)
    return 1;
  return is.readsome(buffer + 1, bufferLength - 1) + 1;

#endif
}

bool CIStream::Eof()
{
  char c;
  (*this) >> c;

  if(this->eof())
    return true;

  this->putback(c);
  return false;
}

int CReader::ParalellLevel() const 
{
  return 1;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.1  2002/01/11 19:06:21  gouriano
* restructured objmgr
*
* Revision 1.6  2001/12/13 00:19:25  kimelman
* bugfixes:
*
* Revision 1.5  2001/12/12 21:46:40  kimelman
* Compare interface fix
*
* Revision 1.4  2001/12/10 20:08:01  butanaev
* Code cleanup.
*
* Revision 1.3  2001/12/07 21:24:59  butanaev
* Interface development, code beautyfication.
*
* Revision 1.2  2001/12/07 16:43:58  butanaev
* Fixed includes.
*
* Revision 1.1  2001/12/07 16:10:22  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:22  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/
