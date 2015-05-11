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
* Author: Azat Badretdin
*
* File Description:
*   Reads ASN file and database search results for the sequences of that file
*   detects and fixes frameshifts, detects partial sequences, strand errors
*   missing RNA annotations, overlaps
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.hpp"


void CReadBlastApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Microbial Genome Submission Check Tool (subcheck) is for the validation of "
         "genome records prior to submission to GenBank. It utilizes a series of "
         "self-consistency checks as well as comparison of submitted annotations to "
         "computed annotations. Some of specified computed annotations could be "
         "pre-computed using BLAST and its modifications and tRNAscanSE. Currently "
         "there is no specific tool for predicting rRNA annotations. Please use the "
         "format specified in documentation"
         );

    // Describe the expected command-line arguments
    arg_desc->AddKey
        ("in", "input_asn",
         "input file in the ASN.1 format, must be either Seq-entry or Seq-submit",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("out", "output_asn",
         "output file in the ASN.1 format, of the same type (Seq-entry or Seq-submit)",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddFlag
        ("kfs",
         "keep frameshifted sequences and make misc_features at the same time. Needs editing after run!");

    arg_desc->AddOptionalKey
        ("inblast", "blast_res_proteins",
         "input file which contains the standard BLAST output results (ran with -IT option) "
         "for all query proteins "
         "sequences specified in the input genome against a protein database (recommended: bact_prot "
         "database of Refseq proteins supplied with the distributed standalone version of this tool)",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("inblastcdd", "blast_res_cdd",
         "input file which contains the standard BLAST output results for all query proteins "
         "sequences specified in input_asn against the CDD database",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("intrna", "input_trna",
         "input tRNAscan predictions in default output format, default value is <-in parameter>.nfsa.tRNA",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("inrrna", "input_rrna",
         "input ribosomal RNA predictions (5S, 16S, 23S), see the manual for format, default value is <-in parameter>.nfsa.rRNA",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey(
         "parentacc", "parent_genome_accession",
         "Refseq accession of the genome which protein annotations need to be excluded from BLAST output results",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
         "inparents", "InputParentsFile",
         "contains a list of all protein accessions/GIs for each Refseq accession/GI",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey(
         "intagmap", "InputTagMap", 
         "use the file to map tags in BLAST",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey("infmt", "InputFormat", "format of input file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("infmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

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
                            CArgDescriptions::eString, "asn");
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
         "the sum of the left and right tails outside the aligned region for "
         "the given sum less than this threshold will make it \"small tails\"",
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

    arg_desc->AddDefaultKey(
         "m_shortProteinThreshold", "m_shortProteinThreshold",
         "proteins shorter than that will be reported and removed",
         CArgDescriptions::eInteger, "11");


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
    const CArgs& args = GetArgs();


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
    m_shortProteinThreshold = args["m_shortProteinThreshold"].AsInteger();

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
          NcbiCerr << "FATAL: only tbl, Seq-submit or Seq-entry formats are accepted at this time. Seq-set has to be present as well" << NcbiEndl;
          throw;
          }
        GetGenomeLen();
        CheckUniqLocusTag();
         
    }}


    if(PrintDetails() )  printGeneralInfo();
    
    if(PrintDetails() )  NcbiCerr << NcbiEndl << "Execution started" << NcbiEndl << NcbiEndl;
    

    PushVerbosity();
    // m_verbosity_threshold=300;
    CopyInfoFromGenesToProteins();
    // m_verbosity_threshold=-1;
    PopVerbosity();
    // Write the entry
    if( args["out"].HasValue() && false)
    {  {
        auto_ptr<CObjectOStream> out
            (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                                  args["out"].AsOutputFile()));
        if(IsSubmit())
          *out << m_Submit;
        else
          *out << m_Entry;
    }}


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
    string tRNA_file;
    if(args["intrna"].HasValue())
      {
      tRNA_file = args["intrna"].AsString();
      }
    else
      {
      tRNA_file = base;
      tRNA_file += ".nfsa.tRNA";
      }
    int ntrna = ReadTRNA2(tRNA_file); // to m_extRNAtable2
    NcbiCerr << "Read TRNAs: " << ntrna << NcbiEndl;

    // end Read TRNA

    // Read RRNA
    string rRNA_file ;
    if(args["inrrna"].HasValue())
      {
      rRNA_file =args["inrrna"].AsString();
      }
    else
      {  
      rRNA_file = base;
      rRNA_file += ".nfsa.rRNA";
      }
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

    NcbiCerr << "Checking short proteins..." << NcbiEndl;
    PushVerbosity();
    short_proteins(); //
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
    string sout  = base + ".tRNA.undef.strand.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eTRNAUndefStrand
                                << "(eTRNAUndefStrand)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eTRNAUndefStrand);
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
    string sout  = base + ".short.annotation.log";
    CNcbiOfstream out( sout.c_str() );
    if(PrintDetails()) NcbiCerr << "Run: before reportProblems: routine start: "
                                << eShortProtein
                                << "(eShortProtein)"
                                << NcbiEndl;
    reportProblems(report_and_forget, m_diag, out, eShortProtein);
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
    LocMap loc_map;
    map<string,string> problem_names;
    CollectFrameshiftedSeqs(problem_names);
    // m_verbosity_threshold = 300; // debuggging only
    RemoveProblems(problem_names, loc_map); // 
    RemoveProblems(problem_names, loc_map); // second run to do whatever was not picked up by the first run
    PopVerbosity();

    PushVerbosity();
    FixStrands(); // 
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
    {{
        args["out"].AsOutputFile().seekp(0);
        auto_ptr<CObjectOStream> out
            (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                                  args["out"].AsOutputFile()));
        if(IsSubmit())
          *out << m_Submit;
        else
          *out << m_Entry;
    }}



    // Exit successfully
    return 0;
}



