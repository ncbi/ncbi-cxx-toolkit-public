#ifndef OBJECTUTIL__HPP
#define OBJECTUTIL__HPP

/*  $Id$
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
* Author: Michael Kholodov
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/04/06 18:32:31  kholodov
* Modified: renamed to SerialEqual, added type checking.
*
* Revision 1.1  2001/04/04 20:04:31  kholodov
* Added: Equals() function template for comparing two objects
*
*
* ===========================================================================
*/

#include <serial/serialdef.hpp>
#include <serial/objecttype.hpp>

BEGIN_NCBI_SCOPE

// Compares two objects by converting them to binary streams and performing
// byte comparison
enum ESerialCompareMethod
{
  eMatchType,
  ePolymorphic
};

extern bool SerialEqual(TConstObjectPtr a, 
			TConstObjectPtr b, 
			TTypeInfo type);

template <class T>
bool SerialEqual(const T& a, 
		 const T& b, 
		 ESerialCompareMethod method = eMatchType) 
{

  if (method == eMatchType  &&  typeid(a) != typeid(b)) {
    return false;
  }
  
  TTypeInfo type = Type<T>::GetTypeInfo();
  return SerialCompare(&a, &b, type);
}

END_NCBI_SCOPE

#endif  /* OBJECTUTIL__HPP */
