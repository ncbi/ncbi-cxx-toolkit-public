#ifndef ASNTYPES__HPP
#define ASNTYPES__HPP

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

#if HAVE_NCBI_C

#include <serial/typeinfo.hpp>
#include <serial/serialutil.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/stdtypes.hpp>
#include <serial/impl/serialasndef.hpp>


/** @addtogroup TypeInfoC
 *
 * @{
 */


struct valnode;
struct bytestore;
struct asnio;
struct asntype;

BEGIN_NCBI_SCOPE

class CSequenceOfTypeInfo : public CContainerTypeInfo {
    typedef CContainerTypeInfo CParent;
public:
    typedef TObjectPtr TObjectType;

    CSequenceOfTypeInfo(TTypeInfo type, bool randomOrder = false);
    CSequenceOfTypeInfo(const char* name,
                        TTypeInfo type, bool randomOrder = false);
    CSequenceOfTypeInfo(const string& name,
                        TTypeInfo type, bool randomOrder = false);

    size_t GetNextOffset(void) const
        {
            return m_NextOffset;
        }
    size_t GetDataOffset(void) const
        {
            return m_DataOffset;
        }
    
    static TObjectPtr& FirstNode(TObjectPtr object)
        {
            return CTypeConverter<TObjectPtr>::Get(object);
        }
    static TObjectPtr FirstNode(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectPtr>::Get(object);
        }
    TObjectPtr& NextNode(TObjectPtr object) const
        {
            return CTypeConverter<TObjectPtr>::Get
                (CRawPointer::Add(object, m_NextOffset));
        }
    TObjectPtr NextNode(TConstObjectPtr object) const
        {
            return CTypeConverter<TObjectPtr>::Get
                (CRawPointer::Add(object, m_NextOffset));
        }
    TObjectPtr Data(TObjectPtr object) const
        {
            return CRawPointer::Add(object, m_DataOffset);
        }
    TConstObjectPtr Data(TConstObjectPtr object) const
        {
            return CRawPointer::Add(object, m_DataOffset);
        }

    static TTypeInfo GetTypeInfo(TTypeInfo base);
    static CTypeInfo* CreateTypeInfo(TTypeInfo base);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    TObjectPtr CreateNode(void) const;
    void DeleteNode(TObjectPtr node) const;

private:
    void InitSequenceOfTypeInfo(void);

    // set this sequence to have ValNode as data holder
    // (used for SET OF (INTEGER, STRING, SET OF etc.)
    void SetValNodeNext(void);
    // SET OF CHOICE (use choice's valnode->next field as link)
    void SetChoiceNext(void);

private:
    size_t m_NextOffset;  // offset in struct of pointer to next object (def 0)
    size_t m_DataOffset;  // offset in struct of data struct (def 0)
};

class CSetOfTypeInfo : public CSequenceOfTypeInfo {
    typedef CSequenceOfTypeInfo CParent;
public:
    CSetOfTypeInfo(TTypeInfo type);
    CSetOfTypeInfo(const char* name, TTypeInfo type);
    CSetOfTypeInfo(const string& name, TTypeInfo type);

    static TTypeInfo GetTypeInfo(TTypeInfo base);
    static CTypeInfo* CreateTypeInfo(TTypeInfo base);
};

class COctetStringTypeInfo : public CPrimitiveTypeInfo {
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef bytestore* TObjectType;

    COctetStringTypeInfo(void);

    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    static TTypeInfo GetTypeInfo(void);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    virtual void GetValueOctetString(TConstObjectPtr objectPtr,
                                     vector<char>& value) const;
    virtual void SetValueOctetString(TObjectPtr objectPtr,
                                     const vector<char>& value) const;
protected:
    
    static void ReadOctetString(CObjectIStream& in,
                                TTypeInfo objectType,
                                TObjectPtr objectPtr);
    static void WriteOctetString(CObjectOStream& out,
                                 TTypeInfo objectType,
                                 TConstObjectPtr objectPtr);
    static void SkipOctetString(CObjectIStream& in,
                                TTypeInfo objectType);
    static void CopyOctetString(CObjectStreamCopier& copier,
                                TTypeInfo objectType);
};

class COldAsnTypeInfo : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef void* TObjectType;

    COldAsnTypeInfo(const char* name,
                    TAsnNewProc newProc, TAsnFreeProc freeProc,
                    TAsnReadProc readProc, TAsnWriteProc writeProc);
    COldAsnTypeInfo(const string& name,
                    TAsnNewProc newProc, TAsnFreeProc freeProc,
                    TAsnReadProc readProc, TAsnWriteProc writeProc);

    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                 ESerialRecursionMode how = eRecursive) const;

protected:
    
    static void ReadOldAsnStruct(CObjectIStream& in,
                                 TTypeInfo objectType,
                                 TObjectPtr objectPtr);
    static void WriteOldAsnStruct(CObjectOStream& out,
                                  TTypeInfo objectType,
                                  TConstObjectPtr objectPtr);

private:
    TAsnNewProc m_NewProc;
    TAsnFreeProc m_FreeProc;
    TAsnReadProc m_ReadProc;
    TAsnWriteProc m_WriteProc;
};

END_NCBI_SCOPE

/* @} */

#endif  /* HAVE_NCBI_C */

#endif  /* ASNTYPES__HPP */
