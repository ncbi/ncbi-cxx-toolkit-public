#ifndef SERIAL__HPP
#define SERIAL__HPP

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
*   Serialization classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/03/25 19:11:58  vasilche
* Beginning of serialization library.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <list>
#include <map>
#include <typeinfo>

BEGIN_NCBI_SCOPE

class CArchiver;
class CTypeInfo;
class CClassInfo;
class CMemberInfo;

// this macro is used to do something for all basic C types
#define FOR_ALL_STD_TYPES \
PROCESS_STD_TYPE(char) \
PROCESS_STD_TYPE(unsigned char) \
PROCESS_STD_TYPE(signed char) \
PROCESS_STD_TYPE(short) \
PROCESS_STD_TYPE(unsigned short) \
PROCESS_STD_TYPE(int) \
PROCESS_STD_TYPE(unsigned int) \
PROCESS_STD_TYPE(long) \
PROCESS_STD_TYPE(unsigned long) \
PROCESS_STD_TYPE(float) \
PROCESS_STD_TYPE(double)

class CArchiver {
public:
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;

    CArchiver(void);
    virtual ~CArchiver(void);

#define PROCESS_STD_TYPE(TYPE) \
\
    virtual void ReadBasic(TYPE& data); \
    virtual void WriteBasic(const TYPE& data);

    FOR_ALL_STD_TYPES

#undef PROCESS_STD_TYPE

    // you should redefine at least one of each pair:
    //     ReadString/ReadCString and WriteString/WriteCString
    // default implementation of each method uses second method of pair
    virtual void ReadString(string& str);
    virtual void WriteString(const string& str);
    virtual void ReadCString(char*& str);
    virtual void WriteCString(const char* const& str);

    void ReadClass(TObjectPtr object, const CClassInfo& info);
    void WriteClass(TConstObjectPtr object, const CClassInfo& info);

protected:
    virtual void ReadObjectBegin(const CClassInfo& info);
    virtual void ReadMemberSeparator(void);
    virtual void ReadObjectEnd(const CClassInfo& info);
    virtual void WriteObjectBegin(const CClassInfo& info);
    virtual void WriteMemberSeparator(void);
    virtual void WriteObjectEnd(const CClassInfo& info);

    virtual string ReadMemberName(void);
    virtual void WriteMemberName(const string& name);
    virtual void SkipMemberValue(const CClassInfo& info, const string& name);
};

class CTypeInfo {
public:
    typedef CArchiver::TObjectPtr TObjectPtr;
    typedef CArchiver::TConstObjectPtr TConstObjectPtr;

    CTypeInfo(void);
    virtual ~CTypeInfo(void);

    // creates object of this type in heap (can be deleted by operator delete)
    virtual void* Create(void) const = 0;

    // read/write object
    //     it would be better to have object argument of type void
    //     but it is impossible in C++ to have reference to void :(
    virtual void Read(CArchiver& archiver, TObjectPtr object) const = 0;
    virtual void Write(CArchiver& archiver, TConstObjectPtr obejct) const = 0;

    // is this object has default value (can be skipped in write)
    virtual bool HasDefaultValue(TConstObjectPtr object) const;

    // returns type info of pointer to this type
    const CTypeInfo& GetPointerTypeInfo(void) const;

private:
    mutable auto_ptr<CTypeInfo> m_PointerTypeInfo;
};

template<class TYPE>
class CStdTypeInfoTmpl : public CTypeInfo {
public:
    typedef TYPE TObjectType;

    static TObjectType& Get(TObjectPtr object)
        { return *static_cast<TObjectType*>(object); }
    static const TObjectType& Get(TConstObjectPtr object)
        { return *static_cast<const TObjectType*>(object); }
};

template<class TYPE>
class CStdTypeInfo : public CStdTypeInfoTmpl<TYPE> {
public:
    virtual void* Create(void) const
        { return new TObjectType(0); }

    virtual void Read(CArchiver& archiver, TObjectPtr object) const
        { archiver.ReadBasic(Get(object)); }
    virtual void Write(CArchiver& archiver, TConstObjectPtr obejct) const
        { archiver.WriteBasic(Get(object)); }

    virtual bool HasDefaultValue(TConstObjectPtr object) const
        { return Get(object) == TObjectType(0); }

    static const CStdTypeInfo sm_TypeInfo;
};

template<>
class CStdTypeInfo<void> : public CTypeInfo {
public:
    virtual void* Create(void) const
        { throw runtime_error("void cannot be created"); }

    virtual void Read(CArchiver& , TObjectPtr ) const
        { throw runtime_error("void cannot be read"); }
    virtual void Write(CArchiver& , TConstObjectPtr ) const
        { throw runtime_error("void cannot be written"); }

    virtual bool HasDefaultValue(TConstObjectPtr ) const
        { throw runtime_error("void cannot be checked"); }

    static const CStdTypeInfo sm_TypeInfo;
};

template<>
class CStdTypeInfo<string> : public CStdTypeInfoTmpl<string> {
public:
    virtual void* Create(void) const
        { return new TObjectType; }

    virtual void Read(CArchiver& archiver, TObjectPtr object) const
        { archiver.ReadString(Get(object)); }
    virtual void Write(CArchiver& archiver, TConstObjectPtr object) const
        { archiver.WriteString(Get(object)); }

    virtual bool HasDefaultValue(TConstObjectPtr object) const
        { return Get(object).empty(); }

    static const CStdTypeInfo sm_TypeInfo;
};

template<>
class CStdTypeInfo<char*> : public CStdTypeInfoTmpl<char*> {
public:
    virtual void* Create(void) const
        { return new TObjectType(0); }

    virtual void Read(CArchiver& archiver, TObjectPtr object) const
        { archiver.ReadCString(Get(object)); }
    virtual void Write(CArchiver& archiver, TConstObjectPtr object) const
        { archiver.WriteCString(Get(object)); }

    virtual bool HasDefaultValue(TConstObjectPtr object) const
        { return Get(object) == 0; }

    static const CStdTypeInfo sm_TypeInfo;
};

inline const CTypeInfo& GetTypeInfo(void)
{
    return CStdTypeInfo<void>::sm_TypeInfo;
}

class CPointerTypeInfo : public CStdTypeInfoTmpl<void*> {
public:
    CPointerTypeInfo(const CTypeInfo& objectTypeInfo)
        : m_ObjectTypeInfo(objectTypeInfo)
        { }
    virtual void* Create(void) const;

    virtual void Read(CArchiver& archiver, TObjectPtr object) const;
    virtual void Write(CArchiver& archiver, TConstObjectPtr obejct) const;

    virtual bool HasDefaultValue(TConstObjectPtr object) const;

    const CTypeInfo& GetObjectTypeInfo(void) const
        { return m_ObjectTypeInfo; }

private:
    const CTypeInfo& m_ObjectTypeInfo;
};

template<>
class less<const type_info*>
{
public:
    bool operator() (const type_info* x, const type_info* y) const
        { return x->before(*y); }
};

class CClassInfo : public CTypeInfo {
public:
    typedef map<const type_info*, CClassInfo*> TTypeByType;
    typedef map<string, CMemberInfo*> TMembers;
    typedef TMembers::const_iterator TMemberIterator;

    CClassInfo(const type_info& info, const CClassInfo* parent,
               void* (*creator)());
    virtual ~CClassInfo(void);

    virtual void* Create(void) const;

    virtual void Read(CArchiver& archiver, TObjectPtr object) const;
    virtual void Write(CArchiver& archiver, TConstObjectPtr obejct) const;

    virtual bool HasDefaultValue(TConstObjectPtr object) const;

    static const CClassInfo& GetTypeInfo(const type_info& info);

    const CMemberInfo* FindMember(const string& name) const;
    TMemberIterator MemberBegin(void) const;
    TMemberIterator MemberEnd(void) const;

private:
    const type_info& m_CTypeInfo;
    const CClassInfo* m_ParentClass;
    void* (*m_Creator)();
    TMembers m_Members;

    static TTypeByType sm_Types;
};

// define type info getter for user classes
template<class CLASS>
const CTypeInfo& GetTypeInfo(const CLASS& object) {
    return CLASS::sm_TypeInfo;
}

template<class CLASS>
inline const CTypeInfo& GetTypeInfo(const CLASS* object)
{
    return GetTypeInfo(*object).GetPointerTypeInfo();
}

// define type info getter for all basic types
#define PROCESS_STD_TYPE(TYPE) \
template<> inline const CTypeInfo& GetTypeInfo(const TYPE&) \
{ return CStdTypeInfo<TYPE>::sm_TypeInfo; }
FOR_ALL_STD_TYPES
PROCESS_STD_TYPE(string)
PROCESS_STD_TYPE(CStdTypeInfo<char*>::TObjectType)
#undef PROCESS_STD_TYPE

class CMemberInfo {
public:
    typedef CArchiver::TObjectPtr TObjectPtr;
    typedef CArchiver::TConstObjectPtr TConstObjectPtr;

    CMemberInfo(void)
        : m_Offset(0), m_Info(GetTypeInfo())
        { }
    CMemberInfo(size_t offset, const CTypeInfo& info)
        : m_Offset(offset), m_Info(info)
        { }

    size_t GetOffset(void) const
        { return m_Offset; }
    const CTypeInfo& GetTypeInfo(void) const
        { return m_Info; }

    TObjectPtr GetMemeberPtr(TObjectPtr object) const
        { return static_cast<char*>(object) + m_Offset; }
    TConstObjectPtr GetMemberPtr(TConstObjectPtr object) const
        { return static_cast<const char*>(object) + m_Offset; }

    bool HasDefaultValue(TConstObjectPtr object);

private:
    size_t m_Offset;
    const CTypeInfo& m_Info;
};

template<class CLASS>
CArchiver& Write(CArchiver& archiver, const CLASS& object)
{
    GetTypeInfo(object).Write(archiver, object);
    return archiver;
}

template<class CLASS>
CArchiver& Read(CArchiver& archiver, CLASS& object)
{
    GetTypeInfo(object).Read(archiver, object);
    return archiver;
}

template<class CLASS, class PARENT_CLASS>
CClassInfo& NewClassInfo(void)
{
    void* (creator*) = CLASS::s_Create;
    CLASS* object = static_cast<CLASS*>(create());
    return new CClassInfo(typeid(*object), PARENT_CLASS::sm_ClassInfo, creator);
}

#include <serial/serial.inl>

END_NCBI_SCOPE

#endif  /* SERIAL__HPP */
