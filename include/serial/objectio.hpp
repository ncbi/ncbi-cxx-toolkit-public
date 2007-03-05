#ifndef OBJECTIO__HPP
#define OBJECTIO__HPP

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
#include <serial/objectiter.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT COStreamFrame
{
public:

    CObjectOStream& GetStream(void) const;

protected:
    COStreamFrame(CObjectOStream& stream);
    ~COStreamFrame(void);
    bool Good(void) const;

private:
    CObjectOStream& m_Stream;
    size_t m_Depth;

    void* operator new(size_t size);
    void* operator new[](size_t size);
    //void operator delete(void* ptr);
    //void operator delete[](void* ptr);
};

class NCBI_XSERIAL_EXPORT CIStreamFrame
{
public:
    CObjectIStream& GetStream(void) const;

protected:
    CIStreamFrame(CObjectIStream& stream);
    ~CIStreamFrame(void);
    bool Good(void) const;

private:
    CObjectIStream& m_Stream;
    size_t m_Depth;

    void* operator new(size_t size);
    void* operator new[](size_t size);
    //void operator delete(void* ptr);
    //void operator delete[](void* ptr);
};


/// Writing class members
///
/// Suggested use:
///    CObjectOStream& out;
///    CObjectTypeInfo::CMemberIterator member;
///    { // block for automatic call of COStreamClassMember destructor
///        COStreamClassMember o(out, member);
///        ... // write member object
///    } // here COStreamClassMember destructor will be called
class NCBI_XSERIAL_EXPORT COStreamClassMember : public COStreamFrame
{
    typedef COStreamFrame CParent;
public:
    COStreamClassMember(CObjectOStream& out,
                        const CObjectTypeInfo::CMemberIterator& member);
    ~COStreamClassMember(void);
};


/// Reading (iterating through) members of the class (SET, SEQUENCE)
///
/// Suggested use:
///   CObjectIStream& in;
///   CObjectTypeInfo classMemberType;
///   for ( CIStreamClassMemberIterator i(in, classMemberType); i; ++i ) {
///       CElementClass element;
///       i >> element;
///   }
class NCBI_XSERIAL_EXPORT CIStreamClassMemberIterator : public CIStreamFrame
{
    typedef CIStreamFrame CParent;
public:
    CIStreamClassMemberIterator(CObjectIStream& in,
                                const CObjectTypeInfo& classMemberType);
    ~CIStreamClassMemberIterator(void);

    bool HaveMore(void) const;
    DECLARE_OPERATOR_BOOL(HaveMore());

    void NextClassMember(void);
    CIStreamClassMemberIterator& operator++(void);

    void ReadClassMember(const CObjectInfo& classMember);
    void SkipClassMember(const CObjectTypeInfo& classMemberType);
    void SkipClassMember(void);

    CObjectTypeInfoMI operator*(void) const;

private:
    void BeginClassMember(void);

    void IllegalCall(const char* message) const;
    void BadState(void) const;

    void CheckState(void);

    const CMemberInfo* GetMemberInfo(void) const;

    CObjectTypeInfo m_ClassType;
    TMemberIndex m_MemberIndex;
};


/// Reading (iterating through) elements of containers (SET OF, SEQUENCE OF).
///
/// Suggested use:
///   CObjectIStream& in;
///   CObjectTypeInfo containerType;
///   for ( CIStreamContainerIterator i(in, containerType); i; ++i ) {
///       CElementClass element;
///       i >> element;
///   }
class NCBI_XSERIAL_EXPORT CIStreamContainerIterator : public CIStreamFrame
{
    typedef CIStreamFrame CParent;
public:
    CIStreamContainerIterator(CObjectIStream& in,
                              const CObjectTypeInfo& containerType);
    ~CIStreamContainerIterator(void);

    const CObjectTypeInfo& GetContainerType(void) const;

    bool HaveMore(void) const;
    DECLARE_OPERATOR_BOOL(HaveMore());

    void NextElement(void);
    CIStreamContainerIterator& operator++(void);

    void ReadElement(const CObjectInfo& element);
    void SkipElement(const CObjectTypeInfo& elementType);
    void SkipElement(void);

private:
    const CContainerTypeInfo* GetContainerTypeInfo(void) const;

    void BeginElement(void);
    void BeginElementData(void);
    void BeginElementData(const CObjectTypeInfo& elementType);

    void IllegalCall(const char* message) const;
    void BadState(void) const;

    enum EState {
        eElementBegin,
        eElementEnd,
        eNoMoreElements,
        eFinished,
        eError // exception was thrown
    };

    void CheckState(EState state);

    CObjectTypeInfo m_ContainerType;
    TTypeInfo m_ElementTypeInfo;
    EState m_State;
};

template<typename T>
inline
void operator>>(CIStreamContainerIterator& i, T& element)
{
    i.ReadElement(ObjectInfo(element));
}

/// Writing containers (SET OF, SEQUENCE OF).
///
/// Suggested use:
///    CObjectOStream& out;
///    CObjectTypeInfo containerType;
///    set<CElementClass> container;
///    {
///        COStreamContainer o(out, containerType);
///        for ( set<CElementClass>::const_iterator i = container.begin();
///              i != container.end(); ++i ) {
///            const CElementClass& element = *i;
///            o << element;
///        }
///    }
class NCBI_XSERIAL_EXPORT COStreamContainer : public COStreamFrame
{
    typedef COStreamFrame CParent;
public:
    COStreamContainer(CObjectOStream& out,
                      const CObjectTypeInfo& containerType);
    ~COStreamContainer(void);

    const CObjectTypeInfo& GetContainerType(void) const;

    void WriteElement(const CConstObjectInfo& element);

private:
    const CContainerTypeInfo* GetContainerTypeInfo(void) const;

    CObjectTypeInfo m_ContainerType;
    TTypeInfo m_ElementTypeInfo;
};

template<typename T>
inline
void operator<<(COStreamContainer& o, const T& element)
{
    o.WriteElement(ConstObjectInfo(element));
}


/* @} */


#include <serial/impl/objectio.inl>

END_NCBI_SCOPE

#endif  /* OBJECTIO__HPP */
