#ifndef OBJLIST__HPP
#define OBJLIST__HPP

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
* Revision 1.12  2000/09/01 13:16:01  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.11  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.9  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.8  2000/03/29 15:55:21  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.7  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  1999/10/04 16:22:09  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.5  1999/09/14 18:54:04  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.4  1999/06/30 16:04:30  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.3  1999/06/24 14:44:40  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/10 21:06:39  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.1  1999/06/07 19:30:16  vasilche
* More bug fixes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/object.hpp>
#include <map>
#include <vector>

BEGIN_NCBI_SCOPE

class CMemberId;
class CMemberInfo;
class COObjectList;

class CWriteObjectInfo {
public:
    typedef int TObjectIndex;
    enum {
        eObjectIndexNotWritten = -1
    };

    CWriteObjectInfo(const CConstObjectInfo& object)
        : m_Object(object), m_Index(eObjectIndexNotWritten)
        {
        }
    CWriteObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo)
        : m_Object(objectPtr, typeInfo), m_Index(eObjectIndexNotWritten)
        {
        }

    bool IsWritten(void) const
        {
            return m_Index != eObjectIndexNotWritten;
        }
    TObjectIndex GetIndex(void) const
        {
            return m_Index;
        }

    const CConstObjectInfo& GetObject(void) const
        {
            return m_Object;
        }

    TTypeInfo GetTypeInfo(void) const
        {
            return GetObject().GetTypeInfo();
        }

private:
    friend class COObjectList;

    CConstObjectInfo m_Object;
    TObjectIndex m_Index;
};

class COObjectList
{
public:
    typedef int TObjectIndex;

    COObjectList(void);
    ~COObjectList(void);

    // check that all objects marked as written
    void CheckAllWritten(void) const;
    void Clear(void);

    size_t GetWrittenObjectCount(void) const;

protected:
    friend class CObjectOStream;

    // add object to object list
    // may throw an exception if there is error in objects placements
    CWriteObjectInfo& RegisterObject(TTypeInfo typeInfo);
    CWriteObjectInfo& RegisterObject(TConstObjectPtr object,
                                     TTypeInfo typeInfo);
    CWriteObjectInfo& RegisterObject(const CConstObjectInfo& object)
        {
            return RegisterObject(object.GetObjectPtr(), object.GetTypeInfo());
        }

    void MarkObjectWritten(CWriteObjectInfo& info);

    // forget pointers of written object (e.g. because we want to delete them)
    void ForgetObjects(size_t from, size_t to);

private:
    // we need reverse order map due to faster algorithm of lookup
    typedef vector< CWriteObjectInfo > TObjects;
    typedef map<TConstObjectPtr, TObjectIndex > TObjectsByPtr;

    TObjects m_Objects;           // registered objects
    TObjectsByPtr m_ObjectsByPtr; // registered objects by pointer
};

#include <serial/objlist.inl>

END_NCBI_SCOPE

#endif  /* OBJLIST__HPP */
