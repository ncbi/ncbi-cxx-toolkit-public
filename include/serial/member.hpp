#ifndef MEMBER__HPP
#define MEMBER__HPP

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
* Revision 1.9  2000/06/15 19:26:33  vasilche
* Fixed compilation error on Mac.
*
* Revision 1.8  2000/04/28 16:58:01  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.7  2000/03/07 14:05:29  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.6  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.5  1999/09/01 17:38:00  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.4  1999/08/31 17:50:03  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/06/30 16:04:21  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:38  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>
#include <serial/typeinfo.hpp>
#include <serial/delaybuf.hpp>

BEGIN_NCBI_SCOPE

class CDelayBuffer;

class CMemberInfo {
public:
    CMemberInfo(size_t offset, const CTypeRef& type)
        : m_Optional(false), m_Pointer(false), m_ObjectPointer(false),
          m_Offset(offset), m_Type(type),
          m_SetFlagOffset(size_t(-1)), m_DelayOffset(size_t(-1)),
          m_Default(0)
        {
        }
    ~CMemberInfo(void)
        {
        }

    bool Optional(void) const
        {
            return m_Optional;
        }
    CMemberInfo* SetOptional(void)
        {
            m_Optional = true;
            return this;
        }
    bool IsPointer(void) const
        {
            return m_Pointer;
        }
    bool IsObjectPointer(void) const
        {
            return m_ObjectPointer;
        }
    CMemberInfo* SetPointer(void)
        {
            m_Pointer = true;
            return this;
        }
    CMemberInfo* SetObjectPointer(void)
        {
            m_Pointer = m_ObjectPointer = true;
            return this;
        }
    TConstObjectPtr GetDefault(void) const
        {
            return m_Default;
        }
    CMemberInfo* SetDefault(TConstObjectPtr def)
        {
            m_Optional = true;
            m_Default = def;
            return this;
        }

    size_t GetOffset(void) const
        {
            return m_Offset;
        }

    TTypeInfo GetTypeInfo(void) const
        {
            return m_Type.Get();
        }

    size_t GetSize(void) const
        {
            return GetTypeInfo()->GetSize();
        }

    bool HaveSetFlag(void) const
        {
            return m_SetFlagOffset != size_t(-1);
        }
    size_t GetSetFlagOffset(void) const
        {
            return m_SetFlagOffset;
        }
    bool GetSetFlag(TConstObjectPtr object) const
        {
            return CType<bool>::Get(Add(object, GetSetFlagOffset()));
        }
    bool& GetSetFlag(TObjectPtr object) const
        {
            return CType<bool>::Get(Add(object, GetSetFlagOffset()));
        }
    CMemberInfo* SetSetFlag(const bool* setFlag)
        {
            m_SetFlagOffset = size_t(setFlag);
            return this;
        }

    bool CanBeDelayed(void) const
        {
            return m_DelayOffset != size_t(-1);
        }
    CMemberInfo* SetDelayBuffer(CDelayBuffer* buffer)
        {
            m_DelayOffset = size_t(buffer);
            return this;
        }
    CDelayBuffer& GetDelayBuffer(TObjectPtr object) const
        {
            return CType<CDelayBuffer>::Get(Add(object, m_DelayOffset));
        }
    const CDelayBuffer& GetDelayBuffer(TConstObjectPtr object) const
        {
            return CType<const CDelayBuffer>::Get(Add(object, m_DelayOffset));
        }

    TObjectPtr GetMember(TObjectPtr object) const
        {
            return Add(object, GetOffset());
        }
    TConstObjectPtr GetMember(TConstObjectPtr object) const
        {
            return Add(object, GetOffset());
        }
    TObjectPtr GetContainer(TObjectPtr object) const
        {
            return Add(object, -GetOffset());
        }
    TConstObjectPtr GetContainer(TConstObjectPtr object) const
        {
            return Add(object, -GetOffset());
        }
    size_t GetEndOffset(void) const
        {
            return GetOffset() + GetSize();
        }
    
private:
    // is optional
    bool m_Optional;
    // is pointer in choice
    bool m_Pointer;
    // is pointer to CObject descendant
    bool m_ObjectPointer;
    // offset of member inside object
    size_t m_Offset;
    // type of member
    CTypeRef m_Type;
    // offset of 'SET' flag inside object
    size_t m_SetFlagOffset;
    // offset of delay buffer inside object
    size_t m_DelayOffset;
    // default value
    TConstObjectPtr m_Default;
};

#if 1
# define CRealMemberInfo CMemberInfo
#else
class CRealMemberInfo : public CMemberInfo {
public:
    // common member
    CRealMemberInfo(size_t offset, const CTypeRef& type);

    virtual size_t GetOffset(void) const;
    virtual TTypeInfo GetTypeInfo(void) const;

private:
    // physical description
};

class CMemberAliasInfo : public CMemberInfo {
public:
    // common member
    CMemberAliasInfo(const CTypeRef& containerType,
                     const string& memberName);

    virtual size_t GetOffset(void) const;
    virtual TTypeInfo GetTypeInfo(void) const;

protected:
    const CMemberInfo* GetBaseMember(void) const;

private:
    // physical description
    mutable const CMemberInfo* m_BaseMember;
    CTypeRef m_ContainerType;
    string m_MemberName;
};

class CTypedMemberAliasInfo : public CMemberAliasInfo {
public:
    // common member
    CTypedMemberAliasInfo(const CTypeRef& type,
                          const CTypeRef& containerType,
                          const string& memberName);

    virtual TTypeInfo GetTypeInfo(void) const;

private:
    // physical description
    CTypeRef m_Type;
};
#endif

#include <serial/member.inl>

END_NCBI_SCOPE

#endif  /* MEMBER__HPP */
