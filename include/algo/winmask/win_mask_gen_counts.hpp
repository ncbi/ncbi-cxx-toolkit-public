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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Header file for CWinMaskCountsGenerator class.
 *
 */

#ifndef C_WIN_MASK_COUNTS_GENERATOR_H
#define C_WIN_MASK_COUNTS_GENERATOR_H

#include <string>
#include <vector>

#include <corelib/ncbitype.h>
#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief This class encapsulates the n-mer frequency counts generation
 **       functionality of winmasker.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CWinMaskCountsGenerator
{
public:

    /**
     **\brief Constructor.
     **
     ** Creates an instance based on configuration parameters.
     **
     **\param input input file name or a name of the file containing
     **             a list of input files (one per line) depending 
     **             on the value of use_list parameter
     **\param output name of the output file (empty means standard 
     **              output)
     **\param th string describing 4 percentage values (comma separated)
     **          used to compute winmask score thresholds
     **\param mem_avail memory (in megabytes) available to the function
     **\param unit_size n-mer size (value of n)
     **\param min_count do not consider n-mers with counts less than 
     **                 the value this parameter
     **\param max_count maximum n-mer count to consider in winmask
     **                 thresholds computations 
     **\param check_duplicates true if input checking for duplicates is
     **                        requested; false otherwise
     **\param use_list true if input file contains the list of fasta
     **                file names; false if input is the name of the
     **                fasta file itself
     **
     **/
    CWinMaskCountsGenerator( const std::string & input,
                             const std::string & output,
                             const std::string & th,
                             Uint4 mem_avail,
                             Uint1 unit_size,
                             Uint4 min_count,
                             Uint4 max_count,
                             bool check_duplicates,
                             bool use_list );

    /**
     **\brief Object destructor.
     **
     **/
    ~CWinMaskCountsGenerator();

    /**
     **\brief This function does the actual n-mer counting.
     **
     ** Determines the prefix length based on the available memory and
     ** calls process for each prefix to compute partial counts.
     **
     **/
    void operator()();

private:

    /**\internal
     **\brief Compute n-mer frequency counts for a given prefix.
     **
     **\param prefix the prefix string
     **\param prefix_size the prefix length in base pairs
     **\param input list of input fasta files
     **
     **/
    void process( Uint4 prefix, Uint1 prefix_size, 
                  const std::vector< std::string > & input );

    std::string input;      /**<\internal input file (or list of input files) */
    CNcbiOstream * out_stream;  /**<\internal output stream (standard C++ ostream) */
    Uint4 max_mem;      /**<\internal available memory in bytes */
    Uint4 unit_size;        /**<\internal n-mer length in base pairs */
    Uint4 min_count;        /**<\internal minimal n-mer count to consider */
    Uint4 max_count;        /**<\internal maximal n-mer count to consider */
    bool check_duplicates;  /**<\internal whether to check input for duplicates */
    bool use_list;      /**<\internal whether input is a fasta file or a file list */

    Uint8 total_ecodes;         /**<\internal total number of different n-mers found */
    vector< Uint4 > score_counts;   /**<\internal counts table for each suffix */
    double th[4];           /**<\internal percentages used to determine threshold scores */
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

