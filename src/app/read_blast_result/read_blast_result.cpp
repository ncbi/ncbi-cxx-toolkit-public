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
* Author of the template:  Aaron Ucko
*
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* Modified: Azat Badretdinov
*   reads seq-submit file, blast file and optional tagmap file to produce list of potential candidates
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.h"


void CReadBlastApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Reads seq-submit file, blast file and optional tagmap file to produce list of potential FS candidates");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("aligndir", "AlignDir",
         "directory to store selected alignments",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("inblast", "InputBlastFile",
         "name of BLAST output to read from",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("inblastcdd", "InputCDDBlastFile",
         "name of CDD BLAST output to read from",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey(
         "intagmap", "InputTagMap", 
         "use the file to map tags in BLAST",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey(
         "inparents", "InputParentsFile",
         "contains the parent ID for the genomic sequences containing given GI",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey(
         "parentacc", "ParentAccessionNumber",
         "disregard all BLAST hits with this parent_acc number",
         CArgDescriptions::eString);

    arg_desc->AddDefaultKey("infmt", "InputFormat", "format of input file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("infmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    arg_desc->AddOptionalKey
        ("out", "OutputFile",
         "name of file to write to (standard output by default)",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("outTbl", "OutputTblFile",
         "name of file to write additional TBL output (/dev/null by default)",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("outPartial", "OutputFilePartial", 
         "name of the output file for reporting \"partial hit\" problems",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("outOverlap", "OutputFileOverlap", 
         "name of the output file for reporting overlap problems",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("outRnaOverlap", "OutputFileRnaOverlap", 
         "name of the output file for reporting RNA overlap problems",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("outCompleteOverlap", "OutputFileCompleteOverlap", 
         "name of the output file for reporting complete overlap problems",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("outOther", "OutputFileOther", 
         "name of the output file for reporting other problems",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey("outfmt", "OutputFormat", "format of output file",
                            CArgDescriptions::eString, "xml");
    arg_desc->SetConstraint
        ("outfmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

// put output control keys here, except file names
    arg_desc->AddDefaultKey(
         "verbosity", "Verbosity", 
         "Verbosity level threshold",
         CArgDescriptions::eInteger,"-1");

// put numerical algorithm related tuning parameters here
    arg_desc->AddDefaultKey(
         "small_tails_threshold", "small_tails_threshold",
         "the sum of the left and right tails outside the aligned region for the given sum less than this threshold will make it \"small tails\"",
         CArgDescriptions::eDouble, "0.1");

    arg_desc->AddDefaultKey(
         "n_best_hit", "n_best_hit",
         "number of BLAST best hits imported for each sequence",
         CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey(
         "m_eThreshold", "m_eThreshold",
         "only CDD hits below this threshold will be used for partial hit definition",
         CArgDescriptions::eDouble, "1e-6");

    arg_desc->AddDefaultKey(
         "m_entireThreshold", "m_entireThreshold",
         "at least this part of the query needs to be in the alignment to be considered for partial hit candidate",
         CArgDescriptions::eDouble, "0.9");

    arg_desc->AddDefaultKey(
         "m_partThreshold", "m_partThreshold",
         "if aligned region with CDD is less than this threshold, this hit will be considered for partial hit candidate",
         CArgDescriptions::eDouble, "0.8");

    arg_desc->AddDefaultKey(
         "m_rna_overlapThreshold", "m_rna_overlapThreshold",
         "if protein and RNA annotations overlapping more than that threshold, it will be reported",
         CArgDescriptions::eInteger, "30");

    arg_desc->AddDefaultKey(
         "m_cds_overlapThreshold", "m_cds_overlapThreshold",
         "if CDS annotations overlapping more than that threshold, it will be reported",
         CArgDescriptions::eInteger, "30");

    arg_desc->AddDefaultKey(
         "m_trnascan_scoreThreshold", "m_trnascan_scoreThreshold",
         "tRNA-scan predictions below that threshold are ignored",
         CArgDescriptions::eDouble, "60");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

// verbosity   
    m_current_verbosity = 0;

// core data type
    m_coreDataType = eUndefined;

//
    m_usemap=false;
    m_tagmap.clear();
    while(!m_saved_verbosity.empty()) m_saved_verbosity.pop();
}


int CReadBlastApp::Run(void)
{
// Get arguments
    CArgs args = GetArgs();


// verbosity
    m_verbosity_threshold=args["verbosity"].AsInteger();

// algorithm control
    m_small_tails_threshold = args["small_tails_threshold"].AsDouble();
    m_n_best_hit            = args["n_best_hit"].AsInteger();
    m_eThreshold            = args["m_eThreshold"].AsDouble();
    m_entireThreshold       = args["m_entireThreshold"].AsDouble();
    m_partThreshold         = args["m_partThreshold"].AsDouble();
    m_rna_overlapThreshold  = args["m_rna_overlapThreshold"].AsInteger();
    m_cds_overlapThreshold  = args["m_cds_overlapThreshold"].AsInteger();
    m_trnascan_scoreThreshold = args["m_trnascan_scoreThreshold"].AsDouble();

// output control
    // m_align_dir = args["aligndir"].AsString();
    string base = args["in"].AsString();

    if(PrintDetails())
      {
      string scratch;
      NcbiCerr << args.Print(scratch) << NcbiEndl;
      }

    // Read original annotation
    {{
        m_coreDataType = getCoreDataType(args["in"].AsInputFile());
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(s_GetFormat(args["infmt"].AsString()),
                                   args["in"].AsInputFile()));
        if(IsSubmit())
          {
          *in >> m_Submit;
          (*m_Submit.SetData().SetEntrys().begin())->Parentize();
          }
        else if(IsEntry() )
          {
          *in >> m_Entry; 
          m_Entry.Parentize();
          }
        else if(IsTbl() )
          {
          NcbiCerr << "WARNING: tbl file will be read but nothing more will be done." << NcbiEndl;
          if(!m_tbl.Read(args["in"].AsInputFile()))
            {
            NcbiCerr << "FATAL: tbl file does not have any records or have been corrupted" << NcbiEndl;
            throw;
            };
          NcbiCerr << "INFO: tbl file is ok" << NcbiEndl;
          throw;
          }
        else
          {
          NcbiCerr << "FATAL: only tbl, Seq-submit or Seq-entry formats are accepted at this time" << NcbiEndl;
          throw;
          }
         
    }}


    if(PrintDetails() )  printGeneralInfo();
    
    if(PrintDetails() )  NcbiCerr << NcbiEndl << "Execution started" << NcbiEndl << NcbiEndl;
    

    PushVerbosity();
    // m_verbosity_threshold=300;
    CopyInfoFromGenesToProteins();
    // m_verbosity_threshold=-1;
    PopVerbosity();
    // Write the entry
    if( args["out"].HasValue() )
    {   
        auto_ptr<CObjectOStream> out
            (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                                  args["out"].AsOutputFile()));
        if(IsSubmit())
          *out << m_Submit;
        else
          *out << m_Entry;
    }


    if(args["intagmap"].HasValue())
      {
      PushVerbosity();
      m_usemap = ReadTagMap(args["intagmap"].AsString().c_str())!=0;
      PopVerbosity();
      }
      
    // go through the blast file and annotate the sequences as we go
    if(args["parentacc"].HasValue())
      {
      if(!ReadPreviousAcc(args["parentacc"].AsString(), m_previous_genome))
        {
        m_previous_genome.push_back(atol(args["parentacc"].AsString().c_str()));
        }
      if(args["inparents"].HasValue())
        {
        PushVerbosity();
        ReadParents(args["inparents"].AsInputFile(), m_previous_genome);
        PopVerbosity();
        }
      }
    if(args["inblast"].HasValue())
      {   
      map<string, blastStr> blastMap;
      PushVerbosity();
      if(! ReadBlast(args["inblast"].AsString().c_str(), blastMap))
        {
        NcbiCerr << "FATAL: ReadBlast returned bad" << NcbiEndl;
        throw;
        }
      StoreBlast(blastMap);
      PopVerbosity();
      }

    if(args["inblastcdd"].HasValue())
      {
      map<string, blastStr> cddMap;
      PushVerbosity();
      ReadBlast(args["inblastcdd"].AsString().c_str(), cddMap);
      ProcessCDD(cddMap);
      PopVerbosity();
      }

    // Read TRNA
    string tRNA_file = base;
    tRNA_file += ".nfsa.tRNA";
    int ntrna = ReadTRNA2(tRNA_file); // to m_extRNAtable2
    NcbiCerr << "Read TRNAs: " << ntrna << NcbiEndl;

    // end Read TRNA

    // Read RRNA
    string rRNA_file = base;
    rRNA_file += ".nfsa.rRNA";
    int nrrna = ReadRRNA2(rRNA_file); // to m_extRNAtable2
    NcbiCerr << "Read RRNAs: " << nrrna << NcbiEndl;
          
    // end Read TRNA
    m_extRNAtable2.sort(less_simple_seq);

    PushVerbosity();
    SortSeqs(); // also marks CD regions
    PopVerbosity();

    PushVerbosity();
    AnalyzeSeqs(); // also marks CD regions
    PopVerbosity();

    PushVerbosity();
    AnalyzeSeqsViaBioseqs(true, false); // also marks CD regions
    PopVerbosity();

    NcbiCerr << "Checking missing RNA..." << NcbiEndl;
    PushVerbosity();
    simple_overlaps(); // 
    PopVerbosity();

    NcbiCerr << "Dumping FASTA file for subsequent HTML blast output..." << NcbiEndl;
    PushVerbosity();
    dump_fasta_for_pretty_blast(m_diag);
    PopVerbosity();

    bool report_and_forget = false;
    PushVerbosity();
    {
    string sout = args["outPartial"].HasValue() ?
       args["outPartial"].AsString() :
       base + ".partial.problems.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: " 
                                << ePartial
                                << "(ePartial)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, ePartial);
    }
    PopVerbosity();

    PushVerbosity();
    {
    string sout = args["outOverlap"].HasValue() ?
       args["outOverlap"].AsString() :
       base + ".overlap.problems.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: " 
                                << eOverlap
                                << "(eOverlap)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eOverlap);
    }
    PopVerbosity();

    PushVerbosity();
    {
    string sout = args["outRnaOverlap"].HasValue() ?
       args["outRnaOverlap"].AsString() :
       base + ".rna.overlap.problems.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eRnaOverlap
                                << "(eRnaOverlap)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eRnaOverlap);
    }
    PopVerbosity();


    PushVerbosity();
    {
    string sout = args["outCompleteOverlap"].HasValue() ?
       args["outCompleteOverlap"].AsString() :
       base + ".complete.overlap.problems.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: " 
                                << eCompleteOverlap
                                << "(eCompleteOverlap)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eCompleteOverlap);
    }
    PopVerbosity();

    PushVerbosity();
    {
    string sout = base + ".overlap.resolved.problems.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eRemoveOverlap 
                                << "(eRemoveOverlap)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eRemoveOverlap);
    }
    PopVerbosity();

    PushVerbosity();
    {
    string sout  = base + ".tRNA.missing.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eTRNAMissing     
                                << "(eTRNAMissing)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eTRNAMissing);
    reportProblems(report_and_forget, m_diag, out, eTRNAAbsent);
    }
    PopVerbosity();


    PushVerbosity();
    {
    string sout  = base + ".tRNA.bad.strand.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eTRNABadStrand
                                << "(eTRNABadStrand)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eTRNABadStrand);
    }
    PopVerbosity();

    PushVerbosity();
    {
    string sout  = base + ".tRNA.complete.mismatch.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eTRNAComMismatch
                                << "(eTRNAComMismatch)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eTRNAComMismatch);
    }
    PopVerbosity();

    PushVerbosity();
    {
    string sout  = base + ".tRNA.mismatch.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eTRNAMismatch
                                << "(eTRNAMismatch)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eTRNAMismatch);
    }
    PopVerbosity();



    PushVerbosity();
    {
    string sout = args["outOther"].HasValue() ?
       args["outOther"].AsString() :
       base + ".frameshifts.problems.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: " 
                                << eRelFrameShift
                                << "(eRelFrameShift)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eRelFrameShift);
    }
    PopVerbosity();

    PushVerbosity();
    RemoveProblems(); // 
    PopVerbosity();

    PushVerbosity();
    int nremoved = RemoveInterim(); // 
    if(PrintDetails()) NcbiCerr <<  "RemoveInterimAligns: top: removed = " << nremoved << NcbiEndl;
    int nremoved2 = RemoveInterim(); // 
    if(PrintDetails()) NcbiCerr <<  "RemoveInterimAligns: top: removed2 = " << nremoved2 << NcbiEndl;
    if(nremoved2)
      {
      if(PrintDetails()) NcbiCerr <<  "RemoveInterimAligns: ERROR: second take still has removeds" << NcbiEndl;
      }
    PopVerbosity();


    // Write the entry
    if( args["out"].HasValue() )
    {
        args["out"].AsOutputFile().seekp(0);
        auto_ptr<CObjectOStream> out
            (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                                  args["out"].AsOutputFile()));
        if(IsSubmit())
          *out << m_Submit;
        else
          *out << m_Entry;
    }



    // Exit successfully
    return 0;
}



/*
* ===========================================================================
*
* $Log: read_blast_result.cpp,v $
* Revision 1.28  2007/11/08 15:49:04  badrazat
* 1. added locus tags
* 2. fixed bugs in detecting RNA problems (simple seq related)
*
* Revision 1.27  2007/10/03 16:28:52  badrazat
* update
*
* Revision 1.26  2007/09/20 17:15:42  badrazat
* more editing
*
* Revision 1.25  2007/09/20 14:40:44  badrazat
* read routines to their own files
*
* Revision 1.24  2007/07/25 13:08:03  badrazat
* fixed second dump problem
*
* Revision 1.23  2007/07/25 12:40:41  badrazat
* read_blast_result_lib.cpp: renaming some routines
* read_blast_result_lib.cpp: attempt at smarting up RemoveInterim: nothing has been done
* read_blast_result.cpp: second dump of -out asn does not happen, first (debug) dump if'd out
*
* Revision 1.22  2007/07/19 20:59:25  badrazat
* SortSeqs is done for all seqsets
*
* Revision 1.21  2007/06/21 16:21:31  badrazat
* split
*
* Revision 1.20  2007/06/20 18:28:41  badrazat
* regular checkin
*
* Revision 1.19  2007/05/16 18:57:49  badrazat
* fixed feature elimination
*
* Revision 1.18  2007/05/04 19:42:56  badrazat
* *** empty log message ***
*
* Revision 1.17  2006/11/03 15:22:29  badrazat
* flatfiles genomica location starts with 1, not 0 as in case of ASN.1 file
* changed corresponding flatfile locations in the TRNA output
*
* Revision 1.16  2006/11/03 14:47:50  badrazat
* changes
*
* Revision 1.15  2006/11/02 16:44:44  badrazat
* various changes
*
* Revision 1.14  2006/10/25 12:06:42  badrazat
* added a run over annotations for the seqset in case of submit input
*
* Revision 1.13  2006/10/17 18:14:38  badrazat
* modified output for frameshifts according to chart
*
* Revision 1.12  2006/10/17 16:47:02  badrazat
* added modifications to the output ASN file:
* addition of frameshifts
* removal of frameshifted CDSs
*
* removed product names from misc_feature record and
* added common subject info instead
*
* Revision 1.11  2006/10/02 12:50:15  badrazat
* checkin
*
* Revision 1.9  2006/09/08 19:24:23  badrazat
* made a change for Linux
*
* Revision 1.8  2006/09/07 14:21:20  badrazat
* added support of external tRNA annotation input
*
* Revision 1.7  2006/09/01 13:17:23  badrazat
* init
*
* Revision 1.6  2006/08/21 17:32:12  badrazat
* added CheckMissingRibosomalRNA
*
* Revision 1.5  2006/08/11 19:36:09  badrazat
* update
*
* Revision 1.4  2006/05/09 15:08:51  badrazat
* new file cut_blast_output_qnd and some changes to read_blast_result.cpp
*
* Revision 1.3  2006/03/29 19:44:21  badrazat
* same borders are included now in complete overlap calculations
*
* Revision 1.2  2006/03/29 17:17:32  badrazat
* added id extraction from whole product record in cdregion annotations
*
* Revision 1.1  2006/03/22 13:32:59  badrazat
* init
*
* Revision 1000.1  2004/06/01 18:31:56  gouriano
* PRODUCTION: UPGRADED [GCC34_MSVC7] Dev-tree R1.3
*
* Revision 1.3  2004/05/21 21:41:41  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/03/10 18:48:48  kuznets
* iterate->ITERATE
*
* Revision 1.1  2002/04/18 16:05:13  ucko
* Add centralized tree for sample apps.
*
*
* ===========================================================================
*/
