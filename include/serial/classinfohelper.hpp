#ifndef CLASSINFOHELPER__HPP
#define CLASSINFOHELPER__HPP

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
#include <serial/serialbase.hpp>
#include <serial/typeinfoimpl.hpp>
#include <typeinfo>


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CClassTypeInfoBase;
class CClassTypeInfo;
class CChoiceTypeInfo;

// these methods are external to avoid inclusion of big headers
class NCBI_XSERIAL_EXPORT CClassInfoHelperBase
{
protected:
    typedef const type_info* (*TGetTypeIdFunction)(TConstObjectPtr object);
    typedef CTypeInfo::TTypeCreate TCreateFunction; 
    typedef TMemberIndex (*TWhichFunction)(const CChoiceTypeInfo* choiceType,
                                           TConstObjectPtr choicePtr);
    typedef void (*TResetFunction)(const CChoiceTypeInfo* choiceType,
                                   TObjectPtr choicePtr);
    typedef void (*TSelectFunction)(const CChoiceTypeInfo* choiceType,
                                    TObjectPtr choicePtr,
                                    TMemberIndex index);
    typedef void (*TSelectDelayFunction)(TObjectPtr object,
                                         TMemberIndex index);

    static CChoiceTypeInfo* CreateChoiceInfo(const char* name, size_t size,
                                             const void* nonCObject,
                                             TCreateFunction createFunc,
                                             const type_info& ti,
                                             TWhichFunction whichFunc,
                                             TSelectFunction selectFunc,
                                             TResetFunction resetFunc);
    static CChoiceTypeInfo* CreateChoiceInfo(const char* name, size_t size,
                                             const CObject* cObject,
                                             TCreateFunction createFunc,
                                             const type_info& ti,
                                             TWhichFunction whichFunc,
                                             TSelectFunction selectFunc,
                                             TResetFunction resetFunc);

public:
#if HAVE_NCBI_C
    static CChoiceTypeInfo* CreateAsnChoiceInfo(const char* name);
    static CClassTypeInfo* CreateAsnStructInfo(const char* name, size_t size,
                                               const type_info& id);
#endif
    
protected:
    static CClassTypeInfo* CreateClassInfo(const char* name, size_t size,
                                           const void* nonCObject,
                                           TCreateFunction createFunc,
                                           const type_info& id,
                                           TGetTypeIdFunction func);
    static CClassTypeInfo* CreateClassInfo(const char* name, size_t size,
                                           const CObject* cObject,
                                           TCreateFunction createFunc,
                                           const type_info& id,
                                           TGetTypeIdFunction func);
};

// template collecting all helper methods for generated classes
template<class C>
class CClassInfoHelper : public CClassInfoHelperBase
{
    typedef CClassInfoHelperBase CParent;
public:
    typedef C CClassType;

    static CClassType& Get(void* object)
        {
            return *static_cast<CClassType*>(object);
        }
    static const CClassType& Get(const void* object)
        {
            return *static_cast<const CClassType*>(object);
        }

    static void* Create(TTypeInfo /*typeInfo*/)
        {
            return new CClassType();
        }
    static void* CreateCObject(TTypeInfo /*typeInfo*/)
        {
            return new CClassType();
        }


    static const type_info* GetTypeId(const void* object)
        {
            const CClassType& x = Get(object);
            return &typeid(x);
        }

    enum EGeneratedChoiceValues {
        eGeneratedChoiceEmpty = 0,
        eGeneratedChoiceToMemberIndex = kEmptyChoice - eGeneratedChoiceEmpty,
        eMemberIndexToGeneratedChoice = eGeneratedChoiceEmpty - kEmptyChoice
    };

    static TMemberIndex WhichChoice(const CChoiceTypeInfo* /*choiceType*/,
                                    const void* choicePtr)
        {
            return static_cast<TMemberIndex>(Get(choicePtr).Which())
                + eGeneratedChoiceToMemberIndex;
        }
    static void ResetChoice(const CChoiceTypeInfo* choiceType,
                            void* choicePtr)
        {
            if ( WhichChoice(choiceType, choicePtr) != kEmptyChoice )
                Get(choicePtr).Reset();
        }
    static void SelectChoice(const CChoiceTypeInfo* choiceType,
                             void* choicePtr,
                             TMemberIndex index)
        {
            typedef typename CClassType::E_Choice E_Choice;
            if (WhichChoice(choiceType,choicePtr) != index) {
                Get(choicePtr).Select(E_Choice(index + eMemberIndexToGeneratedChoice));
            }
        }
    static void SelectDelayBuffer(void* choicePtr,
                                  TMemberIndex index)
        {
            typedef typename CClassType::E_Choice E_Choice;
            Get(choicePtr).SelectDelayBuffer(E_Choice(index + eMemberIndexToGeneratedChoice));
        }

    static void SetReadWriteMethods(NCBI_NS_NCBI::CClassTypeInfo* info)
        {
            const CClassType* object = 0;
            NCBISERSetPreRead(object, info);
            NCBISERSetPostRead(object, info);
            NCBISERSetPreWrite(object, info);
            NCBISERSetPostWrite(object, info);
        }
    static void SetReadWriteMethods(NCBI_NS_NCBI::CChoiceTypeInfo* info)
        {
            const CClassType* object = 0;
            NCBISERSetPreRead(object, info);
            NCBISERSetPostRead(object, info);
            NCBISERSetPreWrite(object, info);
            NCBISERSetPostWrite(object, info);
        }

    static CClassTypeInfo* CreateAbstractClassInfo(const char* name)
        {
            const CClassType* object = 0;
            CClassTypeInfo* info =
                CParent::CreateClassInfo(name, sizeof(CClassType),
                                         object, &CVoidTypeFunctions::Create,
                                         typeid(CClassType), &GetTypeId);
            SetReadWriteMethods(info);
            return info;
        }

    static CClassTypeInfo* CreateClassInfo(const char* name)
        {
            const CClassType* object = 0;
            CClassTypeInfo* info = CreateClassInfo(name, object);
            SetReadWriteMethods(info);
            return info;
        }

    static CChoiceTypeInfo* CreateChoiceInfo(const char* name)
        {
            const CClassType* object = 0;
            CChoiceTypeInfo* info = CreateChoiceInfo(name, object);
            SetReadWriteMethods(info);
            return info;
        }
#ifdef HAVE_NCBI_C
    static CClassTypeInfo* CreateAsnStructInfo(const char* name)
        {
            return CParent::CreateAsnStructInfo(name,
                                                sizeof(CClassType),
                                                typeid(CClassType));
        }
#endif

private:
    static CClassTypeInfo* CreateClassInfo(const char* name,
                                           const void* nonCObject)
        {
            return CParent::CreateClassInfo(name, sizeof(CClassType),
                                            nonCObject, &Create,
                                            typeid(CClassType), &GetTypeId);
        }
    static CClassTypeInfo* CreateClassInfo(const char* name,
                                           const CObject* cObject)
        {
            return CParent::CreateClassInfo(name, sizeof(CClassType),
                                            cObject, &CreateCObject,
                                            typeid(CClassType), &GetTypeId);
        }
    static CChoiceTypeInfo* CreateChoiceInfo(const char* name,
                                             const void* nonCObject)
        {
            return CParent::CreateChoiceInfo(name, sizeof(CClassType),
                                             nonCObject, &Create, 
                                             typeid(CClassType),
                                             &WhichChoice,
                                             &SelectChoice, &ResetChoice);
        }
    static CChoiceTypeInfo* CreateChoiceInfo(const char* name,
                                             const CObject* cObject)
        {
            return CParent::CreateChoiceInfo(name, sizeof(CClassType),
                                             cObject, &CreateCObject, 
                                             typeid(CClassType),
                                             &WhichChoice,
                                             &SelectChoice, &ResetChoice);
        }
};


/* @} */


//#include <serial/classinfohelper.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFOHELPER__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2005/02/24 14:38:44  gouriano
* Added PreRead/PostWrite hooks
*
* Revision 1.11  2004/05/06 13:26:35  ucko
* Conditionalize CClassInfoHelper<>::CreateAsnStructInfo on
* HAVE_NCBI_C, since it otherwise has no base method to call.
*
* Revision 1.10  2004/04/26 16:40:59  ucko
* Tweak for GCC 3.4 compatibility.
*
* Revision 1.9  2004/01/27 17:07:35  ucko
* CClassInfoHelper::GetTypeId: disambiguate for IBM VisualAge C++.
*
* Revision 1.8  2003/04/15 14:15:00  siyan
* Added doxygen support
*
* Revision 1.7  2003/04/10 20:13:37  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.5  2003/03/27 19:26:48  ucko
* WhichChoice(): cast Which() to TMemberIndex.  (Avoids new WorkShop warnings.)
*
* Revision 1.4  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2002/12/12 21:10:26  gouriano
* implemented handling of complex XML containers
*
* Revision 1.2  2000/11/01 20:35:27  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.1  2000/10/13 16:28:29  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* ===========================================================================
*/
