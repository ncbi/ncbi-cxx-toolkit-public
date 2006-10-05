#ifndef OBJISTRIMPL__HPP
#define OBJISTRIMPL__HPP

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
#include <serial/classinfo.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

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

END_NCBI_SCOPE

#endif  /* OBJISTRIMPL__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.4  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2000/12/26 22:23:44  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.2  2000/11/20 17:24:36  vasilche
* Added prototypes of internal functions.
*
* Revision 1.1  2000/09/18 20:00:05  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
