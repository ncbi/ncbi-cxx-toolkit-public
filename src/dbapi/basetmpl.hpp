#ifndef _BASETMPL_HPP_
#define _BASETMPL_HPP_

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
* File Description:  Auxiliary templates
*
*
* $Log$
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <exception>
/*
#include <corelib/ncbiobj.hpp>

USING_NCBI_SCOPE;

template<class T>
class CBaseTmpl 
{
public:

  CRef<T>& GetImpl() { return m_impl; }
  const CRef<T>& GetImpl() const { return m_impl; }

protected:
  CBaseTmpl(T* impl) : m_impl(impl) {}
  virtual ~CBaseTmpl() {}

private:
  CRef<T> m_impl;
};
*/

template<class D, class T>
D CastTo(T* t) {
  if( t == 0 )
    throw runtime_error("CastTo(): null source data");
    
  D temp = dynamic_cast<D>(t);
  //if( temp == 0 )
  //throw runtime_error("CastTo(): invalid cast");
  
  return temp;
}

inline
void crash(bool e) 
{
#ifdef _DEBUG
  if( !e ) {
    int *i = 0;
    *i = 5;
  }
#endif
}
//=================================================================
#endif // _ARRAY_HPP_
