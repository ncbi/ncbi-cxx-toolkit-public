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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
//#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <map>
#include <vector>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CMemberId;
class CMemberInfo;
class CWriteObjectList;
class CReadObjectList;

class NCBI_XSERIAL_EXPORT CReadObjectInfo
{
public:
    typedef size_t TObjectIndex;

    CReadObjectInfo(void);
    CReadObjectInfo(TTypeInfo typeinfo);
    CReadObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);
    
    TTypeInfo GetTypeInfo(void) const;
    TObjectPtr GetObjectPtr(void) const;

    void ResetObjectPtr(void);
    void Assign(TObjectPtr objectPtr, TTypeInfo typeInfo);

private:
    TTypeInfo m_TypeInfo;
    TObjectPtr m_ObjectPtr;
    CConstRef<CObject> m_ObjectRef;
};

class NCBI_XSERIAL_EXPORT CReadObjectList
{
public:
    typedef CReadObjectInfo::TObjectIndex TObjectIndex;

    CReadObjectList(void);
    ~CReadObjectList(void);

    TObjectIndex GetObjectCount(void) const;

protected:
    friend class CObjectIStream;

    void Clear(void);
    void ForgetObjects(TObjectIndex from, TObjectIndex to);

    const CReadObjectInfo& GetRegisteredObject(TObjectIndex index) const;

    void RegisterObject(TTypeInfo typeInfo);
    void RegisterObject(TObjectPtr objectPtr, TTypeInfo typeInfo);

private:
    vector<CReadObjectInfo> m_Objects;
};

class NCBI_XSERIAL_EXPORT CWriteObjectInfo {
public:
    typedef size_t TObjectIndex;

    CWriteObjectInfo(void);
    CWriteObjectInfo(TTypeInfo typeInfo, TObjectIndex index);
    CWriteObjectInfo(TConstObjectPtr objectPtr,
                     TTypeInfo typeInfo, TObjectIndex index);

    TObjectIndex GetIndex(void) const;

    TTypeInfo GetTypeInfo(void) const;
    TConstObjectPtr GetObjectPtr(void) const;
    const CConstRef<CObject>& GetObjectRef(void) const;

    void ResetObjectPtr(void);

private:
    TTypeInfo m_TypeInfo;
    TConstObjectPtr m_ObjectPtr;
    CConstRef<CObject> m_ObjectRef;

    TObjectIndex m_Index;
};

class NCBI_XSERIAL_EXPORT CWriteObjectList
{
public:
    typedef CWriteObjectInfo::TObjectIndex TObjectIndex;

    CWriteObjectList(void);
    ~CWriteObjectList(void);

    TObjectIndex GetObjectCount(void) const;
    TObjectIndex NextObjectIndex(void) const;

protected:
    friend class CObjectOStream;

    // check that all objects marked as written
    void Clear(void);

    // add object to object list
    // may throw an exception if there is error in objects placements
    void RegisterObject(TTypeInfo typeInfo);
    const CWriteObjectInfo* RegisterObject(TConstObjectPtr object,
                                           TTypeInfo typeInfo);

    void MarkObjectWritten(CWriteObjectInfo& info);

    // forget pointers of written object (e.g. because we want to delete them)
    void ForgetObjects(TObjectIndex from, TObjectIndex to);

private:
    // we need reverse order map due to faster algorithm of lookup
    typedef vector<CWriteObjectInfo> TObjects;
    typedef map<TConstObjectPtr, TObjectIndex> TObjectsByPtr;

    TObjects m_Objects;           // registered objects
    TObjectsByPtr m_ObjectsByPtr; // registered objects by pointer
};


/* @} */


#include <serial/impl/objlist.inl>

END_NCBI_SCOPE

#endif  /* OBJLIST__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.17  2003/04/15 16:18:26  siyan
* Added doxygen support
*
* Revision 1.16  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.15  2000/10/17 18:45:25  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.14  2000/10/13 20:59:12  vasilche
* Avoid using of ssize_t absent on some compilers.
*
* Revision 1.13  2000/10/13 20:22:46  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
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
