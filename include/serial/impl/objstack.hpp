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
*/

#define VIRTUAL_MID_LEVEL_IO 1
//#undef VIRTUAL_MID_LEVEL_IO

#ifdef VIRTUAL_MID_LEVEL_IO
# define MLIOVIR virtual
#else
# define MLIOVIR
#endif

#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/typeinfo.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectStack;

class NCBI_XSERIAL_EXPORT CObjectStackFrame
{
public:
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

    void Reset(void);
    
    EFrameType GetFrameType(void) const;
    bool HasTypeInfo(void) const;
    bool HasTypeInfo(TTypeInfo type) const;
    TTypeInfo GetTypeInfo(void) const;
    bool HasMemberId(void) const;
    const CMemberId& GetMemberId(void) const;

    TConstObjectPtr GetObjectPtr(void) const;

    void SetNotag(bool set=true);
    bool GetNotag(void) const;

    void SetNoEOC(bool set=true);
    bool GetNoEOC(void) const;

    const char* GetFrameTypeName(void) const;
    string GetFrameInfo(void) const;
    string GetFrameName(void) const;

    ENsQualifiedMode IsNsQualified(void) const;
    void SetNsQualified(ENsQualifiedMode mode);

protected:
    void SetMemberId(const CMemberId& memberid);

private:
    friend class CObjectStack;

    EFrameType m_FrameType;
    bool m_Notag; // means no XML tag
    bool m_NoEOC; // no binary ASN end-of-content mark
    ENsQualifiedMode m_NsqMode;
    TTypeInfo m_TypeInfo;
    const CMemberId* m_MemberId;
    TConstObjectPtr m_ObjectPtr;
};

#define ThrowError(flag, mess) \
    ThrowError1(DIAG_COMPILE_INFO,flag,mess)

class NCBI_XSERIAL_EXPORT CObjectStack
{
public:
    typedef CObjectStackFrame TFrame;
    typedef TFrame::EFrameType EFrameType;

    CObjectStack(void);
    virtual ~CObjectStack(void);

    size_t GetStackDepth(void) const;

    TFrame& PushFrame(EFrameType type, TTypeInfo typeInfo,
                      TConstObjectPtr objectPtr = 0);
    TFrame& PushFrame(EFrameType type, const CMemberId& memberId);
    TFrame& PushFrame(EFrameType type);

    void PopFrame(void);
    void PopErrorFrame(void);

    void SetTopMemberId(const CMemberId& memberId);
    bool IsNsQualified(void);

#if defined(NCBI_SERIAL_IO_TRACE)
    void TracePushFrame(bool push) const;
#endif


    bool StackIsEmpty(void) const;

    void ClearStack(void);

    string GetStackTraceASN(void) const;
    virtual string GetStackTrace(void) const = 0;
    virtual string GetPosition(void) const = 0;

    const TFrame& TopFrame(void) const;
    TFrame& TopFrame(void);

    TFrame& FetchFrameFromTop(size_t index);
    const TFrame& FetchFrameFromTop(size_t index) const;
    const TFrame& FetchFrameFromBottom(size_t index) const;

    virtual void UnendedFrame(void);
    const string& GetStackPath(void);

    void WatchPathHooks(bool set=true);
protected:
    virtual void x_SetPathHooks(bool set) = 0;
    virtual void ResetState(void);

private:
    TFrame& PushFrame(void);
    TFrame& PushFrameLong(void);
    void x_PushStackPath(void);
    void x_PopStackPath(void);

    TFrame* m_Stack;
    TFrame* m_StackPtr;
    TFrame* m_StackEnd;
    string  m_MemberPath;
    bool    m_WatchPathHooks;
    bool    m_PathValid;
};

#include <serial/impl/objstack.inl>

#define BEGIN_OBJECT_FRAME_OFx(Stream, Args) \
    (Stream).PushFrame Args; \
    try {

#define END_OBJECT_FRAME_OF(Stream) \
    } catch (CSerialException& s_expt) { \
        std::string msg((Stream).TopFrame().GetFrameName()); \
        (Stream).PopFrame(); \
        s_expt.AddFrameInfo(msg); \
        throw; \
    } catch ( CEofException& e_expt ) { \
        (Stream).HandleEOF(e_expt); \
    } catch (CException& expt) { \
        std::string msg((Stream).TopFrame().GetFrameInfo()); \
        (Stream).PopFrame(); \
        NCBI_RETHROW_SAME(expt,msg); \
    } \
    (Stream).PopFrame()


#define BEGIN_OBJECT_FRAME_OF(Stream, Type) \
    BEGIN_OBJECT_FRAME_OFx(Stream, (CObjectStackFrame::Type))
#define BEGIN_OBJECT_FRAME_OF2(Stream, Type, Arg) \
    BEGIN_OBJECT_FRAME_OFx(Stream, (CObjectStackFrame::Type, Arg))
#define BEGIN_OBJECT_FRAME_OF3(Stream, Type, Arg1, Arg2)                \
    BEGIN_OBJECT_FRAME_OFx(Stream, (CObjectStackFrame::Type, Arg1, Arg2))

#define BEGIN_OBJECT_FRAME(Type) BEGIN_OBJECT_FRAME_OF(*this, Type)
#define BEGIN_OBJECT_FRAME2(Type, Arg) BEGIN_OBJECT_FRAME_OF2(*this, Type, Arg)
#define BEGIN_OBJECT_FRAME3(Type, Arg1, Arg2) BEGIN_OBJECT_FRAME_OF3(*this, Type, Arg1, Arg2)
#define END_OBJECT_FRAME() END_OBJECT_FRAME_OF(*this)

#define BEGIN_OBJECT_2FRAMES_OFx(Stream, Args) \
    (Stream).In().PushFrame Args; \
    (Stream).Out().PushFrame Args; \
    try {

#define END_OBJECT_2FRAMES_OF(Stream) \
    } catch (CException& expt) { \
        std::string msg((Stream).In().TopFrame().GetFrameInfo()); \
        (Stream).Out().PopFrame(); \
        (Stream).Out().SetFailFlagsNoError(CObjectOStream::fInvalidData); \
        (Stream).In().PopErrorFrame(); \
        NCBI_RETHROW_SAME(expt,msg); \
    } \
    (Stream).Out().PopFrame(); \
    (Stream).In().PopFrame()


#define BEGIN_OBJECT_2FRAMES_OF(Stream, Type) \
    BEGIN_OBJECT_2FRAMES_OFx(Stream, (CObjectStackFrame::Type))
#define BEGIN_OBJECT_2FRAMES_OF2(Stream, Type, Arg) \
    BEGIN_OBJECT_2FRAMES_OFx(Stream, (CObjectStackFrame::Type, Arg))
#define BEGIN_OBJECT_2FRAMES(Type) BEGIN_OBJECT_2FRAMES_OF(*this, Type)
#define BEGIN_OBJECT_2FRAMES2(Type, Arg) BEGIN_OBJECT_2FRAMES_OF2(*this, Type, Arg)
#define END_OBJECT_2FRAMES() END_OBJECT_2FRAMES_OF(*this)

END_NCBI_SCOPE

#endif  /* OBJSTACK__HPP */


/* @} */
