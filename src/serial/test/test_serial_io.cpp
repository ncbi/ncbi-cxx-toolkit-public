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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   .......
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serialbase.hpp>
#include <serial/serialimpl.hpp>
#include <serial/serialasn.hpp>
#include <serial/objhook.hpp>
#include <util/compress/stream_util.hpp>
#include <util/random_gen.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
#include <sstream>
#include <string>
#include <list>
#include <vector>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// Test ASN serialization stream I/O


class atomic_cout : public stringstream
{
public:
    ~atomic_cout(void) {
        cout << str();
    }
};


class CTestChoice : public CSerialObject
{
public:
    enum E_Choice {
        e_not_set,
        e_Str,
        e_Int
    };

    virtual ~CTestChoice() {}

    DECLARE_INTERNAL_TYPE_INFO();

    virtual void Reset(void) { if (m_choice != e_not_set) ResetSelection(); }
    void ResetSelection(void) { m_choice = e_not_set; }
    void Select(E_Choice index, EResetVariant reset = eDoResetVariant) { Select(index, reset, 0); }

    void Select(E_Choice index, EResetVariant reset, CObjectMemoryPool* pool)
    {
        if (reset == NCBI_NS_NCBI::eDoResetVariant || m_choice != index) {
            if (m_choice != e_not_set)
                ResetSelection();
            DoSelect(index, pool);
        }
    }

    void DoSelect(E_Choice index, NCBI_NS_NCBI::CObjectMemoryPool* pool)
    {
        m_choice = index;
    }

    void SetStr(const string& str)
    {
        Select(e_Str, NCBI_NS_NCBI::eDoNotResetVariant);
        m_Str = str;
    }

    void SetInt(int i)
    {
        Select(e_Int, NCBI_NS_NCBI::eDoNotResetVariant);
        m_Int = i;
    }

    int m_choice = 0;
    string m_Str;
    int m_Int = 0;
};

BEGIN_NAMED_CHOICE_INFO("test-choice", CTestChoice)
{
    ADD_NAMED_STD_CHOICE_VARIANT("str", m_Str);
    ADD_NAMED_STD_CHOICE_VARIANT("int", m_Int);
}
END_CHOICE_INFO


class CTestSubObject : public CSerialObject
{
public:
    virtual ~CTestSubObject(void) {}

    DECLARE_INTERNAL_TYPE_INFO();

    string m_Str;
    int m_Int = 0;
    vector<CRef<CTestSubObject>> m_Children;
};

BEGIN_CLASS_INFO(CTestSubObject)
{
    ADD_NAMED_STD_MEMBER("str", m_Str);
    ADD_NAMED_STD_MEMBER("int", m_Int)->SetOptional();
    ADD_NAMED_MEMBER("children", m_Children, STL_vector, (STL_CRef, (CLASS, (CTestSubObject))))->SetOptional();
}
END_CLASS_INFO


class CTestObject : public CSerialObject
{
public:
    virtual ~CTestObject(void) {}

    DECLARE_INTERNAL_TYPE_INFO();

    string m_Str;
    int m_Int = 0;
    CRef<CTestChoice> m_Choice;
    list<string> m_StrList;
    CRef<CTestSubObject> m_SubObj;
    vector<CRef<CTestSubObject>> m_Children;
    bool m_Bool = false;
};

BEGIN_CLASS_INFO(CTestObject)
{
    ADD_NAMED_STD_MEMBER("str", m_Str);
    ADD_NAMED_STD_MEMBER("int", m_Int)->SetOptional();
    ADD_NAMED_REF_MEMBER("choice", m_Choice, CTestChoice)->SetOptional();
    ADD_NAMED_MEMBER("str-list", m_StrList, STL_list, (STD, (string)))->SetOptional();
    ADD_NAMED_REF_MEMBER("sub-obj", m_SubObj, CTestSubObject)->SetOptional();
    ADD_NAMED_MEMBER("children", m_Children, STL_vector, (STL_CRef, (CLASS, (CTestSubObject))))->SetOptional();
    ADD_NAMED_STD_MEMBER("bool", m_Bool)->SetOptional();
}
END_CLASS_INFO


class CTestChoice2 : public CSerialObject
{
public:
    enum E_Choice {
        e_not_set,
        e_Str,
        e_Int,
        e_Bool
    };

    virtual ~CTestChoice2() {}

    DECLARE_INTERNAL_TYPE_INFO();

    virtual void Reset(void) { if (m_choice != e_not_set) ResetSelection(); }
    void ResetSelection(void) { m_choice = e_not_set; }
    void Select(E_Choice index, EResetVariant reset = eDoResetVariant) { Select(index, reset, 0); }

    void Select(E_Choice index, EResetVariant reset, CObjectMemoryPool* pool)
    {
        if (reset == NCBI_NS_NCBI::eDoResetVariant || m_choice != index) {
            if (m_choice != e_not_set)
                ResetSelection();
            DoSelect(index, pool);
        }
    }

    void DoSelect(E_Choice index, NCBI_NS_NCBI::CObjectMemoryPool* pool)
    {
        m_choice = index;
    }

    void SetStr(const string& str)
    {
        Select(e_Str, NCBI_NS_NCBI::eDoNotResetVariant);
        m_Str = str;
    }

    void SetInt(int i)
    {
        Select(e_Int, NCBI_NS_NCBI::eDoNotResetVariant);
        m_Int = i;
    }

    void SetBool(bool b)
    {
        Select(e_Bool, NCBI_NS_NCBI::eDoNotResetVariant);
        m_Bool = b;
    }

    int m_choice = 0;
    string m_Str;
    bool m_Bool = false;
    int m_Int = 0;
};

BEGIN_NAMED_CHOICE_INFO("test-choice2", CTestChoice2)
{
    ADD_NAMED_STD_CHOICE_VARIANT("str", m_Str);
    ADD_NAMED_STD_CHOICE_VARIANT("int", m_Int);
    ADD_NAMED_STD_CHOICE_VARIANT("bool", m_Bool);
}
END_CHOICE_INFO


class CTestObject2 : public CSerialObject
{
public:
    virtual ~CTestObject2(void) {}

    DECLARE_INTERNAL_TYPE_INFO();

    string m_Str;
    string m_Int;
};

BEGIN_CLASS_INFO(CTestObject2)
{
    ADD_NAMED_STD_MEMBER("str", m_Str);
    ADD_NAMED_STD_MEMBER("int", m_Int);
}
END_CLASS_INFO


enum EIOMethod {
    eIO_File,
    eIO_Socket
};


enum EDataSet {
    eData_Small,
    eData_Medium,
    eData_Large
};

enum EErrorType {
    eErr_None,
    eErr_JunkInObject,
    eErr_EofInObject,
    eErr_JunkInCompressed,
    eErr_EofInCompressed,
    eErr_UnexpectedMember,
    eErr_UnexpectedChoice,
    eErr_BadValue,

    // Socket errors
    eErr_Delay,
    eErr_Timeout
};

enum EErrorPos {
    eErrPos_Start,
    eErrPos_Middle,
    eErrPos_End,
    eErrPos_Random
};


static map<EIOMethod, string> IOMethodNames = {
    { eIO_File, "file" },
    { eIO_Socket, "socket" }
};

static map<ESerialDataFormat, string> SerialFormatNames = {
    { eSerial_AsnText, "asn" },
    { eSerial_AsnBinary, "asnb" },
    { eSerial_Xml, "xml" },
    { eSerial_Json, "json" }
};

static map<CCompressStream::EMethod, string> CompressMethodNames = {
    { CCompressStream::eNone, "none" },
    { CCompressStream::eBZip2, "bzip" },
    { CCompressStream::eLZO, "lzo" },
    { CCompressStream::eZip, "zip" },
    { CCompressStream::eGZipFile, "gzip" }
};

static map<EDataSet, string> DataSetNames = {
    { eData_Small, "small" },
    { eData_Medium, "medium" },
    { eData_Large, "large" }
};

static map<EErrorType, string> ErrorTypeNames = {
    { eErr_None, "none" },
    { eErr_JunkInObject, "junk-obj" },
    { eErr_EofInObject, "eof-obj" },
    { eErr_JunkInCompressed, "junk-comp" },
    { eErr_EofInCompressed, "eof-comp" },
    { eErr_UnexpectedMember, "bad-member" },
    { eErr_UnexpectedChoice, "bad-choice" },
    { eErr_BadValue, "bad-value" },
    { eErr_Delay, "delay" },
    { eErr_Timeout, "timeout" }
};

static map<EErrorPos, string> ErrorPositionNames = {
    { eErrPos_Start, "start" },
    { eErrPos_Middle, "middle" },
    { eErrPos_End, "end" },
    { eErrPos_Random, "random" }
};


static CRandom& Random()
{
    static unique_ptr<CRandom> s_Random;
    if (!s_Random) {
        const CArgs& args = CNcbiApplication::Instance()->GetArgs();
        if (args["seed"]) {
            s_Random.reset(new CRandom(args["seed"].AsInteger()));
        }
        else {
            s_Random.reset(new CRandom);
            s_Random->Randomize();
        }
    }
    return *s_Random;
}


struct STestCase
{
    int m_Index = 0;
    EIOMethod m_IO = eIO_File;
    EDataSet m_Dataset = eData_Small;
    ESerialDataFormat m_Format = eSerial_AsnText;
    CCompressStream::EMethod m_Comp = CCompressStream::eNone;
    int m_ObjCount = 1;
    EErrorType m_Err = eErr_None;
    EErrorPos m_ErrPos = eErrPos_Start;

    STestCase(void) {}

    STestCase(
        int idx,
        EIOMethod io,
        EDataSet dataset,
        ESerialDataFormat fmt,
        CCompressStream::EMethod comp,
        int obj_count,
        EErrorType err,
        EErrorPos err_pos) :
        m_Index(idx), m_IO(io), m_Dataset(dataset), m_Format(fmt), m_Comp(comp), m_ObjCount(obj_count), m_Err(err), m_ErrPos(err_pos) {}
};


static vector<EIOMethod> IOMethods = {
    eIO_File, eIO_Socket
};

static vector<ESerialDataFormat> SerialFormats = {
    eSerial_AsnText, eSerial_AsnBinary, eSerial_Xml, eSerial_Json
};

static vector<CCompressStream::EMethod> CompressMethods = {
    CCompressStream::eNone, CCompressStream::eBZip2,  CCompressStream::eLZO,
    CCompressStream::eZip, CCompressStream::eGZipFile
};

static vector<EDataSet> DataSets = {
    eData_Small, eData_Medium, eData_Large
};

static vector<EErrorType> ErrorTypes = {
    eErr_None, eErr_JunkInObject, eErr_EofInObject,
    eErr_JunkInCompressed, eErr_EofInCompressed,
    eErr_UnexpectedMember, eErr_UnexpectedChoice, eErr_BadValue,
    eErr_Delay, eErr_Timeout
};

static vector<EErrorPos> ErrorPositions = {
    eErrPos_Start, eErrPos_Middle, eErrPos_End, eErrPos_Random
};

static int ObjectsCountMin = 1;
static int ObjectsCountMax = 2;
static int RandomValue = -1;
// NOTE: Double-quotes should be included to prevent false-positive tests
// when junk is inserted inside a string in text format.
static string JunkData = "\"!@#$%junkdatainthemiddleofaserialobject&*()+\\";
static int SockTimeoutMs;
static bool Verbose = false;

static map<EDataSet, CRef<CTestObject>> TestObjects;
static list<STestCase> TestCases;
DEFINE_STATIC_FAST_MUTEX(CaseMutex);
static int TestThreads = 1;


class CTestWriter
{
public:
    CTestWriter(const STestCase& test_case);
    ~CTestWriter(void);

    void WriteData(ostream& out_str);

    void PostTimeout(void) {
        m_DelaySema.Post();
    }

    int GetTimeout() const;

    static CRef<CTestObject> CreateObject(EDataSet dataset);

private:
    static CRef<CTestObject> CreateSmallObject(void);
    static CRef<CTestObject> CreateMediumObject(void);
    static CRef<CTestObject> CreateLargeObject(void);
    static CRef<CTestSubObject> CreateSubObject(int children);

    void x_WriteObjectError(void);
    void x_WriteCompressedError(void);
    void x_WriteSocketError(void);
    void x_WriteUnexpectedMember(void);
    void x_WriteUnexpectedChoice(void);
    void x_WriteBadValue(void);

    CSemaphore m_DelaySema;
    ostream* m_Out;
    stringstream m_StrStream;
    unique_ptr<CCompressOStream> m_CompStream;
    unique_ptr<CObjectOStream> m_ObjStream;
    const STestCase& m_Case;
};


CTestWriter::CTestWriter(const STestCase& test_case)
    : m_DelaySema(0, kMax_UInt),
      m_Out(nullptr),
      m_Case(test_case)
{
}


CTestWriter::~CTestWriter(void)
{
}


CRef<CTestObject> CTestWriter::CreateObject(EDataSet dataset)
{
    switch (dataset) {
    case eData_Small: return CreateSmallObject();
    case eData_Medium: return CreateMediumObject();
    case eData_Large: return CreateLargeObject();
    }
    return CRef<CTestObject>();
}


CRef<CTestSubObject> CTestWriter::CreateSubObject(int children)
{
    static int s_NextId = 0;
    CRef<CTestSubObject> obj(new CTestSubObject);
    obj->m_Int = ++s_NextId;
    obj->m_Str = "sub-" + NStr::NumericToString(obj->m_Int);
    for (int i = 0; i < children; ++i) obj->m_Children.push_back(CreateSubObject(0));
    return obj;
}


CRef<CTestObject> CTestWriter::CreateSmallObject(void)
{
    // Less than 1K ASN.1 binary.
    CRef<CTestObject> obj(new CTestObject);
    obj->m_Str = "small";
    obj->m_Int = -1;
    obj->m_Choice.Reset(new CTestChoice);
    obj->m_Choice->SetInt(999);
    obj->m_SubObj = CreateSubObject(0);
    for (int i = 0; i < 5; ++i) obj->m_StrList.push_back(NStr::NumericToString(i));
    for (int i = 0; i < 3; ++i) obj->m_Children.push_back(CreateSubObject(0));
    obj->m_Bool = true;
    return obj;
}


CRef<CTestObject> CTestWriter::CreateMediumObject()
{
    // Less than 1M ASN.1 binary.
    CRef<CTestObject> obj(new CTestObject);
    obj->m_Str = "medium";
    obj->m_Int = -1;
    obj->m_Choice.Reset(new CTestChoice);
    obj->m_Choice->SetInt(999);
    obj->m_SubObj = CreateSubObject(10);
    for (int i = 0; i < 100; ++i) obj->m_StrList.push_back(NStr::NumericToString(i));
    for (int i = 0; i < 10; ++i) obj->m_Children.push_back(CreateSubObject(10));
    obj->m_Bool = true;
    return obj;
}


CRef<CTestObject> CTestWriter::CreateLargeObject()
{
    // A few megs ASN.1 binary.
    CRef<CTestObject> obj(new CTestObject);
    obj->m_Str = "large";
    obj->m_Int = -1;
    obj->m_Choice.Reset(new CTestChoice);
    obj->m_Choice->SetInt(999);
    obj->m_SubObj = CreateSubObject(100);
    for (int i = 0; i < 1000; ++i) obj->m_StrList.push_back(NStr::NumericToString(i));
    for (int i = 0; i < 100; ++i) obj->m_Children.push_back(CreateSubObject(100));
    obj->m_Bool = true;
    return obj;
}


int CTestWriter::GetTimeout() const
{
    int ret = SockTimeoutMs;
    if (m_Case.m_Dataset == eData_Medium) {
        ret *= 5;
    }
    else if (m_Case.m_Dataset == eData_Large) {
        ret *= 50;
    }
    return ret;
}


void CTestWriter::WriteData(ostream& sout)
{
    m_Out = &sout;
    if (m_Case.m_Err == eErr_JunkInCompressed || m_Case.m_Err == eErr_EofInCompressed) {
        // Need to create compressed stream over temporary string stream to modify it.
        m_CompStream.reset(new CCompressOStream(m_StrStream, m_Case.m_Comp));
    }
    else if (m_Case.m_Comp != CCompressStream::eNone) {
        m_CompStream.reset(new CCompressOStream(*m_Out, m_Case.m_Comp));
    }
    m_ObjStream.reset(CObjectOStream::Open(m_Case.m_Format, m_CompStream ? *m_CompStream : *m_Out));
    // Write good objects, if any.
    for (int i = 1; i < m_Case.m_ObjCount; ++i) {
        *m_ObjStream << *TestObjects[m_Case.m_Dataset];
    }

    switch (m_Case.m_Err) {
    case eErr_JunkInObject:
    case eErr_EofInObject:
        x_WriteObjectError();
        break;
    case eErr_JunkInCompressed:
    case eErr_EofInCompressed:
    {
        x_WriteCompressedError();
        break;
    }
    case eErr_Delay:
    case eErr_Timeout:
        x_WriteSocketError();
        break;
    case eErr_UnexpectedMember:
        x_WriteUnexpectedMember();
        break;
    case eErr_UnexpectedChoice:
        x_WriteUnexpectedChoice();
        break;
    case eErr_BadValue:
        x_WriteBadValue();
        break;
    default: // No error
        *m_ObjStream << *TestObjects[m_Case.m_Dataset];
        // NOTE: If object stream is not closed before Finalize(), sometimes an error is reported:
        // Error: (802.4) [Cannot close serializing output stream] Exception: eStatus_Error: iostream stream error
        m_ObjStream.reset();
        if (m_CompStream) m_CompStream->Finalize();
        break;
    }
    try {
        m_Out->flush();
    }
    catch (...) {}
    try {
        m_CompStream.reset();
    }
    catch (...) {}
    m_Out = nullptr;
}


void CTestWriter::x_WriteObjectError(void)
{
    stringstream str_stream;
    unique_ptr<CObjectOStream> obj_stream(CObjectOStream::Open(m_Case.m_Format, str_stream));
    *obj_stream << *TestObjects[m_Case.m_Dataset];
    Int8 len = str_stream.tellp();
    // Adjust for CR/LF in text mode
    if (m_Case.m_Format != eSerial_AsnBinary) --len;
    Int8 pos;
    switch (m_Case.m_ErrPos) {
    case eErrPos_Start:
        pos = 0;
        break;
    case eErrPos_Middle:
        pos = len / 2;
        break;
    case eErrPos_End:
        pos = len - 1;
        break;
    case eErrPos_Random:
        pos = RandomValue < 0 ? Random().GetRand(0, len - 1) : RandomValue;
        break;
    }

    string data;
    if (m_Case.m_Err == eErr_JunkInObject) {
        { atomic_cout() << "CASE-" << m_Case.m_Index << " :   Inserting junk into object stream at position " << pos << endl; }
        str_stream.seekp(pos);
        str_stream << JunkData;
        data = str_stream.str();
    }
    else if (m_Case.m_Err == eErr_EofInObject) {
        { atomic_cout() << "CASE-" << m_Case.m_Index << " :   Truncating object at position " << pos << endl; }
        data = str_stream.str().substr(0, pos);
    }
    try {
        (m_CompStream ? *m_CompStream : *m_Out) << data;
    }
    catch (...) {}
    try {
        m_ObjStream.reset();
        if (m_CompStream) m_CompStream->Finalize();
    }
    catch (...) {}
}


void CTestWriter::x_WriteCompressedError(void)
{
    *m_ObjStream << *TestObjects[m_Case.m_Dataset];
    m_ObjStream.reset();
    m_CompStream->Finalize();

    Int8 len = m_StrStream.tellp();
    // NOTE: Truncating a few last bytes of a compressed stream does not always result in an error.
    len -= 32;
    Int8 pos = 0;
    switch (m_Case.m_ErrPos) {
    case eErrPos_Start:
        pos = 0;
        break;
    case eErrPos_Middle:
        pos = len / 2;
        break;
    case eErrPos_End:
        pos = len;
        break;
    case eErrPos_Random:
        pos = RandomValue < 0 ? Random().GetRand(0, len) : RandomValue;
        break;
    }

    switch (m_Case.m_Err) {
    case eErr_JunkInCompressed:
    {
        { atomic_cout() << "CASE-" << m_Case.m_Index << " :   Inserting junk into compressed stream at position " << pos << endl; }
        m_StrStream.seekp(pos);
        m_StrStream << JunkData;
        *m_Out << m_StrStream.str();
        break;
    }
    case eErr_EofInCompressed:
    {
        { atomic_cout() << "CASE-" << m_Case.m_Index << " :   Truncating compressed stream at position " << pos << endl; }
        *m_Out << m_StrStream.str().substr(0, pos);
        break;
    }
    default:
        // This should never happen.
        break;
    }
}


void CTestWriter::x_WriteSocketError(void)
{
    m_ObjStream.reset(); // Will use in-memory object stream instead.
    try {
        if (m_CompStream) m_CompStream->flush();
        m_Out->flush();
    }
    catch (...) {
        return;
    }

    stringstream str_stream;
    unique_ptr<CObjectOStream> obj_stream(CObjectOStream::Open(m_Case.m_Format, str_stream));
    *obj_stream << *TestObjects[m_Case.m_Dataset];
    obj_stream.reset();

    Int8 len = str_stream.tellp();
    // NOTE: Truncating a few last bytes of a compressed stream does not always result in an error.
    len -= 32;
    Int8 pos = 0;
    switch (m_Case.m_ErrPos) {
    case eErrPos_Start:
        pos = 0;
        break;
    case eErrPos_Middle:
        pos = len / 2;
        break;
    case eErrPos_End:
        pos = len;
        break;
    case eErrPos_Random:
        pos = RandomValue < 0 ? Random().GetRand(0, len) : RandomValue;
        break;
    }
    string data = str_stream.str();
    if (m_CompStream) {
        try {
            m_CompStream->write(data.c_str(), pos);
            m_CompStream->flush();
        }
        catch (...) {
            return;
        }
    }
    else {
        m_Out->write(data.c_str(), pos);
        m_Out->flush();
    }
    unsigned long ms = GetTimeout();
    if (m_Case.m_Err == eErr_Delay) {
        ms *= 0.4;
    }
    else {
        // NOTE: Need delay of more than two timeouts. The reader socket
        // can read some data already sent, wait for one timeout and return
        // the short read without errors (warning only). The fatal timeout
        // will happen only on the second read.
        ms *= 3;
    }
    { atomic_cout() << "CASE-" << m_Case.m_Index << " :   Adding " << ms << "ms delay at position " << pos << endl; }
    // NOTE: Don not wait for the semaphore in 'delay' tests with compression.
    // Compressed streams are not fully flushed before the delay, so the reader
    // fails to read the first object and never signals timeout start for the second one.
    if (m_Case.m_Err == eErr_Timeout && m_Case.m_Comp != CCompressStream::eNone) {
        if (!m_DelaySema.TryWait(5, 0)) {
            try {
                if (m_CompStream) m_CompStream->Finalize();
            }
            catch (...) {}
            return;
        }
    }
    SleepMilliSec(ms);
    // If timeout occurs, the client may have closed the socket and write() will fail.
    // Ignore the exception.
    try {
        if (m_CompStream) {
            m_CompStream->write(data.c_str() + pos, data.size() - pos);
            m_CompStream->Finalize();
        }
        else {
            m_Out->write(data.c_str() + pos, data.size() - pos);
            m_Out->flush();
        }
    }
    catch (...) {}
}


class CTestWriteMemberHook : public CWriteClassMemberHook
{
public:
    CTestWriteMemberHook(int wrong_index) : m_WrongIndex(wrong_index) {}
    virtual ~CTestWriteMemberHook() {}

    void WriteClassMember(CObjectOStream& out,
        const CConstObjectInfoMI& member) override
    {
        // Output the first member regardless of the actual one.
        const CConstObjectInfo& obj_info = member.GetClassObject();
        const CConstObjectInfoMI& wrong_member = obj_info.GetClassMemberIterator(m_WrongIndex);
        DefaultWrite(out, wrong_member);
    }

private:
    int m_WrongIndex;
};


void CTestWriter::x_WriteUnexpectedMember(void)
{
    string member_name; // member to hook
    int wrong_index = 1; // index of member to be used instead
    CObjectTypeInfo type_info = CType<CTestObject>();
    switch (m_Case.m_ErrPos) {
    case eErrPos_Start:
        member_name = "str";
        wrong_index = 2;
        break;
    case eErrPos_Middle:
        // ERROR: When exception is thrown, the message is misleading:
        // "str": unexpected member, should be one of: "str" "int" ...
        member_name = "sub-obj";
        break;
    case eErrPos_End:
        member_name = "bool";
        break;
    case eErrPos_Random:
    {
        size_t count = type_info.GetClassTypeInfo()->GetMembers().Size();
        size_t idx = RandomValue < 0 ? Random().GetRandIndexSize_t(count - 1) + 1 : RandomValue;
        member_name = type_info.GetMemberIterator(idx).GetAlias();
        wrong_index = idx == 1 ? count : 1;
    }
    }

    if (Verbose) {
        atomic_cout() << "CASE-" << m_Case.m_Index << " :   Replacing member '" << member_name
            << "' with '" << type_info.GetMemberIterator(wrong_index).GetAlias()
            << "', index=" << wrong_index << endl;
    }

    try {
        CTestWriteMemberHook hook(wrong_index);
        CObjectHookGuard<CTestObject> guard(member_name, hook, m_ObjStream.get());
        *m_ObjStream << *TestObjects[m_Case.m_Dataset];
    }
    catch (...) {}
    try {
        m_ObjStream.reset();
        if (m_CompStream) m_CompStream->Finalize();
    }
    catch (...) {}
}


class CTestWriteChoiceHook : public CWriteObjectHook
{
public:
    CTestWriteChoiceHook() {}
    virtual ~CTestWriteChoiceHook() {}

    void WriteObject(CObjectOStream& out,
        const CConstObjectInfo& obj) override
    {
        CTestChoice2 ch;
        ch.SetBool(true);
        CConstObjectInfo obj_info(&ch, ch.GetTypeInfo());
        DefaultWrite(out, obj_info);
    }
};

void CTestWriter::x_WriteUnexpectedChoice(void)
{
    try {
        CTestWriteChoiceHook hook;
        CObjectHookGuard<CTestChoice> guard(hook, m_ObjStream.get());
        *m_ObjStream << *TestObjects[m_Case.m_Dataset];
    }
    catch (...) {}
    try {
        m_ObjStream.reset();
        if (m_CompStream) m_CompStream->Finalize();
    }
    catch (...) {}
}


class CTestWriteValueHook : public CWriteClassMemberHook
{
public:
    CTestWriteValueHook() {}
    virtual ~CTestWriteValueHook() {}

    void WriteClassMember(CObjectOStream& out,
        const CConstObjectInfoMI& member) override
    {
        CTestObject2 obj;
        obj.m_Int = "not-an-int";
        CConstObjectInfo obj_info(&obj, obj.GetTypeInfo());
        CConstObjectInfoMI bad_member = obj_info.GetClassMemberIterator(2);
        DefaultWrite(out, bad_member);
    }
};

void CTestWriter::x_WriteBadValue(void)
{
    try {
        CTestWriteValueHook hook;
        CObjectHookGuard<CTestObject> guard("int", hook, m_ObjStream.get());
        *m_ObjStream << *TestObjects[m_Case.m_Dataset];
    }
    catch (...) {}
    try {
        m_ObjStream.reset();
        if (m_CompStream) m_CompStream->Finalize();
    }
    catch (...) {}
}


static map<int, int> TestResults;
DEFINE_STATIC_FAST_MUTEX(ResultsMutex);

static void s_AddFailed(int case_idx) {
    CFastMutexGuard guard(ResultsMutex);
    TestResults[case_idx]++;
}


#define CHECK(idx, val) \
do { if (!(val)) { \
    s_AddFailed(idx); \
    atomic_cout() << "CASE-" << idx << " ERROR: expected condition '" << #val << "'" << endl; \
} } while (0)

#define CHECK_EQUAL(idx, val1, val2) \
do { if ((val1) != (val2)) { \
    s_AddFailed(idx); \
    atomic_cout() << "CASE-" << idx << " ERROR: " << #val1 << " != " << #val2 << " (" << val1 << " != " << val2 << ")" << endl; \
} } while (0)

#define CHECK_THROW(idx, expr, expt) \
do { \
    try { \
        expr; \
        s_AddFailed(idx); \
        atomic_cout() << "CASE-" << idx << " ERROR: no exception thrown by '" << #expr << "', expected " << #expt << endl; \
    } \
    catch (expt) {} \
    catch (exception& e) { \
        s_AddFailed(idx); \
        atomic_cout() << "CASE-" << idx << " ERROR: unexpected exception thrown by '" << #expr << "': " << e.what() << endl; \
    } \
} while (0)

#define CHECK_NO_THROW(idx, expr) \
do { \
    try {  expr; } \
    catch (exception& e) { \
        s_AddFailed(idx); \
        atomic_cout() << "CASE-" << idx << " ERROR: unexpected exception thrown by '" << #expr << "': " << e.what() << endl; \
    } \
} while (0)


static void s_ReadObjects(
    CTestWriter& writer,
    const CTestObject exp_obj,
    CObjectIStream& in,
    int obj_count,
    int& got)
{
    got = 0;
    for (int i = 1; i <= obj_count; ++i) {
        if (i == obj_count) writer.PostTimeout();
        CTestObject obj;
        in >> obj;
        // NOTE: It is possible that the object with junk reads back fine, e.g. if the junk falls
        // into a string. Just in case, make sure the object is the same as the one written.
        if (!exp_obj.Equals(obj)) {
            NCBI_THROW(CSerialException, eFail, "Read object not equal to the expected one");
        }
        got = i;
    }
}


static void s_CheckStream(
    CTestWriter& writer,
    istream& istr,
    const STestCase& test_case)
{
    unique_ptr<CDecompressIStream> zin;
    if (test_case.m_Comp != CCompressStream::eNone) {
        zin.reset(new CDecompressIStream(istr, test_case.m_Comp));
    }
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(test_case.m_Format, zin ? *zin : istr));

    int got = 0;
    if (Verbose) {
        int expected_objects = 0;
        switch (test_case.m_Err) {
        case eErr_None:
            expected_objects = test_case.m_ObjCount;
            break;
        case eErr_JunkInObject:
        case eErr_EofInObject:
        case eErr_UnexpectedMember:
        case eErr_UnexpectedChoice:
        case eErr_BadValue:
            expected_objects = test_case.m_ObjCount - 1;
            break;
        default:
            break;
        }
        try {
            s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got);
        }
        catch (exception& e) {
            atomic_cout() << "CASE-" << test_case.m_Index << " :   Exception caught: " << e.what() << endl;
        }
        if (expected_objects != 0 && got != expected_objects) {
            atomic_cout() << "CASE-" << test_case.m_Index << " :   Wrong number of objects read: " << got << ", expected " << expected_objects << endl;
        }
        else {
            atomic_cout() << "CASE-" << test_case.m_Index << " :   Read " << got << " objects of " << test_case.m_ObjCount << endl;
        }
    }
    else {
        switch (test_case.m_Err) {
        case eErr_None:
            CHECK_NO_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got));
            CHECK_EQUAL(test_case.m_Index, got, test_case.m_ObjCount);
            break;
        case eErr_JunkInObject:
        case eErr_EofInObject:
            CHECK_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got), CException);
            CHECK_EQUAL(test_case.m_Index, got, test_case.m_ObjCount - 1);
            break;
        case eErr_JunkInCompressed:
        case eErr_EofInCompressed:
            CHECK_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got), CException);
            break;
        case eErr_UnexpectedMember:
        case eErr_UnexpectedChoice:
            CHECK_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got), CException);
            CHECK_EQUAL(test_case.m_Index, got, test_case.m_ObjCount - 1);
            break;
        case eErr_BadValue:
            if (test_case.m_Format == eSerial_AsnBinary || test_case.m_Format == eSerial_Json) {
                CHECK_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got), CSerialException);
            }
            else {
                CHECK_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got), CUtilException);
            }
            CHECK_EQUAL(test_case.m_Index, got, test_case.m_ObjCount - 1);
            break;
        case eErr_Delay:
            CHECK_NO_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got));
            CHECK_EQUAL(test_case.m_Index, got, test_case.m_ObjCount);
            break;
        case eErr_Timeout:
            CHECK_THROW(test_case.m_Index, s_ReadObjects(writer, *TestObjects[test_case.m_Dataset], *in, test_case.m_ObjCount, got), CException);
            // NOTE: When writing small objects to a socket with a timeout, it may happen that even
            // the first object can not be read due to buffering.
            CHECK(test_case.m_Index, got < test_case.m_ObjCount);
            break;
        }
    }
}


class CWriterThread : public CThread
{
public:
    CWriterThread(CTestWriter& writer);

    short GetPort(void) const { return m_Port; }

protected:
    void* Main(void) override;

private:
    CTestWriter& m_Writer;
    CListeningSocket m_LSock;
    short m_Port = 4049;
};


CWriterThread::CWriterThread(CTestWriter& writer)
    : m_Writer(writer)
{
    for (int i = 0; i < 10000; ++i) {
        EIO_Status status = m_LSock.Listen(m_Port);
        if (status == eIO_Success) break;
        ++m_Port;
    }
}

void* CWriterThread::Main(void)
{
    CSocket sock;
    EIO_Status status = m_LSock.Accept(sock, kInfiniteTimeout);
    sock.DisableOSSendDelay();
    if (status == eIO_Success) {
        CConn_SocketStream sout(sock);
        m_Writer.WriteData(sout);
    }
    else {
        atomic_cout() << "Accept failed: " << IO_StatusStr(status) << endl;
    }
    m_LSock.Close();
    return nullptr;
}


class CTestThread : public CThread
{
public:
    CTestThread() {}

protected:
    void* Main(void) override;

private:
};


class CFileGuard
{
public:
    CFileGuard(const string& fname) : m_Filename(fname) {}
    ~CFileGuard(void) { CDirEntry(m_Filename).Remove(CDirEntry::fDir_Self | CDirEntry::fIgnoreMissing); }
private:
    string m_Filename;
};

void* CTestThread::Main(void)
{
    STestCase test_case;
    while (true) {
        {
            CFastMutexGuard guard(CaseMutex);
            if (TestCases.empty()) break;
            test_case = TestCases.front();
            TestCases.pop_front();
        }
        { atomic_cout() << "CASE-" << test_case.m_Index << " - test_serial_io"
            << " -io " << IOMethodNames[test_case.m_IO]
            << " -data " << DataSetNames[test_case.m_Dataset]
            << " -fmt " << SerialFormatNames[test_case.m_Format]
            << " -comp " << CompressMethodNames[test_case.m_Comp]
            << " -obj_count " << test_case.m_ObjCount
            << " -err " << ErrorTypeNames[test_case.m_Err]
            << " -err_pos " << ErrorPositionNames[test_case.m_ErrPos] << endl; }
        switch (test_case.m_IO) {
        case eIO_File:
        {
            CTestWriter writer(test_case);
            stringstream fnamestr;
            fnamestr << "iodata-" << test_case.m_Index;
            string fname = fnamestr.str();
            CFileGuard fguard(fname);
            {
                ofstream fout(fname,
                    (test_case.m_Format == eSerial_AsnBinary || test_case.m_Comp != CCompressStream::eNone) ? ios_base::binary : ios_base::openmode(0));
                writer.WriteData(fout);
            }
            {
                ifstream fin(fname,
                    (test_case.m_Format == eSerial_AsnBinary || test_case.m_Comp != CCompressStream::eNone) ? ios_base::binary : ios_base::openmode(0));
                s_CheckStream(writer, fin, test_case);
            }
            break;
        }
        case eIO_Socket:
        {
            CTestWriter writer(test_case);
            CRef<CWriterThread> thr(new CWriterThread(writer));
            thr->Run();
            STimeout timeout;
            int ms = writer.GetTimeout();
            timeout.sec = ms / 1000;
            timeout.usec = (ms % 1000) * 1000;
            CConn_SocketStream sstr("127.0.0.1", thr->GetPort(), 1, &timeout);
            s_CheckStream(writer, sstr, test_case);
            sstr.Close();
            thr->Join();
            break;
        }
        }
        int failed = 0;
        {
            CFastMutexGuard guard(ResultsMutex);
            failed = TestResults[test_case.m_Index];
        }
        if (failed != 0) {
            atomic_cout() << "CASE-" << test_case.m_Index << " completed with " << failed << " failures." << endl;
        }
    }
    return nullptr;
}


class CArgAllow_Set : public CArgAllow
{
public:
    template<class TKey> CArgAllow_Set(const map<TKey, string>& names)
    {
        for (auto it : names) {
            m_Names.insert(it.second);
        }
    }

protected:
    bool Verify(const string &value) const override
    {
        vector<string> values;
        NStr::Split(value, ",; ", values, NStr::fSplit_Tokenize);
        for (auto it : values) {
            if (m_Names.find(it) == m_Names.end()) return false;
        }
        return true;
    }

    string GetUsage(void) const override
    {
        if (m_Names.empty()) {
            return "ERROR:  Constraint with no names allowed(?!)";
        }

        string str = "any combination of: ";
        char sep = '"';
        for (auto it : m_Names) {
            str += sep;
            str += it;
            sep = ',';
        }
        str += '"';
        return str;
    }

private:
    typedef set<string> TNames;
    TNames m_Names;
};


template<class TKey> void s_ParseValues(
    const string& arg,
    const map<TKey, string>& names,
    vector<TKey>& selected)
{
    selected.clear();
    vector<string> split;
    NStr::Split(arg, ",;", split, NStr::fSplit_Tokenize);
    set<string> values(split.begin(), split.end());
    for (auto it : names) {
        if (values.find(it.second) != values.end()) selected.push_back(it.first);
    }
}


class CSerialIOTestApp : public CNcbiApplication
{
public:
    CSerialIOTestApp(void) {}
    ~CSerialIOTestApp(void) override {}

    void Init(void) override;
    int Run(void) override;
};


void CSerialIOTestApp::Init(void)
{
    CONNECT_Init(&GetConfig());

    auto_ptr<CArgDescriptions> descrs(new CArgDescriptions);

    descrs->AddOptionalKey("io", "IOMethods", "I/O methods", CArgDescriptions::eString);
    descrs->SetConstraint("io", new CArgAllow_Set(IOMethodNames));

    descrs->AddOptionalKey("fmt", "SerialFormats", "serial stream format", CArgDescriptions::eString);
    descrs->SetConstraint("fmt", new CArgAllow_Set(SerialFormatNames));

    descrs->AddOptionalKey("comp", "Compression", "compression method", CArgDescriptions::eString);
    descrs->SetConstraint("comp", new CArgAllow_Set(CompressMethodNames));

    descrs->AddOptionalKey("data", "DataSet", "dataset size", CArgDescriptions::eString);
    descrs->SetConstraint("data", new CArgAllow_Set(DataSetNames));

    descrs->AddOptionalKey("obj_count", "ObjectsCount", "number of objects in the dataset", CArgDescriptions::eInteger);
    descrs->SetConstraint("obj_count", &(*new CArgAllow_Integers(1, 999)));

    descrs->AddOptionalKey("err", "ErrType", "error type", CArgDescriptions::eString);
    descrs->SetConstraint("err", new CArgAllow_Set(ErrorTypeNames));

    descrs->AddOptionalKey("err_pos", "ErrPos", "error position", CArgDescriptions::eString);
    descrs->SetConstraint("err_pos", new CArgAllow_Set(ErrorPositionNames));

    descrs->AddDefaultKey("timeout", "TimeoutMs", "socket timeout base value", CArgDescriptions::eInteger, "100");
    descrs->SetConstraint("timeout", &(*new CArgAllow_Integers(0, 3600000)));

    descrs->AddDefaultKey("threads", "Threads", "number of threads to run", CArgDescriptions::eInteger, "16");
    descrs->SetConstraint("threads", &(*new CArgAllow_Integers(1, 999)));

    descrs->AddOptionalKey("seed", "Seed", "randomization seed", CArgDescriptions::eInteger);

    descrs->AddDefaultKey("junk", "Junk", "junk data string", CArgDescriptions::eString, "\"!@#$%junkdatainthemiddleofaserialobject&*()+\\");

    descrs->AddOptionalKey("rnd_val", "RandomValue", "value to be used instead of the random one", CArgDescriptions::eInteger);

    descrs->AddFlag("verbose", "Report more details", true);

    descrs->SetUsageContext(GetArguments().GetProgramBasename(),
        "Test for serial I/O streams", false);

    SetupArgDescriptions(descrs.release());
}


int CSerialIOTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    if (args["io"]) s_ParseValues(args["io"].AsString(), IOMethodNames, IOMethods);
    if (args["fmt"]) s_ParseValues(args["fmt"].AsString(), SerialFormatNames, SerialFormats);
    if (args["comp"]) s_ParseValues(args["comp"].AsString(), CompressMethodNames, CompressMethods);
    if (args["data"]) s_ParseValues(args["data"].AsString(), DataSetNames, DataSets);
    if (args["err"]) s_ParseValues(args["err"].AsString(), ErrorTypeNames, ErrorTypes);
    if (args["err_pos"]) s_ParseValues(args["err_pos"].AsString(), ErrorPositionNames, ErrorPositions);
    if (args["obj_count"]) {
        ObjectsCountMin = args["obj_count"].AsInteger();
        ObjectsCountMax = ObjectsCountMin;
    }
    if (args["rnd_val"]) {
        RandomValue = args["rnd_val"].AsInteger();
    }
    TestThreads = args["threads"].AsInteger();
    SockTimeoutMs = args["timeout"].AsInteger();
    JunkData = args["junk"].AsString();
    Verbose = args["verbose"].AsBoolean();
    if (!Verbose) {
        // Hide stream errors/warnings.
        SetDiagFilter(eDiagFilter_Post, "!(210.32,45,46,47,67,70,72,86) !(301.17,23) !(302.37) !(315.4,6,7,10) !(802.4,5,9) !(803.7)");
    }
    cout << "Threads: " << TestThreads << endl;
    cout << "Seed: " << Random().GetSeed() << endl;
    cout << "Socket timeout, ms: " << SockTimeoutMs << endl;

    int idx = 0;
    for (auto io : IOMethods) {
        for (auto dataset : DataSets) {
            TestObjects[dataset] = CTestWriter::CreateObject(dataset);
            for (auto fmt : SerialFormats) {
                for (auto comp : CompressMethods) {
                    for (int obj_count = ObjectsCountMin; obj_count <= ObjectsCountMax; ++obj_count) {
                        for (auto err : ErrorTypes) {

                            // Do not test compression errors if compression is disabled.
                            if (comp == CCompressStream::eNone &&
                                (err == eErr_JunkInCompressed || err == eErr_EofInCompressed)) continue;

                            // Not supported by file streams.
                            if (io == eIO_File && (err == eErr_Timeout || err == eErr_Delay)) continue;
                            for (auto err_pos : ErrorPositions) {

                                // Error positions are meaningless for some error types.
                                if ((err == eErr_None || err == eErr_UnexpectedChoice || err == eErr_BadValue)
                                    && err_pos != eErrPos_Start) continue;
                                TestCases.push_back(STestCase(++idx, io, dataset, fmt, comp, obj_count, err, err_pos));
                                TestResults[idx] = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    size_t total = TestCases.size();
    cout << "Running " << total << " test cases..." << endl;
    vector<CRef<CTestThread>> threads;
    for (int i = 0; i < TestThreads; ++i) {
        CRef<CTestThread> thr(new CTestThread());
        threads.push_back(thr);
        thr->Run();

    }
    for (auto thr : threads) {
        thr->Join();
    }
    size_t failed = 0;
    for (auto res : TestResults) {
        failed += res.second;
    }
    cout << "Finished " << total << " test cases, " << failed << " failures found." << endl;
    return failed;
}


END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CSerialIOTestApp().AppMain(argc, argv);
}
