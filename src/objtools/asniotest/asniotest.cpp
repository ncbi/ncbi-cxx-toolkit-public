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
*      test suite for asn serialization library, asn data i/o
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>

#include <memory>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/cdd/Cdd.hpp>

#include "asniotest.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// diagnostic streams
#define INFOMSG(stream) ERR_POST(Info << stream)
#define WARNINGMSG(stream) ERR_POST(Warning << stream)
#define ERRORMSG(stream) ERR_POST(Error << stream << '!')

static string err;  // just so I don't have to keep declaring this all over... ;)

// directory that contains test files (default overridden by env. var. ASNIOTEST_FILES)
static string pathToFiles(".");

// keep track of files created, to remove them at the end
static list < string > filesCreated;

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
bool ReadASNFromFile(const char *filename, ASNClass *ASNobject, bool isBinary, string *err)
{
    err->erase();

    // initialize the binary input stream
    auto_ptr<CNcbiIstream> inStream;
    inStream.reset(new CNcbiIfstream(
        (pathToFiles + CDirEntry::GetPathSeparator() + filename).c_str(),
        IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
        *err = "Cannot open file for reading";
        return false;
    }

    auto_ptr<CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new CObjectIStreamAsnBinary(*inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new CObjectIStreamAsn(*inStream));
    }

    // Read the asn data
    bool okay = true;
    try {
        *inObject >> *ASNobject;
    } catch (exception& e) {
        *err = e.what();
        okay = false;
    }

    return okay;
}

// for writing ASN data; all files written this way will be removed at program end
template < class ASNClass >
bool WriteASNToFile(const char *filename, const ASNClass& ASNobject, bool isBinary,
    string *err, EFixNonPrint fixNonPrint = eFNP_Default)
{
    err->erase();
    string fullPath = pathToFiles + CDirEntry::GetPathSeparator() + filename;
    if (CFile(fullPath).Exists()) {
        *err = "Can't overwrite file ";
        *err += fullPath;
        return false;
    }

    // initialize a binary output stream
    auto_ptr<CNcbiOstream> outStream;
    outStream.reset(new CNcbiOfstream(
        fullPath.c_str(),
        isBinary ? (IOS_BASE::out | IOS_BASE::binary) : IOS_BASE::out));
    if (!(*outStream)) {
        *err = "Cannot open file for writing";
        return false;
    }
    filesCreated.push_back(fullPath);

    auto_ptr<CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new CObjectOStreamAsnBinary(*outStream, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new CObjectOStreamAsn(*outStream, fixNonPrint));
    }

    // write the asn data
    bool okay = true;
    try {
        *outObject << ASNobject;
        outStream->flush();
    } catch (exception& e) {
        *err = e.what();
        okay = false;
    }

    return okay;
}

// test function declaration
#define TEST_FUNC(func) \
    int func (void)

// to call test functions, counting errors
#define RUN_TEST(func) \
    do { \
        INFOMSG("Running " #func " test"); \
        int errors = func(); \
        nErrors += errors; \
        if (errors) \
            ERRORMSG(#func " test failed"); \
    } while (0)

#define ERR_RETURN_N(nErrs, msg) \
    do { \
        ERRORMSG(msg); \
        return nErrs; \
    } while (0)

#define ERR_RETURN(msg) ERR_RETURN_N(1, msg)

#define OKAY 0


TEST_FUNC(BasicFileIO)
{
    // read in a trivial object
    CSeq_id seqId;
    if (!ReadASNFromFile("pdbSeqId.txt", &seqId, false, &err))
        ERR_RETURN("failed to load pdbSeqId.txt: " << err);

    // test text and binary output
    if (!WriteASNToFile("t1.txt", seqId, false, &err) ||
        !WriteASNToFile("t1.bin", seqId, true, &err))
        ERR_RETURN("failed to write CSeq_id: " << err);

    // now try a bigger object (in binary this time)
    CCdd cdd;
    if (!ReadASNFromFile("ADF.bin", &cdd, true, &err))
        ERR_RETURN("failed to load ADF.bin: " << err);
    if (!WriteASNToFile("t2.txt", seqId, false, &err) ||
        !WriteASNToFile("t2.bin", seqId, true, &err))
        ERR_RETURN("failed to write CCdd: " << err);

    return OKAY;
}

int ASNIOTestApp::Run(void)
{
    if (GetEnvironment().Get("ASNIOTEST_FILES").size() > 0)
        pathToFiles = GetEnvironment().Get("ASNIOTEST_FILES");
    INFOMSG("Looking for sample files in " << pathToFiles);

    int nErrors = 0;
    try {
        INFOMSG("Running tests...");

        // individual tests
        RUN_TEST(BasicFileIO);

    } catch (exception& e) {
        ERRORMSG("uncaught exception: " << e.what());
        nErrors++;
    }

    while (filesCreated.size() > 0) {
        CFile file(filesCreated.front());
        if (!file.Remove()) {
            ERRORMSG("Couldn't remove file " << filesCreated.front());
            nErrors++;
        }
        filesCreated.pop_front();
    }

    if (nErrors == 0)
        INFOMSG("No errors encountered, all test succeeded!");
    else
        ERRORMSG(nErrors << " error" << ((nErrors == 1) ? "" : "s") << " encountered");
    return nErrors;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // diagnostic streams setup
    SetDiagStream(&NcbiCout);       // send all diagnostic messages to cout
    SetDiagPostLevel(eDiag_Info);   // show all messages

    // turn on C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    ASNIOTestApp app;
    return app.AppMain(argc, argv, NULL, eDS_Default, NULL);    // don't use config file
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/11/21 14:48:51  thiessen
* initial checkin
*
*/
