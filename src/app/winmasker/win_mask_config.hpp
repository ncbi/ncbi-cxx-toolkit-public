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
 *   Header file for CWinMaskConfig class.
 *
 */

#ifndef C_WIN_MASK_CONFIG_H
#define C_WIN_MASK_CONFIG_H

#include <string>
#include <set>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE

class CWinMaskReader;
class CWinMaskWriter;

/**
 **\brief Objects of this class contain winmasker configuration data.
 **
 ** The class is also responsible for validation of command line arguments.
 **/
class CWinMaskConfig
{
public:

    /**
     **\brief Winmasker configuration errors.
     **
     ** This class encapsulates information about different kind of
     ** exceptions that may occur in winmasker configuration.
     **/
    class CWinMaskConfigException : public CException
    {
    public:

        /**
         **\brief Error codes.
         **
         **/
        enum EErrCode
        {
            eInputOpenFail, /**< Can not open input file. */
            eReaderAllocFail,     /**< Memory allocation for input 
                                    reader object failed. */
            eInconsistentOptions    /**< Option validation failure. */
        };

        /**
         **\brief Get the description of an error.
         **
         ** Returns the string corresponding to the error code of
         ** this exception object.
         **
         **\return error description.
         **
         **/
        virtual const char * GetErrCodeString() const;

        NCBI_EXCEPTION_DEFAULT( CWinMaskConfigException, CException );
    };

    /**
     **\brief Object constructor.
     **
     **\param args C++ toolkit style command line arguments.
     **
     **/
    CWinMaskConfig( const CArgs & args );

    /**
     **\brief Winmasker configuration validation.
     **
     ** This function checks the program configuration data for
     ** consistency.
     **
     **/
    void Validate() const;

    /**
     **\brief Get the input reader object.
     **
     **\return the current input reader.
     **
     **/
    CWinMaskReader & Reader() { return *reader; }

    /**
     **\brief Get the output writer object.
     **
     **\return the current output writer.
     **
     **/
    CWinMaskWriter & Writer() { return *writer; }

    /**
     **\brief Get the t_extend value.
     **
     **\return the current t_extend value.
     **/
    Uint4 Textend() const { return textend; }

    /**
     **\brief Get the average unit score threshold.
     **
     **\return the current value of average unit score 
     **        that triggers masking.
     **
     **/
    Uint4 CutoffScore() const { return cutoff_score; }

    /**
     **\brief Get the maximum unit score.
     **
     **\return the current value of the maximum unit score.
     **        Any ecode with larger score will be assigned
     **        the score value specified by -setmaxscore
     **        command line option.
     **
     **/
    Uint4 MaxScore() const { return max_score; }

    /**
     **\brief Get the minimum unit score.
     **
     **\return the current value of the minimum unit score.
     **        Any ecode with smaller score will be assigned
     **        the score value specified by -setminscore
     **        command line option.
     **
     **/
    Uint4 MinScore() const { return min_score; }

    /**
     **\brief Get the alternative score for high scoring units.
     **
     **\return the score value that is assigned to units that
     **        have original score larger than the value
     **        specified by -maxscore command line option.
     **
     **/
    Uint4 SetMaxScore() const { return set_max_score; }

    /**
     **\brief Get the alternative score for low scoring units.
     **
     **\return the score value that is assigned to units that
     **        have original score smaller than the value
     **        specified by -minscore command line option.
     **
     **/
    Uint4 SetMinScore() const { return set_min_score; }

    /**
     **\brief Get the window size.
     **
     **\return the current window size.
     **/
    Uint1 WindowSize() const { return window_size; }

    /**
     **\brief Get the name of the length statistics file.
     **
     **\return the name of the file containing the unit length
     **        statistics.
     **
     **/
    const string LStatName() const { return lstat_name; }

    /**
     **\brief Flag to run the interval merging passes.
     **
     **\return true if interval merging is requested, false
     **        otherwise.
     **
     **/
    bool MergePass() const { return merge_pass; }

    /**
     **\brief Average unit score triggering the interval merging.
     **
     ** For each pair of consequtive mask intervals winmasker 
     ** that are candidates for merging (see description of
     ** CWinMaskConfig::MeanMergeCutoffDist()) winmasker evaluates
     ** the mean unit score of all units in the interval starting
     ** at the start of the first interval and ending at the end
     ** of the second interval. If the result is greater or equal
     ** than the value returned by this function the intervals are
     ** merged.
     **
     **\return the value of the mean unit score triggering 
     **        merging of masked intervals which is the value
     **        of -mscore command line option to winmasker.
     **
     **/
    Uint4 MergeCutoffScore() const { return merge_cutoff_score; }

    /**
     **\brief Distance at which intervals are merged unconditionally.
     **
     **\return The distance in base pairs such that if two consequtive
     **        masked intervals are closer to each other than that
     **        distance then they are merged unconditionally. This is 
     **        the value of -mabs command line option to winmasker.
     **
     **/
    Uint4 AbsMergeCutoffDist() const { return abs_merge_cutoff_dist; }

    /**
     **\brief Distance at which intervals are considered candidates for
     **       merging.
     **
     **\return The distance in base pairs such that if two consequtive
     **        masked intervals are closer to each other tham that 
     **        distance then they are considered candidates for 
     **        merging. They have to pass mean average unit score
     **        test to be merged (see description of
     **        CWinMaskConfig::MergeCutoffScore()). This is the
     **        value of -mmean command line option to winmasker.
     **
     **/
    Uint4 MeanMergeCutoffDist() const { return mean_merge_cutoff_dist; }

    /**
     **\brief Type of the event triggering the masking.
     **
     **\return string describing the type of the event that would trigger
     **        masking of a window. The allowed values are:\n
     **        \b mean - average unit score exceeds the threshold;\n
     **        \b min - minimum unit score exceeds the threshold.
     **
     **/
    const string Trigger() const { return trigger; }

    /**
     **\brief Number of units to count.
     **
     ** If "-trigger min" was specified on the command line, then
     ** this parameter is the number of units that have to be
     ** above threshold to trigger masking.
     **
     **\return number of units to count.
     **
     **/
    Uint1 TMin_Count() const { return tmin_count; }

    /**
     **\brief Whether discontiguous units are used.
     **
     **\return true if discontiguous units should be used;
     **        false otherwise
     **
     **/
    bool Discontig() const { return discontig; }

    /**
     **\brief Pattern to form discontiguous units.
     **
     ** Pattern is a 4-byte long bit mask. Bit n is set
     ** to 1 iff the n-th base in a window should not be
     ** used in a pattern.
     **
     **\return the base pattern to from discontiguous units
     **
     **/
    Uint4 Pattern() const { return pattern; }

    /**
     **\brief Window step.
     **
     **\return the number of bases between two consequtive
     **        windows
     **
     **/
    Uint4 WindowStep() const { return window_step; }

    /**
     **\brief Unit step.
     **
     **\return the distance between consequtive units within
     **        a window
     **
     **/
    Uint1 UnitStep() const { return unit_step; }

    /**
     **\brief Unit step to use for interval merging.
     **
     **\return the distance between units used to estimate
     **        average unit score of the span of two 
     **        intervals that are candidates for merging.
     **
     **/
    Uint1 MergeUnitStep() const { return merge_unit_step; }

    /**
     **\brief Generate n-mer frequency counts.
     **
     **\return true, to generate n-mer frequency counts;
     **        false, for normal behaviour (masking)
     **
     **/
    bool MakeCounts() const { return mk_counts; }

    /**
     **\brief Use a list of fasta files.
     **
     **\return true indicates that -input parameter specifies a file
     **             containing a list of input fasta files;
     **        flase indicates that -input paramater specifies a
     **              single input fasta file
     **
     **/
    bool FaList() const { return fa_list; }

    /**
     **\brief Memory available for n-mer frequency counting.
     **
     **\return memory in megabytes
     **
     **/
    Uint4 Mem() const { return mem; }

    /**
     **\brief n-mer size used for n-mer frequency counting.
     **
     **\return n-mer size in base pairs
     **
     **/
    Uint1 UnitSize() const { return unit_size; }

    /**\brief Total genome length
     **
     **\return genome length as supplied on command line
     **/
    Uint8 GenomeSize() const { return genome_size; }

    /**
     **\brief Value of the -input parameter.
     **
     **\return value of the -input parameter.
     **
     **/
    string Input() const { return input; }

    /**
     **\brief Value of the -output parameter.
     **
     **\return value of the -output parameter.
     **
     **/
    string Output() const { return output; }

    /**
     **\brief Percentage thresholds.
     **
     ** Comma separated list of floating point numbers 
     ** between 0.0 and 100.0 used to compute winmask
     ** score thresholds. The corresponding score 
     ** thresholds will be added as comments to the
     ** end of the output. For each number the program
     ** finds the score such that the corresponding 
     ** fraction of different n-mers has the lower score.
     **
     **\return comma separated list of values
     **
     **/
    string Th() const { return th; }

    /**
     **\brief Dust the input in addition to window masking.
     **
     **\return true if dusting is requested,
     **        false otherwise
     **
     **/
    bool UseDust() const { return use_dust; }

    /**
     **\brief Dust window.
     **
     **\return dust window
     **
     **/
    Uint4 DustWindow() const { return dust_window; }

    /**
     **\brief Dust level.
     **
     **\return dust level
     **
     **/
    Uint4 DustLevel() const { return dust_level; }

    /**
     **\brief Dust linker (in bps).
     **
     **\return dust linker
     **
     **/
    Uint4 DustLinker() const { return dust_linker; }

    /**
     **\brief Check for possibly duplicate sequences in the input.
     **
     **\return true to check for duplicates;
     **        false otherwise
     **
     **/
    bool CheckDup() const { return checkdup; }

    /**
     **\brief The list of query ids to process.
     **
     ** Only the sequences from the input file that match 
     ** one of the ids in this list will be processed.
     **
     **\return the list of query ids to process
     **/
    const set< objects::CSeq_id_Handle > & Ids() const { return ids; }

    /**
     **\brief The list of query ids to exclude from processing.
     **
     ** The sequences from the input file that match this
     ** one of the ids in this list will be excluded from
     ** processing.
     **
     **\return the list of query ids to exclude from processing
     **/
    const set< objects::CSeq_id_Handle > & ExcludeIds() const { return exclude_ids; }

private:

    /**\internal
     **\brief This class is the resource allocator/initializer for 
     **       winmasker input streams (used for safe exception
     **       handling).
     **
     **/
    class CIstreamProxy
    {
    public:

        /**\internal
         **\brief Object constructor.
         **
         ** Objects are usually constructed at the point of
         ** dynamic allocation of corresponding resource.
         **
         **\param newResource points to the istream resource.
         **
         **/
        CIstreamProxy( CNcbiIstream * newResource = NULL )
            : resource( newResource ) {}

        /**\internal
         **\brief Object destructor.
         **
         ** Frees the resource unless it points to the standard input.
         **
         **/
        ~CIstreamProxy()
        { if( resource && resource != &NcbiCin ) delete resource; }

        /**\internal
         **\brief Cast to bool operator.
         **
         **\return true if the resource is non NULL, false otherwise.
         **
         **/
        operator bool() const { return resource != NULL; }

        //@{
        /**\internal
         **\brief Dereference operator.
         **
         **\return resource object pointed to by the internal pointer.
         **
         **/
        CNcbiIstream & operator*() { return *resource; }
        const CNcbiIstream & operator*() const { return *resource; }
        //@}

        //@{
        /**\internal
         **\brief Field access operator.
         **
         **\return this operator return the internal pointer stored in
         **        the object.
         **
         **/
        CNcbiIstream * operator->() { return resource; }
        const CNcbiIstream * operator->() const { return resource; }
        //@}

    private:

        /**\internal
         **\brief Pointer to the resource that is managed by this
         **       object.
         **
         **/
        CNcbiIstream * resource;
    };

    /**\internal
     **\brief This class is the resource allocator/initializer for 
     **       winmasker output streams (used for safe exception
     **       handling).
     **
     **/
    class COstreamProxy
    {
    public:

        /**\internal
         **\brief Object constructor.
         **
         ** Objects are usually constructed at the point of
         ** dynamic allocation of the corresponding resource.
         **
         **\param newResource points to the ostream object.
         **
         **/
        COstreamProxy( CNcbiOstream * newResource = NULL )
            : resource( newResource ) {}

        /**\internal
         **\brief Object destructor.
         **
         ** Frees the resource unless it points to the standard output.
         **
         **/
        ~COstreamProxy()
        { if( resource && resource != &NcbiCout ) delete resource; }

        /**\internal
         **\brief Cast to bool operator.
         **
         **\return true if the resource is non NULL, false otherwise.
         **
         **/
        operator bool() const { return resource != NULL; }

        //@{
        /**\internal
         **\brief Dereference operator.
         **
         **\return resource object pointed to by the internal pointer.
         **
         **/
        CNcbiOstream & operator*() { return *resource; }
        const CNcbiOstream & operator*() const { return *resource; }
        //@}

        //@{
        /**\internal
         **\brief Field access operator.
         **
         **\return this operator return the internal pointer stored in
         **        the object.
         **
         **/
        CNcbiOstream * operator->() { return resource; }
        const CNcbiOstream * operator->() const { return resource; }
        //@}

    private:

        /**\internal
         **\brief Pointer to the resource that is managed by this
         **       object.
         **
         **/
        CNcbiOstream * resource;
    };

    /**
     **\brief Read the list of sequence ids from a given file.
     **
     **\param file_name file to read the ids from
     **\param id_list where to store the ids
     **/
    static void FillIdList( const string & file_name, 
                            set< objects::CSeq_id_Handle > & id_list );

    /**\name Window based masker configuration.
     **/
    //@{
    CIstreamProxy is;               /**< input file resource manager */
    CWinMaskReader * reader;        /**< input reader object */
    COstreamProxy os;               /**< output file resource manager */
    CWinMaskWriter * writer;        /**< output writer object */
    string lstat_name;              /**< name of the file containing unit length statitsics */
    Uint4 textend;                  /**< t_extend value for extension of masked intervals */
    Uint4 cutoff_score;             /**< window score that triggers masking */
    Uint4 max_score;                /**< maximum allowed unit score */
    Uint4 min_score;                /**< minimum allowed unit score */
    Uint4 set_max_score;            /**< score to use for high scoring units */
    Uint4 set_min_score;            /**< score to use for low scoring units */
    Uint1 window_size;              /**< length of a window in base pairs */
    bool merge_pass;                /**< perform extra interval merging passes or not */
    Uint4 merge_cutoff_score;       /**< average unit score triggering interval merging */
    Uint4 abs_merge_cutoff_dist;    /**< distance triggering unconditional interval merging */
    Uint4 mean_merge_cutoff_dist;   /**< distance at which intervals are considered for merging */
    string trigger;                 /**< type of the event that triggers masking */
    Uint1 tmin_count;               /**< number of units to count for min trigger */
    bool discontig;                 /**< true, if using discontiguous units */
    Uint4 pattern;                  /**< base pattern to use for discontiguous units */
    Uint4 window_step;              /**< window step */
    Uint1 unit_step;                /**< unit step */
    Uint1 merge_unit_step;          /**< unit step to use when merging intervals */
    bool mk_counts;                 /**< generate unit counts */
    bool fa_list;                   /**< indicates whether input is a list of fasta file names */
    Uint4 mem;                      /**< memory available for unit counts generator */
    Uint1 unit_size;                /**< unit size (used in unit counts generator */
    Uint8 genome_size;              /**< total size of the genome in bases */
    string input;                   /**< input file name */
    string output;                  /**< output file name (may be empty to indicate stdout) */
    string th;                      /**< percetages to compute winmask thresholds */
    bool use_dust;                  /**< perform dusting in addition to the winmasking */
    Uint4 dust_window;              /**< window size for dusting */
    Uint4 dust_level;               /**< level value for dusting */
    Uint4 dust_linker;              /**< number of bases to use for linking */
    bool checkdup;                  /**< check for duplicate contigs */
    set< objects::CSeq_id_Handle > ids;              /**< list of ids to process */
    set< objects::CSeq_id_Handle > exclude_ids;      /**< list of ids to exclude from processing */
    //@}
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.2  2005/03/08 17:02:30  morgulis
 * Changed unit counts file to include precomputed threshold values.
 * Changed masking code to pick up threshold values from the units counts file.
 * Unit size is computed automatically from the genome length.
 * Added extra option for specifying genome length.
 * Removed all experimental command line options.
 * Fixed id strings in duplicate sequence checking code.
 *
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
 * Revision 1.3  2005/02/25 21:09:18  morgulis
 * 1. Reduced the number of binary searches by the factor of 2 by locally
 *    caching some search results.
 * 2. Automatically compute -lowscore value if it is not specified on the
 *    command line during the counts generation pass.
 *
 * Revision 1.2  2005/02/12 19:58:03  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

