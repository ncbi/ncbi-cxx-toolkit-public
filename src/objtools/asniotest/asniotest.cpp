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
        INFOMSG("read file " << filename);
    } catch (exception& e) {
        *err = e.what();
        okay = false;
    }

    return okay;
}

// for writing out ASN data (to temporary files); all files written this way will be removed at program end
template < class ASNClass >
bool WriteASNToFile(const ASNClass& ASNobject, bool isBinary,
    string *err, EFixNonPrint fixNonPrint = eFNP_Default)
{
    err->erase();

    // construct a temp file name
    CNcbiOstrstream oss;
    oss << "test_" << (filesCreated.size() + 1) << (isBinary ? ".bin" : ".txt") << '\0';
    auto_ptr<char> filename;
    filename.reset(oss.str());
    string fullPath = pathToFiles + CDirEntry::GetPathSeparator() + filename.get();
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
        INFOMSG("wrote file " << filename.get());
    } catch (exception& e) {
        *err = e.what();
        okay = false;
    }

    return okay;
}

// test function macros
#define BEGIN_TEST_FUNC(func) \
    int func (void) { \
        int nErrors = 0; \
        string err; \
        INFOMSG("Running " #func " test");

#define END_TEST_FUNC \
        return nErrors; \
    }

#define ADD_ERR(msg) \
    do { \
        ERRORMSG(msg); \
        nErrors++; \
    } while (0)

#define ADD_ERR_RETURN(msg) \
    do { \
        ERRORMSG(msg); \
        nErrors++; \
        return nErrors; \
    } while (0)

BEGIN_TEST_FUNC(BasicFileIO)

    // read in a trivial object
    CSeq_id seqId;
    if (!ReadASNFromFile("pdbSeqId.txt", &seqId, false, &err))
        ADD_ERR_RETURN("failed to load pdbSeqId.txt: " << err);

    // test text and binary output
    if (!WriteASNToFile(seqId, false, &err) ||
        !WriteASNToFile(seqId, true, &err))
        ADD_ERR("failed to write CSeq_id: " << err);

    // now try a bigger object (in binary this time)
    CCdd cdd;
    if (!ReadASNFromFile("ADF.bin", &cdd, true, &err))
        ADD_ERR_RETURN("failed to load ADF.bin: " << err);
    if (!WriteASNToFile(cdd, false, &err) ||
        !WriteASNToFile(cdd, true, &err))
        ADD_ERR("failed to write CCdd: " << err);

END_TEST_FUNC


BEGIN_TEST_FUNC(AssignAndOutput)

    // try to output a copied simple object
    CSeq_id s1;
    if (!ReadASNFromFile("pdbSeqId.txt", &s1, false, &err))
        ADD_ERR_RETURN("failed to load pdbSeqId.txt: " << err);
    CSeq_id s2;
    s2.Assign(s1);
    if (!WriteASNToFile(s2, false, &err) ||
        !WriteASNToFile(s2, true, &err))
        ADD_ERR("failed to write Assign()'ed Seq-id: " << err);

    // try to output a copied complex object
    CCdd c1;
    if (!ReadASNFromFile("ADF.bin", &c1, true, &err))
        ADD_ERR_RETURN("failed to load ADF.bin: " << err);
    CCdd c2;
    c2.Assign(c1);
    if (!WriteASNToFile(c2, false, &err) ||
        !WriteASNToFile(c2, true, &err))
        ADD_ERR("failed to write Assign()'ed Cdd: " << err);

END_TEST_FUNC

// to call test functions, counting errors
#define RUN_TEST(func) \
    do { \
        int errors = func(); \
        nErrors += errors; \
        if (errors) \
            ERRORMSG(#func " test failed"); \
    } while (0)

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
        RUN_TEST(AssignAndOutput);

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
        INFOMSG("No errors encountered, all tests succeeded!");
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
* Revision 1.3  2003/11/25 14:36:16  thiessen
* restructure macros
*
* Revision 1.2  2003/11/21 15:26:12  thiessen
* add Assign test
*
* Revision 1.1  2003/11/21 14:48:51  thiessen
* initial checkin
*
*/
