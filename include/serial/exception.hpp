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
*   Standard exception classes used in serial package
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
*
* ===========================================================================
*/

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

// root class for all serialization exceptions
class CSerialException : public runtime_error
{
public:
    CSerialException(const string& msg) THROWS_NONE;
    ~CSerialException(void) THROWS_NONE;
};

// this exception is thrown when value doesn't fit in place
class CSerialOverflowException : public CSerialException
{
public:
    CSerialOverflowException(const string& msg) THROWS_NONE;
    ~CSerialOverflowException(void) THROWS_NONE;
};

// this exception is thrown when file format is bad
class CSerialFormatException : public CSerialException
{
public:
    CSerialFormatException(const string& msg) THROWS_NONE;
    ~CSerialFormatException(void) THROWS_NONE;
};

// this exception is thrown when IO error occured in serialization
class CSerialIOException : public CSerialException
{
public:
    CSerialIOException(const string& msg) THROWS_NONE;
    ~CSerialIOException(void) THROWS_NONE;
};

// this exception is thrown when unexpected end of file found
class CSerialEofException : public CSerialIOException
{
public:
    CSerialEofException(void) THROWS_NONE;
    ~CSerialEofException(void) THROWS_NONE;
};

END_NCBI_SCOPE
