#if defined(OBJISTRIMPL__HPP)  &&  !defined(OBJISTRIMPL__INL)
#define OBJISTRIMPL__INL

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

#define ClassRandomContentsBegin(classType) \
{ \
    vector<Uint1> read(classType->GetMembers().LastIndex() + 1); \
    BEGIN_OBJECT_FRAME(eFrameClassMember); \
    {

#define ClassRandomContentsMember(Func, Args) \
    { \
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index); \
        SetTopMemberId(memberInfo->GetId()); \
        _ASSERT(index >= kFirstMemberIndex && index <= read.size()); \
        if ( read[index] ) \
            DuplicatedMember(memberInfo); \
        else { \
            read[index] = true; \
            { \
                memberInfo->NCBI_NAME2(Func,Member)Args; \
            } \
        } \
    }

#define ClassRandomContentsEnd(Func, Args) \
    } \
    END_OBJECT_FRAME(); \
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) { \
        if ( !read[*i] ) { \
            classType->GetMemberInfo(i)->NCBI_NAME2(Func,MissingMember)Args; \
        } \
    } \
}

#define ReadClassRandomContentsBegin(classType) \
    ClassRandomContentsBegin(classType)
#define ReadClassRandomContentsMember(classPtr) \
    ClassRandomContentsMember(Read, (*this, classPtr))
#define ReadClassRandomContentsEnd() \
    ClassRandomContentsEnd(Read, (*this, classPtr))

#define SkipClassRandomContentsBegin(classType) \
    ClassRandomContentsBegin(classType)
#define SkipClassRandomContentsMember() \
    ClassRandomContentsMember(Skip, (*this))
#define SkipClassRandomContentsEnd() \
    ClassRandomContentsEnd(Skip, (*this))

#define ClassSequentialContentsBegin(classType) \
{ \
    CClassTypeInfo::CIterator pos(classType); \
    BEGIN_OBJECT_FRAME(eFrameClassMember); \
    {

#define ClassSequentialContentsMember(Func, Args) \
    { \
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index); \
        SetTopMemberId(memberInfo->GetId()); \
        for ( TMemberIndex i = *pos; i < index; ++i ) { \
            classType->GetMemberInfo(i)->NCBI_NAME2(Func,MissingMember)Args; \
        } \
        { \
            memberInfo->NCBI_NAME2(Func,Member)Args; \
        } \
        pos.SetIndex(index + 1); \
    }

#define ClassSequentialContentsEnd(Func, Args) \
    } \
    END_OBJECT_FRAME(); \
    for ( ; pos.Valid(); ++pos ) { \
        classType->GetMemberInfo(pos)->NCBI_NAME2(Func,MissingMember)Args; \
    } \
}

#define ReadClassSequentialContentsBegin(classType) \
    ClassSequentialContentsBegin(classType)
#define ReadClassSequentialContentsMember(classPtr) \
    ClassSequentialContentsMember(Read, (*this, classPtr))
#define ReadClassSequentialContentsEnd(classPtr) \
    ClassSequentialContentsEnd(Read, (*this, classPtr))

#define SkipClassSequentialContentsBegin(classType) \
    ClassSequentialContentsBegin(classType)
#define SkipClassSequentialContentsMember() \
    ClassSequentialContentsMember(Skip, (*this))
#define SkipClassSequentialContentsEnd() \
    ClassSequentialContentsEnd(Skip, (*this))

#endif /* def OBJISTRIMPL__HPP  &&  ndef OBJISTRIMPL__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2005/04/27 16:58:28  vasilche
* Use vector<Uint1> instead of inefficient vector<bool>.
*
* Revision 1.5  2004/01/05 14:24:08  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.4  2003/08/25 15:58:32  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.3  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.2  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.1  2000/09/18 20:00:06  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
