#ifndef OBJOSTR__HPP
#define OBJOSTR__HPP

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
#include <corelib/ncbifloat.h>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <util/strbuffer.hpp>
#include <serial/objlist.hpp>
#include <serial/objstack.hpp>
#include <serial/hookdatakey.hpp>
#include <serial/objhook.hpp>
#include <serial/pathhook.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


struct asnio;

BEGIN_NCBI_SCOPE

class CMemberId;
class CDelayBuffer;

class CConstObjectInfo;
class CConstObjectInfoMI;

class CWriteObjectHook;
class CWriteClassMembersHook;
class CWriteChoiceVariantHook;

class CContainerTypeInfo;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CObjectStreamCopier;
class CAliasTypeInfo;

class CWriteObjectInfo;
class CWriteObjectList;

class NCBI_XSERIAL_EXPORT CObjectOStream : public CObjectStack
{
public:
    typedef size_t TObjectIndex;

    // open methods
    static CObjectOStream* Open(ESerialDataFormat format,
                                const string& fileName,
                                TSerialOpenFlags openFlags = 0);
    static CObjectOStream* Open(const string& fileName,
                                ESerialDataFormat format);
    static CObjectOStream* Open(ESerialDataFormat format,
                                CNcbiOstream& outStream,
                                bool deleteOutStream = false);

    void Close(void);
    void ResetLocalHooks(void);

    // when enabled, stream verifies data on output
    // and throws CUnassignedMember exception

    // for this particular stream
    void SetVerifyData(ESerialVerifyData verify);
    ESerialVerifyData GetVerifyData(void) const;
    // for streams created by the current thread
    static  void SetVerifyDataThread(ESerialVerifyData verify);
    // for streams created by the current process
    static  void SetVerifyDataGlobal(ESerialVerifyData verify);

    void SetUseIndentation(bool set);
    bool GetUseIndentation(void) const;

    // constructors
protected:
    CObjectOStream(ESerialDataFormat format,
                   CNcbiOstream& out, bool deleteOut = false);

public:
    virtual ~CObjectOStream(void);

    // get data format
    ESerialDataFormat GetDataFormat(void) const;

    // USER INTERFACE
    // flush buffer
    void FlushBuffer(void);
    // flush buffer and underlying stream
    void Flush(void);
    // perform default flush defined by flags
    void DefaultFlush(void);

    // root writer
    void Write(const CConstObjectInfo& object);
    void Write(TConstObjectPtr object, TTypeInfo type);
    void Write(TConstObjectPtr object, const CTypeRef& type);

    // subtree writer
    void WriteObject(const CConstObjectInfo& object);
    void WriteObject(TConstObjectPtr object, TTypeInfo typeInfo);

    void CopyObject(TTypeInfo objectType,
                    CObjectStreamCopier& copier);
    
    void WriteSeparateObject(const CConstObjectInfo& object);

    // internal writer
    void WriteExternalObject(TConstObjectPtr object, TTypeInfo typeInfo);

    // member interface
    void WriteClassMember(const CConstObjectInfoMI& member);

    // choice variant interface
    void WriteChoiceVariant(const CConstObjectInfoCV& member);

    // Separator management
    string GetSeparator(void) const;
    void SetSeparator(const string sep);
    // Controls auto-output of the separator after each object. By default
    // this flag is true for text ASN.1 streams only.
    bool GetAutoSeparator(void);
    void SetAutoSeparator(bool value);

    CObjectOStream& operator<< (CObjectOStream& (*mod)(CObjectOStream& os));
    friend CObjectOStream& Separator(CObjectOStream& os);

    // END OF USER INTERFACE

    // low level writers

    // root object
    virtual void WriteFileHeader(TTypeInfo type);
    virtual void EndOfWrite(void);

    // std C types readers
    // bool
    void WriteStd(const bool& data);

    // char
    void WriteStd(const char& data);

    // integer number
    void WriteStd(const signed char& data);
    void WriteStd(const unsigned char& data);
    void WriteStd(const short& data);
    void WriteStd(const unsigned short& data);
    void WriteStd(const int& data);
    void WriteStd(const unsigned int& data);
#if SIZEOF_LONG == 4
    void WriteStd(const long& data);
    void WriteStd(const unsigned long& data);
#endif
    void WriteStd(const Int8& data);
    void WriteStd(const Uint8& data);

    // float number
    void WriteStd(const float& data);
    void WriteStd(const double& data);
#if SIZEOF_LONG_DOUBLE != 0
    void WriteStd(const long double& data);
#endif

    // string
    void WriteStd(const string& data);
    void WriteStd(const CStringUTF8& data);

    // C string; VisualAge can't cope with refs here.
    void WriteStd(const char* const data);
    void WriteStd(char* const data);

    // primitive writers
    // bool
    virtual void WriteBool(bool data) = 0;

    // char
    virtual void WriteChar(char data) = 0;

    // integer numbers
    virtual void WriteInt4(Int4 data) = 0;
    virtual void WriteUint4(Uint4 data) = 0;
    virtual void WriteInt8(Int8 data) = 0;
    virtual void WriteUint8(Uint8 data) = 0;

    // float numbers
    virtual void WriteFloat(float data);
    virtual void WriteDouble(double data) = 0;
#if SIZEOF_LONG_DOUBLE != 0
    virtual void WriteLDouble(long double data);
#endif

    // string
    virtual void WriteString(const string& str,
                             EStringType type = eStringTypeVisible) = 0;
    virtual void CopyString(CObjectIStream& in) = 0;

    // StringStore
    virtual void WriteStringStore(const string& data) = 0;
    virtual void CopyStringStore(CObjectIStream& in) = 0;

    // C string
    virtual void WriteCString(const char* str) = 0;

    // NULL
    virtual void WriteNull(void) = 0;

    // enum
    virtual void WriteEnum(const CEnumeratedTypeValues& values,
                           TEnumValueType value) = 0;
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in) = 0;

    // any content object
    virtual void WriteAnyContentObject(const CAnyContentObject& obj) = 0;
    virtual void CopyAnyContentObject(CObjectIStream& in) = 0;

    // delayed buffer
    virtual bool Write(CByteSource& source);

    // low level readers:
    enum EFailFlags {
        fNoError       = 0,             eNoError     = fNoError,
        fEOF           = 1 << 0,        eEOF         = fEOF,
        fWriteError    = 1 << 1,        eWriteError  = fWriteError,
        fFormatError   = 1 << 2,        eFormatError = fFormatError,
        fOverflow      = 1 << 3,        eOverflow    = fOverflow,
        fInvalidData   = 1 << 4,        eInvalidData = fInvalidData,
        fIllegalCall   = 1 << 5,        eIllegalCall = fIllegalCall,
        fFail          = 1 << 6,        eFail        = fFail,
        fNotOpen       = 1 << 7,        eNotOpen     = fNotOpen,
        fUnassigned    = 1 << 8,        eUnassigned  = fUnassigned
    };
    typedef int TFailFlags;
    bool fail(void) const;
    TFailFlags GetFailFlags(void) const;
    TFailFlags SetFailFlagsNoError(TFailFlags flags);
    TFailFlags SetFailFlags(TFailFlags flags, const char* message);
    TFailFlags ClearFailFlags(TFailFlags flags);
    bool InGoodState(void);
    virtual string GetStackTrace(void) const;
    virtual string GetPosition(void) const;
    CNcbiStreamoff GetStreamOffset(void) const;

    enum EFlags {
        fFlagNone                = 0,
        eFlagNone                = fFlagNone,
        fFlagAllowNonAsciiChars  = 1 << 0,
        eFlagAllowNonAsciiChars  = fFlagAllowNonAsciiChars,
        fFlagNoAutoFlush         = 1 << 1
    };
    typedef int TFlags;
    TFlags GetFlags(void) const;
    TFlags SetFlags(TFlags flags);
    TFlags ClearFlags(TFlags flags);

    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const char* message);
    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const string& message);
#define ThrowError(flag, mess) ThrowError1(DIAG_COMPILE_INFO,flag,mess)

    class ByteBlock;
    friend class ByteBlock;
    class NCBI_XSERIAL_EXPORT ByteBlock
    {
    public:
        ByteBlock(CObjectOStream& out, size_t length);
        ~ByteBlock(void);

        CObjectOStream& GetStream(void) const;

        size_t GetLength(void) const;

        void Write(const void* bytes, size_t length);

        void End(void);

    private:
        CObjectOStream& m_Stream;
        size_t m_Length;
        bool m_Ended;
    };
    class CharBlock;
    friend class CharBlock;
    class NCBI_XSERIAL_EXPORT CharBlock
    {
    public:
        CharBlock(CObjectOStream& out, size_t length);
        ~CharBlock(void);

        CObjectOStream& GetStream(void) const;

        size_t GetLength(void) const;

        void Write(const char* chars, size_t length);

        void End(void);

    private:
        CObjectOStream& m_Stream;
        size_t m_Length;
        bool m_Ended;
    };

#if HAVE_NCBI_C
    class NCBI_XSERIAL_EXPORT AsnIo
    {
    public:
        AsnIo(CObjectOStream& out, const string& rootTypeName);
        ~AsnIo(void);

        CObjectOStream& GetStream(void) const;

        void Write(const char* data, size_t length);

        void End(void);

        operator asnio*(void);
        asnio* operator->(void);
        const string& GetRootTypeName(void) const;

    private:
        CObjectOStream& m_Stream;
        string m_RootTypeName;
        asnio* m_AsnIo;
        bool m_Ended;

    public:
        size_t m_Count;
    };
    friend class AsnIo;
public:
#endif

    // mid level I/O
    // named type (alias)
    MLIOVIR void WriteNamedType(TTypeInfo namedTypeInfo,
                                TTypeInfo typeInfo, TConstObjectPtr object);
    // container
    MLIOVIR void WriteContainer(const CContainerTypeInfo* containerType,
                                TConstObjectPtr containerPtr);
    void WriteContainerElement(const CConstObjectInfo& element);
    // class
    void WriteClassRandom(const CClassTypeInfo* classType,
                          TConstObjectPtr classPtr);
    void WriteClassSequential(const CClassTypeInfo* classType,
                              TConstObjectPtr classPtr);
    MLIOVIR void WriteClass(const CClassTypeInfo* objectType,
                            TConstObjectPtr objectPtr);
    MLIOVIR void WriteClassMember(const CMemberId& memberId,
                                  TTypeInfo memberType,
                                  TConstObjectPtr memberPtr);
    MLIOVIR bool WriteClassMember(const CMemberId& memberId,
                                  const CDelayBuffer& buffer);
    // choice
    MLIOVIR void WriteChoice(const CChoiceTypeInfo* choiceType,
                             TConstObjectPtr choicePtr);
    // alias
    MLIOVIR void WriteAlias(const CAliasTypeInfo* aliasType,
                            TConstObjectPtr aliasPtr);

    // COPY
    // named type (alias)
    MLIOVIR void CopyNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo,
                               CObjectStreamCopier& copier);
    // container
    MLIOVIR void CopyContainer(const CContainerTypeInfo* containerType,
                               CObjectStreamCopier& copier);
    // class
    MLIOVIR void CopyClassRandom(const CClassTypeInfo* objectType,
                                 CObjectStreamCopier& copier);
    MLIOVIR void CopyClassSequential(const CClassTypeInfo* objectType,
                                     CObjectStreamCopier& copier);
    // choice
    MLIOVIR void CopyChoice(const CChoiceTypeInfo* choiceType,
                            CObjectStreamCopier& copier);
    // alias
    MLIOVIR void CopyAlias(const CAliasTypeInfo* AliasType,
                            CObjectStreamCopier& copier);

    // low level I/O
    // named type
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    // container
    virtual void BeginContainer(const CContainerTypeInfo* containerType) = 0;
    virtual void EndContainer(void);
    virtual void BeginContainerElement(TTypeInfo elementType);
    virtual void EndContainerElement(void);

    // class
    virtual void BeginClass(const CClassTypeInfo* classInfo) = 0;
    virtual void EndClass(void);

    virtual void BeginClassMember(const CMemberId& id) = 0;
    virtual void EndClassMember(void);

    // choice
    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id) = 0;
    virtual void EndChoiceVariant(void);

    // write byte blocks
    virtual void BeginBytes(const ByteBlock& block);
    virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length) = 0;
    virtual void EndBytes(const ByteBlock& block);

    // write char blocks
    virtual void BeginChars(const CharBlock& block);
    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length) = 0;
    virtual void EndChars(const CharBlock& block);

    void WritePointer(TConstObjectPtr object, TTypeInfo typeInfo);
protected:
    friend class CObjectStreamCopier;

    // low level writers
    virtual void WriteNullPointer(void) = 0;
    virtual void WriteObjectReference(TObjectIndex index) = 0;
    virtual void WriteThis(TConstObjectPtr object,
                           TTypeInfo typeInfo);
    virtual void WriteOtherBegin(TTypeInfo typeInfo) = 0;
    virtual void WriteOtherEnd(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);

    void RegisterObject(TTypeInfo typeInfo);
    void RegisterObject(TConstObjectPtr object, TTypeInfo typeInfo);

    // report error about unended block
    void Unended(const string& msg);
    // report error about unended object stack frame
    virtual void UnendedFrame(void);

    void SetPathWriteObjectHook( const string& path, CWriteObjectHook*        hook);
    void SetPathWriteMemberHook( const string& path, CWriteClassMemberHook*   hook);
    void SetPathWriteVariantHook(const string& path, CWriteChoiceVariantHook* hook);


    static CObjectOStream* OpenObjectOStreamAsn(CNcbiOstream& out,
                                                bool deleteOut);
    static CObjectOStream* OpenObjectOStreamAsnBinary(CNcbiOstream& out,
                                                      bool deleteOut);
    static CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& out,
                                                bool deleteOut);

protected:
    void x_SetPathHooks(bool set);

    COStreamBuffer m_Output;

    TFailFlags m_Fail;
    TFlags m_Flags;

    AutoPtr<CWriteObjectList> m_Objects;

    // Write current separator to the stream
    virtual void WriteSeparator(void);
    string m_Separator;
    bool   m_AutoSeparator;
    ESerialDataFormat   m_DataFormat;

private:
    ESerialVerifyData   m_VerifyData;
    static ESerialVerifyData ms_VerifyDataDefault;
    static ESerialVerifyData x_GetVerifyDataDefault(void);
    CStreamObjectPathHook<CWriteObjectHook*>                m_PathWriteObjectHooks;
    CStreamPathHook<CMemberInfo*, CWriteClassMemberHook*>   m_PathWriteMemberHooks;
    CStreamPathHook<CVariantInfo*,CWriteChoiceVariantHook*> m_PathWriteVariantHooks;

public:
    // hook support
    CLocalHookSet<CWriteObjectHook> m_ObjectHookKey;
    CLocalHookSet<CWriteClassMemberHook> m_ClassMemberHookKey;
    CLocalHookSet<CWriteChoiceVariantHook> m_ChoiceVariantHookKey;
};


/* @} */


#include <serial/objostr.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTR__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.90  2005/02/23 21:07:44  vasilche
* Allow to skip underlying stream flush.
*
* Revision 1.89  2004/09/22 13:32:17  kononenk
* "Diagnostic Message Filtering" functionality added.
* Added function SetDiagFilter()
* Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
* Module, class and function attribute added to CNcbiDiag and CException
* Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
* 	CDiagCompileInfo + fixes on derived classes and their usage
* Macro NCBI_MODULE can be used to set default module name in cpp files
*
* Revision 1.88  2004/08/30 18:13:24  gouriano
* use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.87  2004/08/17 14:36:43  dicuccio
* Added export specifiers for nested classes
*
* Revision 1.86  2004/05/24 18:12:24  gouriano
* In text output files make indentation optional
*
* Revision 1.85  2004/04/30 13:28:40  gouriano
* Remove obsolete function declarations
*
* Revision 1.84  2004/03/10 23:18:11  gorelenk
* Added prefix NCBI_XSERIAL_EXPORT for class CObjectOStream::CharBlock .
*
* Revision 1.83  2004/02/09 18:21:53  gouriano
* enforced checking environment vars when setting initialization
* verification parameters
*
* Revision 1.82  2004/01/05 14:24:09  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.81  2003/11/26 19:59:38  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.80  2003/11/13 14:06:45  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.79  2003/10/21 21:08:46  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.78  2003/08/19 18:32:38  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.77  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.76  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.75  2003/05/22 20:08:41  gouriano
* added UTF8 strings
*
* Revision 1.74  2003/05/15 17:45:25  gouriano
* added GetStreamOffset method
*
* Revision 1.73  2003/04/29 18:29:06  gouriano
* object data member initialization verification
*
* Revision 1.72  2003/04/15 16:18:28  siyan
* Added doxygen support
*
* Revision 1.71  2003/04/10 20:13:37  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.69  2003/03/10 18:52:37  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.68  2003/02/04 18:15:22  gouriano
* added reference to corelib/ncbifloat.h
*
* Revision 1.67  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.66  2002/11/14 20:52:01  gouriano
* added BeginChoice/EndChoice methods
*
* Revision 1.65  2002/11/04 21:28:59  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.64  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.63  2002/09/19 14:00:37  grichenk
* Implemented CObjectHookGuard for write and copy hooks
* Added DefaultRead/Write/Copy methods to base hook classes
*
* Revision 1.62  2002/08/26 18:32:24  grichenk
* Added Get/SetAutoSeparator() to CObjectOStream to control
* output of separators.
*
* Revision 1.61  2002/03/07 22:01:59  grichenk
* Added "Separator" modifier for CObjectOStream
*
* Revision 1.60  2002/02/13 22:39:14  ucko
* Support AIX.
*
* Revision 1.59  2001/10/17 20:41:19  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.58  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.57  2001/01/05 20:10:36  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.56  2000/12/26 22:23:45  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.55  2000/12/15 21:28:47  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.54  2000/12/15 15:38:00  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.53  2000/11/20 17:24:36  vasilche
* Added prototypes of internal functions.
*
* Revision 1.52  2000/10/20 19:29:15  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.51  2000/10/20 15:51:27  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.50  2000/10/17 18:45:25  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.49  2000/10/13 16:28:31  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.48  2000/10/04 19:18:54  vasilche
* Fixed processing floating point data.
*
* Revision 1.47  2000/10/03 17:22:35  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.46  2000/09/29 16:18:14  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.45  2000/09/18 20:00:06  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.44  2000/09/01 13:16:01  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.43  2000/08/15 19:44:41  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.42  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.41  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.40  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.39  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* Revision 1.38  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.37  2000/05/09 16:38:34  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.36  2000/05/04 16:22:23  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.35  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.34  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.33  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.32  2000/03/29 15:55:21  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.31  2000/03/07 14:05:31  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.30  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.29  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.28  2000/01/05 19:43:46  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.27  1999/12/20 15:29:31  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.26  1999/12/17 19:04:53  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/10/04 16:22:09  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.24  1999/09/27 14:17:59  vasilche
* Fixed bug with overloaded constructors of Block.
*
* Revision 1.23  1999/09/24 18:19:14  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.22  1999/09/23 18:56:53  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.21  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.20  1999/09/14 18:54:04  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.19  1999/08/16 16:07:43  vasilche
* Added ENUMERATED type.
*
* Revision 1.18  1999/08/13 15:53:44  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.17  1999/07/22 17:33:43  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.16  1999/07/21 14:19:58  vasilche
* Added serialization of bool.
*
* Revision 1.15  1999/07/19 15:50:17  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/14 18:58:03  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.13  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:46  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:20  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:30  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:40  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:02  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:23  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:20:03  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:40  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:17  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:35  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:26  vasilche
* Commit just in case.
*
* ===========================================================================
*/
