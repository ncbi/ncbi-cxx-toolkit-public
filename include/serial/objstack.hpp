#ifndef OBJSTACK__HPP
#define OBJSTACK__HPP

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
* Revision 1.3  2000/06/07 19:45:44  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.2  2000/06/01 19:06:58  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:14  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

class COStreamBuffer;

class CObjectStackFrame;
class CObjectStackNamedFrame;
class CObjectStackArray;
class CObjectStackArrayElement;
class CObjectStackClass;
class CObjectStackClassMember;
class CObjectStackChoice;
class CObjectStackChoiceVariant;

class CObjectStack {
public:
    CObjectStack(void)
        : m_Top(0)
        {
        }
    virtual ~CObjectStack(void);

protected:
    virtual void UnendedFrame(void);

    CObjectStackFrame* GetTop() const
        {
            return m_Top;
        }

private:
    friend class CObjectStackFrame;

    CObjectStackFrame* m_Top;
};

class CObjectStackFrame {
public:
    enum ENameType {
        eNameNone,
        eNameCharPtr,
        eNameString,
        eNameId,
        eNameChar,
        eNameTypeInfo
    };
    enum EFrameType {
        eFrameOther,
        eFrameNamed,
        eFrameArray,
        eFrameArrayElement,
        eFrameClass,
        eFrameClassMember,
        eFrameChoice,
        eFrameChoiceVariant
    };

public:
    bool Ended(void) const
        {
            return m_Ended;
        }

    void End(void);
    void Begin(void);
    
public:
    void SetNoName(void);
    void SetName(const char* name);
    void SetName(char name);
    void SetName(const string& name);
    void SetName(TTypeInfo type);
    void SetName(const CMemberId& name);

protected:
    CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                      bool ended = false);
    CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                      const string& name);
    CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                      const char* name);
    CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                      char name, bool ended = false);
    CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                      const CMemberId& name);
    CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                      TTypeInfo type);
public:

    ~CObjectStackFrame(void);

    EFrameType GetFrameType(void) const
        {
            return EFrameType(m_FrameType);
        }

    ENameType GetNameType(void) const
        {
            return ENameType(m_NameType);
        }
    bool HaveName(void) const
        {
            return m_NameType != eNameNone;
        }

    const char* GetNameCharPtr(void) const
        {
            _ASSERT(m_NameType == eNameCharPtr);
            return m_NameCharPtr;
        }
    const string& GetNameString(void) const
        {
            _ASSERT(m_NameType == eNameString);
            return *m_NameString;
        }
    const CMemberId& GetNameId(void) const
        {
            _ASSERT(m_NameType == eNameId);
            return *m_NameId;
        }
    char GetNameChar(void) const
        {
            _ASSERT(m_NameType == eNameChar);
            return m_NameChar;
        }
    TTypeInfo GetNameTypeInfo(void) const
        {
            _ASSERT(m_NameType == eNameTypeInfo);
            return m_NameTypeInfo;
        }
    
    CObjectStackFrame* GetPrevous(void) const
        {
            return m_Previous;
        }
    
    bool AppendStackTo(string& s) const;
    void AppendTo(string& s) const;
    void AppendTo(COStreamBuffer& out) const;
    
    operator CObjectStack&(void) const
        {
            return m_Stack;
        }
    
private:
    void Push(CObjectStack& stack, EFrameType frameType, bool ended = false);
    void Pop(void);
    void PopUnended(void);

    CObjectStack& m_Stack;
    CObjectStackFrame* m_Previous;
    
    union {
        char m_NameChar;
        const char* m_NameCharPtr;
        const string* m_NameString;
        const CMemberId* m_NameId;
        TTypeInfo m_NameTypeInfo;
    };
    char m_NameType;
    char m_FrameType;
    bool m_Ended;
    
private:
    // to prevent allocation in heap
    void *operator new(size_t size);
    void *operator new(size_t size, size_t count);
    // to prevent copying
    CObjectStackFrame(const CObjectStackFrame&);
    CObjectStackFrame& operator=(const CObjectStackFrame&);
};

class CObjectStackNamedFrame : public CObjectStackFrame
{
public:
    CObjectStackNamedFrame(CObjectStack& stack, TTypeInfo type);
};

class CObjectStackBlock : public CObjectStackFrame
{
public:
    CObjectStackBlock(CObjectStack& stack, EFrameType frameType,
                      TTypeInfo typeInfo, bool randomOrder);
    
    bool IsEmpty(void) const
        {
            return m_Empty;
        }
    void SetNonEmpty(void)
        {
            m_Empty = false;
        }

    bool RandomOrder(void) const
        {
            return m_RandomOrder;
        }

    TTypeInfo GetTypeInfo(void) const
        {
            return GetNameTypeInfo();
        }

private:
    bool m_Empty;
    bool m_RandomOrder;
};

class CObjectStackArray : public CObjectStackBlock
{
public:
    CObjectStackArray(CObjectStack& stack,
                      TTypeInfo arrayType, bool randomOrder);
};

class CObjectStackArrayElement : public CObjectStackFrame
{
public:
    CObjectStackArrayElement(CObjectStack& stack, bool ended = true);
    CObjectStackArrayElement(CObjectStackArray& array,
                             TTypeInfo elementType, bool ended = true);

    CObjectStackArray& GetArrayFrame(void) const
        {
            return *static_cast<CObjectStackArray*>(GetPrevous());
        }

    TTypeInfo GetElementType(void) const
        {
            return m_ElementType;
        }

private:
    TTypeInfo m_ElementType;
};

class CObjectStackClass : public CObjectStackBlock
{
public:
    CObjectStackClass(CObjectStack& stack,
                      TTypeInfo classInfo, bool randomOrder);
};

class CObjectStackClassMember : public CObjectStackFrame
{
public:
    CObjectStackClassMember(CObjectStack& stack, bool ended = true);
    CObjectStackClassMember(CObjectStack& stack, const CMemberId& id);
    CObjectStackClassMember(CObjectStackClass& cls);
    CObjectStackClassMember(CObjectStackClass& cls, const CMemberId& id);
    
    CObjectStackClass& GetClassFrame(void) const
        {
            return *static_cast<CObjectStackClass*>(GetPrevous());
        }
};

class CObjectStackChoice : public CObjectStackFrame
{
public:
    CObjectStackChoice(CObjectStack& stack, TTypeInfo choiceInfo);

    TTypeInfo GetChoiceType(void) const
        {
            return GetNameTypeInfo();
        }
};

class CObjectStackChoiceVariant : public CObjectStackFrame
{
public:
    CObjectStackChoiceVariant(CObjectStack& stack);
    CObjectStackChoiceVariant(CObjectStack& stack, const CMemberId& id);
};

#include <serial/objstack.inl>

END_NCBI_SCOPE

#endif  /* OBJSTACK__HPP */
