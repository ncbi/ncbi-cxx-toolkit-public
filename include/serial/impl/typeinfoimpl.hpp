#ifndef TYPEINFOIMPL__HPP
#define TYPEINFOIMPL__HPP

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
#include <serial/serialdef.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CVoidTypeFunctions
{
public:
    static TObjectPtr Create(TTypeInfo objectType,
                             CObjectMemoryPool* memPool);

    static bool IsDefault(TConstObjectPtr objectPtr);
    static void SetDefault(TObjectPtr objectPtr);
    static bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                       ESerialRecursionMode how = eRecursive);
    static void Assign(TObjectPtr dst, TConstObjectPtr src,
                       ESerialRecursionMode how = eRecursive);

    static void Read(CObjectIStream& in, TTypeInfo objectType,
                     TObjectPtr objectPtr);
    static void Write(CObjectOStream& out, TTypeInfo objectType,
                      TConstObjectPtr objectPtr);
    static void Copy(CObjectStreamCopier& copier, TTypeInfo objectType);
    static void Skip(CObjectIStream& in, TTypeInfo objectType);

    NCBI_NORETURN
    static void ThrowException(const char* operation, TTypeInfo objectType);
};


/* @} */


END_NCBI_SCOPE

#endif  /* TYPEINFOIMPL__HPP */
