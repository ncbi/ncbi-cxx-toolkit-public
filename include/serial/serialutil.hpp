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


/** @addtogroup GenClassSupport
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
    static TObjectType* GetPointer(TObjectPtr object)
        {
            return static_cast<TObjectType*>(object);
        }
    static const TObjectType* GetPointer(TConstObjectPtr object)
        {
            return static_cast<const TObjectType*>(object);
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
