#ifndef HOOKFUNC__HPP
#define HOOKFUNC__HPP

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


/** @addtogroup HookSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;
class CTypeInfo;
class CMemberInfo;
class CVariantInfo;

typedef void (*TTypeReadFunction)(CObjectIStream& in,
                                  const CTypeInfo* objectType,
                                  TObjectPtr objectPtr);
typedef void (*TTypeWriteFunction)(CObjectOStream& out,
                                   const CTypeInfo* objectType,
                                   TConstObjectPtr objectPtr);
typedef void (*TTypeCopyFunction)(CObjectStreamCopier& copier,
                                  const CTypeInfo* objectType);
typedef void (*TTypeSkipFunction)(CObjectIStream& in,
                                  const CTypeInfo* objectType);

typedef void (*TMemberReadFunction)(CObjectIStream& in,
                                    const CMemberInfo* memberInfo,
                                    TObjectPtr classPtr);
typedef void (*TMemberWriteFunction)(CObjectOStream& out,
                                     const CMemberInfo* memberInfo,
                                     TConstObjectPtr classPtr);
typedef void (*TMemberCopyFunction)(CObjectStreamCopier& copier,
                                    const CMemberInfo* memberInfo);
typedef void (*TMemberSkipFunction)(CObjectIStream& in,
                                    const CMemberInfo* memberInfo);

struct SMemberReadFunctions
{
    SMemberReadFunctions(TMemberReadFunction main = 0,
                         TMemberReadFunction missing = 0)
        : m_Main(main), m_Missing(missing)
        {
        }
    TMemberReadFunction m_Main, m_Missing;
};

struct SMemberSkipFunctions
{
    SMemberSkipFunctions(TMemberSkipFunction main = 0,
                         TMemberSkipFunction missing = 0)
        : m_Main(main), m_Missing(missing)
        {
        }
    TMemberSkipFunction m_Main, m_Missing;
};

struct SMemberCopyFunctions
{
    SMemberCopyFunctions(TMemberCopyFunction main = 0,
                         TMemberCopyFunction missing = 0)
        : m_Main(main), m_Missing(missing)
        {
        }
    TMemberCopyFunction m_Main, m_Missing;
};

typedef void (*TVariantReadFunction)(CObjectIStream& in,
                                     const CVariantInfo* variantInfo,
                                     TObjectPtr classPtr);
typedef void (*TVariantWriteFunction)(CObjectOStream& out,
                                      const CVariantInfo* variantInfo,
                                      TConstObjectPtr classPtr);
typedef void (*TVariantCopyFunction)(CObjectStreamCopier& copier,
                                     const CVariantInfo* variantInfo);
typedef void (*TVariantSkipFunction)(CObjectIStream& in,
                                     const CVariantInfo* variantInfo);

/* @} */


END_NCBI_SCOPE

#endif  /* HOOKFUNC__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2006/10/12 15:07:28  gouriano
* Some header files moved into impl
*
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.6  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.5  2003/04/15 14:50:09  ucko
* Add missing close-comment delimiter!
*
* Revision 1.4  2003/04/15 14:15:18  siyan
* Added doxygen support
*
* Revision 1.3  2003/03/26 16:13:32  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.2  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.1  2000/10/03 17:22:31  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* ===========================================================================
*/
