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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbithr.hpp>

#include <util/bytesrc.hpp>

#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/typeref.hpp>
#include <serial/objlist.hpp>
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/delaybuf.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/aliasinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/member.hpp>
#include <serial/variant.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objlist.hpp>
#include <serial/serialimpl.hpp>

#undef _TRACE
#define _TRACE(arg) ((void)0)

BEGIN_NCBI_SCOPE


CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     const string& fileName,
                                     TSerialOpenFlags openFlags)
{
    CNcbiOstream* outStream = 0;
    bool deleteStream;
    if ( (openFlags & eSerial_StdWhenEmpty) && fileName.empty() ||
         (openFlags & eSerial_StdWhenDash) && fileName == "-" ||
         (openFlags & eSerial_StdWhenStd) && fileName == "stdout" ) {
        outStream = &NcbiCout;
        deleteStream = false;
    }
    else {
        switch ( format ) {
        case eSerial_AsnText:
        case eSerial_Xml:
            outStream = new CNcbiOfstream(fileName.c_str());
            break;
        case eSerial_AsnBinary:
            outStream = new CNcbiOfstream(fileName.c_str(),
                                          IOS_BASE::out | IOS_BASE::binary);
            break;
        default:
            NCBI_THROW(CSerialException,eFail,
                       "CObjectOStream::Open: unsupported format");
        }
        if ( !*outStream ) {
            delete outStream;
            NCBI_THROW(CSerialException,eFail, "cannot open file");
        }
        deleteStream = true;
    }

    return Open(format, *outStream, deleteStream);
}

CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     CNcbiOstream& outStream,
                                     bool deleteStream)
{
    switch ( format ) {
    case eSerial_AsnText:
        return OpenObjectOStreamAsn(outStream, deleteStream);
    case eSerial_AsnBinary:
        return OpenObjectOStreamAsnBinary(outStream, deleteStream);
    case eSerial_Xml:
        return OpenObjectOStreamXml(outStream, deleteStream);
    default:
        break;
    }
    NCBI_THROW(CSerialException,eFail,
               "CObjectOStream::Open: unsupported format");
}

/////////////////////////////////////////////////////////////////////////////
// data verification setup

ESerialVerifyData CObjectOStream::ms_VerifyDataDefault = eSerialVerifyData_Default;
static CSafeStaticRef< CTls<int> > s_VerifyTLS;


void CObjectOStream::SetVerifyDataThread(ESerialVerifyData verify)
{
    x_GetVerifyDataDefault();
    ESerialVerifyData tls_verify = ESerialVerifyData(long(s_VerifyTLS->GetValue()));
    if (tls_verify != eSerialVerifyData_Never &&
        tls_verify != eSerialVerifyData_Always &&
        tls_verify != eSerialVerifyData_DefValueAlways) {
        s_VerifyTLS->SetValue(reinterpret_cast<int*>(verify));
    }
}

void CObjectOStream::SetVerifyDataGlobal(ESerialVerifyData verify)
{
    x_GetVerifyDataDefault();
    if (ms_VerifyDataDefault != eSerialVerifyData_Never &&
        ms_VerifyDataDefault != eSerialVerifyData_Always &&
        ms_VerifyDataDefault != eSerialVerifyData_DefValueAlways) {
        ms_VerifyDataDefault = verify;
    }
}

ESerialVerifyData CObjectOStream::x_GetVerifyDataDefault(void)
{
    ESerialVerifyData verify;
    if (ms_VerifyDataDefault == eSerialVerifyData_Never ||
        ms_VerifyDataDefault == eSerialVerifyData_Always ||
        ms_VerifyDataDefault == eSerialVerifyData_DefValueAlways) {
        verify = ms_VerifyDataDefault;
    } else {
        verify = ESerialVerifyData(long(s_VerifyTLS->GetValue()));
        if (verify == eSerialVerifyData_Default) {
            if (ms_VerifyDataDefault == eSerialVerifyData_Default) {

                // change the default here, if you wish
                ms_VerifyDataDefault = eSerialVerifyData_Yes;
                //ms_VerifyDataDefault = eSerialVerifyData_No;

                const char* str = getenv(SERIAL_VERIFY_DATA_WRITE);
                if (str) {
                    if (NStr::CompareNocase(str,"YES") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_Yes;
                    } else if (NStr::CompareNocase(str,"NO") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_No;
                    } else if (NStr::CompareNocase(str,"NEVER") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_Never;
                    } else  if (NStr::CompareNocase(str,"ALWAYS") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_Always;
                    } else  if (NStr::CompareNocase(str,"DEFVALUE") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_DefValue;
                    } else  if (NStr::CompareNocase(str,"DEFVALUE_ALWAYS") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_DefValueAlways;
                    }
                }
            }
            verify = ms_VerifyDataDefault;
        }
    }
    return verify;
}


/////////////////////////////////////////////////////////////////////////////

CObjectOStream::CObjectOStream(ESerialDataFormat format,
                               CNcbiOstream& out, bool deleteOut)
    : m_Output(out, deleteOut), m_Fail(fNoError), m_Flags(fFlagNone),
      m_Separator(""), m_AutoSeparator(false),
      m_DataFormat(format),
      m_VerifyData(x_GetVerifyDataDefault())
{
}

CObjectOStream::~CObjectOStream(void)
{
    try {
        Close();
        ResetLocalHooks();
    }
    catch (...) {
        ERR_POST("Can not close output stream");
    }
}


void CObjectOStream::DefaultFlush(void)
{
    if ( GetFlags() & fFlagNoAutoFlush ) {
        FlushBuffer();
    }
    else {
        Flush();
    }
}


void CObjectOStream::Close(void)
{
    if (m_Fail != fNotOpen) {
        try {
            DefaultFlush();
            if ( m_Objects )
                m_Objects->Clear();
            ClearStack();
            m_Fail = fNotOpen;
        }
        catch (...) {
            if ( InGoodState() )
                ThrowError(fWriteError, "cannot close output stream");
        }
    }
}

void CObjectOStream::ResetLocalHooks(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ObjectHookKey.Clear();
    m_ClassMemberHookKey.Clear();
    m_ChoiceVariantHookKey.Clear();
}

CObjectOStream::TFailFlags CObjectOStream::SetFailFlagsNoError(TFailFlags flags)
{
    TFailFlags old = m_Fail;
    m_Fail |= flags;
    return old;
}

CObjectOStream::TFailFlags CObjectOStream::SetFailFlags(TFailFlags flags,
                                                        const char* message)
{
    TFailFlags old = m_Fail;
    m_Fail |= flags;
    if ( !old && flags ) {
        // first fail
        ERR_POST("CObjectOStream: error at "<<
                 GetPosition()<<": "<<GetStackTrace() << ": " << message);
    }
    return old;
}

bool CObjectOStream::InGoodState(void)
{
    if ( fail() ) {
        // fail flag already set
        return false;
    }
    else if ( m_Output.fail() ) {
        // IO exception thrown without setting fail flag
        SetFailFlags(fWriteError, m_Output.GetError());
        m_Output.ResetFail();
        return false;
    }
    else {
        // ok
        return true;
    }
}

void CObjectOStream::Unended(const string& msg)
{
    if ( InGoodState() )
        ThrowError(fFail, msg);
}

void CObjectOStream::UnendedFrame(void)
{
    Unended("internal error: unended object stack frame");
}

void CObjectOStream::x_SetPathHooks(bool set)
{
    if (!m_PathWriteObjectHooks.IsEmpty()) {
        CWriteObjectHook* hook = m_PathWriteObjectHooks.GetHook(*this);
        if (hook) {
            CTypeInfo* item = m_PathWriteObjectHooks.FindType(*this);
            if (item) {
                if (set) {
                    item->SetLocalWriteHook(*this, hook);
                } else {
                    item->ResetLocalWriteHook(*this);
                }
            }
        }
    }
    if (!m_PathWriteMemberHooks.IsEmpty()) {
        CWriteClassMemberHook* hook = m_PathWriteMemberHooks.GetHook(*this);
        if (hook) {
            CMemberInfo* item = m_PathWriteMemberHooks.FindItem(*this);
            if (item) {
                if (set) {
                    item->SetLocalWriteHook(*this, hook);
                } else {
                    item->ResetLocalWriteHook(*this);
                }
            }
        }
    }
    if (!m_PathWriteVariantHooks.IsEmpty()) {
        CWriteChoiceVariantHook* hook = m_PathWriteVariantHooks.GetHook(*this);
        if (hook) {
            CVariantInfo* item = m_PathWriteVariantHooks.FindItem(*this);
            if (item) {
                if (set) {
                    item->SetLocalWriteHook(*this, hook);
                } else {
                    item->ResetLocalWriteHook(*this);
                }
            }
        }
    }
}

void CObjectOStream::SetPathWriteObjectHook(const string& path,
                                            CWriteObjectHook*   hook)
{
    m_PathWriteObjectHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectOStream::SetPathWriteMemberHook(const string& path,
                                            CWriteClassMemberHook*   hook)
{
    m_PathWriteMemberHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectOStream::SetPathWriteVariantHook(const string& path,
                                             CWriteChoiceVariantHook* hook)
{
    m_PathWriteVariantHooks.SetHook(path,hook);
    WatchPathHooks();
}

string CObjectOStream::GetStackTrace(void) const
{
    return GetStackTraceASN();
}

CNcbiStreamoff CObjectOStream::GetStreamOffset(void) const
{
    return m_Output.GetStreamOffset();
}

string CObjectOStream::GetPosition(void) const
{
    return "byte "+NStr::UIntToString(GetStreamOffset());
}

void CObjectOStream::ThrowError1(const CDiagCompileInfo& diag_info, 
                                 TFailFlags fail, const char* message)
{
    ThrowError1(diag_info,fail,string(message));
}

void CObjectOStream::ThrowError1(const CDiagCompileInfo& diag_info, 
                                 TFailFlags fail, const string& message)
{
    CSerialException::EErrCode err;
    SetFailFlags(fail, message.c_str());
    try {
        DefaultFlush();
    } catch(...) {
    }
    switch(fail)
    {
    case fNoError:
        CNcbiDiag(diag_info, eDiag_Trace) << message;
        return;
    case fEOF:         err = CSerialException::eEOF;         break;
    default:
    case fWriteError:  err = CSerialException::eIoError;     break;
    case fFormatError: err = CSerialException::eFormatError; break;
    case fOverflow:    err = CSerialException::eOverflow;    break;
    case fInvalidData: err = CSerialException::eInvalidData; break;
    case fIllegalCall: err = CSerialException::eIllegalCall; break;
    case fFail:        err = CSerialException::eFail;        break;
    case fNotOpen:     err = CSerialException::eNotOpen;     break;
    case fUnassigned:
        throw CUnassignedMember(diag_info,0,CUnassignedMember::eWrite,
                                GetPosition()+": "+message);
    }
    throw CSerialException(diag_info,0,err,GetPosition()+": "+message);
}

void CObjectOStream::EndOfWrite(void)
{
    FlushBuffer();
    if ( m_Objects )
        m_Objects->Clear();
}    

void CObjectOStream::WriteObject(const CConstObjectInfo& object)
{
    WriteObject(object.GetObjectPtr(), object.GetTypeInfo());
}

void CObjectOStream::WriteClassMember(const CConstObjectInfo::CMemberIterator& member)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    TConstObjectPtr classPtr = member.GetClassObject().GetObjectPtr();
    WriteClassMember(memberInfo->GetId(),
                     memberInfo->GetTypeInfo(),
                     memberInfo->GetMemberPtr(classPtr));
}

void CObjectOStream::WriteChoiceVariant(const CConstObjectInfoCV& object)
{
    const CVariantInfo* variantInfo = object.GetVariantInfo();
    TConstObjectPtr choicePtr = object.GetChoiceObject().GetObjectPtr();
    variantInfo->DefaultWriteVariant(*this, choicePtr);
}

void CObjectOStream::Write(const CConstObjectInfo& object)
{
    // root writer
    BEGIN_OBJECT_FRAME2(eFrameNamed, object.GetTypeInfo());
    
    WriteFileHeader(object.GetTypeInfo());

    WriteObject(object);

    EndOfWrite();
    
    END_OBJECT_FRAME();

    if ( GetAutoSeparator() )
        Separator(*this);
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    // root writer
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
    
    WriteFileHeader(typeInfo);

    WriteObject(object, typeInfo);

    EndOfWrite();
    
    END_OBJECT_FRAME();

    if ( GetAutoSeparator() )
        Separator(*this);
}

void CObjectOStream::Write(TConstObjectPtr object, const CTypeRef& type)
{
    Write(object, type.Get());
}

void CObjectOStream::RegisterObject(TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(typeInfo);
}

void CObjectOStream::RegisterObject(TConstObjectPtr object,
                                    TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(object, typeInfo);
}

void CObjectOStream::WriteSeparateObject(const CConstObjectInfo& object)
{
    if ( m_Objects ) {
        size_t firstObject = m_Objects->GetObjectCount();
        WriteObject(object);
        size_t lastObject = m_Objects->GetObjectCount();
        m_Objects->ForgetObjects(firstObject, lastObject);
    }
    else {
        WriteObject(object);
    }
}

void CObjectOStream::WriteExternalObject(TConstObjectPtr objectPtr,
                                         TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::WriteExternalObject(" <<
           NStr::PtrToString(objectPtr) << ", "
           << typeInfo->GetName() << ')');
    RegisterObject(objectPtr, typeInfo);
    WriteObject(objectPtr, typeInfo);
}

bool CObjectOStream::Write(CByteSource& source)
{
    CRef<CByteSourceReader> reader = source.Open();
    m_Output.Write(*reader);
    return true;
}

void CObjectOStream::WriteFileHeader(TTypeInfo /*type*/)
{
    // do nothing by default
}

void CObjectOStream::WritePointer(TConstObjectPtr objectPtr,
                                  TTypeInfo declaredTypeInfo)
{
    _TRACE("WritePointer("<<NStr::PtrToString(objectPtr)<<", "
           <<declaredTypeInfo->GetName()<<")");
    if ( objectPtr == 0 ) {
        _TRACE("WritePointer: "<<NStr::PtrToString(objectPtr)<<": null");
        WriteNullPointer();
        return;
    }
    TTypeInfo realTypeInfo = declaredTypeInfo->GetRealTypeInfo(objectPtr);
    if ( m_Objects ) {
        const CWriteObjectInfo* info =
            m_Objects->RegisterObject(objectPtr, realTypeInfo);
        if ( info ) {
            // old object
            WriteObjectReference(info->GetIndex());
            return;
        }
    }
    if ( declaredTypeInfo == realTypeInfo ) {
        _TRACE("WritePointer: "<<NStr::PtrToString(objectPtr)<<": new");
        WriteThis(objectPtr, realTypeInfo);
    }
    else {
        _TRACE("WritePointer: "<<NStr::PtrToString(objectPtr)<<
               ": new "<<realTypeInfo->GetName());
        WriteOther(objectPtr, realTypeInfo);
    }
}

void CObjectOStream::WriteThis(TConstObjectPtr object, TTypeInfo typeInfo)
{
    WriteObject(object, typeInfo);
}

void CObjectOStream::WriteFloat(float data)
{
    WriteDouble(data);
}

#if SIZEOF_LONG_DOUBLE != 0
void CObjectOStream::WriteLDouble(long double data)
{
    WriteDouble(data);
}
#endif

void CObjectOStream::BeginNamedType(TTypeInfo /*namedTypeInfo*/)
{
}

void CObjectOStream::EndNamedType(void)
{
}

void CObjectOStream::WriteNamedType(TTypeInfo
#ifndef VIRTUAL_MID_LEVEL_IO
                                    namedTypeInfo
#endif
                                    ,
                                    TTypeInfo objectType,
                                    TConstObjectPtr objectPtr)
{
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);
#endif
    WriteObject(objectPtr, objectType);
#ifndef VIRTUAL_MID_LEVEL_IO
    EndNamedType();
    END_OBJECT_FRAME();
#endif
}

void CObjectOStream::CopyNamedType(TTypeInfo namedTypeInfo,
                                   TTypeInfo objectType,
                                   CObjectStreamCopier& copier)
{
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);
    BeginNamedType(namedTypeInfo);

    CopyObject(objectType, copier);

    EndNamedType();
    copier.In().EndNamedType();
    END_OBJECT_2FRAMES_OF(copier);
#else
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);

    CopyObject(objectType, copier);

    copier.In().EndNamedType();
    END_OBJECT_FRAME_OF(copier.In());
#endif
}

void CObjectOStream::WriteOther(TConstObjectPtr object,
                                TTypeInfo typeInfo)
{
    WriteOtherBegin(typeInfo);
    WriteObject(object, typeInfo);
    WriteOtherEnd(typeInfo);
}

void CObjectOStream::WriteOtherEnd(TTypeInfo /*typeInfo*/)
{
}

void CObjectOStream::EndContainer(void)
{
}

void CObjectOStream::BeginContainerElement(TTypeInfo /*elementType*/)
{
}

void CObjectOStream::EndContainerElement(void)
{
}

void CObjectOStream::WriteContainer(const CContainerTypeInfo* cType,
                                    TConstObjectPtr containerPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, cType);
    BeginContainer(cType);
        
    CContainerTypeInfo::CConstIterator i;
    if ( cType->InitIterator(i, containerPtr) ) {
        TTypeInfo elementType = cType->GetElementType();
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        do {
            BeginContainerElement(elementType);
            
            WriteObject(cType->GetElementPtr(i), elementType);
            
            EndContainerElement();
        } while ( cType->NextElement(i) );

        END_OBJECT_FRAME();
    }

    EndContainer();
    END_OBJECT_FRAME();
}

void CObjectOStream::WriteContainerElement(const CConstObjectInfo& element)
{
    BeginContainerElement(element.GetTypeInfo());

    WriteObject(element);

    EndContainerElement();
}

void CObjectOStream::CopyContainer(const CContainerTypeInfo* cType,
                                   CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArray, cType);
    copier.In().BeginContainer(cType);
    BeginContainer(cType);

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArrayElement, elementType);

    while ( copier.In().BeginContainerElement(elementType) ) {
        BeginContainerElement(elementType);

        CopyObject(elementType, copier);

        EndContainerElement();
        copier.In().EndContainerElement();
    }

    END_OBJECT_2FRAMES_OF(copier);
    
    EndContainer();
    copier.In().EndContainer();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::EndClass(void)
{
}

void CObjectOStream::EndClassMember(void)
{
}

void CObjectOStream::WriteClass(const CClassTypeInfo* classType,
                                TConstObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        classType->GetMemberInfo(*i)->WriteMember(*this, classPtr);
    }
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectOStream::WriteClassMember(const CMemberId& memberId,
                                      TTypeInfo memberType,
                                      TConstObjectPtr memberPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    BeginClassMember(memberId);

    WriteObject(memberPtr, memberType);

    EndClassMember();
    END_OBJECT_FRAME();
}

bool CObjectOStream::WriteClassMember(const CMemberId& memberId,
                                      const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(GetDataFormat()) )
        return false;

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    BeginClassMember(memberId);

    Write(buffer.GetSource());

    EndClassMember();
    END_OBJECT_FRAME();
    return true;
}

void CObjectOStream::CopyClassRandom(const CClassTypeInfo* classType,
                                     CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameClass, classType);
    copier.In().BeginClass(classType);
    BeginClass(classType);

    vector<Uint1> read(classType->GetMembers().LastIndex() + 1);

    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());
        copier.SetPathHooks(*this, true);

        if ( read[index] ) {
            copier.In().DuplicatedMember(memberInfo);
        }
        else {
            read[index] = true;
            BeginClassMember(memberInfo->GetId());

            memberInfo->CopyMember(copier);

            EndClassMember();
        }
        
        copier.SetPathHooks(*this, false);
        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        if ( !read[*i] ) {
            classType->GetMemberInfo(*i)->CopyMissingMember(copier);
        }
    }

    EndClass();
    copier.In().EndClass();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::CopyClassSequential(const CClassTypeInfo* classType,
                                         CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameClass, classType);
    copier.In().BeginClass(classType);
    BeginClass(classType);

    CClassTypeInfo::CIterator pos(classType);
    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType, *pos)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());
        copier.SetPathHooks(*this, true);

        for ( TMemberIndex i = *pos; i < index; ++i ) {
            // init missing member
            classType->GetMemberInfo(i)->CopyMissingMember(copier);
        }
        BeginClassMember(memberInfo->GetId());

        memberInfo->CopyMember(copier);
        
        pos.SetIndex(index + 1);

        EndClassMember();
        copier.SetPathHooks(*this, false);
        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( ; pos.Valid(); ++pos ) {
        classType->GetMemberInfo(*pos)->CopyMissingMember(copier);
    }

    EndClass();
    copier.In().EndClass();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::BeginChoice(const CChoiceTypeInfo* /*choiceType*/)
{
}
void CObjectOStream::EndChoice(void)
{
}
void CObjectOStream::EndChoiceVariant(void)
{
}

void CObjectOStream::WriteChoice(const CChoiceTypeInfo* choiceType,
                                 TConstObjectPtr choicePtr)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
    BeginChoice(choiceType);
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());
    BeginChoiceVariant(choiceType, variantInfo->GetId());

    variantInfo->WriteVariant(*this, choicePtr);

    EndChoiceVariant();
    END_OBJECT_FRAME();
    EndChoice();
    END_OBJECT_FRAME();
}

void CObjectOStream::CopyChoice(const CChoiceTypeInfo* choiceType,
                                CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameChoice, choiceType);

    BeginChoice(choiceType);
    copier.In().BeginChoice(choiceType);
    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameChoiceVariant);
    TMemberIndex index = copier.In().BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember ) {
        copier.ThrowError(CObjectIStream::fFormatError,
                          "choice variant id expected");
    }

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    if (variantInfo->GetId().IsAttlist()) {
        const CMemberInfo* memberInfo =
            dynamic_cast<const CMemberInfo*>(
                choiceType->GetVariants().GetItemInfo(index));
        BeginClassMember(memberInfo->GetId());
        memberInfo->CopyMember(copier);
        EndClassMember();
        index = copier.In().BeginChoiceVariant(choiceType);
        if ( index == kInvalidMember )
            copier.ThrowError(CObjectIStream::fFormatError,
                          "choice variant id expected");
        variantInfo = choiceType->GetVariantInfo(index);
    }
    copier.In().SetTopMemberId(variantInfo->GetId());
    copier.Out().SetTopMemberId(variantInfo->GetId());
    copier.SetPathHooks(copier.Out(), true);
    BeginChoiceVariant(choiceType, variantInfo->GetId());

    variantInfo->CopyVariant(copier);

    EndChoiceVariant();
    copier.SetPathHooks(copier.Out(), false);
    copier.In().EndChoiceVariant();
    END_OBJECT_2FRAMES_OF(copier);
    copier.In().EndChoice();
    EndChoice();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::WriteAlias(const CAliasTypeInfo* aliasType,
                                TConstObjectPtr aliasPtr)
{
    WriteNamedType(aliasType, aliasType->GetPointedType(),
        aliasType->GetDataPtr(aliasPtr));
}

void CObjectOStream::CopyAlias(const CAliasTypeInfo* aliasType,
                                CObjectStreamCopier& copier)
{
    CopyNamedType(aliasType, aliasType->GetPointedType(),
        copier);
}

void CObjectOStream::BeginBytes(const ByteBlock& )
{
}

void CObjectOStream::EndBytes(const ByteBlock& )
{
}

void CObjectOStream::ByteBlock::End(void)
{
    _ASSERT(!m_Ended);
    _ASSERT(m_Length == 0);
    if ( GetStream().InGoodState() ) {
        GetStream().EndBytes(*this);
        m_Ended = true;
    }
}

CObjectOStream::ByteBlock::~ByteBlock(void)
{
    if ( !m_Ended ) {
        try {
            GetStream().Unended("byte block not fully written");
        }
        catch (...) {
            ERR_POST("unended byte block");
        }
    }
}

void CObjectOStream::BeginChars(const CharBlock& )
{
}

void CObjectOStream::EndChars(const CharBlock& )
{
}

void CObjectOStream::CharBlock::End(void)
{
    _ASSERT(!m_Ended);
    _ASSERT(m_Length == 0);
    if ( GetStream().InGoodState() ) {
        GetStream().EndChars(*this);
        m_Ended = true;
    }
}

CObjectOStream::CharBlock::~CharBlock(void)
{
    if ( !m_Ended ) {
        try {
            GetStream().Unended("char block not fully written");
        }
        catch (...) {
            ERR_POST("unended char block");
        }
    }
}


void CObjectOStream::WriteSeparator(void)
{
    // flush stream buffer by default
    FlushBuffer();
}


END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.100  2005/04/27 16:52:56  vasilche
* Use vector<Uint1> instead of inefficient vector<bool>.
* Fixed warning on Sun WorkShop.
*
* Revision 1.99  2005/02/23 21:07:44  vasilche
* Allow to skip underlying stream flush.
*
* Revision 1.98  2004/11/30 15:06:04  dicuccio
* Added #include for ncbithr.hpp
*
* Revision 1.97  2004/09/22 13:32:17  kononenk
* "Diagnostic Message Filtering" functionality added.
* Added function SetDiagFilter()
* Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
* Module, class and function attribute added to CNcbiDiag and CException
* Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
* 	CDiagCompileInfo + fixes on derived classes and their usage
* Macro NCBI_MODULE can be used to set default module name in cpp files
*
* Revision 1.96  2004/08/30 18:19:39  gouriano
* Use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.95  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.94  2004/02/09 18:22:34  gouriano
* enforced checking environment vars when setting initialization
* verification parameters
*
* Revision 1.93  2004/01/05 14:25:21  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.92  2003/11/26 20:04:47  vasilche
* Method put in wrong file.
*
* Revision 1.91  2003/11/26 19:59:40  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.90  2003/11/24 14:10:05  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.89  2003/11/13 14:07:38  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.88  2003/10/24 15:54:28  grichenk
* Removed or blocked exceptions in destructors
*
* Revision 1.87  2003/10/21 21:08:46  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.86  2003/07/29 18:47:48  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.85  2003/07/17 18:48:01  gouriano
* re-enabled initialization verification
*
* Revision 1.84  2003/06/25 17:49:05  gouriano
* fixed verification flag initialization, disabled verification
*
* Revision 1.83  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.82  2003/05/15 17:44:38  gouriano
* added GetStreamOffset method
*
* Revision 1.81  2003/05/13 20:13:29  vasilche
* Catch errors while closing broken CObjectOStream.
*
* Revision 1.80  2003/04/30 15:45:38  gouriano
* added catching all exceptions when flashing stream in ThrowError
*
* Revision 1.79  2003/04/30 15:38:43  gouriano
* added Flush stream when throwing an exception
*
* Revision 1.78  2003/04/29 18:30:37  gouriano
* object data member initialization verification
*
* Revision 1.77  2003/04/10 20:13:39  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.75  2003/04/03 21:47:24  gouriano
* verify initialization of data members
*
* Revision 1.74  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.73  2003/01/22 18:53:27  gouriano
* corrected stream destruction
*
* Revision 1.72  2002/12/26 21:35:49  gouriano
* corrected handling choice's XML attributes
*
* Revision 1.71  2002/12/26 19:32:34  gouriano
* changed XML I/O streams to properly handle object copying
*
* Revision 1.70  2002/12/13 21:50:42  gouriano
* corrected reading of choices
*
* Revision 1.69  2002/11/14 20:59:48  gouriano
* added BeginChoice/EndChoice methods
*
* Revision 1.68  2002/11/04 21:29:20  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.67  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.66  2002/09/19 14:00:38  grichenk
* Implemented CObjectHookGuard for write and copy hooks
* Added DefaultRead/Write/Copy methods to base hook classes
*
* Revision 1.65  2002/08/30 16:22:22  vasilche
* Removed excessive _TRACEs
*
* Revision 1.64  2002/08/26 18:32:29  grichenk
* Added Get/SetAutoSeparator() to CObjectOStream to control
* output of separators.
*
* Revision 1.63  2002/03/07 22:02:01  grichenk
* Added "Separator" modifier for CObjectOStream
*
* Revision 1.62  2001/10/17 20:41:25  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.61  2001/07/30 14:42:46  lavr
* eDiag_Trace and eDiag_Fatal always print as much as possible
*
* Revision 1.60  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.59  2001/01/05 20:10:51  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.58  2000/12/26 22:24:13  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.57  2000/12/15 22:07:02  vasilche
* Fixed typo eNotOpen -> eNoError.
*
* Revision 1.56  2000/12/15 21:29:02  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.55  2000/12/15 15:38:44  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.54  2000/10/20 19:29:36  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.53  2000/10/20 15:51:42  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.52  2000/10/17 18:45:35  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.51  2000/10/13 16:28:39  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.50  2000/10/03 17:22:44  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.49  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.48  2000/09/26 19:24:57  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.47  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.46  2000/09/18 20:00:24  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.45  2000/09/01 13:16:18  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.44  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.43  2000/07/03 18:42:45  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.42  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.41  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.40  2000/06/01 19:07:04  vasilche
* Added parsing of XML data.
*
* Revision 1.39  2000/05/24 20:08:48  vasilche
* Implemented XML dump.
*
* Revision 1.38  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.37  2000/05/04 16:22:20  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.36  2000/04/28 16:58:13  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.35  2000/04/13 14:50:27  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.34  2000/04/06 16:11:00  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.33  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.32  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.31  2000/02/11 17:10:25  vasilche
* Optimized text parsing.
*
* Revision 1.30  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.29  2000/01/05 19:43:56  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.28  1999/12/28 18:55:51  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.27  1999/12/20 15:29:35  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.26  1999/12/17 19:05:03  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/10/04 16:22:18  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.24  1999/09/27 14:18:02  vasilche
* Fixed bug with overloaded constructors of Block.
*
* Revision 1.23  1999/09/24 18:19:19  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.22  1999/09/23 21:16:08  vasilche
* Removed dependence on asn.h
*
* Revision 1.21  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.20  1999/09/23 18:57:00  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.19  1999/09/14 18:54:19  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.18  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.17  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.16  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.15  1999/07/22 17:33:55  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.14  1999/07/19 15:50:35  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.13  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:58  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:33  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:59  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:06  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:19:50  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:54  vasilche
* Commit just in case.
*
* ===========================================================================
*/
