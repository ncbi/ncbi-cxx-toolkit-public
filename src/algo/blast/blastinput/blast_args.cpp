/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_args.cpp

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_args.cpp
 * convert blast-related command line
 * arguments into blast options
*/

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

#define ARG_QUERY "i"
#define ARG_EVALUE "e"
#define ARG_FORMAT "m"
#define ARG_OUT "o"
#define ARG_FILTER "F"
#define ARG_GAPOPEN "G"
#define ARG_GAPEXT "E"
#define ARG_XDROP "X"
#define ARG_SHOWGI "I"
#define ARG_DESCRIPTIONS "v"
#define ARG_ALIGNMENTS "b"
#define ARG_THREADS "a"
#define ARG_ASNOUT "O"
#define ARG_BELIEVEQUERY "J"
#define ARG_WORDSIZE "W"
#define ARG_CULLING_LIMIT "K"
#define ARG_MULTIPLE_HITS "P"
#define ARG_SEARCHSP "Y"
#define ARG_HTML "T"
#define ARG_LCASE "U"
#define ARG_XDROP_UNGAPPED "y"
#define ARG_XDROP_FINAL "Z"
#define ARG_QUERYLOC "L"
#define ARG_WINDOW "A"
#define ARG_NUMQUERIES "B"
#define ARG_FORCE_OLD "V"

CBlastArgs::CBlastArgs(const char *prog_name,
                       const char *prog_desc)
    : m_ProgName(prog_name),
      m_ProgDesc(prog_desc)
{
}

void
CBlastArgs::x_SetArgDescriptions(CArgDescriptions * arg_desc)
{
    // program description
    arg_desc->SetUsageContext(string(m_ProgName),
                          string(m_ProgDesc) + " v. " +
                          blast::Version.Print() +
                          " released " +
                          blast::Version.GetReleaseDate());

    // query filename
    arg_desc->AddDefaultKey(ARG_QUERY, "queryfile", 
                     "Query file name (default is standard input)",
                     CArgDescriptions::eInputFile, "-",
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-query", ARG_QUERY);

    //-----------------------------------------------------------------
    arg_desc->SetCurrentGroup("output options");

    // alignment view
    arg_desc->AddOptionalKey(ARG_FORMAT, "alignview", 
                    "alignment view options:\n"
                    "  0 = pairwise (the default),\n"
                    "  1 = query-anchored showing identities,\n"
                    "  2 = query-anchored no identities,\n"
                    "  3 = flat query-anchored, show identities,\n"
                    "  4 = flat query-anchored, no identities,\n"
                    "  5 = query-anchored no identities and blunt ends,\n"
                    "  6 = flat query-anchored, no identities and blunt ends,\n"
                    "  7 = XML Blast output,\n"
                    "  8 = tabular, \n"
                    "  9 tabular with comment lines,\n"
                    "  10 ASN, text,\n"
                    "  11 ASN, binary\n",
                   CArgDescriptions::eInteger,
                   CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_FORMAT, new CArgAllow_Integers(0,11));
    arg_desc->AddAlias("-format", ARG_FORMAT);

    // report output file
    arg_desc->AddDefaultKey(ARG_OUT, "outfile", 
                   "BLAST report output file",
                   CArgDescriptions::eOutputFile, "-",
                   CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-out", ARG_OUT);

    // show GI's in deflines
    arg_desc->AddOptionalKey(ARG_SHOWGI, "showgi", 
                 "Show GI's in deflines",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-show_gi", ARG_SHOWGI);

    // number of one-line to display
    arg_desc->AddOptionalKey(ARG_DESCRIPTIONS, "num_summary",
                 "Number of database sequences to show one-line "
                 "descriptions for (default 500)",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-num_summary", ARG_DESCRIPTIONS);

    // number of alignments per DB sequence
    arg_desc->AddOptionalKey(ARG_ALIGNMENTS, "num_dbseq",
                 "Number of database sequences to show alignments "
                 "for (default 250)",
                 CArgDescriptions::eInteger, 
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-num_dbseq", ARG_ALIGNMENTS);

    // ASN.1 output file
    arg_desc->AddOptionalKey(ARG_ASNOUT, "asn_outfile", 
                   "File name for writing the seqalign results in ASN.1 form",
                   CArgDescriptions::eOutputFile,
                   CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-asn_out", ARG_ASNOUT);

    // believe query ID
    arg_desc->AddOptionalKey(ARG_BELIEVEQUERY, "believe_query", 
                            "Believe the query defline (default F)",
                            CArgDescriptions::eBoolean,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-believe_query", ARG_BELIEVEQUERY);

    // HTML output
    arg_desc->AddOptionalKey(ARG_HTML, "html", 
                            "Produce HTML output (default F)",
                            CArgDescriptions::eBoolean,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-html", ARG_HTML);

    //-----------------------------------------------------------------
    arg_desc->SetCurrentGroup("general search options");

    // evalue cutoff
    arg_desc->AddOptionalKey(ARG_EVALUE, "evalue", 
                     "Expectation value (E) threshold for "
                     "saving hits (default 10.0)",
                     CArgDescriptions::eDouble,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-evalue", ARG_EVALUE);

    // filter string
    arg_desc->AddOptionalKey(ARG_FILTER, "filter_string", 
                  "Filter query sequence (DUST with blastn, SEG with others)",
                  CArgDescriptions::eString,
                  CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-filter", ARG_FILTER);

    // gap open penalty
    arg_desc->AddOptionalKey(ARG_GAPOPEN, "open_penalty", 
                            "Cost to open a gap",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-gapopen", ARG_GAPOPEN);

    // gap extend penalty
    arg_desc->AddOptionalKey(ARG_GAPEXT, "extend_penalty",
                          "Cost to extend a gap",
                          CArgDescriptions::eInteger,
                          CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-gapextend", ARG_GAPEXT);

    // ungapped X-drop
    arg_desc->AddOptionalKey(ARG_XDROP_UNGAPPED, "xdrop_ungap", 
                            "X dropoff value for ungapped extensions in bits",
                            CArgDescriptions::eDouble,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-xdrop_ungap", ARG_XDROP_UNGAPPED);

    // initial gapped X-drop
    arg_desc->AddOptionalKey(ARG_XDROP, "xdrop_gap", 
                 "X-dropoff value (in bits) for preliminary gapped extensions",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-xdrop_gap", ARG_XDROP);

    // final gapped X-drop
    arg_desc->AddOptionalKey(ARG_XDROP_FINAL, "xdrop_final", 
                         "X dropoff value for final gapped alignment in bits",
                         CArgDescriptions::eInteger, 
                         CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-xdrop_gap_final", ARG_XDROP_FINAL);

    // number of threads
    arg_desc->AddOptionalKey(ARG_THREADS, "num_threads",
                            "Number of processors to use in preliminary stage "
                            "of the search",
                            CArgDescriptions::eInteger, 
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-num_threads", ARG_THREADS);

    // word size
    arg_desc->AddOptionalKey(ARG_WORDSIZE, "wordsize", 
                            "Word size (default blastn 11, "
                            "megablast 28, all others 3)",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-wordsize", ARG_WORDSIZE);

    // culling limit
    arg_desc->AddOptionalKey(ARG_CULLING_LIMIT, "culling_limit",
                     "If the query range of a hit is enveloped by that of at "
                     "least this many higher-scoring hits, delete the hit",
                     CArgDescriptions::eInteger,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-culling_limit", ARG_CULLING_LIMIT);

    // choice of ungapped wordfinder
    arg_desc->AddOptionalKey(ARG_MULTIPLE_HITS, "wordfinder", 
                            "0 for multiple hit, 1 for single hit (default 0)",
                            CArgDescriptions::eInteger, 
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_MULTIPLE_HITS, 
                                    new CArgAllow_Integers(0,1));
    arg_desc->AddAlias("-wordfinder", ARG_MULTIPLE_HITS);

    // effective search space
    arg_desc->AddOptionalKey(ARG_SEARCHSP, "searchsp", 
                            "Effective length of the search space "
                            "(default is the real size)",
                            CArgDescriptions::eDouble, 
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-searchsp", ARG_SEARCHSP);

    // lowercase masking
    arg_desc->AddOptionalKey(ARG_LCASE, "lcase", 
                            "Use lower case filtering of query sequence "
                            "(default F)",
                            CArgDescriptions::eBoolean,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-lcase", ARG_LCASE);

    // query location
    arg_desc->AddOptionalKey(ARG_QUERYLOC, "queryloc", 
                            "Location on the query sequence "
                            "(default entire sequence)",
                            CArgDescriptions::eString,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-queryloc", ARG_QUERYLOC);

    // 2-hit wordfinder window size
    arg_desc->AddOptionalKey(ARG_WINDOW, "window", 
                            "Multiple Hits window size",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-window", ARG_WINDOW);

    // placeholder for num queries
    arg_desc->AddOptionalKey(ARG_NUMQUERIES, "numquery", 
                    "Number of concatenated queries. This argument is "
                    "ignored, and only present for backward compatibility",
                    CArgDescriptions::eInteger,
                    CArgDescriptions::fOptionalSeparator);

    // placeholder for forcing use of old engine
    arg_desc->AddOptionalKey(ARG_FORCE_OLD, "oldengine", 
                    "Force use of legacy blast engine. This argument is "
                    "ignored, and only present for backward compatibility",
                    CArgDescriptions::eBoolean,
                    CArgDescriptions::fOptionalSeparator);

    arg_desc->SetCurrentGroup("");
}

CNcbiIstream&
CBlastArgs::GetQueryFile(const CArgs& args)
{
    return args[ARG_QUERY].AsInputFile();
}

CNcbiOstream&
CBlastArgs::GetOutputFile(const CArgs& args)
{
    return args[ARG_OUT].AsOutputFile();
}

string
CBlastArgs::GetAsnOutputFile(const CArgs& args)
{
    if (!args[ARG_ASNOUT])
        return string();
    return args[ARG_ASNOUT].AsString();
}

void
CBlastArgs::GetQueryLoc(const CArgs& args, int& from, int& to)
{
    from = to = 0;
    if (!args[ARG_QUERYLOC])
        return;

    const char *c = args[ARG_QUERYLOC].AsString().c_str();
    char *next_field;
                
    if (c == NULL || c[0] == '\0')
        return;
                
    from = strtol(c, &next_field, 10);
    while (*next_field && !isdigit(*next_field))
        next_field++;
    to = atoi(next_field);

    from = max(from, 0);
    to = max(to, 0);
    if (to > 0 && from > to)
        throw std::runtime_error("Invalid query location");
}

int
CBlastArgs::GetNumThreads(const CArgs& args)
{
    if (!args[ARG_THREADS])
        return 1;
    return args[ARG_THREADS].AsInteger();
}
int
CBlastArgs::GetFormatType(const CArgs& args)
{
    if (!args[ARG_FORMAT])
        return 0;
    return args[ARG_FORMAT].AsInteger();
}

int
CBlastArgs::GetNumDescriptions(const CArgs& args)
{
    if (!args[ARG_DESCRIPTIONS])
        return 500;
    return args[ARG_DESCRIPTIONS].AsInteger();
}

int
CBlastArgs::GetNumDBSeq(const CArgs& args)
{
    if (!args[ARG_ALIGNMENTS])
        return 250;
    return args[ARG_ALIGNMENTS].AsInteger();
}

int
CBlastArgs::GetWordfinder(const CArgs& args)
{
    if (!args[ARG_MULTIPLE_HITS])
        return 0;
    return args[ARG_MULTIPLE_HITS].AsInteger();
}

int
CBlastArgs::GetWindowSize(const CArgs& args)
{
    if (!args[ARG_WINDOW])
        return 0;
    return args[ARG_WINDOW].AsInteger();
}

bool
CBlastArgs::GetShowGi(const CArgs& args)
{
    if (!args[ARG_SHOWGI])
        return false;
    return args[ARG_SHOWGI].AsBoolean();
}

bool
CBlastArgs::GetBelieveQuery(const CArgs& args)
{
    if (!args[ARG_BELIEVEQUERY])
        return false;
    return args[ARG_BELIEVEQUERY].AsBoolean();
}

bool
CBlastArgs::GetLowercase(const CArgs& args)
{
    if (!args[ARG_LCASE])
        return false;
    return args[ARG_LCASE].AsBoolean();
}

bool
CBlastArgs::GetHtml(const CArgs& args)
{
    if (!args[ARG_HTML])
        return false;
    return args[ARG_HTML].AsBoolean();
}

int
CBlastArgs::GetQueryBatchSize(const CArgs& args)
{
    return kMax_Int;
}


void
CBlastArgs::x_SetOptions(const CArgs& args,
                         CBlastOptionsHandle* handle)
{
    CBlastOptions& opt = handle->SetOptions();

    if (args[ARG_FILTER])
        opt.SetFilterString(args[ARG_FILTER].AsString().c_str());

    if (args[ARG_GAPOPEN])
        opt.SetGapOpeningCost(args[ARG_GAPOPEN].AsInteger());
    if (args[ARG_GAPEXT])
        opt.SetGapExtensionCost(args[ARG_GAPEXT].AsInteger());

    // default window size will be 0 for nucleotide and 40
    // for protein. Derived classes may override that, but
    // for now set the window size if it's specified
    // (and applicable)

    if (args[ARG_MULTIPLE_HITS]) {
        if (args[ARG_MULTIPLE_HITS].AsInteger() == 1)
            opt.SetWindowSize(0);
    }
    else if (args[ARG_WINDOW]) {
        opt.SetWindowSize(args[ARG_WINDOW].AsInteger());
    }

    if (args[ARG_WORDSIZE])
        opt.SetWordSize(args[ARG_WORDSIZE].AsInteger());
    if (args[ARG_XDROP_UNGAPPED])
        opt.SetXDropoff(args[ARG_XDROP_UNGAPPED].AsDouble());
    if (args[ARG_XDROP])
        opt.SetGapXDropoff(args[ARG_XDROP].AsInteger());
    if (args[ARG_XDROP_FINAL])
        opt.SetGapXDropoffFinal(args[ARG_XDROP_FINAL].AsInteger());
    if (args[ARG_EVALUE])
        opt.SetEvalueThreshold(args[ARG_EVALUE].AsDouble());
    if (args[ARG_SEARCHSP])
        opt.SetEffectiveSearchSpace((Int8) args[ARG_SEARCHSP].AsDouble());
    if (args[ARG_ALIGNMENTS])
        opt.SetHitlistSize(args[ARG_ALIGNMENTS].AsInteger());
    if (args[ARG_CULLING_LIMIT])
        opt.SetCullingLimit(args[ARG_CULLING_LIMIT].AsInteger());
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*---------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2006/09/26 21:44:12  papadopo
 * add to blast scope; add CVS log
 *
 *-------------------------------------------------------------------*/
