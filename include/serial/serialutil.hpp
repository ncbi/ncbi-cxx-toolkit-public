#ifndef SERIALUTIL__HPP
#define SERIALUTIL__HPP

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
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// helper template for various types:
template<typename T>
class CTypeConverter
{
public:
    typedef T TObjectType; // type of object

    // object getters:
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }

    // set of SafeCast functions which will check validity of casting by
    // dynamic_cast<> in debug mode and will use static_cast<> in release
    // mode for performance.
    static const TObjectType* SafeCast(TTypeInfo type)
        {
            _ASSERT(dynamic_cast<const TObjectType*>(type));
            return static_cast<const TObjectType*>(type);
        }
    static const TObjectType* SafeCast(const CObject* obj)
        {
            _ASSERT(dynamic_cast<const TObjectType*>(obj));
            return static_cast<const TObjectType*>(obj);
        }
    static TObjectType* SafeCast(CObject* obj)
        {
            _ASSERT(dynamic_cast<TObjectType*>(obj));
            return static_cast<TObjectType*>(obj);
        }

private:
    static const TObjectType* SafeCast2(TTypeInfo /*selector*/,
                                        const void* ptr)
        {
            return SafeCast(static_cast<TTypeInfo>(ptr));
        }
    static const TObjectType* SafeCast2(const CObject* /*selector*/,
                                        const void* ptr)
        {
            return SafeCast(static_cast<const CObject*>(ptr));
        }
    static TObjectType* SafeCast2(const CObject* /*selector*/,
                                  void* ptr)
        {
            return SafeCast(static_cast<CObject*>(ptr));
        }
    static const TObjectType* SafeCast2(const void* /*selector*/,
                                        const void* ptr)
        {
            // cannot check types not inherited from CObject or CTypeInfo
            return static_cast<const TObjectType*>(ptr);
        }
    static TObjectType* SafeCast2(const void* /*selector*/,
                                  void* ptr)
        {
            // cannot check types not inherited from CObject or CTypeInfo
            return static_cast<TObjectType*>(ptr);
        }

public:
    static const TObjectType* SafeCast(const void* ptr)
        {
            const T* selector = static_cast<const T*>(0);
            return SafeCast2(selector, ptr);
        }
    static TObjectType* SafeCast(void* ptr)
        {
            const T* selector = static_cast<const T*>(0);
            return SafeCast2(selector, ptr);
        }
};

/* @} */


END_NCBI_SCOPE

#endif  /* SERIALUTIL__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2006/10/12 15:08:28  gouriano
* Some header files moved into impl
*
* Revision 1.6  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.5  2003/12/01 19:04:22  grichenk
* Moved Add and Sub from serialutil to ncbimisc, made them methods
* of CRawPointer class.
*
* Revision 1.4  2003/04/15 16:18:55  siyan
* Added doxygen support
*
* Revision 1.3  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.2  2000/10/13 20:22:46  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.1  2000/09/18 20:00:10  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
