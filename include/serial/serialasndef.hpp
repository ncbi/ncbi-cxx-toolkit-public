#ifndef SERIALASNDEF__HPP
#define SERIALASNDEF__HPP

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
*/

#include <corelib/ncbistd.hpp>


/** @addtogroup TypeInfoC
 *
 * @{
 */


struct asnio;
struct asntype;

BEGIN_NCBI_SCOPE

#ifndef ASNCALL
# ifdef HAVE_WINDOWS_H
#  define ASNCALL __stdcall
# else
#  define ASNCALL
# endif
#endif

typedef TObjectPtr (ASNCALL*TAsnNewProc)(void);
typedef TObjectPtr (ASNCALL*TAsnFreeProc)(TObjectPtr);
typedef TObjectPtr (ASNCALL*TAsnReadProc)(asnio*, asntype*);
typedef unsigned char (ASNCALL*TAsnWriteProc)(TObjectPtr, asnio*, asntype*);

NCBI_XSERIAL_EXPORT
TTypeInfo COctetStringTypeInfoGetTypeInfo(void);

NCBI_XSERIAL_EXPORT
TTypeInfo CAutoPointerTypeInfoGetTypeInfo(TTypeInfo type);

NCBI_XSERIAL_EXPORT
TTypeInfo CSetOfTypeInfoGetTypeInfo(TTypeInfo type);

NCBI_XSERIAL_EXPORT
TTypeInfo CSequenceOfTypeInfoGetTypeInfo(TTypeInfo type);

NCBI_XSERIAL_EXPORT
TTypeInfo COldAsnTypeInfoGetTypeInfo(const string& name,
                                     TAsnNewProc newProc,
                                     TAsnFreeProc freeProc,
                                     TAsnReadProc readProc,
                                     TAsnWriteProc writeProc);

END_NCBI_SCOPE


/* @} */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/08/17 14:39:06  dicuccio
* Added export specifiers
*
* Revision 1.3  2003/04/15 16:18:47  siyan
* Added doxygen support
*
* Revision 1.2  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.1  2000/10/13 16:28:32  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* ===========================================================================
*/

#endif  /* SERIALASNDEF__HPP */
