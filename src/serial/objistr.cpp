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
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbithr.hpp>

#include <exception>

#include <util/bytesrc.hpp>

#include <serial/objistr.hpp>
#include <serial/typeref.hpp>
#include <serial/member.hpp>
#include <serial/variant.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/aliasinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/delaybuf.hpp>
#include <serial/objistrimpl.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objlist.hpp>
#include <serial/choiceptr.hpp>
#include <serial/serialimpl.hpp>
#include <serial/pack_string.hpp>

#include <limits.h>
#if HAVE_WINDOWS_H
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif

#undef _TRACE
#define _TRACE(arg) ((void)0)

BEGIN_NCBI_SCOPE

CRef<CByteSource> CObjectIStream::GetSource(ESerialDataFormat format,
                                            const string& fileName,
                                            TSerialOpenFlags openFlags)
{
    if ( (openFlags & eSerial_StdWhenEmpty) && fileName.empty() ||
         (openFlags & eSerial_StdWhenDash) && fileName == "-" ||
         (openFlags & eSerial_StdWhenStd) && fileName == "stdin" ) {
        return CRef<CByteSource>(new CStreamByteSource(NcbiCin));
    }
    else {
        bool binary;
        switch ( format ) {
        case eSerial_AsnText:
        case eSerial_Xml:
            binary = false;
            break;
        case eSerial_AsnBinary:
            binary = true;
            break;
        default:
            NCBI_THROW(CSerialException,eFail,
                       "CObjectIStream::Open: unsupported format");
        }
        
        if ( (openFlags & eSerial_UseFileForReread) )  {
            // use file as permanent file
            return CRef<CByteSource>(new CFileByteSource(fileName, binary));
        }
        else {
            // open file as stream
            return CRef<CByteSource>(new CFStreamByteSource(fileName, binary));
        }
    }
}

CRef<CByteSource> CObjectIStream::GetSource(CNcbiIstream& inStream,
                                            bool deleteInStream)
{
    if ( deleteInStream ) {
        return CRef<CByteSource>(new CFStreamByteSource(inStream));
    }
    else {
        return CRef<CByteSource>(new CStreamByteSource(inStream));
    }
}

CObjectIStream* CObjectIStream::Create(ESerialDataFormat format)
{
    switch ( format ) {
    case eSerial_AsnText:
        return CreateObjectIStreamAsn();
    case eSerial_AsnBinary:
        return CreateObjectIStreamAsnBinary();
    case eSerial_Xml:
        return CreateObjectIStreamXml();
    default:
        break;
    }
    NCBI_THROW(CSerialException,eFail,
               "CObjectIStream::Open: unsupported format");
}

CObjectIStream* CObjectIStream::Create(ESerialDataFormat format,
                                       CByteSource& source)
{
    AutoPtr<CObjectIStream> stream(Create(format));
    stream->Open(source);
    return stream.release();
}

CObjectIStream* CObjectIStream::Create(ESerialDataFormat format,
                                       CByteSourceReader& reader)
{
    AutoPtr<CObjectIStream> stream(Create(format));
    stream->Open(reader);
    return stream.release();
}

CObjectIStream* CObjectIStream::Open(ESerialDataFormat format,
                                     CNcbiIstream& inStream,
                                     bool deleteInStream)
{
    CRef<CByteSource> src = GetSource(inStream, deleteInStream);
    return Create(format, *src);
}

CObjectIStream* CObjectIStream::Open(ESerialDataFormat format,
                                     const string& fileName,
                                     TSerialOpenFlags openFlags)
{
    CRef<CByteSource> src = GetSource(format, fileName, openFlags);
    return Create(format, *src);
}

/////////////////////////////////////////////////////////////////////////////
// data verification setup

ESerialVerifyData CObjectIStream::ms_VerifyDataDefault = eSerialVerifyData_Default;
static CSafeStaticRef< CTls<int> > s_VerifyTLS;


void CObjectIStream::SetVerifyDataThread(ESerialVerifyData verify)
{
    x_GetVerifyDataDefault();
    ESerialVerifyData tls_verify = ESerialVerifyData(long(s_VerifyTLS->GetValue()));
    if (tls_verify != eSerialVerifyData_Never &&
        tls_verify != eSerialVerifyData_Always &&
        tls_verify != eSerialVerifyData_DefValueAlways) {
        s_VerifyTLS->SetValue(reinterpret_cast<int*>(verify));
    }
}

void CObjectIStream::SetVerifyDataGlobal(ESerialVerifyData verify)
{
    x_GetVerifyDataDefault();
    if (ms_VerifyDataDefault != eSerialVerifyData_Never &&
        ms_VerifyDataDefault != eSerialVerifyData_Always &&
        ms_VerifyDataDefault != eSerialVerifyData_DefValueAlways) {
        ms_VerifyDataDefault = verify;
    }
}

ESerialVerifyData CObjectIStream::x_GetVerifyDataDefault(void)
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

                const char* str = getenv(SERIAL_VERIFY_DATA_READ);
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
// skip unknown members setup

ESerialSkipUnknown CObjectIStream::ms_SkipUnknownDefault = eSerialSkipUnknown_Default;
static CSafeStaticRef< CTls<int> > s_SkipTLS;


void CObjectIStream::SetSkipUnknownThread(ESerialSkipUnknown skip)
{
    x_GetSkipUnknownDefault();
    ESerialSkipUnknown tls_skip = ESerialSkipUnknown(long(s_SkipTLS->GetValue()));
    if (tls_skip != eSerialSkipUnknown_Never &&
        tls_skip != eSerialSkipUnknown_Always) {
        s_SkipTLS->SetValue(reinterpret_cast<int*>(skip));
    }
}

void CObjectIStream::SetSkipUnknownGlobal(ESerialSkipUnknown skip)
{
    x_GetSkipUnknownDefault();
    if (ms_SkipUnknownDefault != eSerialSkipUnknown_Never &&
        ms_SkipUnknownDefault != eSerialSkipUnknown_Always) {
        ms_SkipUnknownDefault = skip;
    }
}

ESerialSkipUnknown CObjectIStream::x_GetSkipUnknownDefault(void)
{
    ESerialSkipUnknown skip;
    if (ms_SkipUnknownDefault == eSerialSkipUnknown_Never ||
        ms_SkipUnknownDefault == eSerialSkipUnknown_Always) {
        skip = ms_SkipUnknownDefault;
    } else {
        skip = ESerialSkipUnknown(long(s_SkipTLS->GetValue()));
        if (skip == eSerialSkipUnknown_Default) {
            if (ms_SkipUnknownDefault == eSerialSkipUnknown_Default) {

                // change the default here, if you wish
                ms_SkipUnknownDefault = eSerialSkipUnknown_No;
                //ms_SkipUnknownDefault = eSerialSkipUnknown_Yes;

                const char* str = getenv(SERIAL_SKIP_UNKNOWN_MEMBERS);
                if (str) {
                    if (NStr::CompareNocase(str,"YES") == 0) {
                        ms_SkipUnknownDefault = eSerialSkipUnknown_Yes;
                    } else if (NStr::CompareNocase(str,"NO") == 0) {
                        ms_SkipUnknownDefault = eSerialSkipUnknown_No;
                    } else if (NStr::CompareNocase(str,"NEVER") == 0) {
                        ms_SkipUnknownDefault = eSerialSkipUnknown_Never;
                    } else  if (NStr::CompareNocase(str,"ALWAYS") == 0) {
                        ms_SkipUnknownDefault = eSerialSkipUnknown_Always;
                    }
                }
            }
            skip = ms_SkipUnknownDefault;
        }
    }
    return skip;
}

/////////////////////////////////////////////////////////////////////////////

CObjectIStream::CObjectIStream(ESerialDataFormat format)
    : m_DiscardCurrObject(false),
      m_DataFormat(format),
      m_VerifyData(x_GetVerifyDataDefault()),
      m_SkipUnknown(x_GetSkipUnknownDefault()),
      m_Fail(fNotOpen),
      m_Flags(fFlagNone)
{
}

CObjectIStream::~CObjectIStream(void)
{
    try {
        Close();
        ResetLocalHooks();
    }
    catch (...) {
        ERR_POST("Can not close input stream");
    }
}

void CObjectIStream::Open(CByteSourceReader& reader)
{
    Close();
    _ASSERT(m_Fail == fNotOpen);
    m_Input.Open(reader);
    m_Fail = 0;
}

void CObjectIStream::Open(CByteSource& source)
{
    CRef<CByteSourceReader> reader = source.Open();
    Open(*reader);
}

void CObjectIStream::Open(CNcbiIstream& inStream, bool deleteInStream)
{
    CRef<CByteSource> src = GetSource(inStream, deleteInStream);
    Open(*src);
}

void CObjectIStream::ResetLocalHooks(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ObjectHookKey.Clear();
    m_ClassMemberHookKey.Clear();
    m_ChoiceVariantHookKey.Clear();
    m_ObjectSkipHookKey.Clear();
    m_ClassMemberSkipHookKey.Clear();
    m_ChoiceVariantSkipHookKey.Clear();
}

void CObjectIStream::Close(void)
{
    if (m_Fail != fNotOpen) {
        m_Input.Close();
        if ( m_Objects )
            m_Objects->Clear();
        ClearStack();
        m_Fail = fNotOpen;
    }
}

CObjectIStream::TFailFlags
CObjectIStream::SetFailFlags(TFailFlags flags,
                             const char* /* message */)
{
    TFailFlags old = m_Fail;
    if (flags == fNoError) {
        m_Fail = flags;
    } else {
        m_Fail |= flags;
        if ( !old && flags ) {
            // first fail
// redundant
//            ERR_POST(Error << "CObjectIStream: error at "<<
//                     GetPosition()<<": "<<GetStackTrace() << ": " << message);
        }
    }
    return old;
}

bool CObjectIStream::InGoodState(void)
{
    if ( fail() ) {
        // fail flag already set
        return false;
    }
    else if ( m_Input.fail() ) {
        // IO exception thrown without setting fail flag
        SetFailFlags(fReadError, m_Input.GetError());
        m_Input.ResetFail();
        return false;
    }
    else {
        // ok
        return true;
    }
}

void CObjectIStream::Unended(const string& msg)
{
    if ( InGoodState() )
        ThrowError(fFail, msg);
}

void CObjectIStream::UnendedFrame(void)
{
    Unended("internal error: unended object stack frame");
}

void CObjectIStream::x_SetPathHooks(bool set)
{
    if (!m_PathReadObjectHooks.IsEmpty()) {
        CReadObjectHook* hook = m_PathReadObjectHooks.GetHook(*this);
        if (hook) {
            CTypeInfo* item = m_PathReadObjectHooks.FindType(*this);
            if (item) {
                if (set) {
                    item->SetLocalReadHook(*this, hook);
                } else {
                    item->ResetLocalReadHook(*this);
                }
            }
        }
    }
    if (!m_PathSkipObjectHooks.IsEmpty()) {
        CSkipObjectHook* hook = m_PathSkipObjectHooks.GetHook(*this);
        if (hook) {
            CTypeInfo* item = m_PathSkipObjectHooks.FindType(*this);
            if (item) {
                if (set) {
                    item->SetLocalSkipHook(*this, hook);
                } else {
                    item->ResetLocalSkipHook(*this);
                }
            }
        }
    }
    if (!m_PathReadMemberHooks.IsEmpty()) {
        CReadClassMemberHook* hook = m_PathReadMemberHooks.GetHook(*this);
        if (hook) {
            CMemberInfo* item = m_PathReadMemberHooks.FindItem(*this);
            if (item) {
                if (set) {
                    item->SetLocalReadHook(*this, hook);
                } else {
                    item->ResetLocalReadHook(*this);
                }
            }
        }
    }
    if (!m_PathSkipMemberHooks.IsEmpty()) {
        CSkipClassMemberHook* hook = m_PathSkipMemberHooks.GetHook(*this);
        if (hook) {
            CMemberInfo* item = m_PathSkipMemberHooks.FindItem(*this);
            if (item) {
                if (set) {
                    item->SetLocalSkipHook(*this, hook);
                } else {
                    item->ResetLocalSkipHook(*this);
                }
            }
        }
    }
    if (!m_PathReadVariantHooks.IsEmpty()) {
        CReadChoiceVariantHook* hook = m_PathReadVariantHooks.GetHook(*this);
        if (hook) {
            CVariantInfo* item = m_PathReadVariantHooks.FindItem(*this);
            if (item) {
                if (set) {
                    item->SetLocalReadHook(*this, hook);
                } else {
                    item->ResetLocalReadHook(*this);
                }
            }
        }
    }
    if (!m_PathSkipVariantHooks.IsEmpty()) {
        CSkipChoiceVariantHook* hook = m_PathSkipVariantHooks.GetHook(*this);
        if (hook) {
            CVariantInfo* item = m_PathSkipVariantHooks.FindItem(*this);
            if (item) {
                if (set) {
                    item->SetLocalSkipHook(*this, hook);
                } else {
                    item->ResetLocalSkipHook(*this);
                }
            }
        }
    }
}

void CObjectIStream::SetPathReadObjectHook(const string& path,
                                           CReadObjectHook* hook)
{
    m_PathReadObjectHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectIStream::SetPathSkipObjectHook(const string& path,
                                           CSkipObjectHook* hook)
{
    m_PathSkipObjectHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectIStream::SetPathReadMemberHook(const string& path,
                                            CReadClassMemberHook* hook)
{
    m_PathReadMemberHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectIStream::SetPathSkipMemberHook(const string& path,
                                            CSkipClassMemberHook* hook)
{
    m_PathSkipMemberHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectIStream::SetPathReadVariantHook(const string& path,
                                            CReadChoiceVariantHook* hook)
{
    m_PathReadVariantHooks.SetHook(path,hook);
    WatchPathHooks();
}
void CObjectIStream::SetPathSkipVariantHook(const string& path,
                                            CSkipChoiceVariantHook* hook)
{
    m_PathSkipVariantHooks.SetHook(path,hook);
    WatchPathHooks();
}

string CObjectIStream::GetStackTrace(void) const
{
    return GetStackTraceASN();
}

CNcbiStreamoff CObjectIStream::GetStreamOffset(void) const
{
    return m_Input.GetStreamOffset();
}

void CObjectIStream::SetStreamOffset(CNcbiStreamoff pos)
{
    m_Input.SetStreamOffset(pos);
}

string CObjectIStream::GetPosition(void) const
{
    return "byte "+NStr::UIntToString(GetStreamOffset());
}

void CObjectIStream::ThrowError1(const CDiagCompileInfo& diag_info, 
                                 TFailFlags fail, const char* message)
{
    ThrowError1(diag_info,fail,string(message));
}

void CObjectIStream::ThrowError1(const CDiagCompileInfo& diag_info, 
                                 TFailFlags fail, const string& message)
{
    CSerialException::EErrCode err;
    SetFailFlags(fail, message.c_str());
    switch(fail)
    {
    case fNoError:
        CNcbiDiag(diag_info, eDiag_Trace) << message;
        return;
    case fEOF:          err = CSerialException::eEOF;          break;
    default:
    case fReadError:    err = CSerialException::eIoError;      break;
    case fFormatError:  err = CSerialException::eFormatError;  break;
    case fOverflow:     err = CSerialException::eOverflow;     break;
    case fInvalidData:  err = CSerialException::eInvalidData;  break;
    case fIllegalCall:  err = CSerialException::eIllegalCall;  break;
    case fFail:         err = CSerialException::eFail;         break;
    case fNotOpen:      err = CSerialException::eNotOpen;      break;
    case fMissingValue: err = CSerialException::eMissingValue; break;
    }
    throw CSerialException(diag_info,0,err,GetPosition()+": "+message);
}

static inline
TTypeInfo MapType(const string& name)
{
    return CClassTypeInfoBase::GetClassInfoByName(name);
}

void CObjectIStream::RegisterObject(TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(typeInfo);
}

void CObjectIStream::RegisterObject(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(objectPtr, typeInfo);
}

const CReadObjectInfo&
CObjectIStream::GetRegisteredObject(CReadObjectInfo::TObjectIndex index)
{
    if ( !m_Objects ) {
        ThrowError(fFormatError,"invalid object index: NO_COLLECT defined");
    }
    return m_Objects->GetRegisteredObject(index);
}

// root reader
void CObjectIStream::SkipFileHeader(TTypeInfo typeInfo)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
    
    string name = ReadFileHeader();
    const string& tname = typeInfo->GetName();
    if ( !name.empty() && !tname.empty() && name != tname ) {
        ThrowError(fFormatError,
                   "incompatible type "+name+"<>"+typeInfo->GetName());
    }

    END_OBJECT_FRAME();
}

void CObjectIStream::EndOfRead(void)
{
    if ( m_Objects )
        m_Objects->Clear();
}

void CObjectIStream::Read(const CObjectInfo& object, ENoFileHeader)
{
    // root object
    BEGIN_OBJECT_FRAME2(eFrameNamed, object.GetTypeInfo());
    
    ReadObject(object);

    EndOfRead();
    
    END_OBJECT_FRAME();
}

void CObjectIStream::Read(const CObjectInfo& object)
{
    // root object
    SkipFileHeader(object.GetTypeInfo());
    Read(object, eNoFileHeader);
}

void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo, ENoFileHeader)
{
    // root object
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);

    ReadObject(object, typeInfo);
    
    EndOfRead();

    END_OBJECT_FRAME();
}

void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo)
{
    // root object
    SkipFileHeader(typeInfo);
    Read(object, typeInfo, eNoFileHeader);
}

CObjectInfo CObjectIStream::Read(TTypeInfo typeInfo)
{
    // root object
    SkipFileHeader(typeInfo);
    CObjectInfo info(typeInfo->Create(), typeInfo);
    Read(info, eNoFileHeader);
    return info;
}

CObjectInfo CObjectIStream::Read(const CObjectTypeInfo& type)
{
    return Read(type.GetTypeInfo());
}

void CObjectIStream::Skip(TTypeInfo typeInfo, ENoFileHeader)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);

    SkipObject(typeInfo);
    
    EndOfRead();

    END_OBJECT_FRAME();
}

void CObjectIStream::Skip(TTypeInfo typeInfo)
{
    SkipFileHeader(typeInfo);
    Skip(typeInfo, eNoFileHeader);
}

void CObjectIStream::Skip(const CObjectTypeInfo& type)
{
    Skip(type.GetTypeInfo());
}

void CObjectIStream::StartDelayBuffer(void)
{
    m_Input.StartSubSource();
}

CRef<CByteSource> CObjectIStream::EndDelayBuffer(void)
{
    return m_Input.EndSubSource();
}

void CObjectIStream::EndDelayBuffer(CDelayBuffer& buffer,
                                    const CItemInfo* itemInfo,
                                    TObjectPtr objectPtr)
{
    CRef<CByteSource> src = EndDelayBuffer();
    buffer.SetData(itemInfo, objectPtr, GetDataFormat(), *src);
}

void CObjectIStream::ExpectedMember(const CMemberInfo* memberInfo)
{
    if (GetVerifyData() == eSerialVerifyData_Yes) {
        ThrowError(fFormatError,
                   "member "+memberInfo->GetId().ToString()+" expected");
    } else {
        ERR_POST("member "+memberInfo->GetId().ToString()+" is missing");
    }
}

void CObjectIStream::DuplicatedMember(const CMemberInfo* memberInfo)
{
    ThrowError(fFormatError,
               "duplicated member: "+memberInfo->GetId().ToString());
}

void CObjectIStream::ReadSeparateObject(const CObjectInfo& object)
{
    if ( m_Objects ) {
        size_t firstObject = m_Objects->GetObjectCount();
        ReadObject(object);
        size_t lastObject = m_Objects->GetObjectCount();
        m_Objects->ForgetObjects(firstObject, lastObject);
    }
    else {
        ReadObject(object);
    }
}

void CObjectIStream::ReadExternalObject(TObjectPtr objectPtr,
                                        TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read("<<NStr::PtrToString(objectPtr)<<", "<<
           typeInfo->GetName()<<")");
    RegisterObject(objectPtr, typeInfo);
    ReadObject(objectPtr, typeInfo);
}

CObjectInfo CObjectIStream::ReadObject(void)
{
    TTypeInfo typeInfo = MapType(ReadFileHeader());
    TObjectPtr objectPtr;
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);

    objectPtr = typeInfo->Create();
    CRef<CObject> ref;
    if ( typeInfo->IsCObject() )
        ref.Reset(static_cast<CObject*>(objectPtr));
    RegisterObject(objectPtr, typeInfo);
    ReadObject(objectPtr, typeInfo);
    if ( typeInfo->IsCObject() )
        ref.Release();
    END_OBJECT_FRAME();
    return make_pair(objectPtr, typeInfo);
}

void CObjectIStream::ReadObject(const CObjectInfo& object)
{
    ReadObject(object.GetObjectPtr(), object.GetTypeInfo());
}

void CObjectIStream::SkipObject(const CObjectTypeInfo& objectType)
{
    SkipObject(objectType.GetTypeInfo());
}

void CObjectIStream::ReadClassMember(const CObjectInfo::CMemberIterator& member)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    TObjectPtr classPtr = member.GetClassObject().GetObjectPtr();
    ReadObject(memberInfo->GetMemberPtr(classPtr),
               memberInfo->GetTypeInfo());
}

void CObjectIStream::ReadChoiceVariant(const CObjectInfoCV& object)
{
    const CVariantInfo* variantInfo = object.GetVariantInfo();
    TObjectPtr choicePtr = object.GetChoiceObject().GetObjectPtr();
    variantInfo->DefaultReadVariant(*this, choicePtr);
}

string CObjectIStream::ReadFileHeader(void)
{
    // this is to check if the file is empty or not
    m_Input.PeekChar();
    return NcbiEmptyString;
}

string CObjectIStream::PeekNextTypeName(void)
{
    return NcbiEmptyString;
}

pair<TObjectPtr, TTypeInfo> CObjectIStream::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::ReadPointer("<<declaredType->GetName()<<")");
    TObjectPtr objectPtr;
    TTypeInfo objectType;
    switch ( ReadPointerType() ) {
    case eNullPointer:
        _TRACE("CObjectIStream::ReadPointer: null");
        return pair<TObjectPtr, TTypeInfo>(0, declaredType);
    case eObjectPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: @...");
            TObjectIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::ReadPointer: @" << index);
            const CReadObjectInfo& info = GetRegisteredObject(index);
            objectType = info.GetTypeInfo();
            objectPtr = info.GetObjectPtr();
            if ( !objectPtr ) {
                ThrowError(fFormatError,
                    "invalid reference to skipped object: object ptr is NULL");
            }
            break;
        }
    case eThisPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            objectPtr = declaredType->Create();
            CRef<CObject> ref;
            if ( declaredType->IsCObject() )
                ref.Reset(static_cast<CObject*>(objectPtr));
            RegisterObject(objectPtr, declaredType);
            ReadObject(objectPtr, declaredType);
            if ( declaredType->IsCObject() )
                ref.Release();
            return make_pair(objectPtr, declaredType);
        }
    case eOtherPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new...");
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            objectType = MapType(className);

            BEGIN_OBJECT_FRAME2(eFrameNamed, objectType);
                
            objectPtr = objectType->Create();
            CRef<CObject> ref;
            if ( declaredType->IsCObject() )
                ref.Reset(static_cast<CObject*>(objectPtr));
            RegisterObject(objectPtr, objectType);
            ReadObject(objectPtr, objectType);
            if ( declaredType->IsCObject() )
                ref.Release();
                
            END_OBJECT_FRAME();

            ReadOtherPointerEnd();
            break;
        }
    default:
        ThrowError(fFormatError,"illegal pointer type");
        objectPtr = 0;
        objectType = 0;
        break;
    }
    while ( objectType != declaredType ) {
        // try to check parent class pointer
        if ( objectType->GetTypeFamily() != eTypeFamilyClass ) {
            ThrowError(fFormatError,"incompatible member type");
        }
        const CClassTypeInfo* parentClass =
            CTypeConverter<CClassTypeInfo>::SafeCast(objectType)->GetParentClassInfo();
        if ( parentClass ) {
            objectType = parentClass;
        }
        else {
            ThrowError(fFormatError,"incompatible member type");
        }
    }
    return make_pair(objectPtr, objectType);
}

void CObjectIStream::ReadOtherPointerEnd(void)
{
}

void CObjectIStream::SkipExternalObject(TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::SkipExternalObject("<<typeInfo->GetName()<<")");
    RegisterObject(typeInfo);
    SkipObject(typeInfo);
}

void CObjectIStream::SkipPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::SkipPointer("<<declaredType->GetName()<<")");
    switch ( ReadPointerType() ) {
    case eNullPointer:
        _TRACE("CObjectIStream::SkipPointer: null");
        return;
    case eObjectPointer:
        {
            _TRACE("CObjectIStream::SkipPointer: @...");
            TObjectIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::SkipPointer: @" << index);
            GetRegisteredObject(index);
            break;
        }
    case eThisPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            RegisterObject(declaredType);
            SkipObject(declaredType);
            break;
        }
    case eOtherPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new...");
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            TTypeInfo typeInfo = MapType(className);
            BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
                
            RegisterObject(typeInfo);
            SkipObject(typeInfo);

            END_OBJECT_FRAME();
            ReadOtherPointerEnd();
            break;
        }
    default:
        ThrowError(fFormatError,"illegal pointer type");
    }
}

void CObjectIStream::BeginNamedType(TTypeInfo /*namedTypeInfo*/)
{
}

void CObjectIStream::EndNamedType(void)
{
}

void CObjectIStream::ReadNamedType(TTypeInfo
#ifndef VIRTUAL_MID_LEVEL_IO
                                   namedTypeInfo
#endif
                                   ,
                                   TTypeInfo typeInfo, TObjectPtr object)
{
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);
#endif
    ReadObject(object, typeInfo);
#ifndef VIRTUAL_MID_LEVEL_IO
    EndNamedType();
    END_OBJECT_FRAME();
#endif
}

void CObjectIStream::SkipNamedType(TTypeInfo namedTypeInfo,
                                   TTypeInfo typeInfo)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);

    SkipObject(typeInfo);

    EndNamedType();
    END_OBJECT_FRAME();
}

void CObjectIStream::EndContainerElement(void)
{
}

void CObjectIStream::ReadContainer(const CContainerTypeInfo* containerType,
                                   TObjectPtr containerPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
    BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

    CContainerTypeInfo::CIterator iter;
    bool old_element = containerType->InitIterator(iter, containerPtr);
    while ( BeginContainerElement(elementType) ) {
        if ( old_element ) {
            elementType->ReadData(*this, containerType->GetElementPtr(iter));
            old_element = containerType->NextElement(iter);
        }
        else {
            containerType->AddElement(containerPtr, *this);
        }
        EndContainerElement();
    }
    if ( old_element ) {
        containerType->EraseAllElements(iter);
    }

    END_OBJECT_FRAME();

    EndContainer();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipContainer(const CContainerTypeInfo* containerType)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
    BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

    while ( BeginContainerElement(elementType) ) {
        SkipObject(elementType);
        EndContainerElement();
    }

    END_OBJECT_FRAME();

    EndContainer();
    END_OBJECT_FRAME();
}

void CObjectIStream::EndClass(void)
{
}

void CObjectIStream::EndClassMember(void)
{
}

void CObjectIStream::ReadClassRandom(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);

    ReadClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {

        ReadClassRandomContentsMember(classPtr);
        
        EndClassMember();
    }

    ReadClassRandomContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::ReadClassSequential(const CClassTypeInfo* classType,
                                         TObjectPtr classPtr)
{
    TMemberIndex prevIndex = kInvalidMember;
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    ReadClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType, *pos)) != kInvalidMember ) {

        if ((prevIndex != kInvalidMember) && (prevIndex > index)) {
            const CMemberInfo *mem_info = classType->GetMemberInfo(index);
            if (mem_info->GetId().HaveNoPrefix()) {
                UndoClassMember();
                break;
            }
        }
        prevIndex = index;

        ReadClassSequentialContentsMember(classPtr);

        EndClassMember();
    }

    ReadClassSequentialContentsEnd(classPtr);
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipClassRandom(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    SkipClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {

        SkipClassRandomContentsMember();

        EndClassMember();
    }

    SkipClassRandomContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipClassSequential(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    SkipClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType, *pos)) != kInvalidMember ) {

        SkipClassSequentialContentsMember();

        EndClassMember();
    }

    SkipClassSequentialContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::BeginChoice(const CChoiceTypeInfo* /*choiceType*/)
{
}
void CObjectIStream::EndChoice(void)
{
}
void CObjectIStream::EndChoiceVariant(void)
{
}

void CObjectIStream::ReadChoice(const CChoiceTypeInfo* choiceType,
                                TObjectPtr choicePtr)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
    BeginChoice(choiceType);
    BEGIN_OBJECT_FRAME(eFrameChoiceVariant);
    TMemberIndex index = BeginChoiceVariant(choiceType);
    _ASSERT(index != kInvalidMember);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    SetTopMemberId(variantInfo->GetId());

    variantInfo->ReadVariant(*this, choicePtr);

    EndChoiceVariant();
    END_OBJECT_FRAME();
    EndChoice();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipChoice(const CChoiceTypeInfo* choiceType)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
    BeginChoice(choiceType);
    BEGIN_OBJECT_FRAME(eFrameChoiceVariant);
    TMemberIndex index = BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember )
        ThrowError(fFormatError,"choice variant id expected");

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    SetTopMemberId(variantInfo->GetId());

    variantInfo->SkipVariant(*this);

    EndChoiceVariant();
    END_OBJECT_FRAME();
    EndChoice();
    END_OBJECT_FRAME();
}

void CObjectIStream::ReadAlias(const CAliasTypeInfo* aliasType,
                               TObjectPtr aliasPtr)
{
    ReadNamedType(aliasType, aliasType->GetPointedType(),
        aliasType->GetDataPtr(aliasPtr));
}

void CObjectIStream::SkipAlias(const CAliasTypeInfo* aliasType)
{
    SkipNamedType(aliasType, aliasType->GetPointedType());
}

///////////////////////////////////////////////////////////////////////
//
// CObjectIStream::ByteBlock
//

CObjectIStream::ByteBlock::ByteBlock(CObjectIStream& in)
    : m_Stream(in), m_KnownLength(false), m_Ended(false), m_Length(1)
{
    in.BeginBytes(*this);
}

CObjectIStream::ByteBlock::~ByteBlock(void)
{
    if ( !m_Ended ) {
        try {
            GetStream().Unended("byte block not fully read");
        }
        catch (...) {
            ERR_POST("unended byte block");
        }
    }
}

void CObjectIStream::ByteBlock::End(void)
{
    _ASSERT(!m_Ended);
    if ( m_Length == 0 ) {
        GetStream().EndBytes(*this);
        m_Ended = true;
    }
}

size_t CObjectIStream::ByteBlock::Read(void* dst, size_t needLength,
                                       bool forceLength)
{
    size_t length;
    if ( KnownLength() ) {
        if ( m_Length < needLength )
            length = m_Length;
        else
            length = needLength;
    }
    else {
        if ( m_Length == 0 )
            length = 0;
        else
            length = needLength;
    }
    
    if ( length == 0 ) {
        if ( forceLength && needLength != 0 )
            GetStream().ThrowError(fReadError, "read fault");
        return 0;
    }

    length = GetStream().ReadBytes(*this, static_cast<char*>(dst), length);
    if ( KnownLength() )
        m_Length -= length;
    if ( forceLength && needLength != length )
        GetStream().ThrowError(fReadError, "read fault");
    return length;
}

///////////////////////////////////////////////////////////////////////
//
// CObjectIStream::CharBlock
//

CObjectIStream::CharBlock::CharBlock(CObjectIStream& in)
    : m_Stream(in), m_KnownLength(false), m_Ended(false), m_Length(1)
{
    in.BeginChars(*this);
}

CObjectIStream::CharBlock::~CharBlock(void)
{
    if ( !m_Ended ) {
        try {
            GetStream().Unended("char block not fully read");
        }
        catch (...) {
            ERR_POST("unended char block");
        }
    }
}

void CObjectIStream::CharBlock::End(void)
{
    _ASSERT(!m_Ended);
    if ( m_Length == 0 ) {
        GetStream().EndChars(*this);
        m_Ended = true;
    }
}

size_t CObjectIStream::CharBlock::Read(char* dst, size_t needLength,
                                       bool forceLength)
{
    size_t length;
    if ( KnownLength() ) {
        if ( m_Length < needLength )
            length = m_Length;
        else
            length = needLength;
    }
    else {
        if ( m_Length == 0 )
            length = 0;
        else
            length = needLength;
    }
    
    if ( length == 0 ) {
        if ( forceLength && needLength != 0 )
            GetStream().ThrowError(fReadError, "read fault");
        return 0;
    }

    length = GetStream().ReadChars(*this, dst, length);
    if ( KnownLength() )
        m_Length -= length;
    if ( forceLength && needLength != length )
        GetStream().ThrowError(fReadError, "read fault");
    return length;
}


void CObjectIStream::EndBytes(const ByteBlock& /*b*/)
{
}

void CObjectIStream::EndChars(const CharBlock& /*b*/)
{
}

Int1 CObjectIStream::ReadInt1(void)
{
    Int4 data = ReadInt4();
    Int1 ret = Int1(data);
    if ( ret != data )
        ThrowError(fOverflow, "integer overflow");
    return ret;
}

Uint1 CObjectIStream::ReadUint1(void)
{
    Uint4 data = ReadUint4();
    Uint1 ret = Uint1(data);
    if ( ret != data )
        ThrowError(fOverflow, "integer overflow");
    return ret;
}

Int2 CObjectIStream::ReadInt2(void)
{
    Int4 data = ReadInt4();
    Int2 ret = Int2(data);
    if ( ret != data )
        ThrowError(fOverflow, "integer overflow");
    return ret;
}

Uint2 CObjectIStream::ReadUint2(void)
{
    Uint4 data = ReadUint4();
    Uint2 ret = Uint2(data);
    if ( ret != data )
        ThrowError(fOverflow, "integer overflow");
    return ret;
}

Int4 CObjectIStream::ReadInt4(void)
{
    Int8 data = ReadInt8();
    Int4 ret = Int4(data);
    if ( ret != data )
        ThrowError(fOverflow, "integer overflow");
    return ret;
}

Uint4 CObjectIStream::ReadUint4(void)
{
    Uint8 data = ReadUint8();
    Uint4 ret = Uint4(data);
    if ( ret != data )
        ThrowError(fOverflow, "integer overflow");
    return ret;
}

float CObjectIStream::ReadFloat(void)
{
    double data = ReadDouble();
#if defined(FLT_MIN) && defined(FLT_MAX)
    if ( data < FLT_MIN || data > FLT_MAX )
        ThrowError(fOverflow, "float overflow");
#endif
    return float(data);
}

#if SIZEOF_LONG_DOUBLE != 0
long double CObjectIStream::ReadLDouble(void)
{
    return ReadDouble();
}
#endif

char* CObjectIStream::ReadCString(void)
{
    string s;
    ReadString(s);
    return strdup(s.c_str());
}

void CObjectIStream::ReadStringStore(string& s)
{
    ReadString(s);
}

void CObjectIStream::ReadString(string& s,
                                CPackString& pack_string,
                                EStringType /* type */)
{
    ReadString(s);
    pack_string.Pack(s);
}

void CObjectIStream::SkipInt1(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint1(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt2(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint2(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt4(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint4(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt8(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint8(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipFloat(void)
{
    SkipFNumber();
}

void CObjectIStream::SkipDouble(void)
{
    SkipFNumber();
}

#if SIZEOF_LONG_DOUBLE != 0
void CObjectIStream::SkipLDouble(void)
{
    SkipFNumber();
}
#endif

void CObjectIStream::SkipCString(void)
{
    SkipString();
}

void CObjectIStream::SkipStringStore(void)
{
    SkipString();
}


char ReplaceVisibleChar(char c, EFixNonPrint fix_method, size_t at_line)
{
    _ASSERT(fix_method != eFNP_Allow);
    if ( fix_method != eFNP_Replace ) {
        string message = "Bad char in VisibleString" +
            ((at_line > 0) ?
             " starting at line " + NStr::UIntToString(at_line) :
             string("")) + ": " + NStr::IntToString(int(c) & 0xff);
        switch (fix_method) {
        case eFNP_ReplaceAndWarn:
            CNcbiDiag(eDiag_Error, eDPF_Default) << message << Endm;
            break;
        case eFNP_Throw:
            NCBI_THROW(CSerialException,eFormatError,message);
        case eFNP_Abort:
            CNcbiDiag(eDiag_Fatal, eDPF_Default) << message << Endm;
            break;
        default:
            break;
        }
    }
    return '#';
}


END_NCBI_SCOPE

/*
* $Log$
* Revision 1.135  2005/02/09 14:26:00  gouriano
* Check type name for being non-empty when skipping file header
*
* Revision 1.134  2005/02/01 21:47:14  grichenk
* Fixed warnings
*
* Revision 1.133  2004/12/06 18:27:38  gouriano
* Check if the file is empty in ReadFileHeader
*
* Revision 1.132  2004/11/30 15:06:04  dicuccio
* Added #include for ncbithr.hpp
*
* Revision 1.131  2004/09/22 13:32:17  kononenk
* "Diagnostic Message Filtering" functionality added.
* Added function SetDiagFilter()
* Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
* Module, class and function attribute added to CNcbiDiag and CException
* Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
* 	CDiagCompileInfo + fixes on derived classes and their usage
* Macro NCBI_MODULE can be used to set default module name in cpp files
*
* Revision 1.130  2004/08/30 18:19:39  gouriano
* Use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.129  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.128  2004/03/23 15:39:23  gouriano
* Added setup options for skipping unknown data members
*
* Revision 1.127  2004/03/18 20:18:05  gouriano
* remove redundant diagnostic message
*
* Revision 1.126  2004/03/05 20:29:38  gouriano
* make it possible to skip unknown data fields
*
* Revision 1.125  2004/02/09 18:22:34  gouriano
* enforced checking environment vars when setting initialization
* verification parameters
*
* Revision 1.124  2004/01/22 20:46:41  gouriano
* Added new exception error code (eMissingValue)
*
* Revision 1.123  2004/01/05 14:25:20  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.122  2003/12/31 21:02:58  gouriano
* added possibility to seek (when possible) in CObjectIStream
*
* Revision 1.121  2003/11/26 20:04:46  vasilche
* Method put in wrong file.
*
* Revision 1.120  2003/11/26 19:59:39  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.119  2003/11/24 14:10:05  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.118  2003/11/13 14:07:38  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.117  2003/10/24 15:54:28  grichenk
* Removed or blocked exceptions in destructors
*
* Revision 1.116  2003/10/21 21:08:46  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.115  2003/09/30 16:10:37  vasilche
* Added one more variant of object streams creation.
*
* Revision 1.114  2003/09/16 14:48:36  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.113  2003/09/10 20:56:45  gouriano
* added possibility to ignore missing mandatory members on reading
*
* Revision 1.112  2003/08/26 19:25:58  gouriano
* added possibility to discard a member of an STL container
* (from a read hook)
*
* Revision 1.111  2003/08/25 15:59:09  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.110  2003/08/19 18:32:38  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.109  2003/08/14 20:03:58  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.108  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.107  2003/05/20 14:25:22  vasilche
* Removed warnings on WorkShop.
*
* Revision 1.106  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.105  2003/05/15 17:44:38  gouriano
* added GetStreamOffset method
*
* Revision 1.104  2003/05/05 20:09:10  gouriano
* fixed "skipping" an object
*
* Revision 1.103  2003/03/26 16:14:22  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.102  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.101  2003/02/10 20:09:43  vasilche
* Restored _TRACE in CObjectIStream.
*
* Revision 1.100  2003/01/28 15:26:14  vasilche
* Added low level CObjectIStream::EndDelayBuffer(void);
*
* Revision 1.99  2003/01/22 18:53:28  gouriano
* corrected stream destruction
*
* Revision 1.98  2002/12/13 21:50:42  gouriano
* corrected reading of choices
*
* Revision 1.97  2002/11/26 22:12:02  gouriano
* corrected ReadClassSequential
*
* Revision 1.96  2002/11/14 20:58:19  gouriano
* added BeginChoice/EndChoice methods
*
* Revision 1.95  2002/11/04 21:29:20  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.94  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.93  2002/10/22 20:22:13  gouriano
* undo the prev change, but reduce severity of the ERR_POST to Info
*
* Revision 1.92  2002/10/22 19:02:59  gouriano
* commented out ERR_POST in CObjectIStream::SetFailFlags
*
* Revision 1.91  2002/10/15 13:48:00  gouriano
* modified to handle "standard" (generated from DTD) XML i/o
*
* Revision 1.90  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.89  2002/09/19 14:00:38  grichenk
* Implemented CObjectHookGuard for write and copy hooks
* Added DefaultRead/Write/Copy methods to base hook classes
*
* Revision 1.88  2002/09/09 18:14:02  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.87  2002/08/30 16:22:21  vasilche
* Removed excessive _TRACEs
*
* Revision 1.86  2001/10/22 15:17:41  grichenk
* Protected objects being read from destruction by temporary CRef<>-s
*
* Revision 1.85  2001/10/17 20:41:23  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.84  2001/10/15 23:30:10  vakatov
* Get rid of extraneous "break;" after "throw"
*
* Revision 1.83  2001/07/30 14:42:46  lavr
* eDiag_Trace and eDiag_Fatal always print as much as possible
*
* Revision 1.82  2001/07/25 19:14:25  grichenk
* Implemented functions for reading/writing parent classes
*
* Revision 1.81  2001/07/17 14:52:48  kholodov
* Fixed: replaced int argument by size_t in CheckVisibleChar() and
* ReplaceVisibleChar to avoid truncation in 64 bit mode.
*
* Revision 1.80  2001/07/16 16:24:08  grichenk
* Added "case eFNP_Allow:" to ReplaceVisibleChar()
*
* Revision 1.79  2001/06/07 17:12:50  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.78  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.77  2001/05/16 17:55:39  grichenk
* Redesigned support for non-blocking stream read operations
*
* Revision 1.76  2001/05/11 20:41:17  grichenk
* Added support for non-blocking stream reading
*
* Revision 1.75  2001/01/22 23:11:22  vakatov
* CObjectIStream::{Read,Skip}ClassSequential() -- use curr.member "pos"
*
* Revision 1.74  2001/01/05 20:10:50  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.73  2001/01/05 13:57:31  vasilche
* Fixed overloaded method ambiguity on Mac.
*
* Revision 1.72  2000/12/26 22:24:10  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.71  2000/12/26 17:26:16  vasilche
* Added one more Read() interface method.
*
* Revision 1.70  2000/12/15 21:29:01  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.69  2000/12/15 15:38:43  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.68  2000/11/01 20:38:37  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.67  2000/10/20 19:29:36  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.66  2000/10/20 15:51:40  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.65  2000/10/17 18:45:34  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.64  2000/10/04 19:18:58  vasilche
* Fixed processing floating point data.
*
* Revision 1.63  2000/10/03 17:22:43  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.62  2000/09/29 16:18:22  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.61  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.60  2000/09/18 20:00:23  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.59  2000/09/01 13:16:17  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.58  2000/08/15 19:44:48  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.57  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.56  2000/06/16 20:01:26  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.55  2000/06/16 18:04:11  thiessen
* avoid uncaught_exception() unimplemented on Mac/CodeWarrior
*
* Revision 1.54  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.53  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.52  2000/06/01 19:07:03  vasilche
* Added parsing of XML data.
*
* Revision 1.51  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.50  2000/05/09 16:38:38  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.49  2000/05/04 16:22:19  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.48  2000/05/03 14:38:14  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.47  2000/04/28 16:58:12  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.46  2000/04/13 14:50:26  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.45  2000/04/06 16:10:59  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.44  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.43  2000/03/14 14:42:30  vasilche
* Fixed error reporting.
*
* Revision 1.42  2000/03/10 17:59:20  vasilche
* Fixed error reporting.
* Added EOF bug workaround on MIPSpro compiler (not finished).
*
* Revision 1.41  2000/03/07 14:06:22  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.40  2000/02/17 20:02:43  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.39  2000/02/11 17:10:24  vasilche
* Optimized text parsing.
*
* Revision 1.38  2000/01/10 19:46:40  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.37  2000/01/05 19:43:53  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.36  1999/12/28 18:55:49  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.35  1999/12/20 15:29:35  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.34  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.33  1999/11/18 20:18:08  vakatov
* ReadObjectInfo() -- get rid of the CodeWarrior(MAC) C++ compiler warning
*
* Revision 1.32  1999/10/18 20:21:41  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.31  1999/10/04 16:22:16  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.30  1999/09/27 16:17:19  vasilche
* Fixed several incompatibilities with Windows
*
* Revision 1.29  1999/09/27 14:18:02  vasilche
* Fixed bug with overloaded constructors of Block.
*
* Revision 1.28  1999/09/26 05:19:02  vakatov
* FLT_MIN/MAX are not #define'd on some platforms
*
* Revision 1.27  1999/09/24 18:19:17  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.26  1999/09/23 21:16:07  vasilche
* Removed dependence on asn.h
*
* Revision 1.25  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.24  1999/09/23 18:56:58  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.23  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.22  1999/09/16 19:22:15  vasilche
* Removed some warnings under GCC.
*
* Revision 1.21  1999/09/14 18:54:17  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.20  1999/08/16 16:08:30  vasilche
* Added ENUMERATED type.
*
* Revision 1.19  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.18  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.17  1999/07/26 18:31:34  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.16  1999/07/22 17:33:49  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.15  1999/07/19 15:50:32  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.13  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.12  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.11  1999/07/02 21:31:54  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:29  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:53  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:54  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/16 20:35:30  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:19:48  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/11 19:14:57  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.4  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:52  vasilche
* Commit just in case.
*
* ===========================================================================
*/
