#ifndef OBJOSTRASNB__HPP
#define OBJOSTRASNB__HPP

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
* Revision 1.2  1999/06/30 16:04:33  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:42  vasilche
* Added binary ASN.1 output.
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStreamAsnBinary : public CObjectOStream
{
public:
    typedef unsigned char TByte;
    typedef int TTag;

    CObjectOStreamAsnBinary(CNcbiOstream& out);
    virtual ~CObjectOStreamAsnBinary(void);

    virtual void WriteStd(const bool& data);
    virtual void WriteStd(const char& data);
    virtual void WriteStd(const unsigned char& data);
    virtual void WriteStd(const signed char& data);
    virtual void WriteStd(const short& data);
    virtual void WriteStd(const unsigned short& data);
    virtual void WriteStd(const int& data);
    virtual void WriteStd(const unsigned int& data);
    virtual void WriteStd(const long& data);
    virtual void WriteStd(const unsigned long& data);
    virtual void WriteStd(const float& data);
    virtual void WriteStd(const double& data);
    virtual void WriteStd(const string& data);
    virtual void WriteStd(char* const& data);
    virtual void WriteStd(const char* const& data);

    void WriteNull(void);
    void WriteByte(TByte byte);
    void WriteBytes(const char* bytes, size_t size);

    enum EClass {
        eUniversal,
        eApplication,
        eContextSpecific,
        ePrivate
    };

    enum ETag {
        eNone = 0,
        eBoolean = 1,
        eInteger = 2,
        eBitString = 3,
        eOctetString = 4,
        eNull = 5,
        eObjectIdentifier = 6,
        eObjectDescriptor = 7,
        eExternal = 8,
        eReal = 9,
        eEnumerated = 10,

        eSequence = 16,
        eSequenceOf = eSequence,
        eSet = 17,
        eSetOf = eSet,
        eNumericString = 18,
        ePrintableString = 19,
        eTeletextString = 20,
        eT61String = 20,
        eVideotextString = 21,
        eIA5String = 22,

        eUTCTime = 23,
        eGeneralizedTime = 24,

        eGraphicString = 25,
        eVisibleString = 26,
        eISO646String = 26,
        eGeneralString = 27
    };

    void WriteTag(EClass c, bool constructed, TTag index);
    void WriteShortTag(EClass c, bool constructed, TTag index);
    void WriteSysTag(ETag index);
    void WriteClassTag(TTypeInfo typeInfo);

    void WriteLength(size_t length);
    void WriteShortLength(size_t length);
    void WriteIndefiniteLength(void);

    void WriteEndOfContent(void);

protected:

    virtual void WriteMember(const CMemberId& member);
    virtual void WriteMemberPrefix(COObjectInfo& info);
    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TIndex index);
    virtual void WriteThisTypeReference(TTypeInfo typeInfo);
    virtual void WriteOtherTypeReference(TTypeInfo typeInfo);

    virtual void VBegin(Block& block);
    virtual void VNext(const Block& block);
    virtual void VEnd(const Block& block);

    virtual void WriteString(const string& s);

private:
    CNcbiOstream& m_Output;
};

//#include <serial/objostrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRASNB__HPP */
