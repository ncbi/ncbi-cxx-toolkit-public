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
* Authors:  Paul Thiessen
*
* File Description:
*      template for reading/writing ASN data of any type from/to a file
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/12/22 19:25:46  thiessen
* write cdd output files
*
* Revision 1.1  2000/12/21 23:42:24  thiessen
* load structures from cdd's
*
* ===========================================================================
*/

#ifndef CN3D_ASN_READER__HPP
#define CN3D_ASN_READER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>

#include <string>


BEGIN_SCOPE(Cn3D)

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
static bool ReadASNFromFile(const char *filename, ASNClass& ASNobject, bool isBinary, std::string& err)
{
    // initialize the binary input stream
    auto_ptr<ncbi::CNcbiIstream> inStream;
    inStream.reset(new ncbi::CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
        err = "Cannot open file for reading";
        return false;
    }

    auto_ptr<ncbi::CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsnBinary(*inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsn(*inStream));
    }

    // Read the data
    try {
        err.erase();
        *inObject >> ASNobject;
    } catch (exception& e) {
        err = e.what();
        return false;
    }

    return true;
}

// for writing ASN data
template < class ASNClass >
static bool WriteASNToFile(const char *filename, ASNClass& mime, bool isBinary, std::string& err)
{
    // initialize a binary output stream
    auto_ptr<ncbi::CNcbiOstream> outStream;
    outStream.reset(new ncbi::CNcbiOfstream(filename, IOS_BASE::out | IOS_BASE::binary));
    if (!(*outStream)) {
        err = "Cannot open file for writing";
        return false;
    }

    auto_ptr<ncbi::CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsnBinary(*outStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsn(*outStream));
    }

    // Read the CNcbi_mime_asn1 data
    try {
        *outObject << mime;
    } catch (exception& e) {
        err = e.what();
        return false;
    }

    return true;
}

END_SCOPE(Cn3D)

#endif // CN3D_ASN_READER__HPP
