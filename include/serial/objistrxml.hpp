#ifndef OBJISTRXML__HPP
#define OBJISTRXML__HPP

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
* Revision 1.1  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/lightstr.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStreamXml : public CObjectIStream
{
public:
    typedef unsigned char TByte;

    CObjectIStreamXml(void);
    ~CObjectIStreamXml(void);

    ESerialDataFormat GetDataFormat(void) const;

    virtual string GetPosition(void) const;

    virtual string ReadTypeName(void);

protected:
    EPointerType ReadPointerType(void);
    TIndex ReadObjectPointer(void);
    string ReadOtherPointer(void);

    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void);
    virtual unsigned long ReadULong(void);
    virtual double ReadDouble(void);
    virtual void ReadNull(void);
    virtual void ReadString(string& s);
    pair<long, bool> ReadEnum(const CEnumeratedTypeValues& values);

    virtual void SkipBool(void);
    virtual void SkipChar(void);
    virtual void SkipSNumber(void);
    virtual void SkipUNumber(void);
    virtual void SkipFNumber(void);
    virtual void SkipString(void);
    virtual void SkipNull(void);
    virtual void SkipByteBlock(void);

    CLightString SkipTagName(CLightString tag, char c);
    CLightString SkipTagName(CLightString tag, const char* s, size_t length);
    CLightString SkipTagName(CLightString tag, const char* s);
    CLightString SkipTagName(CLightString tag, const string& s);
    CLightString SkipTagName(CLightString tag, const CObjectStackFrame& e);

    bool NextTagIsClosing(void);
    void OpenTag(const string& e);
    void CloseTag(const string& e);
    void OpenTag(const CObjectStackFrame& e);
    void CloseTag(const CObjectStackFrame& e);

    void BeginArray(CObjectStackArray& array);
    void EndArray(CObjectStackArray& array);
    bool BeginArrayElement(CObjectStackArrayElement& e);
    void EndArrayElement(CObjectStackArrayElement& e);

    void ReadNamedType(TTypeInfo namedTypeInfo,
                       TTypeInfo typeInfo,
                       TObjectPtr object);

    void BeginClass(CObjectStackClass& cls);
    void EndClass(CObjectStackClass& cls);
    TMemberIndex BeginClassMember(CObjectStackClassMember& m,
                                  const CMembers& members);
    TMemberIndex BeginClassMember(CObjectStackClassMember& m,
                                  const CMembers& members,
                                  CClassMemberPosition& pos);
    void EndClassMember(CObjectStackClassMember& m);

    TMemberIndex BeginChoiceVariant(CObjectStackChoiceVariant& v,
                                    const CMembers& members);
    void EndChoiceVariant(CObjectStackChoiceVariant& v);
    
    void BeginBytes(ByteBlock& );
    int GetHexChar(void);
    size_t ReadBytes(ByteBlock& block, char* dst, size_t length);
    void EndBytes(const ByteBlock& );

private:
    bool OutsideTag(void) const;
    bool InsideTag(void) const;
    bool InsideOpeningTag(void) const;
    bool InsideClosingTag(void) const;
    bool SelfClosedTag(void) const;

    void Found_lt(void);
    void Back_lt(void);
    void Found_lt_slash(void);
    void Found_gt(void);
    void Found_slash_gt(void);

    void EndSelfClosedTag(void);

    void EndTag(void);
    void EndOpeningTag(void);
    bool EndOpeningTagSelfClosed(void); // true if '/>' found, false if '>'
    void EndClosingTag(void);
    char BeginOpeningTag(void);
    char BeginClosingTag(void);
    void BeginData(void);

    int ReadEscapedChar(char endingChar);
    void ReadData(string& s);

    CLightString ReadName(char c);
    CLightString ReadAttributeName(void);
    void ReadAttributeValue(string& value);

    void SkipAttributeValue(char c);
    void SkipQDecl(void);

    char SkipWS(void);
    char SkipWSAndComments(void);

    void UnexpectedMember(const CLightString& id, const CMembers& members);

    enum ETagState {
        eTagOutside,
        eTagInsideOpening,
        eTagInsideClosing,
        eTagSelfClosed
    };
    ETagState m_TagState;
};

//#include <serial/objistrxml.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRXML__HPP */
