#ifndef SERIALDEF__HPP
#define SERIALDEF__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/12/17 19:04:54  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.1  1999/06/24 14:44:44  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

// forward declaration of two main classes
class CTypeInfo;
class CTypeRef;

// typedef for object references (constant and nonconstant)
typedef void* TObjectPtr;
typedef const void* TConstObjectPtr;

// shortcut typedef: almost everywhere in code we have pointer to const CTypeInfo
typedef const CTypeInfo* TTypeInfo;
typedef TTypeInfo (*TTypeInfoGetter)(void);
typedef TTypeInfo (*TTypeInfoGetter1)(TTypeInfo);
typedef TTypeInfo (*TTypeInfoGetter2)(TTypeInfo, TTypeInfo);

// helper address functions:
// add offset to object reference (to get object's member)
inline
TObjectPtr Add(TObjectPtr object, int offset);
inline
TConstObjectPtr Add(TConstObjectPtr object, int offset);
// calculate offset of member inside object
inline
int Sub(TConstObjectPtr first, TConstObjectPtr second);

#include <serial/serialdef.inl>

END_NCBI_SCOPE

#endif  /* SERIALDEF__HPP */
