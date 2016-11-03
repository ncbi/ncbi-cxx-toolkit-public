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
*      templates for reading/writing ASN data from/to a file, or copying
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
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <string>
#include <memory>

#ifndef NS_AUTO_PTR
#ifdef HAVE_NO_AUTO_PTR
#define NS_AUTO_PTR ncbi::auto_ptr
#else
#define NS_AUTO_PTR std::auto_ptr
#endif
#endif


BEGIN_SCOPE(Cn3D)

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
bool ReadASNFromFile(const char *filename, ASNClass *ASNobject, bool isBinary, std::string *err)
{
    err->erase();

    // initialize the binary input stream
    ncbi::CNcbiIfstream inStream(filename, IOS_BASE::in | IOS_BASE::binary);
    if (!inStream) {
        *err = "Cannot open file for reading";
        return false;
    }

    NS_AUTO_PTR<ncbi::CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsnBinary(inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsn(inStream));
    }

    // Read the asn data
    bool okay = true;
    ncbi::SetDiagTrace(ncbi::eDT_Disable);
    try {
        *inObject >> *ASNobject;
    } catch (std::exception& e) {
        *err = e.what();
        okay = false;
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    inStream.close();
    return okay;
}

// for writing ASN data
template < class ASNClass >
bool WriteASNToFile(const char *filename, const ASNClass& ASNobject, bool isBinary,
    std::string *err, ncbi::EFixNonPrint fixNonPrint = ncbi::eFNP_Default)
{
    err->erase();

    // initialize a binary output stream
    ncbi::CNcbiOfstream outStream(filename,
        isBinary ? (IOS_BASE::out | IOS_BASE::binary) : IOS_BASE::out);
    if (!outStream) {
        *err = "Cannot open file for writing";
        return false;
    }

    NS_AUTO_PTR<ncbi::CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsnBinary(outStream, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsn(outStream, fixNonPrint));
    }

    // write the asn data
    bool okay = true;
    ncbi::SetDiagTrace(ncbi::eDT_Disable);
    try {
        *outObject << ASNobject;
        outStream.flush();
    } catch (std::exception& e) {
        *err = e.what();
        okay = false;
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    outStream.close();
    return okay;
}

//  ***  Do not use:  always uses http, even if give port = 443  ***
// for loading ASN data via HTTP connection
template < class ASNClass >
NCBI_DEPRECATED
bool GetAsnDataViaHTTP(
    const std::string& host, const std::string& path, const std::string& args,
    ASNClass *asnObject, std::string *err, bool binaryData = true, unsigned short port = 80)
{
    err->erase();
    bool okay = false;

    // set up registry field to set GET connection method for HTTP
    REG origREG = CORE_GetREG();
    ncbi::CNcbiRegistry* reg = new ncbi::CNcbiRegistry;
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "FALSE");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "GET");
    CORE_SetREG(REG_cxx2c(reg, true));

    try {
        // load data from stream using given URL params
        ncbi::CConn_HttpStream httpStream(host, path, args, kEmptyStr, port);
        NS_AUTO_PTR<ncbi::CObjectIStream> inObject;
        if (binaryData)
            inObject.reset(new ncbi::CObjectIStreamAsnBinary(httpStream));
        else
            inObject.reset(new ncbi::CObjectIStreamAsn(httpStream));
        ncbi::SetDiagTrace(ncbi::eDT_Disable);
        *inObject >> *asnObject;
        okay = true;

    } catch (ncbi::CSerialException& se) {
        *err = std::string("Data is not in expected format; error: ") + se.what();
    } catch (std::exception& e) {
        *err = std::string("Network connection failed or data is not in expected format; error: ") + e.what();
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    CORE_SetREG(origREG);
    return okay;
}

// for loading ASN data via HTTPS connection
template < class ASNClass >
bool GetAsnDataViaHTTPS(
    const string& host, const string& path, const string& args,
    ASNClass *asnObject, string *err,
    bool binaryData = true)

    //
    // Makes a simple attempt to enforce to URLs like
    //
    //   https://host/....
    //
    // a)  prepend 'https://' if 'host' does not start with it
    // b)  if 'host' starts with non-secure 'http://', change to 'https://'
    //
    // for loading ASN data via HTTPS connection
    // 
{
    err->erase();
    bool okay = false;
    string hostCopy(host), url;

    if (NStr::StartsWith(host, "http://"))
        hostCopy = "https://" + host.substr(7);
    else if (!NStr::StartsWith(host, "https://"))
        hostCopy = "https://" + host;

    url = hostCopy + path + "?" + args;

    if (!asnObject) {
        if (err) *err = "GetAsnDataViaHTTPS:  Empty object pointer provided.";
        return false;
    }

    try {
        // load data from stream using given URL params
        CConn_HttpStream httpsStream(url);
        ESerialDataFormat dataFormat = (binaryData) ? eSerial_AsnBinary : eSerial_AsnText;
        // cout << httpsStream.GetType() << ", " << httpsStream.GetDescription() << endl;

        NS_AUTO_PTR<CObjectIStream> inObject(CObjectIStream::Open(dataFormat, httpsStream));
        *inObject >> *asnObject;
        okay = true;

    }
    catch (exception& e) {
        *err = e.what();
    }

    return okay;
}

END_SCOPE(Cn3D)

#endif // CN3D_ASN_READER__HPP
