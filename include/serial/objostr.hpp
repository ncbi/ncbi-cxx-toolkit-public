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
*   Base class of object output stream classes
*   Having a data object, it encodes it and saves in an output stream
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbifloat.h>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/strbuffer.hpp>
#include <serial/impl/objlist.hpp>
#include <serial/impl/hookdatakey.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/pathhook.hpp>
#include <serial/error_codes.hpp>


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

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectOStream --
///
/// Base class of serial object stream encoders
class NCBI_XSERIAL_EXPORT CObjectOStream : public CObjectStack
{
public:
    /// Destructor.
    ///
    /// Constructors are protected;
    /// use any one of 'Create' methods to construct the stream
    virtual ~CObjectOStream(void);

//---------------------------------------------------------------------------
// Create methods
    // CObjectOStream will be created on heap, and must be deleted later on

    /// Create serial object writer and attach it to an output stream.
    ///
    /// @param format
    ///   Format of the output data
    /// @param outStream
    ///   Output stream
    /// @param deleteOutStream
    ///   When TRUE, the output stream will be deleted automatically
    ///   when the writer is deleted
    /// @return
    ///   Writer (created on heap)
    /// @deprecated
    ///   Use one with EOwnership enum instead
    static NCBI_DEPRECATED CObjectOStream* Open(ESerialDataFormat format,
                                CNcbiOstream& outStream,
                                bool deleteOutStream);

    /// Create serial object writer and attach it to an output stream.
    ///
    /// @param format
    ///   Format of the output data
    /// @param outStream
    ///   Output stream
    /// @param deleteOutStream
    ///   When eTakeOwnership, the output stream will be deleted automatically
    ///   when the writer is deleted
    /// @param formatFlags
    ///   Formatting flags (see ESerial_xxx_Flags)
    /// @return
    ///   Writer (created on heap)
    /// @sa ESerial_AsnText_Flags, ESerial_Xml_Flags, ESerial_Json_Flags
    static CObjectOStream* Open(ESerialDataFormat format,
                                CNcbiOstream& outStream,
                                EOwnership deleteOutStream = eNoOwnership,
                                TSerial_Format_Flags formatFlags = 0);

    /// Create serial object writer and attach it to a file stream.
    ///
    /// @param format
    ///   Format of the output data
    /// @param fileName
    ///   Output file name
    /// @param openFlags
    ///   File open flags
    /// @param formatFlags
    ///   Formatting flags (see ESerial_xxx_Flags)
    /// @return
    ///   Writer (created on heap)
    /// @sa ESerialOpenFlags, ESerial_AsnText_Flags, ESerial_Xml_Flags, ESerial_Json_Flags
    static CObjectOStream* Open(ESerialDataFormat format,
                                const string& fileName,
                                TSerialOpenFlags openFlags = 0,
                                TSerial_Format_Flags formatFlags = 0);

    /// Create serial object writer and attach it to a file stream.
    ///
    /// @param fileName
    ///   Output file name
    /// @param format
    ///   Format of the output data
    /// @param formatFlags
    ///   Formatting flags (see ESerial_xxx_Flags)
    /// @return
    ///   Writer (created on heap)
    /// @sa ESerial_AsnText_Flags, ESerial_Xml_Flags, ESerial_Json_Flags
    static CObjectOStream* Open(const string& fileName,
                                ESerialDataFormat format,
                                TSerial_Format_Flags formatFlags = 0);

    /// Get data format
    ///
    /// @return
    ///   Output data format
    ESerialDataFormat GetDataFormat(void) const;

//---------------------------------------------------------------------------
// Data verification setup

    // for this particular stream
    /// Set up output data verification for this particular stream
    ///
    /// @param verify
    ///   Data verification parameter
    void SetVerifyData(ESerialVerifyData verify);

    /// Get output data verification parameter.
    ///
    /// When verification is enabled, stream verifies data on output
    /// and throws CUnassignedMember exception
    ///
    /// @return
    ///   Data verification parameter
    ESerialVerifyData GetVerifyData(void) const;

    /// Set up default output data verification for streams
    /// created by the current thread
    ///
    /// @param verify
    ///   Data verification parameter
    static  void SetVerifyDataThread(ESerialVerifyData verify);

    /// Set up default output data verification for streams
    /// created by the current process
    ///
    /// @param verify
    ///   Data verification parameter
    static  void SetVerifyDataGlobal(ESerialVerifyData verify);

    EFixNonPrint FixNonPrint(EFixNonPrint how)
    {
        EFixNonPrint tmp = m_FixMethod;
        m_FixMethod = how == eFNP_Default ? x_GetFixCharsMethodDefault() : how;
        return tmp;
    }

    // Enforce explicit writing of values with default,
    // even when they were never set
    //
    // This is dangerous, and contradicts specification
    // but might be useful to make sure defaults are properly reported to recipient
    // Use at your own risk
    void EnforceWritingDefaultValues(bool enforce = true);

    // Is explicit writing of values with default enforced
    bool IsWritingDefaultValuesEnforced() const;

//---------------------------------------------------------------------------
// Formatting of the output

    /// Set up indentation usage in text streams.
    /// 
    /// @param set
    ///   When TRUE, the writer puts white space chars in the beginning
    ///   of each line of text
    void SetUseIndentation(bool set);

    /// Get indentation usage in text streams.
    /// 
    /// @return
    ///   TRUE or FALSE
    bool GetUseIndentation(void) const;

    /// Set up end-of-line symbol usage in text streams.
    /// 
    /// @param set
    ///   When TRUE, the writer puts end-of-line symbol where needed,
    ///   otherwise, the output is a single line.
    void SetUseEol(bool set);

    /// Get end-of-line symbol usage in text streams.
    /// 
    /// @return
    ///   TRUE or FALSE
    bool GetUseEol(void) const;
    
    /// Set up writing named integers (in ANS.1 sense) by value only.
    ///
    /// The setting affects text streams only and is provided
    /// for convenience: so that legacy applications can read data files
    /// generated by newer ones.
    /// 
    /// @param set
    ///   When TRUE, the writer does not write the name of the value,
    ///   but only its numeric value instead
    void SetWriteNamedIntegersByValue(bool set);

    /// Get writing named integers by value parameter
    /// 
    /// @return
    ///   TRUE or FALSE
    bool GetWriteNamedIntegersByValue(void) const;

    /// Get separator.
    ///
    /// @return
    ///   Separator string
    string GetSeparator(void) const;

    /// Set separator.
    ///
    /// Separator string is written into text stream after each object
    ///
    /// @param sep
    ///   Separator string
    void SetSeparator(const string sep);
    
    /// Get separator auto-output paramater
    ///
    /// Controls auto-output of the separator after each object. By default
    /// this flag is true for text ASN.1 streams only.
    ///
    /// @return
    ///   TRUE or FALSE
    bool GetAutoSeparator(void);
    
    /// Set separator auto-output paramater.
    ///
    /// When TRUE, writer puts separator string after each object. By default
    /// this flag is TRUE for text ASN.1 streams only.
    ///
    /// @param value
    ///   TRUE or FALSE
    void SetAutoSeparator(bool value);

    /// Set output formatting flags
    ///
    /// @param flags
    ///   Formatting flag
    virtual void SetFormattingFlags(TSerial_Format_Flags flags);

//---------------------------------------------------------------------------
// Stream state

    /// Fail flags
    enum EFailFlags {
        /// No error
        fNoError       = 0,             eNoError     = fNoError,
//        fEOF           = 1 << 0,        eEOF         = fEOF,
        /// An unknown error when writing into output file
        fWriteError    = 1 << 1,        eWriteError  = fWriteError,
//        fFormatError   = 1 << 2,        eFormatError = fFormatError,
        /// Internal buffer overflow
        fOverflow      = 1 << 3,        eOverflow    = fOverflow,
        /// Output data is incorrect
        fInvalidData   = 1 << 4,        eInvalidData = fInvalidData,
        /// Illegal in a given context function call
        fIllegalCall   = 1 << 5,        eIllegalCall = fIllegalCall,
        /// Internal error, the real reason is unclear
        fFail          = 1 << 6,        eFail        = fFail,
        /// No output file
        fNotOpen       = 1 << 7,        eNotOpen     = fNotOpen,
        /// Method is not implemented
        fNotImplemented= 1 << 8,        eNotImplemented = fNotImplemented,
        /// Mandatory object member is unassigned
        /// Normally this results in throwing CUnassignedMember exception
        fUnassigned    = 1 << 9,        eUnassigned  = fUnassigned
    };
    typedef int TFailFlags;

    /// Check if any of fail flags is set.
    ///
    /// @return
    ///   TRUE or FALSE
    bool fail(void) const;

    /// Get fail flags.
    ///
    /// @return
    ///   Fail flags
    TFailFlags GetFailFlags(void) const;

    /// Set fail flags, but do not ERR_POST any messages
    ///
    /// @param flags
    ///   Fail flags
    TFailFlags SetFailFlagsNoError(TFailFlags flags);

    /// Set fail flags
    ///
    /// @param flags
    ///   Fail flags
    /// @param message
    ///   Text message
    TFailFlags SetFailFlags(TFailFlags flags, const char* message);

    /// Reset fail flags
    ///
    /// @param flags
    ///   Flags to reset
    TFailFlags ClearFailFlags(TFailFlags flags);

    /// Check fail flags and also the state of output stream
    ///
    /// @return
    ///   TRUE is there is no errors
    bool InGoodState(void);

    /// Set cancellation check callback.
    /// The stream will periodically check for a cancellation request and
    /// throw an exception when requested.
    void SetCanceledCallback(const ICanceled* callback);

    /// @deprecated
    ///   Use GetStreamPos() instead
    /// @sa GetStreamPos()
    NCBI_DEPRECATED CNcbiStreampos GetStreamOffset(void) const;

    /// Get the current stream position
    ///
    /// NOTE: 
    ///   This is not the same as ostream::tellp();
    ///   rather, this is an offset in the current output
    ///
    /// @return
    ///   stream position
    CNcbiStreampos GetStreamPos(void) const;

    /// Get current stack trace as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetStackTrace(void) const;

    /// Get current stream position as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetPosition(void) const;

//---------------------------------------------------------------------------
// Local write hooks
    void SetPathWriteObjectHook( const string& path, CWriteObjectHook*        hook);
    void SetPathWriteMemberHook( const string& path, CWriteClassMemberHook*   hook);
    void SetPathWriteVariantHook(const string& path, CWriteChoiceVariantHook* hook);
    
    /// DelayBuffer parsing policy
    enum EDelayBufferParsing {
        /// Parse only if local hook are present
        eDelayBufferPolicyNotSet,
        /// Parse always
        eDelayBufferPolicyAlwaysParse,
        /// Never parse
        eDelayBufferPolicyNeverParse
    };
    void SetDelayBufferParsingPolicy(EDelayBufferParsing policy);
    EDelayBufferParsing GetDelayBufferParsingPolicy(void) const;
    bool ShouldParseDelayBuffer(void) const;

//---------------------------------------------------------------------------
// User interface
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

    // file header
    virtual void WriteFileHeader(TTypeInfo type);

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


    CObjectOStream& operator<< (CObjectOStream& (*mod)(CObjectOStream& os));
    friend CObjectOStream& Separator(CObjectOStream& os);

//---------------------------------------------------------------------------
// Standard types
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
#ifndef NCBI_INT8_IS_LONG
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

    // C string; VisualAge can't cope with refs here.
    void WriteStd(const char* const data);
    void WriteStd(char* const data);

    void WriteStd(const CBitString& data);

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
    virtual void CopyString(CObjectIStream& in,
                            EStringType type = eStringTypeVisible) = 0;

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

    virtual void WriteBitString(const CBitString& obj) = 0;
    virtual void CopyBitString(CObjectIStream& in) = 0;

    // delayed buffer
    virtual bool Write(CByteSource& source);

#ifdef NCBI_STRICT_GI
    void WriteStd(const TGi& data);

    virtual void WriteGi(const TGi& obj);
    virtual void CopyGi(CObjectIStream& in);
#endif

//---------------------------------------------------------------------------
// Internals
    void Close(void);
    virtual void EndOfWrite(void);
    void ResetLocalHooks(void);
    void HandleEOF(CEofException&);

    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const char* message,
                     CException* exc = 0);
    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const string& message,
                     CException* exc = 0);
#define RethrowError(flag,mess,exc) \
    ThrowError1(DIAG_COMPILE_INFO,flag,mess,&exc)

    // report error about unended block
    void Unended(const string& msg);
    // report error about unended object stack frame
    virtual void UnendedFrame(void);

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

//---------------------------------------------------------------------------
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
    // alias
    MLIOVIR void WriteAlias(const CAliasTypeInfo* aliasType,
                            TConstObjectPtr aliasPtr);

//---------------------------------------------------------------------------
// Copying
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

//---------------------------------------------------------------------------
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

    enum ESpecialCaseWrite {
        eWriteAsNormal  = 0,
        eWriteAsDefault = 1,
        eWriteAsNil     = 2
    };
    void  SetSpecialCaseWrite( ESpecialCaseWrite how) {
        m_SpecialCaseWrite = how;
    }
    MLIOVIR void WriteClassMemberSpecialCase(
        const CMemberId& memberId, TTypeInfo memberType,
        TConstObjectPtr memberPtr, ESpecialCaseWrite how);

protected:
    CObjectOStream(ESerialDataFormat format,
                   CNcbiOstream& out, EOwnership deleteOut = eNoOwnership);

    // low level writers
    typedef size_t TObjectIndex;
    virtual void WriteNullPointer(void) = 0;
    virtual void WriteObjectReference(TObjectIndex index) = 0;
    virtual void WriteThis(TConstObjectPtr object,
                           TTypeInfo typeInfo);
    virtual void WriteOtherBegin(TTypeInfo typeInfo) = 0;
    virtual void WriteOtherEnd(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);

    void RegisterObject(TTypeInfo typeInfo);
    void RegisterObject(TConstObjectPtr object, TTypeInfo typeInfo);

    void x_SetPathHooks(bool set);
    EFixNonPrint x_GetFixCharsMethodDefault(void) const;
    EFixNonPrint x_FixCharsMethod(void) const {
        return m_FixMethod;
    }
    // Write current separator to the stream
    virtual void WriteSeparator(void);

    COStreamBuffer m_Output;
    TFailFlags m_Fail;
    TFlags m_Flags;
    AutoPtr<CWriteObjectList> m_Objects;
    string m_Separator;
    bool   m_AutoSeparator;
    ESerialDataFormat   m_DataFormat;
    bool  m_WriteNamedIntegersByValue;
    EDelayBufferParsing  m_ParseDelayBuffers;
    bool  m_FastWriteDouble;
    ESpecialCaseWrite m_SpecialCaseWrite;
    bool  m_EnforceWritingDefaults;

private:
    static CObjectOStream* OpenObjectOStreamAsn(CNcbiOstream& out,
                                                EOwnership deleteOut);
    static CObjectOStream* OpenObjectOStreamAsnBinary(CNcbiOstream& out,
                                                      EOwnership deleteOut);
    static CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& out,
                                                EOwnership deleteOut);
    static CObjectOStream* OpenObjectOStreamJson(CNcbiOstream& out,
                                                EOwnership deleteOut);
    static ESerialVerifyData x_GetVerifyDataDefault(void);

    EFixNonPrint m_FixMethod; // method of fixing wrong (eg, non-printable) chars
    ESerialVerifyData   m_VerifyData;
    CStreamObjectPathHook<CWriteObjectHook*>                m_PathWriteObjectHooks;
    CStreamPathHook<CMemberInfo*, CWriteClassMemberHook*>   m_PathWriteMemberHooks;
    CStreamPathHook<CVariantInfo*,CWriteChoiceVariantHook*> m_PathWriteVariantHooks;

public:
    // hook support
    CLocalHookSet<CWriteObjectHook> m_ObjectHookKey;
    CLocalHookSet<CWriteClassMemberHook> m_ClassMemberHookKey;
    CLocalHookSet<CWriteChoiceVariantHook> m_ChoiceVariantHookKey;

    friend class CObjectStreamCopier;
};

inline void
CObjectOStream::EnforceWritingDefaultValues(bool enforce) {
    m_EnforceWritingDefaults = enforce;
}
inline bool
CObjectOStream::IsWritingDefaultValuesEnforced() const {
    return m_EnforceWritingDefaults;
}


/* @} */


#include <serial/impl/objostr.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTR__HPP */
