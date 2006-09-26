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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_args.hpp
 * Interface for converting blast-related command line
 * arguments into blast options
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Base class for converting command-line arguments into
/// blast engine options. Note that because CNcbiApplication
/// owns the actual command line argument values, this class
/// only sets up CArgDescriptions, then produces blast options
/// from an external source of argument values
class CBlastArgs : public CObject
{
public:
    /// Constructor
    /// @param prog_name Desired name of blast program [in]
    /// @param prog_desc Description of blast program [in]
    CBlastArgs(const char *prog_name,
               const char *prog_desc);

    // Getters for command-line arguments that are not
    // consumed by the blast options but are still needed
    // to implement functionality of any blast binary

    /// Retrieve file containing query sequences
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    CNcbiIstream& GetQueryFile(const CArgs& args);

    /// Retrieve file for outputting blast results
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    CNcbiOstream& GetOutputFile(const CArgs& args);

    /// Retrieve filename for outputting ASN.1 blast results
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetAsnOutputFile(const CArgs& args);

    /// Retrieve the range limitation on all query sequences read in.
    /// Output range is a pair of numbers from 0...(query_len - 1)
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    /// @param from Start of query range, 0 if not specified in
    ///             command line [out]
    /// @param to End of query range, 0 if not specified in
    ///             command line [out]
    void GetQueryLoc(const CArgs& args, int& from, int& to);

    /// Retrieve number of threads used to perform the preliminary
    /// blast search
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetNumThreads(const CArgs& args);

    /// Retrieve the desired type of formatted output
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetFormatType(const CArgs& args);

    /// Retrieve the number of 1-line summaries in formatted output
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetNumDescriptions(const CArgs& args);

    /// Retrieve the number of database sequences to print in
    /// formatted output
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetNumDBSeq(const CArgs& args);

    /// Return true if GI numbers should appear in sequence descriptions
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetShowGi(const CArgs& args);

    /// Return true if query sequence IDs may be parsed
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetBelieveQuery(const CArgs& args);

    /// Return true lowercase query letters are to be masked
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetLowercase(const CArgs& args);

    /// Return true if blast output is to be in HTML format
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetHtml(const CArgs& args);

    /// Return the maximum size of a batch of different query sequences
    /// that will be searched simultaneously
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    virtual int GetQueryBatchSize(const CArgs& args);

protected:
    /// Fill in the command line arguments expected of all blast programs
    /// param arg_desc Previously allocated arguments, to be
    ///               augmented with default arguments [in][out]
    void x_SetArgDescriptions(CArgDescriptions * arg_desc);

    /// Convert the default command-line arguments into blast options
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    /// @param handle Previously allocated blast options [in][out]
    void x_SetOptions(const CArgs& args,
                      blast::CBlastOptionsHandle *handle);

    /// Retrieve the type of wordfinder to use (0=2-hit, 1=1-hit)
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetWordfinder(const CArgs& args);

    /// Retrieve the 2-hit window size to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetWindowSize(const CArgs& args);

    const char *m_ProgName;
    const char *m_ProgDesc;
};


/// Handle command line arguments for blastall binary
class CBlastallArgs : public CBlastArgs
{
public:
    /// Constructor
    CBlastallArgs();

    /// Retrieve the blast program
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetProgram(const CArgs& args);

    /// Retrieve the blast database name
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetDatabase(const CArgs& args);

    /// Retrieve the name of a psi-tblastn checkpoint file
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetPsiTblastnCheckpointFile(const CArgs& args);

    /// Retrieve list of GIs to search
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetGilist(const CArgs& args);

    /// Retrieve the strand of query sequences to search
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    objects::ENa_strand GetQueryStrand(const CArgs& args);

    /// Retrieve the size of the database to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    double GetDBSize(const CArgs& args);

    /// Return the maximum size of a batch of different query sequences
    /// that will be searched simultaneously
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    virtual int GetQueryBatchSize(const CArgs& args);

    /// Compute the command line arguments to use
    CArgDescriptions * SetCommandLine();

    /// Compute the blast options to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    blast::CBlastOptionsHandle * SetOptions(const CArgs& args);
};

/// Handle command line arguments for megablast binary
class CMegablastArgs : public CBlastArgs
{
public:
    /// Constructor
    CMegablastArgs();

    /// Retrieve the blast database name
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetDatabase(const CArgs& args);

    /// Retrieve list of GIs to search
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetGilist(const CArgs& args);

    /// Return true if a masked version of the query is to be generated
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetMaskedQueryOut(const CArgs& args);

    /// Return true if full database sequence IDs are to be parsed
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetFullId(const CArgs& args);

    /// Return true if a completion line is to follow tabular output
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    bool GetLogEnd(const CArgs& args);

    /// Retrieve the strand number of query sequences to search
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    objects::ENa_strand GetQueryStrand(const CArgs& args);

    /// Retrieve the megablast output type
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    int GetOutputType(const CArgs& args);

    /// Retrieve the size of the database to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    double GetDBSize(const CArgs& args);

    /// Return the maximum size of a batch of different query sequences
    /// that will be searched simultaneously
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    virtual int GetQueryBatchSize(const CArgs& args);

    /// Compute the command line arguments to use
    CArgDescriptions * SetCommandLine();

    /// Compute the blast options to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    blast::CBlastOptionsHandle * SetOptions(const CArgs& args);
};

class CRPSBlastArgs : public CBlastArgs
{
public:
    CRPSBlastArgs();

    /// Retrieve the blast database name
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    string GetDatabase(const CArgs& args);

    /// Retrieve the size of the database to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    double GetDBSize(const CArgs& args);

    /// Return the maximum size of a batch of different query sequences
    /// that will be searched simultaneously
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    virtual int GetQueryBatchSize(const CArgs& args);

    /// Compute the command line arguments to use
    CArgDescriptions * SetCommandLine();

    /// Compute the blast options to use
    /// @param args The list of command line args from which data
    ///             will be retrieved [in]
    blast::CBlastOptionsHandle * SetOptions(const CArgs& args);
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP */
