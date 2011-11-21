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
#include <serial/impl/typeref.hpp>
#include <serial/impl/objlist.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/typeinfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/delaybuf.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/aliasinfo.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/member.hpp>
#include <serial/impl/variant.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/impl/objlist.hpp>
#include <serial/serialimpl.hpp>
#include <serial/error_codes.hpp>

#undef _TRACE
#define _TRACE(arg) ((void)0)

#define NCBI_USE_ERRCODE_X   Serial_OStream

BEGIN_NCBI_SCOPE


CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     const string& fileName,
                                     TSerialOpenFlags openFlags)
{
    CNcbiOstream* outStream = 0;
    bool deleteStream;
    if ( ((openFlags & eSerial_StdWhenEmpty) && fileName.empty()) ||
         ((openFlags & eSerial_StdWhenDash) && fileName == "-") ||
         ((openFlags & eSerial_StdWhenStd) && fileName == "stdout") ) {
        outStream = &NcbiCout;
        deleteStream = false;
    }
    else {
        switch ( format ) {
        case eSerial_AsnText:
        case eSerial_Xml:
        case eSerial_Json:
            outStream = new CNcbiOfstream(fileName.c_str());
            break;
        case eSerial_AsnBinary:
            outStream = new CNcbiOfstream(fileName.c_str(),
                                          IOS_BASE::out | IOS_BASE::binary);
            break;
        default:
            NCBI_THROW(CSerialException,eNotImplemented,
                       "CObjectOStream::Open: unsupported format");
        }
        if ( !*outStream ) {
            delete outStream;
            NCBI_THROW(CSerialException,eNotOpen, string("cannot open file: ")+fileName);
        }
        deleteStream = true;
    }

    return Open(format, *outStream, deleteStream ? eTakeOwnership : eNoOwnership);
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
    case eSerial_Json:
        return OpenObjectOStreamJson(outStream, deleteStream);
    default:
        break;
    }
    NCBI_THROW(CSerialException,eNotImplemented,
               "CObjectOStream::Open: unsupported format");
}

CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     CNcbiOstream& outStream,
                                     EOwnership edeleteStream)
{
    bool deleteStream = edeleteStream == eTakeOwnership;
    switch ( format ) {
    case eSerial_AsnText:
        return OpenObjectOStreamAsn(outStream, deleteStream);
    case eSerial_AsnBinary:
        return OpenObjectOStreamAsnBinary(outStream, deleteStream);
    case eSerial_Xml:
        return OpenObjectOStreamXml(outStream, deleteStream);
    case eSerial_Json:
        return OpenObjectOStreamJson(outStream, deleteStream);
    default:
        break;
    }
    NCBI_THROW(CSerialException,eNotImplemented,
               "CObjectOStream::Open: unsupported format");
}

/////////////////////////////////////////////////////////////////////////////
// data verification setup

ESerialVerifyData CObjectOStream::ms_VerifyDataDefault = eSerialVerifyData_Default;
static CStaticTls<int> s_VerifyTLS;


void CObjectOStream::SetVerifyDataThread(ESerialVerifyData verify)
{
    x_GetVerifyDataDefault();
    ESerialVerifyData tls_verify = ESerialVerifyData(intptr_t(s_VerifyTLS.GetValue()));
    if (tls_verify != eSerialVerifyData_Never &&
        tls_verify != eSerialVerifyData_Always &&
        tls_verify != eSerialVerifyData_DefValueAlways) {
        if (tls_verify != verify &&
            (verify == eSerialVerifyData_No || verify == eSerialVerifyData_Never)) {
            ERR_POST_X_ONCE(2, Warning <<
                "CObjectOStream::SetVerifyDataThread: data verification disabled");
        }
        s_VerifyTLS.SetValue(reinterpret_cast<int*>(verify));
    }
}

void CObjectOStream::SetVerifyDataGlobal(ESerialVerifyData verify)
{
    x_GetVerifyDataDefault();
    if (ms_VerifyDataDefault != eSerialVerifyData_Never &&
        ms_VerifyDataDefault != eSerialVerifyData_Always &&
        ms_VerifyDataDefault != eSerialVerifyData_DefValueAlways) {
        if (ms_VerifyDataDefault != verify &&
            (verify == eSerialVerifyData_No || verify == eSerialVerifyData_Never)) {
            ERR_POST_X_ONCE(3, Warning <<
                "CObjectOStream::SetVerifyDataGlobal: data verification disabled");
        }
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
        verify = ESerialVerifyData(intptr_t(s_VerifyTLS.GetValue()));
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
      m_WriteNamedIntegersByValue(false),
      m_ParseDelayBuffers(eDelayBufferPolicyNotSet),
      m_VerifyData(x_GetVerifyDataDefault())
{
}

CObjectOStream::~CObjectOStream(void)
{
    try {
        Close();
        ResetLocalHooks();
    }
    NCBI_CATCH_X(4, "Cannot close serializing output stream")
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
        catch (CException& exc) {
            if ( InGoodState() )
                RethrowError(fWriteError, "cannot close output stream",exc);
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
        ERR_POST_X(5, "CObjectOStream: error at "<<
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

void CObjectOStream::HandleEOF(CEofException&)
{
    PopFrame();
    throw;
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

void CObjectOStream::SetDelayBufferParsingPolicy(EDelayBufferParsing policy)
{
    m_ParseDelayBuffers = policy;
}
CObjectOStream::EDelayBufferParsing
CObjectOStream::GetDelayBufferParsingPolicy(void) const
{
    return m_ParseDelayBuffers;
}

bool CObjectOStream::ShouldParseDelayBuffer(void) const
{
    if (m_ParseDelayBuffers != eDelayBufferPolicyNotSet) {
        return m_ParseDelayBuffers == eDelayBufferPolicyAlwaysParse;
    }
    return
        !m_ObjectHookKey.IsEmpty() ||
        !m_ClassMemberHookKey.IsEmpty() ||
        !m_ChoiceVariantHookKey.IsEmpty() ||
        !m_PathWriteObjectHooks.IsEmpty() ||
        !m_PathWriteMemberHooks.IsEmpty() ||
        !m_PathWriteVariantHooks.IsEmpty();
}

string CObjectOStream::GetStackTrace(void) const
{
    return GetStackTraceASN();
}

CNcbiStreampos CObjectOStream::GetStreamOffset(void) const
{
    return m_Output.GetStreamPos();
}

CNcbiStreampos CObjectOStream::GetStreamPos(void) const
{
    return m_Output.GetStreamPos();
}

string CObjectOStream::GetPosition(void) const
{
    return "byte "+NStr::Int8ToString(NcbiStreamposToInt8(GetStreamPos()));
}

void CObjectOStream::ThrowError1(const CDiagCompileInfo& diag_info, 
                                 TFailFlags fail, const char* message,
                                 CException* exc)
{
    ThrowError1(diag_info,fail,string(message),exc);
}

void CObjectOStream::ThrowError1(const CDiagCompileInfo& diag_info, 
                                 TFailFlags fail, const string& message,
                                 CException* exc)
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
        CNcbiDiag(diag_info, eDiag_Trace) << ErrCode(NCBI_ERRCODE_X, 12)
                                          << message;
        return;
//    case fEOF:         err = CSerialException::eEOF;         break;
    default:
    case fWriteError:     err = CSerialException::eIoError;        break;
//    case fFormatError: err = CSerialException::eFormatError; break;
    case fOverflow:       err = CSerialException::eOverflow;       break;
    case fInvalidData:    err = CSerialException::eInvalidData;    break;
    case fIllegalCall:    err = CSerialException::eIllegalCall;    break;
    case fFail:           err = CSerialException::eFail;           break;
    case fNotOpen:        err = CSerialException::eNotOpen;        break;
    case fNotImplemented: err = CSerialException::eNotImplemented; break;
    case fUnassigned:
        throw CUnassignedMember(diag_info,exc,CUnassignedMember::eWrite,
                                GetPosition()+": cannot write unassigned member "+message);
    }
    throw CSerialException(diag_info,exc,err,GetPosition()+": "+message);
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
            ERR_POST_X(6, "unended byte block");
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
            ERR_POST_X(7, "unended char block");
        }
    }
}


void CObjectOStream::WriteSeparator(void)
{
    // flush stream buffer by default
    FlushBuffer();
}


void CObjectOStream::SetCanceledCallback(const ICanceled* callback)
{
    m_Output.SetCanceledCallback(callback);
}


END_NCBI_SCOPE
