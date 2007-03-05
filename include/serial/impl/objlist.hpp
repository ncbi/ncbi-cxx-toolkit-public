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
