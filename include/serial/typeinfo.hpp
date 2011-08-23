#ifndef TYPEINFO__HPP
#define TYPEINFO__HPP

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
*   Data type information: members and memory layout
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/impl/hookdata.hpp>
#include <serial/impl/hookfunc.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObject;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CClassTypeInfo;

class CObjectTypeInfo;
class CConstObjectInfo;
class CObjectInfo;

class CReadObjectHook;
class CWriteObjectHook;
class CSkipObjectHook;
class CCopyObjectHook;

class CTypeInfoFunctions;

class CNamespaceInfoItem;
class CObjectMemoryPool;

/// CTypeInfo class contains all information about C++ types (both basic and
/// classes): members and layout in memory.
class NCBI_XSERIAL_EXPORT CTypeInfo
{
protected:
    CTypeInfo(ETypeFamily typeFamily, size_t size);
    CTypeInfo(ETypeFamily typeFamily, size_t size, const char* name);
    CTypeInfo(ETypeFamily typeFamily, size_t size, const string& name);
public:
    // various function pointers
    typedef TObjectPtr (*TTypeCreate)(TTypeInfo objectType,
                                      CObjectMemoryPool* memoryPool);

    virtual ~CTypeInfo(void);

    ETypeFamily GetTypeFamily(void) const;

    /// Get name of this type
    /// @return
    ///   Data type name
    const string& GetName(void) const;

    /// Check if data type has namespace name
    bool HasNamespaceName(void) const;
    /// Get namespace name
    const string& GetNamespaceName(void) const;
    /// Set namespace name
    const CTypeInfo* SetNamespaceName(const string& ns_name) const;
    /// Set namespace 'qualified' property
    const CTypeInfo* SetNsQualified(bool qualified) const;
    /// Get namespace 'qualified' property
    ENsQualifiedMode IsNsQualified(void) const;

    /// Check if data type has namespace prefix
    bool HasNamespacePrefix(void) const;
    /// Get namespace prefix
    const string& GetNamespacePrefix(void) const;
    /// Set namespace prefix
    void SetNamespacePrefix(const string& ns_prefix) const;

    /// Get module name
    virtual const string& GetModuleName(void) const;
    /// Set module name
    void SetModuleName(const string& name);
    /// Set module name
    void SetModuleName(const char* name);

    /// Get size of data object in memory (like sizeof in C)
    size_t GetSize(void) const;

    /// Create object of this type on heap (can be deleted by operator delete)
    TObjectPtr Create(CObjectMemoryPool* memoryPool = 0) const;

    /// Delete object
    virtual void Delete(TObjectPtr object) const;

    // clear object contents so Delete will not leave unused memory allocated
    // note: object contents is not guaranteed to be in initial state
    //       (as after Create), to do so you should call SetDefault after
    virtual void DeleteExternalObjects(TObjectPtr object) const;

    /// Check, whether the object contains default value
    virtual bool IsDefault(TConstObjectPtr object) const = 0;
    /// Check if both objects contain the same values
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                        ESerialRecursionMode how = eRecursive) const = 0;
    /// Set object to default value
    virtual void SetDefault(TObjectPtr dst) const = 0;
    /// Set object to copy of another one
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const = 0;

    /// Check is this TypeInfo object is kind of CClassTypeInfoBase
    bool IsCObject(void) const;
    virtual const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;
    // return true CTypeInfo of object (redefined in polymorphic classes)
    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;

    /// Check if this TypeInfo describes internal unnamed type
    bool IsInternal(void) const;
    /// Return internal type access string e.g. Int-fuzz.range
    const string& GetInternalName(void) const;
    /// Return internal type's owner module name
    const string& GetInternalModuleName(void) const;
    /// Mark this type as internal
    void SetInternalName(const string& name);

    /// Return internal or regular name
    const string& GetAccessName(void) const;
    /// Return internal or regular module name
    const string& GetAccessModuleName(void) const;

    // I/O interface:
    void ReadData(CObjectIStream& in, TObjectPtr object) const;
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
    void CopyData(CObjectStreamCopier& copier) const;
    void SkipData(CObjectIStream& in) const;

    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
    virtual bool IsType(TTypeInfo type) const;

    bool MayContainType(TTypeInfo type) const;

    enum EMayContainType
    {
        eMayContainType_no,
        eMayContainType_recursion, // real value may be yes or no, no caching
        eMayContainType_yes
    };
    EMayContainType IsOrMayContainType(TTypeInfo type) const;
    virtual EMayContainType GetMayContainType(TTypeInfo type) const;

    // hooks
    /// Set global (for all input streams) read hook
    void SetGlobalReadHook(CReadObjectHook* hook);
    /// Set local (for a specific input stream) read hook
    void SetLocalReadHook(CObjectIStream& in, CReadObjectHook* hook);
    /// Reset global read hooks
    void ResetGlobalReadHook(void);
    /// Reset local read hook
    void ResetLocalReadHook(CObjectIStream& in);
    /// Set local context-specific read hook
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadObjectHook* hook);

    /// Set global (for all input streams) write hook
    void SetGlobalWriteHook(CWriteObjectHook* hook);
    /// Set local (for a specific input stream) write hook
    void SetLocalWriteHook(CObjectOStream& out, CWriteObjectHook* hook);
    /// Reset global write hooks
    void ResetGlobalWriteHook(void);
    /// Reset local write hook
    void ResetLocalWriteHook(CObjectOStream& out);
    /// Set local context-specific write hook
    void SetPathWriteHook(CObjectOStream* out, const string& path,
                          CWriteObjectHook* hook);

    /// Set global (for all input streams) skip hook
    void SetGlobalSkipHook(CSkipObjectHook* hook);
    /// Set local (for a specific input stream) skip hook
    void SetLocalSkipHook(CObjectIStream& in, CSkipObjectHook* hook);
    /// Reset global skip hooks
    void ResetGlobalSkipHook(void);
    /// Reset local skip hook
    void ResetLocalSkipHook(CObjectIStream& in);
    /// Set local context-specific skip hook
    void SetPathSkipHook(CObjectIStream* in, const string& path,
                         CSkipObjectHook* hook);

    /// Set global (for all input streams) copy hook
    void SetGlobalCopyHook(CCopyObjectHook* hook);
    /// Set local (for a specific input stream) copy hook
    void SetLocalCopyHook(CObjectStreamCopier& copier, CCopyObjectHook* hook);
    /// Reset global copy hooks
    void ResetGlobalCopyHook(void);
    /// Reset local copy hook
    void ResetLocalCopyHook(CObjectStreamCopier& copier);
    /// Set local context-specific copy hook
    void SetPathCopyHook(CObjectStreamCopier* copier, const string& path,
                         CCopyObjectHook* hook);

    // default methods without checking hook
    void DefaultReadData(CObjectIStream& in, TObjectPtr object) const;
    void DefaultWriteData(CObjectOStream& out, TConstObjectPtr object) const;
    void DefaultCopyData(CObjectStreamCopier& copier) const;
    void DefaultSkipData(CObjectIStream& in) const;

private:
    // private constructors to avoid copying
    CTypeInfo(const CTypeInfo&);
    CTypeInfo& operator=(const CTypeInfo&);

    // type information
    ETypeFamily m_TypeFamily;
    size_t m_Size;
    string m_Name;
    string m_ModuleName;
    mutable CNamespaceInfoItem* m_InfoItem;

protected:
    void SetCreateFunction(TTypeCreate func);
    void SetReadFunction(TTypeReadFunction func);
    void SetWriteFunction(TTypeWriteFunction func);
    void SetCopyFunction(TTypeCopyFunction func);
    void SetSkipFunction(TTypeSkipFunction func);

    bool m_IsCObject;
    bool m_IsInternal;

private:
    // type specific function pointers
    TTypeCreate m_CreateFunction;

    CHookData<CReadObjectHook, TTypeReadFunction> m_ReadHookData;
    CHookData<CWriteObjectHook, TTypeWriteFunction> m_WriteHookData;
    CHookData<CSkipObjectHook, TTypeSkipFunction> m_SkipHookData;
    CHookData<CCopyObjectHook, TTypeCopyFunction> m_CopyHookData;

    friend class CTypeInfoFunctions;

    void x_CreateInfoItemIfNeeded(void) const;
};


/* @} */


#include <serial/impl/typeinfo.inl>

END_NCBI_SCOPE

#endif  /* TYPEINFO__HPP */
