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
* Revision 1.1  1999/05/19 19:56:53  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistrb.hpp>
#include <serial/objstrb.hpp>

BEGIN_NCBI_SCOPE


CObjectIStreamBinary::CObjectIStreamBinary(CNcbiIstream* in)
    : m_In(in)
{
    m_Strings.push_back(string());
}

unsigned char CObjectIStreamBinary::ReadByte(void)
{
    int c = m_In->get();
    if ( c == m_In->eof() )
        throw runtime_error("unexpected EOF");
    return c;
}

CTypeInfo::TTypeInfo CObjectIStreamBinary::ReadTypeInfo(void)
{
    unsigned code = ReadByte();

    if ( code >= CObjectStreamBinaryDefs::eStd_first &&
         code <= CObjectStreamBinaryDefs::eStd_last )
        return CObjectStreamBinaryDefs::sm_StdTypes[code - eStd_first];

    switch ( code ) {
    case CObjectStreamBinaryDefs::eClassReference:
        return GetRegiteredClass(ReadId());
    case CObjectStreamBinaryDefs::eClass:
        return RegisterClass(CTypeInfo::GetTypeInfo(ReadString()));
    case CObjectStreamBinaryDefs::eTemplate:
        return RegisterClass(CTemplateResolver::Resolve(*this));
    default:
        ERR_POST("CObjectIStreamBinary::ReadTypeInfo(): undefined type: " << code);
        throw runtime_error("undefined type ");
    }
}

void CObjectIStreamBinary::ReadStd(char& data)
{
    data = ReadByte();
}

void CObjectIStreamBinary::ReadStd(signed char& data)
{
    data = ReadByte();
}

void CObjectIStreamBinary::ReadStd(unsigned char& data)
{
    data = ReadByte();
}

void CObjectIStreamBinary::ReadStd(short& data)
{
    data = short(ReadInt(16));
}

void CObjectIStreamBinary::ReadStd(unsigned short& data)
{
    data = static_cast<unsigned short>(ReadUInt(16));
}

void CObjectIStreamBinary::ReadStd(int& data)
{
    data = ReadInt();
}

void CObjectIStreamBinary::ReadStd(unsigned int& data)
{
    data = ReadUInt();
}

void CObjectIStreamBinary::ReadStd(long& data)
{
    int high = ReadInt();
    data = long(high << 32) | ReadUInt();
}

void CObjectIStreamBinary::ReadStd(unsigned long& data)
{
    unsigned high = ReadUInt();
    data = (unsigned long)(high << 32) | ReadUInt();
}

void CObjectIStreamBinary::ReadStd(float& data)
{
    throw runtime_error("float is not supported");
}

void CObjectIStreamBinary::ReadStd(double& data)
{
    throw runtime_error("double is not supported");
}

void CObjectIStreamBinary::ReadString(string& data)
{
    data = ReadString();
}

unsigned int CObjectIStreamBinary::ReadUInt(void)
{
    unsigned char c0 = ReadByte();
    unsigned char c1, c2, c3;
    switch ( c0 >> 4 ) {
    case 8: case 9: case 10: case 11:
        return ((c0 & 0x3F) << 8) | ReadByte();
    case 12: case 13:
        c1 = ReadByte();
        return ((c0 & 0x1F) << 16) | (c1 << 8) | ReadByte();
    case 14:
        c1 = ReadByte();
        c2 = ReadByte();
        return ((c0 & 0x0F) << 24) | (c1 << 16) | (c2 << 8) | ReadByte();
    case 15:
        if ( c0 == 0xF0 ) {
            c1 = ReadByte();
            c2 = ReadByte();
            c3 = ReadByte();
            return (c1 << 24) | (c2 << 16) | (c3 << 8) | ReadByte();
        }
        else if ( c0 >= 0xF8 ) {
            return MAX_UINT - (c0 & 0x07);
        }
        else {
            throw runtime_error("bad number start");
        }
    default:
        return c0;
    }
}

inline
int makesign(unsigned n, unsigned bits)
{
    unsigned highbit = 1 << (bits - 1);
    return int(n ^ hignbit) - int(highbit);
}

int CObjectIStreamBinary::ReadInt(void)
{
    unsigned char c0 = ReadByte();
    unsigned char c1, c2, c3;
    switch ( c0 >> 4 ) {
    case 8: case 9: case 10: case 11:
        return makesign(((c0 & 0x3F) << 8) | ReadByte(), 14);
    case 12: case 13:
        c1 = ReadByte();
        return makesign(((c0 & 0x1F) << 16) | (c1 << 8) | ReadByte(), 21);
    case 14:
        c1 = ReadByte();
        c2 = ReadByte();
        return makesign(((c0 & 0x0F) << 24) | (c1 << 16) | (c2 << 8) | ReadByte(), 28);
    case 15:
        if ( c0 == 0xF0 ) {
            c1 = ReadByte();
            c2 = ReadByte();
            c3 = ReadByte();
            return (c1 << 24) | (c2 << 16) | (c3 << 8) | ReadByte();
        }
        else if ( c0 >= 0xF8 ) {
            return MAX_INT - makesign(c0 & 0x07, 3);
        }
        else {
            throw runtime_error("bad number start");
        }
    default:
        return makesign(c0, 7);
    }
}

const string& CObjectIStreamBinary::ReadString(void)
{
    unsigned index = ReadUInt();
    if ( index == unsigned(-1) ) {
        // new string
        m_Strings.push_back(string());
        string& s = m_Strings.back();
        unsigned length = ReadUInt();
        s.reserve(length);
        return s;
    }
    else {
        return m_Strings[index];
    }
}

END_NCBI_SCOPE
