#ifndef _ARRAY_HPP_
#define _ARRAY_HPP_

/* $Id$
* ===========================================================================
*
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
*
* ===========================================================================
*
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  Array template class with bounds check
*
*
* $Log$
* Revision 1.2  2002/02/06 17:48:43  kholodov
* Added correct virtual destructor for GCC 3.0.3
*
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <exception>
#include <string>

using namespace std;

class CArrayOutOfBoundsException : public exception 
{

public:
  CArrayOutOfBoundsException(const string& msg);
  virtual ~CArrayOutOfBoundsException() throw();
  
  virtual const char* what() const throw();

private:
  string m_msg;
};

//=================================================================

template <class T>
class CArray 
{
public:
  // Constructors/destructors
  CArray(unsigned int size, const T& v)  
    : m_start(0), m_end(0)
  {
    if( size > 0 ) {
      m_start = static_cast<T*>(::operator new(size * sizeof(T)));
      uninitialized_fill_n(m_start, size, v);
      m_end = m_start + size;
    }
  }

  CArray(T* buf, unsigned int size) 
    : m_start(0), m_end(0)
  {
    if( size > 0 ) {
      m_start = static_cast<T*>(::operator new(size * sizeof(T)));
      m_end = (buf == 0 ? m_start : uninitialized_copy(buf, 
						       buf+size, 
						       m_start));
    }
  }    

  CArray(const CArray<T>& x) 
    : m_start(0), m_end(0)
  {
    if( x.GetSize() > 0 ) {
      m_start = ::operator new (x.GetSize());
      m_end = uninitialized_copy(x.m_start, x.m_end, m_start);
    }
  }
    
  ~CArray() 
  { 
    for( T* p = m_start; p != m_end; ++p )
      p->~T();

    ::operator delete(m_start); 
  }

  // Getters
  int GetSize() const {
    return m_end - m_start;
  }

  T* GetBuffer() const { return m_start; } 

  T& elementAt(unsigned int idx) const { 
    if( idx >= GetSize() )
      throw CArrayOutOfBoundsException("CArray::elementAt(): Wrong array index");

    return m_start[idx]; 
  }

    
  // operators

  CArray<T>& operator=(const CArray<T>& array) {
    
    CArray<T> temp(array);
    Swap(temp);
    return *this;
  }
    
  T& operator[](unsigned int idx) {
    return elementAt(idx);
  }

  const T& operator[](unsigned int idx) const {
    return elementAt(idx);
  }

protected:

  void Swap(CArray<T>& x) throw () {
    swap(m_start, x.m_start);
    swap(m_end, x.m_end);
  }

private:
  T *m_start;
  T *m_end;
};


typedef CArray<unsigned char> CByteArray;

//====================================================================
template <class T>
class CDynArray 
{
public:
  // Constructors/destructors
  CDynArray()  
    : m_start(0), m_finish(0), m_end(0)
  {}

  CDynArray(unsigned int initSize, const T& v)  
  {
    T* temp = static_cast<T*>(::operator new(initSize * sizeof(T)));
    try {
      uninitialized_fill_n(temp, initSize, v);
    }
    catch(...) {
      ::operator delete(temp);
      throw;
    }
    m_start = temp;
    m_finish = m_start + initSize;
    m_end = m_finish;
  }

  CDynArray(T* buf, unsigned int size, unsigned int capacity = 0) 
  {
    unsigned int temp = size > capacity ? size : capacity;

    m_start = static_cast<T*>(::operator new(temp * sizeof(T)));
    m_finish = (buf == 0 ? m_start : uninitialized_copy(buf, 
							buf+size, 
							m_start));
    m_end = m_start + temp;
  }    

  CDynArray(const CDynArray<T>& x) 
  {
    m_start = static_cast<T*>(::operator new(x.m_finish - x.m_start));
    m_finish = uninitialized_copy(x.m_start, x.m_finish-x.m_start, m_start);
    m_end = m_finish;
  }
    
  ~CDynArray() 
  { 
    for( T* p = m_finish; p != m_start; )
      (--p)->~T();

    ::operator delete(m_start); 
  }

  // Getters
  unsigned int GetSize() const {
    return m_finish - m_start;
  }

  T* GetBuffer() const { return m_start; } 

  T& elementAt(unsigned int idx) const { 
    if( idx >= GetSize() )
      throw CArrayOutOfBoundsException("CDynArray::elementAt(): Wrong array index");

    return m_start[idx]; 
  }


  // adding
  void Add(const T& v) {

    if( m_finish != m_end ) {
      new (m_finish) T(v);
      ++m_finish;
    }
    else {
      AddAux(v);
    }
  }
    

  // operators
  CDynArray<T>& operator=(const CDynArray<T>& array) {
    
    CDynArray<T> temp(array);
    Swap(temp);
    return *this;
  }
    
  T& operator[](unsigned int idx) {
    return elementAt(idx);
  }

  const T& operator[](unsigned int idx) const {
    return elementAt(idx);
  }

protected:

  void Swap(CDynArray<T>& array) throw () {
    swap(m_start, array.m_start);
    swap(m_finish, array.m_finish);
    swap(m_end, array.m_end);
  }

  void AddAux(const T& v) {
    
    unsigned int newCapacity = (m_end - m_start) * 2 + 1;

    CDynArray a(m_start, GetSize(), newCapacity);
    a.Add(v);
    Swap(a);

  } 

private:
  T *m_start;
  T *m_finish;
  T *m_end;

};


typedef CDynArray<unsigned char> CByteDynArray;
#endif // _ARRAY_HPP_
