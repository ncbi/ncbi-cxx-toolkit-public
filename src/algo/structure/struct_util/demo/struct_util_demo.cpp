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
*      Test/demo for utility alignment algorithms
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/structure/struct_util/struct_util.hpp>

#include <objects/cdd/Cdd.hpp>

#include "struct_util_demo.hpp"

// borrow Cn3D's asn i/o functions
#include "../src/app/cn3d/asn_reader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

#define ERROR_MESSAGE(s) ERR_POST(Error << "struct_util_demo: " << s << '!')
#define WARNING_MESSAGE(s) ERR_POST(Warning << "struct_util_demo: " << s)
#define INFO_MESSAGE(s) ERR_POST(Info << "struct_util_demo: " << s)
#define TRACE_MESSAGE(s) ERR_POST(Trace << "struct_util_demo: " << s)

void SUApp::Init(void)
{
    // create command-line argument descriptions
    CArgDescriptions *argDescr = new CArgDescriptions();

    // input/output CD
    argDescr->AddKey("i", "filename", "name of input CD", argDescr->eString);
    argDescr->AddKey("o", "filename", "name of output CD", argDescr->eString);

    // output binary
    argDescr->AddFlag("ob", "output binary data");

    // do IBM
    argDescr->AddFlag("IBM", "do only IBM");

    SetupArgDescriptions(argDescr);
}

static bool ReadCD(const string& filename, CCdd *cdd)
{
    // try to decide if it's binary or ascii
    auto_ptr<CNcbiIstream> inStream(new CNcbiIfstream(filename.c_str(), IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
        ERROR_MESSAGE("Cannot open file '" << filename << "' for reading");
        return false;
    }
    string firstWord;
    *inStream >> firstWord;
    bool isBinary = !(firstWord == "Cdd");
    inStream->seekg(0);     // rewind file
    firstWord.erase();

    string err;
    SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
    bool readOK = Cn3D::ReadASNFromFile(filename.c_str(), cdd, isBinary, &err);
    SetDiagPostLevel(eDiag_Info);
    if (!readOK)
        ERROR_MESSAGE("can't read input file: " << err);
    return readOK;
}

int SUApp::Run(void)
{
    // process arguments
    CArgs args = GetArgs();

    // read input file
    CCdd cdd;
    if (!ReadCD(args["i"].AsString(), &cdd)) {
        ERROR_MESSAGE("error reading input file " << args["i"].AsString());
        return 1;
    }

    // create AlignmentUtility class object
    AlignmentUtility au(cdd.GetSequences(), cdd.GetSeqannot());
    if (!au.Okay())
        return 2;

    // perform IBM on this alignment
    if (!au.DoIBM())
        return 3;

    // repace Seq-annots in cdd with new data
    cdd.SetSeqannot() = au.GetSeqAnnots();

    // write out processed data
    string err;
    if (!Cn3D::WriteASNToFile(args["o"].AsString().c_str(), cdd, args["ob"].HasValue(), &err)) {
        ERROR_MESSAGE("error writing output file " << args["o"].AsString());
        return 4;
    }

    return 0;
}

END_SCOPE(struct_util)


USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

int main(int argc, const char* argv[])
{
    SetDiagStream(&NcbiCerr); // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Info);   // show all messages
//    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams

    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
#ifdef _DEBUG
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
#else
    UnsetDiagTraceFlag(eDPF_File);
    UnsetDiagTraceFlag(eDPF_Line);
#endif

    // C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectIStream::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    SUApp app;
    return app.AppMain(argc, argv, NULL, eDS_Default, NULL);    // don't use config file
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/25 15:52:18  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/
