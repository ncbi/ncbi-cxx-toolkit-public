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
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>


/** @addtogroup UserCodeSupport
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


/*************************
//class for writing class members
//   suggested use:

    CObjectOStream& out;
    CObjectTypeInfo::CMemberIterator member;

    { // block for automatic call of COStreamClassMember destructor
        COStreamClassMember o(out, member);
        ... // write member object
    } // here COStreamClassMember destructor will be called
**************************/
class NCBI_XSERIAL_EXPORT COStreamClassMember : public COStreamFrame
{
    typedef COStreamFrame CParent;
public:
    COStreamClassMember(CObjectOStream& out,
                        const CObjectTypeInfo::CMemberIterator& member);
    ~COStreamClassMember(void);
};


/*************************
// class for reading (iterating through)
// members of the class (SET, SEQUENCE)
//   suggested use:

   CObjectIStream& in;
   CObjectTypeInfo classMemberType;

   for ( CIStreamClassMemberIterator i(in, classMemberType); i; ++i ) {
       CElementClass element;
       i >> element;
   }
**************************/
class NCBI_XSERIAL_EXPORT CIStreamClassMemberIterator : public CIStreamFrame
{
    typedef CIStreamFrame CParent;
public:
    CIStreamClassMemberIterator(CObjectIStream& in,
                                const CObjectTypeInfo& classMemberType);
    ~CIStreamClassMemberIterator(void);

    bool HaveMore(void) const;
    operator bool(void) const;

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


/*************************
// class for reading (iterating through)
// elements of containers (SET OF, SEQUENCE OF).
//   suggested use:

   CObjectIStream& in;
   CObjectTypeInfo containerType;

   for ( CIStreamContainerIterator i(in, containerType); i; ++i ) {
       CElementClass element;
       i >> element;
   }
**************************/
class NCBI_XSERIAL_EXPORT CIStreamContainerIterator : public CIStreamFrame
{
    typedef CIStreamFrame CParent;
public:
    CIStreamContainerIterator(CObjectIStream& in,
                              const CObjectTypeInfo& containerType);
    ~CIStreamContainerIterator(void);

    const CObjectTypeInfo& GetContainerType(void) const;

    bool HaveMore(void) const;
    operator bool(void) const;

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

/*************************
//class for writing containers (SET OF, SEQUENCE OF).
//   suggested use:

    CObjectOStream& out;
    CObjectTypeInfo containerType;
    set<CElementClass> container;

    { // block for automatic call of COStreamContainer destructor
        COStreamContainer o(out, containerType);
        for ( set<CElementClass>::const_iterator i = container.begin();
              i != container.end(); ++i ) {
            const CElementClass& element = *i;
            o << element;
        }
    } // here COStreamContainer destructor will be called
**************************/
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


#include <serial/objectio.inl>

END_NCBI_SCOPE

#endif  /* OBJECTIO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/06/08 20:25:42  gouriano
* Made functions, that are not visible from the outside, protected
*
* Revision 1.7  2003/10/24 15:54:27  grichenk
* Removed or blocked exceptions in destructors
*
* Revision 1.6  2003/04/15 16:18:13  siyan
* Added doxygen support
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2001/05/17 14:57:32  lavr
* Typos corrected
*
* Revision 1.3  2001/01/22 23:23:57  vakatov
* Added   CIStreamClassMemberIterator
* Renamed CIStreamContainer --> CIStreamContainerIterator
*
* Revision 1.2  2000/10/20 19:29:15  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.1  2000/10/20 15:51:25  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/
