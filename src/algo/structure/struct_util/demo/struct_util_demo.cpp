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
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>

#include <algo/structure/struct_util/struct_util.hpp>
#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>

#include <objects/cdd/Cdd.hpp>

#include "struct_util_demo.hpp"

#include <memory>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

#define ERROR_MESSAGE(s) ERR_POST(Error << "struct_util_demo: " << s << '!')
#define WARNING_MESSAGE(s) ERR_POST(Warning << "struct_util_demo: " << s)
#define INFO_MESSAGE(s) ERR_POST(Info << "struct_util_demo: " << s)
#define TRACE_MESSAGE(s) ERR_POST(Trace << "struct_util_demo: " << s)

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
bool ReadASNFromFile(const char *filename, ASNClass *ASNobject, bool isBinary, std::string *err)
{
    err->erase();

    // initialize the binary input stream
    CNcbiIfstream inStream(filename, IOS_BASE::in | IOS_BASE::binary);
    if (!inStream) {
        *err = "Cannot open file for reading";
        return false;
    }

    auto_ptr<CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new CObjectIStreamAsnBinary(inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new CObjectIStreamAsn(inStream));
    }

    // Read the asn data
    bool okay = true;
    SetDiagTrace(eDT_Disable);
    try {
        *inObject >> *ASNobject;
    } catch (std::exception& e) {
        *err = e.what();
        okay = false;
    }
    SetDiagTrace(eDT_Default);

    inStream.close();
    return okay;
}

// for writing ASN data
template < class ASNClass >
bool WriteASNToFile(const char *filename, const ASNClass& ASNobject, bool isBinary,
    std::string *err, EFixNonPrint fixNonPrint = eFNP_Default)
{
    err->erase();

    // initialize a binary output stream
    CNcbiOfstream outStream(filename,
        isBinary ? (IOS_BASE::out | IOS_BASE::binary) : IOS_BASE::out);
    if (!outStream) {
        *err = "Cannot open file for writing";
        return false;
    }

    auto_ptr<CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new CObjectOStreamAsnBinary(outStream, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new CObjectOStreamAsn(outStream, fixNonPrint));
    }

    // write the asn data
    bool okay = true;
    SetDiagTrace(eDT_Disable);
    try {
        *outObject << ASNobject;
        outStream.flush();
    } catch (std::exception& e) {
        *err = e.what();
        okay = false;
    }
    SetDiagTrace(eDT_Default);

    outStream.close();
    return okay;
}

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

    // leave-one-out args
    argDescr->AddFlag("LOO", "do leave-one-out");
    argDescr->AddOptionalKey("r", "integer", "row to realign (>=2, master=1)", argDescr->eInteger);
    argDescr->SetConstraint("r", new CArgAllow_Integers(2, kMax_Int));
    argDescr->AddOptionalKey("f", "integer", "first block to realign (post-IBM, from 1)", argDescr->eInteger);
    argDescr->SetConstraint("f", new CArgAllow_Integers(1, kMax_Int));
    argDescr->AddOptionalKey("l", "integer", "last block to realign (post-IBM, from 1)", argDescr->eInteger);
    argDescr->SetConstraint("l", new CArgAllow_Integers(1, kMax_Int));
    argDescr->AddOptionalKey("p", "double", "allowed loop length percentile", argDescr->eDouble);
    argDescr->SetConstraint("p", new CArgAllow_Doubles(0.0, kMax_Double));
    argDescr->AddOptionalKey("e", "integer", "allowed loop length extension", argDescr->eInteger);
    argDescr->SetConstraint("e", new CArgAllow_Integers(0, kMax_Int));

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
    bool readOK = ReadASNFromFile(filename.c_str(), cdd, isBinary, &err);
    SetDiagPostLevel(eDiag_Info);
    if (!readOK)
        ERROR_MESSAGE("can't read input file: " << err);
    return readOK;
}

static inline int SumAllRowScores(AlignmentUtility& au)
{
    const BlockMultipleAlignment *aln = au.GetBlockMultipleAlignment();
    int score = 0;
    for (unsigned int row=0; row<aln->NRows(); ++row)
        score += au.ScoreRowByPSSM(row);
    return score;
}

int SUApp::Run(void)
{
    // process arguments
    const CArgs& args = GetArgs();

    // arg interactions
    if (args["LOO"].HasValue() && !args["IBM"].HasValue()) {
        if (!args["r"].HasValue() || !args["f"].HasValue() || !args["l"].HasValue() ||
            !args["e"].HasValue() || !args["p"].HasValue())
        {
            ERROR_MESSAGE("-LOO requires -r, -p, -e, -f, and -l");
            return 5;
        }
    } else if (args["IBM"].HasValue() && !args["LOO"].HasValue()) {
        if (args["r"].HasValue() || args["f"].HasValue() || args["l"].HasValue())
            WARNING_MESSAGE("-IBM specified; ignoring -r, -f, and -l");
    } else {
        ERROR_MESSAGE("must specify either -IBM or -LOO");
        return 7;
    }

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
    if (args["IBM"].HasValue()) {
        if (!au.DoIBM()) {
            ERROR_MESSAGE("IBM failed");
            return 3;
        }
    }

    // perform leave-one-out
    else if (args["LOO"].HasValue()) {
        vector < unsigned int > blocksToRealign;
        for (int i=args["f"].AsInteger(); i<=args["l"].AsInteger(); ++i)
            blocksToRealign.push_back(i - 1);               // convert to zero-numbering
        INFO_MESSAGE("score of row " << args["r"].AsInteger() << " vs. PSSM(N) before realignment: "
            << au.ScoreRowByPSSM(args["r"].AsInteger() - 1));
        INFO_MESSAGE("sum of row scores before realignment: " << SumAllRowScores(au));
        if (!au.DoLeaveOneOut(
                args["r"].AsInteger() - 1, blocksToRealign,         // what to realign
                args["p"].AsDouble(), args["e"].AsInteger(), 0))    // max loop length calculation parameters
        {
            ERROR_MESSAGE("LOO failed");
            return 8;
        }
        INFO_MESSAGE("score of row " << args["r"].AsInteger() << " vs PSSM(N) after realignment: "
            << au.ScoreRowByPSSM(args["r"].AsInteger() - 1));
        INFO_MESSAGE("sum of row scores vs. PSSM(N) after realignment: " << SumAllRowScores(au));
    }

    // replace Seq-annots in cdd with new data
    cdd.SetSeqannot() = au.GetSeqAnnots();

    // write out processed data
    string err;
    if (!WriteASNToFile(args["o"].AsString().c_str(), cdd, args["ob"].HasValue(), &err)) {
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
    SetDiagStream(&NcbiCerr);       // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Info);   // show all messages

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
