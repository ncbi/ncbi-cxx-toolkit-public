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
* Revision 1.9  1999/07/22 17:33:45  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.8  1999/07/21 14:20:00  vasilche
* Added serialization of bool.
*
* Revision 1.7  1999/07/19 15:50:19  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.6  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.5  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.4  1999/07/02 21:31:47  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:21  vasilche
* Implemented ASN.1 binary write.
*
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
#include <serial/objstrasnb.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStreamAsnBinary : public CObjectOStream
{
public:
    typedef CObjectStreamAsnBinaryDefs::TByte TByte;
    typedef CObjectStreamAsnBinaryDefs::TTag TTag;
    typedef CObjectStreamAsnBinaryDefs::ETag ETag;
    typedef CObjectStreamAsnBinaryDefs::EClass EClass;

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

    void WriteNull(void);
    void WriteByte(TByte byte);
    void WriteBytes(const char* bytes, size_t size);

    void WriteTag(EClass c, bool constructed, TTag index);
    void WriteShortTag(EClass c, bool constructed, TTag index);
    void WriteSysTag(ETag index);
    void WriteClassTag(TTypeInfo typeInfo);

    void WriteLength(size_t length);
    void WriteShortLength(size_t length);
    void WriteIndefiniteLength(void);

    void WriteEndOfContent(void);

    virtual unsigned GetAsnFlags(void);
    virtual void AsnWrite(AsnIo& asn, const char* data, size_t length);

protected:

    virtual void WriteMemberPrefix(const CMemberId& id);
    virtual void WriteMemberSuffix(const CMemberId& id);
    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TIndex index);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);

    virtual void VBegin(Block& block);
    virtual void VEnd(const Block& block);
    virtual void StartMember(Member& member, const CMemberId& id);
    virtual void EndMember(const Member& member);
	virtual void Begin(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block, const char* bytes, size_t length);

    virtual void WriteString(const string& s);
    virtual void WriteCString(const char* str);

private:
    CNcbiOstream& m_Output;
};

//#include <serial/objostrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRASNB__HPP */
