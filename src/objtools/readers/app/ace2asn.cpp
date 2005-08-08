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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Tool to convert ACE file to ASN.1 Seq-entry
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/phrap.hpp>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Phrap reader application
//


class CAce2AsnApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CAce2AsnApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("ace", "AceFile", "Source ACE file",
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("asn", "AsnFile",
                             "Output ASN.1 text file",
                             CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("asnb", "AsnBinFile",
                             "Output ASN.1 binary file",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fBinary);

    arg_desc->AddDefaultKey("no_complement",
                            "fPhrap_NoComplement",
                            "Ignore 'complemented' flags of traces",
                            CArgDescriptions::eBoolean,
                            "f");
    arg_desc->AddDefaultKey("pack_data",
                            "fPhrap_PackSeqData",
                            "Use best coding to pack sequence data",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("gaps",
                            "fPhrap_FeatGaps",
                            "Add features with list of gaps",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("base_segs",
                            "fPhrap_FeatBaseSegs",
                            "Add features with base segments",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("read_locs",
                            "fPhrap_FeatReadLocs",
                            "Add features with padded read starts",
                            CArgDescriptions::eBoolean,
                            "f");
    arg_desc->AddDefaultKey("tags",
                            "fPhrap_FeatTags",
                            "Convert CT and RT tags to features",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("quality",
                            "fPhrap_FeatQuality",
                            "Add quality/alignment features",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("descr",
                            "fPhrap_Descr",
                            "Add descriptors (DS and WA tags)",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("align", "AlignType",
                            "Alignment type",
                            CArgDescriptions::eString, "optimized");
    arg_desc->SetConstraint("align",
                            &(*new CArgAllow_Strings,
                            "none", "optimized", "all", "pairs"));
    arg_desc->AddDefaultKey("ace_version", "AceVersion",
                            "ACE format version",
                            CArgDescriptions::eString, "auto");
    arg_desc->SetConstraint("ace_version",
                            &(*new CArgAllow_Strings,
                            "auto", "old", "new"));

    string prog_description = "ACE to ASN.1 converter\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CAce2AsnApp::Run(void)
{
    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();

    TPhrapReaderFlags flags = 0;


    if ( args["no_complement"].AsBoolean() ) {
        flags |= fPhrap_NoComplement;
    };
    if ( args["pack_data"].AsBoolean() ) {
        flags |= fPhrap_PackSeqData;
    }
    if ( args["gaps"].AsBoolean() ) {
        flags |= fPhrap_FeatGaps;
    }
    if ( args["base_segs"].AsBoolean() ) {
        flags |= fPhrap_FeatBaseSegs;
    }
    if ( args["read_locs"].AsBoolean() ) {
        flags |= fPhrap_FeatReadLocs;
    }
    if ( args["tags"].AsBoolean() ) {
        flags |= fPhrap_FeatTags;
    }
    if ( args["quality"].AsBoolean() ) {
        flags |= fPhrap_FeatQuality;
    }
    if ( args["descr"].AsBoolean() ) {
        flags |= fPhrap_Descr;
    }

    if (args["align"].AsString() == "optimized") {
        flags |= fPhrap_AlignOptimized;
    }
    else if (args["align"].AsString() == "all") {
        flags |= fPhrap_AlignAll;
    }
    else if (args["align"].AsString() == "pairs") {
        flags |= fPhrap_AlignPairs;
    }

    if (args["ace_version"].AsString() == "new") {
        flags |= fPhrap_NewVersion;
    }
    if (args["ace_version"].AsString() == "old") {
        flags |= fPhrap_OldVersion;
    }

    NcbiCout << "Reading ACE file..." << NcbiEndl;
    CRef<CSeq_entry> entry = ReadPhrap(args["ace"].AsInputFile(), flags);
    bool to_file = args["asn"]  ||  args["asnb"];
    if ( to_file ) {
        NcbiCout << "Saving Seq-entry..." << NcbiEndl;
        if (args["asn"]) {
            args["asn"].AsOutputFile() << MSerial_AsnText << *entry;
        }
        if (args["asnb"]) {
            args["asnb"].AsOutputFile() << MSerial_AsnBinary << *entry;
        }
    }
    else {
        NcbiCout << MSerial_AsnText << *entry;
    }
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CAce2AsnApp().AppMain(argc, argv, 0, eDS_Default, 0);
}




/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2005/08/08 14:58:04  grichenk
 * Adjusted version flags to autodetect ACE version by default.
 *
 * Revision 1.2  2005/08/04 18:12:17  grichenk
 * Changed command line arguments from flags to bool keys.
 *
 * Revision 1.1  2005/07/26 15:38:59  grichenk
 * Initial revision
 *
 * Revision 1.22  2004/11/24 16:22:24  grichenk
 * Do not use GetBioseqCore() to get IDs
 *
 * Revision 1.21  2004/11/01 19:33:08  grichenk
 * Removed deprecated methods
 *
 * Revision 1.20  2004/10/19 19:24:38  grichenk
 * Fixed limit by TSE.
 *
 * Revision 1.19  2004/09/07 14:12:35  grichenk
 * Use GetIds() and Seq-annot handle.
 *
 * Revision 1.18  2004/08/31 21:05:35  grichenk
 * Updated seq-vector coding
 *
 * Revision 1.17  2004/08/19 21:10:51  grichenk
 * Updated object manager sample code
 *
 * Revision 1.16  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.15  2004/05/21 21:41:42  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.14  2004/02/09 19:18:51  grichenk
 * Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
 * and CSeqdesc_CI to avoid using data directly.
 *
 * Revision 1.13  2004/01/07 17:38:03  vasilche
 * Fixed include path to genbank loader.
 *
 * Revision 1.12  2003/06/02 16:06:17  dicuccio
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
 * Revision 1.11  2003/04/24 16:17:10  vasilche
 * Added '-repeat' option.
 * Updated includes.
 *
 * Revision 1.10  2003/04/15 14:25:06  vasilche
 * Added missing includes.
 *
 * Revision 1.9  2003/03/10 18:48:48  kuznets
 * iterate->ITERATE
 *
 * Revision 1.8  2002/11/08 19:43:34  grichenk
 * CConstRef<> constructor made explicit
 *
 * Revision 1.7  2002/11/04 21:29:01  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 1.6  2002/08/30 18:10:20  ucko
 * Let CGBDataLoader pick a driver automatically rather than forcing it
 * to use ID1.
 *
 * Revision 1.5  2002/06/12 18:35:16  ucko
 * Take advantage of new CONNECT_Init() function.
 *
 * Revision 1.4  2002/05/31 13:51:31  grichenk
 * Added comment about iterate()
 *
 * Revision 1.3  2002/05/22 14:29:25  grichenk
 * +registry and logs setup; +iterating over CRef<> container
 *
 * Revision 1.2  2002/05/10 16:57:10  kimelman
 * upper gi bound increased twice
 *
 * Revision 1.1  2002/05/08 14:33:53  ucko
 * Added sample object manager application from stock objmgr_lab code.
 *
 *
 * ---------------------------------------------------------------------------
 * Log for previous incarnation (c++/src/internal/objmgr_lab/sample/demo.cpp):
 *
 * Revision 1.5  2002/05/06 03:46:18  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.4  2002/04/16 15:48:37  gouriano
 * correct gi limits
 *
 * Revision 1.3  2002/04/11 16:33:48  vakatov
 * Get rid of the extraneous USING_NCBI_SCOPE
 *
 * Revision 1.2  2002/04/11 02:32:20  vakatov
 * Prepare for public demo:  revamp names and comments, change output format
 *
 * Revision 1.1  2002/04/09 19:40:16  gouriano
 * ObjMgr sample project
 *
 * ===========================================================================
 */
