#ifndef OBJISTRASNB__HPP
#define OBJISTRASNB__HPP

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
* Revision 1.1  1999/07/02 21:31:45  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/objstrasnb.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStreamAsnBinary : public CObjectIStream,
                                public CObjectStreamAsnBinaryDefs
{
public:
    CObjectIStreamAsnBinary(CNcbiIstream& in);

    virtual void ReadStd(char& data);
    virtual void ReadStd(unsigned char& data);
    virtual void ReadStd(signed char& data);
    virtual void ReadStd(short& data);
    virtual void ReadStd(unsigned short& data);
    virtual void ReadStd(int& data);
    virtual void ReadStd(unsigned int& data);
    virtual void ReadStd(long& data);
    virtual void ReadStd(unsigned long& data);
    virtual void ReadStd(float& data);
    virtual void ReadStd(double& data);
    virtual void ReadStd(string& data);
    virtual void ReadStd(char*& data);

    virtual TObjectPtr ReadPointer(TTypeInfo declaredType);

    virtual void SkipValue(void);

    unsigned char ReadByte(void);
    signed char ReadSByte(void);
    void ReadBytes(char* bytes, unsigned size);
    virtual string ReadString(void);

    void ExpectByte(TByte byte);

    ETag ReadSysTag(bool allowLong = false);
    void BackSysTag(void);
    void FlushSysTag(bool constructed = false);
    TTag ReadTag(void);

    ETag ReadSysTag(EClass c, bool constructed);
    TTag ReadTag(EClass c, bool constructed);

    void ExpectSysTag(ETag tag);
    void ExpectSysTag(EClass c, bool constructed, ETag tag);
    bool LastTagWas(EClass c, bool constructed);

    size_t ReadShortLength(void);
    size_t ReadLength(void);

    void ExpectShortLength(size_t length);
    void ExpectIndefiniteLength(void);
    void ExpectEndOfContent(void);

protected:
    virtual void VBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member);
    virtual void EndMember(const Member& member);

private:
    CIObjectInfo ReadObjectPointer(void);

    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

    CNcbiIstream& m_Input;

    TByte m_LastTagByte;
    enum ELastTagState {
        eNoTagRead,
        eSysTagRead,
        eLongTagRead,
        eSysTagBack,
        eLongTagBack
    };
    ELastTagState m_LastTagState;
};

//#include <serial/objistrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRASNB__HPP */
