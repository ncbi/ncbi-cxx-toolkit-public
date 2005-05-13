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

    // input file name (required)
    argDescr->AddPositional("in", "name of input file", argDescr->eString);

    SetupArgDescriptions(argDescr);
}

int CAVApp::Run(void)
{
    // process arguments
    CArgs args = GetArgs();

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
        if (nBytes+n > asnString.size())    // ... then allocate new memory in 256k chunks
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


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2005/05/13 14:17:41  thiessen
* require asn memory buffer size
*
* Revision 1.4  2004/07/26 19:15:33  thiessen
* add option to not color HTML paragraphs
*
* Revision 1.3  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/06/02 16:06:41  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.1  2003/03/19 19:04:23  thiessen
* move again
*
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.14  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.13  2003/01/21 18:01:07  thiessen
* add condensed alignment display
*
* Revision 1.12  2002/11/08 19:38:11  thiessen
* add option for lowercase unaligned in FASTA
*
* Revision 1.11  2002/02/12 12:54:11  thiessen
* feature legend at bottom; annot only where aligned
*
* Revision 1.10  2002/02/08 19:53:17  thiessen
* add annotation to text/HTML displays
*
* Revision 1.9  2001/03/02 01:19:25  thiessen
* add FASTA output
*
* Revision 1.8  2001/02/15 19:23:44  thiessen
* add identity coloring
*
* Revision 1.7  2001/02/14 16:06:10  thiessen
* add block and conservation coloring to HTML display
*
* Revision 1.6  2001/01/29 18:13:34  thiessen
* split into C-callable library + main
*
* Revision 1.5  2001/01/25 20:19:12  thiessen
* fix in-memory asn read/write
*
* Revision 1.4  2001/01/25 00:51:20  thiessen
* add command-line args; can read asn data from stdin
*
* Revision 1.3  2001/01/23 17:34:12  thiessen
* add HTML output
*
* Revision 1.2  2001/01/22 15:55:11  thiessen
* correctly set up ncbi namespacing
*
* Revision 1.1  2001/01/22 13:15:24  thiessen
* initial checkin
*
*/
