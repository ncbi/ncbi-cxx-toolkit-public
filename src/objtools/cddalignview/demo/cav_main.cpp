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
*      Main application class for CDDAlignmentViewer
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_limits.h>

#include <objtools/cddalignview/demo/cav_main.hpp>
#include <objtools/cddalignview/cddalignview.h>


BEGIN_NCBI_SCOPE

void CAVApp::Init(void)
{
    // create command-line argument descriptions
    CArgDescriptions *argDescr = new CArgDescriptions();

    // usage
    argDescr->SetUsageContext(GetArguments().GetProgramName(), "CDD Alignment Viewer");

    // output type (required)
    argDescr->AddKey("type", "type", "one of: 'text' (unformatted), 'HTML', or 'FASTA'", argDescr->eString);
    argDescr->SetConstraint("type", (new CArgAllow_Strings())->Allow("text")->Allow("HTML")->Allow("FASTA"));

    // lowercase flag for FASTA
    argDescr->AddFlag("fasta_lc", "whether to show unaligned residues in lowercase in FASTA output");

    // paragraph width (optional, default 60)
    argDescr->AddDefaultKey("width", "integer", "paragraph width", argDescr->eInteger, "60");
    argDescr->SetConstraint("width", new CArgAllow_Integers(1, kMax_Int));

    // conservation threshhold (optional, default 2.0)
    argDescr->AddDefaultKey("cons", "bits", "conservation threshhold (bit score)", argDescr->eDouble, "2.0");
    argDescr->AddFlag("identity", "show identity, ignoring bit score");

    // whether to output left/right tails
    argDescr->AddFlag("lefttails", "whether to show left tails");
    argDescr->AddFlag("righttails", "whether to show right tails");

    // whether to do condensed display
    argDescr->AddFlag("condensed", "condensed incompletely aligned columns (text/HTML only)");

    // don't use colored backgrounds
    argDescr->AddFlag("no_color_bg", "don't use colored backgrounds for alignment paragraphs (HTML only)");

    // ignore bad pairwise alignments
    argDescr->AddFlag("ignore_bad_aln", "ignore invalid pairwise alignments in input data");

    // input file name (required)
    argDescr->AddPositional("in", "name of input file", argDescr->eString);

    SetupArgDescriptions(argDescr);
}

int CAVApp::Run(void)
{
    // process arguments
    const CArgs& args = GetArgs();

    // open input file
    CNcbiIfstream *asnIfstream = new CNcbiIfstream(args["in"].AsString().c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (!asnIfstream) {
        ERR_POST(Error << "Can't open input file " << args["in"].AsString());
        return -1;
    }

    // load input file into memory
    string asnString;
    static const int bufSize = 65536;
    char buf[bufSize];
    asnString.resize(bufSize);  // start small...
    int nBytes = 0, n, i;
    while (!(asnIfstream->eof() || asnIfstream->fail() || asnIfstream->bad())) {
        asnIfstream->read(buf, bufSize);
        n = asnIfstream->gcount();
        if (nBytes+n > (int)asnString.size())    // ... then allocate new memory in 256k chunks
            asnString.resize(asnString.size() + bufSize*4);
        for (i=0; i<n; i++) asnString[nBytes + i] = buf[i];
        nBytes += n;
    }
    delete asnIfstream;

    // process options
    unsigned int options = CAV_DEBUG;

    if (args["type"].AsString() == "text")
        options |= CAV_TEXT;
    else if (args["type"].AsString() == "HTML")
        options |= CAV_HTML | CAV_HTML_HEADER;
    else if (args["type"].AsString() == "FASTA")
        options |= CAV_FASTA;

    if (args["lefttails"].HasValue()) options |= CAV_LEFTTAILS;
    if (args["righttails"].HasValue()) options |= CAV_RIGHTTAILS;
    if (args["condensed"].HasValue()) options |= CAV_CONDENSED;
    if (args["identity"].HasValue()) options |= CAV_SHOW_IDENTITY;
    if (args["fasta_lc"].HasValue()) options |= CAV_FASTA_LOWERCASE;
    if (args["no_color_bg"].HasValue()) options |= CAV_NO_PARAG_COLOR;
    if (args["ignore_bad_aln"].HasValue()) options |= CAV_IGNORE_BAD_ALN;

    // for testing alignment features
#if 0
    AlignmentFeature feats[2];
    int nFeats = 2;

    feats[0].nLocations = 90/5;
    feats[0].locations = new int[90/5];
    for (i=5; i<=90; i+=5) feats[0].locations[i/5 - 1] = i - 1;
    feats[0].shortName = "Feature 1";
    feats[0].description = "Long description of Feature 1";
    feats[0].featChar = '@';

    feats[1].nLocations = 90/3;
    feats[1].locations = new int[90/3];
    for (i=3; i<=90; i+=3) feats[1].locations[i/3 - 1] = i - 1;
    feats[1].shortName = "Feature 2";
    feats[1].description = "Long description of Feature 2";
    feats[1].featChar = '%';

    options |= CAV_ANNOT_BOTTOM;

#else
    AlignmentFeature *feats = NULL;
    int nFeats = 0;
#endif

    // actually do the function
    return
        CAV_DisplayMultiple(
            static_cast<const void*>(asnString.data()),
            asnString.size(),
            options,
            args["width"].AsInteger(),
            args["cons"].AsDouble(),
            "Alignment - created from command line",
            nFeats, feats
        );
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    SetDiagStream(&NcbiCerr); // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Info);   // show all messages

    CAVApp app;
    return app.AppMain(argc, argv, NULL, eDS_Default, NULL);    // don't use config file
}
