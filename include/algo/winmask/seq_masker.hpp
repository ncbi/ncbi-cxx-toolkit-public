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
 *   Header file for CSeqMasker class.
 *
 */

#ifndef C_SEQ_MASKER_H
#define C_SEQ_MASKER_H

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

#include <algo/winmask/seq_masker_window.hpp>
#include <algo/winmask/seq_masker_istat.hpp>

BEGIN_NCBI_SCOPE

class CSeqMaskerScore;

/**
 **\brief Main interface to window based masker functionality.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMasker
{
public:

    /**
     **\brief Type representing a masked interval within a sequence.
     **
     ** If A is an object of type TMaskedInterval, then A.first is
     ** the offset (starting from 0) of the beginning of the
     ** interval; A.second is the offset of the end of the interval.
     **
     **/
    typedef pair< TSeqPos, TSeqPos > TMaskedInterval;

    /**
     **\brief A type representing the total of masking information 
     **       about a sequence.
     **
     **/
    typedef vector< TMaskedInterval > TMaskList;

    /**
     **\brief Represents different error situations that can occur
     **       in the masking process.
     **/
    class CSeqMaskerException : public CException
    {
    public:

        /**
         **\brief Integer error codes.
         **/
        enum EErrCode
        {
            eLstatStreamIpenFail,   /**< Error opening the length statistics file */
            eLstatSyntax,           /**< Error parsing the length statistics file */
            eLstatParam,            /**< Error deducing parameters from lstat or command line */
            eScoreAllocFail,        /**< Error allocating the score function object */
            eScoreP3AllocFail,      /**< Error allocating the score function object for merging pass */
            eValidation             /**< Insconsistent internal parameters */
        };

        /**
         **\brief Get the exception description string.
         **
         ** The method translates internal error code in the exception
         ** object into a human readable explanation string.
         **
         **\return explanation string for the exception
         **
         **/
        virtual const char * GetErrCodeString() const;

        NCBI_EXCEPTION_DEFAULT( CSeqMaskerException, CException );
    };

    /**
     **\brief Merge together two result lists.
     **
     ** Used to merge results lists obtained from winmask and dust
     ** algorithms.
     **
     **\param dest this list will contain the merged data
     **\param src the other results list
     **/
    static void MergeMaskInfo( TMaskList * dest, const TMaskList * src );

    /**
     **\brief Object constructor.
     **
     ** Parameters to the constructor determine the behaviour of the
     ** window based masking procedure.
     **
     **\param lstat_name the name of the file containing length statistics
     **\param arg_window_size the window size in bps
     **\param arg_window_step the window step
     **\param arg_unit_step the unit step
     **\param arg_textend the score above which it is allowed to keep masking
     **\param arg_cutoff_score the unit score triggering the masking
     **\param arg_max_score maximum allowed unit score
     **\param arg_min_score minimum allowed unit score
     **\param arg_set_max_score score to use for units exceeding max_score
     **\param arg_set_min_score score to use for units below min_score
     **\param arg_merge_pass whether or not to perform an interval merging pass
     **\param arg_merge_cutoff_score combined average score at which intervals
     **                              should be merged
     **\param arg_abs_merge_cutoff_dist maximum distance between intervals
     **                                 at which they can be merged 
     **                                 unconditionally
     **\param arg_mean_merge_cutoff_dist maximum distance between intervals
     **                                  at which they can be merged if they
     **                                  satisfy arg_merge_cutoff_score 
     **                                  threshold
     **\param arg_merge_unit_step unit step to use for interval merging
     **\param arg_trigger determines which method to use to trigger masking
     **\param tmin_count if arg_trigger is "min" then determines how many of
     **                  the units in a window should be above the score
     **                  threshold in order to trigger masking
     **\param arg_discontig whether or not to use discontiguous units
     **\param arg_pattern base pattern to form discontiguous units
     **\param arg_use_ba use bit array optimization, if available
     **
     **/
    CSeqMasker( const string & lstat_name,
                Uint1 arg_window_size,
                Uint4 arg_window_step,
                Uint1 arg_unit_step,
                Uint4 arg_textend,
                Uint4 arg_cutoff_score,
                Uint4 arg_max_score,
                Uint4 arg_min_score,
                Uint4 arg_set_max_score,
                Uint4 arg_set_min_score,
                bool arg_merge_pass,
                Uint4 arg_merge_cutoff_score,
                Uint4 arg_abs_merge_cutoff_dist,
                Uint4 arg_mean_merge_cutoff_dist,
                Uint1 arg_merge_unit_step,
                const string & arg_trigger,
                Uint1 tmin_count,
                bool arg_discontig,
                Uint4 arg_pattern,
                bool arg_use_ba );

    /**
     **\brief Object destructor.
     **
     **/
    ~CSeqMasker(); 

    /**
     **\brief Sequence masking operator.
     **
     ** seq_masker objects are function objects with. Main
     ** processing is done by () operator.
     **
     **\param data the original sequence data in iupacna format
     **\return pointer to the list of masked intervals
     **
     **/
    TMaskList * operator()( const objects::CSeqVector & data ) const;

private:

    /**\internal
     **\brief Internal representation of a sequence interval.
     **/
    struct mitem
    {
        Uint4 start;    /**< Start of the interval */
        Uint4 end;  /**< End of the interval */
        double avg; /**< Average score of the units in the interval */

        /**
         **\brief Object constructor.
         **
         ** All the additional parameters are used by the constructor to compute
         ** the value of avg.
         **
         **\param start the start of the interval
         **\param end the end of the interval
         **\param unit_size the unit size in bases
         **\param data the original sequence data in iupacna format
         **\param owner back pointer to the seq_masker instance
         **
         **/
        mitem( Uint4 start, Uint4 end, Uint1 unit_size, 
               const objects::CSeqVector & data, const CSeqMasker & owner );
    };

    friend struct CSeqMasker::mitem;

    /**\internal
     **\brief Type used for storing intermediate masked and unmasked intervals.
     **/
    typedef list< mitem > TMList;

    /** \internal
        \brief Final masking pass with lookups of the actual Nmer scores.
        \param data the sequence data
        \param start start masking at this location
        \param end stop masking at this location
        \return container with masked intervals
     */
    TMaskList * DoMask( const objects::CSeqVector & data,
                        TSeqPos start, TSeqPos end ) const;

    /**\internal
     **\brief Computes the average score of an interval generated by 
     **       connecting two neighbouring masked intervals.
     **
     **\param mi points to the first masked interval
     **\param umi points to the right unmasked neighbour of mi
     **\param unit_size the unit size to use in computations
     **\return the average score of an interval formed by
     **        mi, umi, and mi+1
     **
     **/
    double MergeAvg( TMList::iterator mi, const TMList::iterator & umi,
                     Uint4 unit_size ) const;

    /**\internal
     **\brief Merge two neighbouring masked intervals.
     **
     ** Merges intervals mi and mi+1 into one with average of the
     ** triple mi,umi,mi+1. Removes mi mi+1 from m and substitues
     ** mi with the merged interval. Removes umi from um.
     **
     **\param m list of intervals containing mi
     **\param mi points to the first masked interval in the pair 
     **          that is being merged
     **\param um list of intervals containing umi
     **\param umi points to the right unmasked neighbour of mi
     **
     **/
    void Merge( TMList & m, TMList::iterator mi, 
                TMList & um, TMList::iterator & umi ) const;

    /**\internal
     **\brief Container of the unit score statistics.
     **/
    CRef< CSeqMaskerIstat > ustat;

    /**\internal
     **\brief Score function object to use for extensions.
     **/
    CSeqMaskerScore * score;

    /**\internal
     **\brief Score function object to use for merging.
     **/
    CSeqMaskerScore * score_p3;

    /**\internal
     **\brief Score function object to use for triggering masking.
     **/
    CSeqMaskerScore * trigger_score;

    /**\internal
     **\brief The window size in bases.
     **/
    Uint1 window_size;

    /**\internal
     **\brief The window step.
     **
     ** Only windows that start at 0 mod window_step will be considered.
     **
     **/
    Uint4 window_step;

    /**\internal
     **\brief The unit step.
     **
     ** The distance between consequtive units within a window.
     **
     **/
    Uint1 unit_step;

    /**\internal
     **\brief Flag indicating whether the merging pass is required.
     **/
    bool merge_pass;

    /**\internal
     **\brief Average score that triggers merging of neighbouring 
     **       masked intervals.
     **/
    Uint4 merge_cutoff_score;

    /**\internal
     **\brief Neighbouring masked intervals that closer to each other
     **       than this distance are merged unconditionally.
     **/
    Uint4 abs_merge_cutoff_dist;

    /**\internal
     **\brief Neighbouring masked intervals that are farther apart from
     **       each other than this distance are never merged.
     **/
    Uint4 mean_merge_cutoff_dist;

    /**\internal
     **\brief Unit step to use for interval merging.
     **
     ** This is the unit step value that should be used when 
     ** computing the unit score average over the total span of
     ** two intervals that are candidates for merging.
     **
     **/
    Uint1 merge_unit_step;

    /**\internal
     **\brief Symbolic names for different masking triggering methods.
     **/
    enum
    {
        eTrigger_Mean = 0,  /**< Using mean of unit scores in the window. */
        eTrigger_Min        /**< Using min score of k unit in the window. */
    } trigger;

    /**\internal
     **\brief Flag indicating the use of discontiguous units.
     **/
    bool discontig;

    /**\internal
     **\brief Base pattern to form discontiguous units.
     **/
    Uint4 pattern;
};

END_NCBI_SCOPE

#endif
