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
 *   Implementation of CheckDuplicates() function.
 *
 */

#include <ncbi_pch.hpp>
#include <vector>
#include <string>
#include <map>

#include <corelib/ncbitype.h>
#include <corelib/ncbistre.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include "win_mask_fasta_reader.hpp"
#include "win_mask_dup_table.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


extern string mkdata( const CSeq_entry & entry );

const Uint4 SAMPLE_LENGTH    = 100; /**<\internal length of a sample segment */
const Uint4 SAMPLE_SKIP      = 10000;   /**<\internal distance between subsequent samples */
const Uint4 MIN_SEQ_LENGTH   = 50000;   /**<\internal sequences of length below MIN_SEQ_LENGTH are not considered for 
                                          duplication check */
const Uint4 MAX_OFFSET_ERROR = 5;   /**<\internal fuzziness in distance between consequtive matches */
const Uint4 MIN_MATCH_COUNT  = 4;   /**<\internal minimal number of successful consequtive sample matches that
                                      defines a duplication */

//------------------------------------------------------------------------------
/**\internal
 **\brief This class represents a lookup table used to check incoming
 **       sequences for duplication.
 **/
class dup_lookup_table
{
public:

    /**\internal
     **\brief Structure describing a location of a sample in a sequence.
     **/
    struct sample_loc
    {
        size_t seqnum; /**<\internal sequence number (in the order the sequences appear in the input) */
        Uint4 offset;       /**<\internal offset of the sample in the sequence defined by seqnum */

        /**\internal
         **\brief Instance constructor.
         **
         **\param new_seqnum the sequence number
         **\param new_offset the sample offset
         **/
        sample_loc( size_t new_seqnum, Uint4 new_offset )
            : seqnum( new_seqnum ), offset( new_offset ) {}
    };

    /**\internal
     **\brief Class describing the data about a particular sample value.
     **
     ** A sample is a segment of sequence data of length SAMPLE_LENGTH.
     ** This class encapsulates information about all occurances of the
     ** given sample value in the sequences that have been read so far.
     **/
    class sample
    {
    private:

        /**\internal \brief Type for lists of sample locations. */
        typedef vector< sample_loc > loc_list_type;

    public:

        /**\internal
         **\brief Iterator type to traverse lists of sample locations.
         **/
        typedef loc_list_type::const_iterator iterator;

        /**\internal
         **\brief Add a new sample location.
         **
         **\param loc new location
         **/
        void add_loc( const sample_loc & loc )
        { locs.push_back( loc ); }

        /**\internal\name Location traversal.*/
        /**@{*/
        const iterator begin() const { return locs.begin(); }
        const iterator end() const { return locs.end(); }
        /**@}*/

    private:

        loc_list_type locs; /**<\internal the list of sample locations */
    };

private:

    /**\internal
     **\brief Type representing a list of sequence id strings for all 
     **       sequences that have been read so far.
     **/
    typedef vector< string > seq_id_list_type;

    /**\internal
     **\brief A type mapping sample strings to the information about their
     **       occurances in the input.
     **/
    typedef map< string, sample > sample_set_type;

public:

    /**\internal \brief Alias for sample::iterator. */
    typedef sample::iterator iterator;

    /**\internal
     **\brief Augment the data structure with the information about the
     **       new sequence.
     **
     **\param seq_id id string for the new sequence
     **\param seq_data new sequence data in IUPACNA format
     **/
    void add_seq_info( const string & seq_id,
                       const string & seq_data );

    /**\internal
     **\brief Get the sequence id string from the sequence number.
     **
     **\param seqnum the sequence number
     **\return the sequence id string in FASTA format
     **/
    const string seqid( Uint4 seqnum ) const
    { return seq_id_list[seqnum]; }

    /**\internal
     **\brief Get the information about positions of the sample in the
     **       data base from the sample value.
     **
     **\param index the sample value
     **\return pointer to the corresponding instance of the sample class
     **/
    const sample * operator[]( const string & index ) const
    {
        sample_set_type::const_iterator i( sample_set.find( index ) );
        return i == sample_set.end() ? 0 : &(i->second);
    }

private:

    /**\internal
     **\brief Add a sample location to the sample information structure.
     **
     **\param sample the sample value
     **\param loc the sample location description
     **/
    void add_loc( const string & sample, const sample_loc & loc )
    { sample_set[sample].add_loc( loc ); }

    seq_id_list_type seq_id_list;   /**<\internal the list of sequence id strings */
    sample_set_type sample_set;     /**<\internal the sample->sample information map */
};

//------------------------------------------------------------------------------
/**\internal
 **\brief "Less than" comparison between two sample locations.
 **
 ** Sample locations are compared lexicographically on the (seqnum, offset)
 ** pair.
 **
 **\param lhs the first sample location
 **\param rhs the second sample location
 **\return true if lhs < rhs; false otherwise
 **/
inline bool operator<( const dup_lookup_table::sample_loc & lhs, 
                       const dup_lookup_table::sample_loc & rhs )
{
    return lhs.seqnum < rhs.seqnum ? true  :
        lhs.seqnum > rhs.seqnum ? false :
        lhs.offset < rhs.offset ? true  : false;
}

//------------------------------------------------------------------------------
/**\internal
 **\brief "Greater than" comparison of two sample locations.
 **
 ** Defined as: a > b iff b < a.
 **
 **\param lhs the first sample location
 **\param rhs the second sample location
 **\return true if lhs > rhs; false otherwise
 **/
inline bool operator>( const dup_lookup_table::sample_loc & lhs, 
                       const dup_lookup_table::sample_loc & rhs )
{ return rhs < lhs; }

//------------------------------------------------------------------------------
/**\internal
 **\brief Comparisong of two sample locations for equality.
 **
 ** Defined as !(lhs < rhs) and !(rhs < lhs).
 **
 **\param lhs the first sample location
 **\param rhs the second sample location
 **\return true if lhs == rhs; false otherwise
 **/
inline bool operator==( const dup_lookup_table::sample_loc & lhs,
                        const dup_lookup_table::sample_loc & rhs )
{ return !(lhs < rhs) && !(rhs < lhs); }

//------------------------------------------------------------------------------
void dup_lookup_table::add_seq_info( const string & seq_id, 
                                     const string & seq_data )
{
    static string::size_type next_offset( 0 );

    seq_id_list.push_back( seq_id );
    string::size_type data_len( seq_data.length() );

    while( next_offset < data_len - SAMPLE_LENGTH )
    {
        string sample( seq_data.substr( next_offset, SAMPLE_LENGTH ) );
        sample_loc loc( seq_id_list.size() - 1, next_offset );
        add_loc( sample, loc );
        next_offset += SAMPLE_SKIP;
    }

    next_offset = (next_offset <= data_len) ? 0 : next_offset - data_len;
}

//------------------------------------------------------------------------------
/**\internal
 **\brief This class encapsulates the state of duplication search process.
 **
 ** An instance of this class is created for each subject sequence to search
 ** for duplicates among the sequences that were processed earlier.
 **/
class tracker
{
public:

    /**\internal \brief Alias for the sample location description type. */
    typedef dup_lookup_table::sample_loc sample_loc;

private:

    /**\internal
     **\brief Alias for the sample location information iterator type.
     **/
    typedef dup_lookup_table::iterator iterator;

    /**\internal
     **\brief Type representing a possible match currently being tracked
     **       by the tracker instance.
     **/
    struct result
    {
        Uint4 count;            /**<\internal current number of consequtive sample matches */
        sample_loc loc;         /**<\internal information about query location of the last sample match */
        string::size_type s_offset;    /**<\internal location in the subject of the last sample match */

        /**\internal
         **\brief Object constructor.
         **
         **\param newloc location in the query
         **\param new_offset location in the subject
         **\param new_count initial value of match count
         **/
        result( const sample_loc & newloc,
                string::size_type new_offset, 
                Uint4 new_count = 1 )
            : count( new_count ), loc( newloc ), s_offset( new_offset ) {}
    };

    /**\internal
     **\brief type used to store the set of currently tracked matches. 
     **/
    typedef vector< result > result_list_type;

public:

    /**\internal
     **\brief Object constructor.
     **
     **\param the_table the lookup table to search against
     **\param the_subject_id the id string for the subject sequence
     **/
    tracker( const dup_lookup_table & the_table, const string & the_subject_id  ) 
        : table( the_table ), subject_id( the_subject_id ) {}

    /**\internal \brief Object destructor. */
    ~tracker();

    /**\internal
     **\brief Process a set of matches to the lookup table.
     **
     ** The list of matches is given by the pair of iterators. For the
     ** current set of results determines which could be extended. The
     ** ones that can not be extended and whose subject offset is too
     ** far in the past are deleted. The location in [start, end) that
     ** do not extend existing matches start new matches.
     **
     **\param index the sample sequence at the current subject offset
     **\param seqnum the sequence number of the current sequence
     **\param subject_offset current position in the subject sequence
     **\param start start of the list of matches to the lookup table
     **\param end end of the list of matches to the lookup table
     **/
    void operator()( const string & index, Uint4 seqnum,
                     string::size_type subject_offset,
                     iterator start, iterator end );

private:

    const dup_lookup_table & table; /**<\internal lookup table to use */
    const string & subject_id;      /**<\internal id string of the current subject sequence */

    /**\internal
     **\brief Report a candidate for duplicate sequence pair to the
     **       standard error.
     **
     **\param queryseq number of the query sequence
     **\param match_count number consequtive sample matches detected
     **\param s_off last position in the subject sequence
     **\param q_off last position in the query sequence
     **/
    void tracker::report_match( Uint4 queryseq, 
                                Uint4 match_count,
                                string::size_type s_off,
                                string::size_type q_off );

    result_list_type main_list;     /**<\internal current result list */
    result_list_type aux_list;      /**<\internal additional (helper) result list */
};

//------------------------------------------------------------------------------
void tracker::report_match( Uint4 queryseq, Uint4 match_count,
                            string::size_type s_off,
                            string::size_type q_off )
{
    string query_id( table.seqid( queryseq ) );
    NcbiCerr << "Possible duplication of sequences:\n"
        << "subject: " << subject_id << " and query: " << query_id << "\n"
        << "at intervals\n"
        << "subject: " << s_off - match_count*SAMPLE_SKIP
        << " --- " << s_off - SAMPLE_SKIP << "\n"
        << "query  : " << q_off - match_count*SAMPLE_SKIP
        << " --- " << q_off - SAMPLE_SKIP << "\n" << endl;
}

//------------------------------------------------------------------------------
tracker::~tracker()
{
    typedef result_list_type::const_iterator r_iterator;

    r_iterator riter( main_list.begin() );
    r_iterator rend( main_list.end() );

    while( riter != rend )
    {
        if( riter->count >= MIN_MATCH_COUNT )
            report_match( riter->loc.seqnum, riter->count, 
                          riter->s_offset + SAMPLE_SKIP, riter->loc.offset );

        ++riter;
    }
}

//------------------------------------------------------------------------------
void tracker::operator()( const string & index, 
                          Uint4 seqnum,
                          string::size_type subject_offset,
                          dup_lookup_table::iterator iter,
                          dup_lookup_table::iterator end )
{
    typedef result_list_type::const_iterator r_iterator;
    typedef dup_lookup_table::sample_loc sample_loc;

    r_iterator riter( main_list.begin() );
    r_iterator rend( main_list.end() );

    bool do_swap( iter == end ? false : true );

    while( true )
        if( riter == rend )
            if( iter == end )
                break;
            else
            {
                aux_list.push_back( result( sample_loc( iter->seqnum, 
                                                        iter->offset + SAMPLE_SKIP ), 
                                            subject_offset ) );
                ++iter;
            }
        else if( iter == end )
        {
            if( riter->s_offset + SAMPLE_SKIP + MAX_OFFSET_ERROR < subject_offset )
            {
                if( riter->count >= MIN_MATCH_COUNT )
                    report_match( riter->loc.seqnum, riter->count,
                                  riter->s_offset + SAMPLE_SKIP, riter->loc.offset );
            }
            else aux_list.push_back( *riter );

            ++riter;
        }
        else // both iter and riter are valid
        {
            if( *iter < riter->loc )
            {
                aux_list.push_back( result( sample_loc( iter->seqnum,
                                                        iter->offset + SAMPLE_SKIP ),
                                            subject_offset ) );
                ++iter;
            }
            else if( *iter > riter->loc )
            {
                if( riter->s_offset + SAMPLE_SKIP + MAX_OFFSET_ERROR < subject_offset )
                {
                    if( riter->count >= MIN_MATCH_COUNT )
                        report_match( riter->loc.seqnum, riter->count,
                                      riter->s_offset + SAMPLE_SKIP, riter->loc.offset );
                }
                else aux_list.push_back( *riter );

                ++riter;
            }
            else // *iter == riter->loc --- same sequence and corresponding offsets
            {
                Uint4 count( 1 );

                while( riter != rend && riter->loc == *iter )
                {
                    if( subject_offset < riter->s_offset + SAMPLE_SKIP - MAX_OFFSET_ERROR )
                        aux_list.push_back( *riter );
                    else if( subject_offset > riter->s_offset + SAMPLE_SKIP 
                             + MAX_OFFSET_ERROR )
                    {
                        if( riter->count >= MIN_MATCH_COUNT )
                            report_match( riter->loc.seqnum, riter->count,
                                          riter->s_offset + SAMPLE_SKIP, riter->loc.offset );
                    }
                    else // MATCH!!! Extend it.
                        count = riter->count + 1;

                    ++riter;
                }

                aux_list.push_back( result( sample_loc( iter->seqnum,
                                                        iter->offset + SAMPLE_SKIP ),
                                            subject_offset, count ) );
                ++iter;
            }
        }

    // Swap the lists.
    if( do_swap )
    {
        main_list.clear();
        main_list.swap( aux_list );
    }
}

//------------------------------------------------------------------------------
/**\internal
 **\brief Get a FASTA formatted id string (the first available) from the 
 **       CSeq_entry structure.
 **
 **\param entry sequence description structure
 **\return the first id string corresponding to entry
 **/
static const string GetIdString( const CSeq_entry & entry )
{
    const CBioseq & seq = entry.GetSeq();
    list< CRef< CSeq_id > > idlist = seq.GetId();

    if( idlist.empty() ) 
        return "???";
    else
    {
        CNcbiOstrstream os;
        (*idlist.begin())->WriteAsFasta( os );
        return CNcbiOstrstreamToString(os);
    }
}

//------------------------------------------------------------------------------
void CheckDuplicates( const vector< string > & input )
{
    typedef vector< string >::const_iterator input_iterator;

    dup_lookup_table table;

    for( input_iterator i( input.begin() ); i != input.end(); ++i )
    {
        CNcbiIfstream istream( i->c_str() );
        CWinMaskFastaReader reader( istream );
        CRef< CSeq_entry > entry( 0 );
        Uint4 seqnum( 0 );

        while( (entry = reader.GetNextSequence()).NotEmpty() )
        {
            string data( mkdata( *entry ) );
            string::size_type data_len( data.length() );

            if( data_len < MIN_SEQ_LENGTH )
                continue;

            string id( GetIdString( *entry ) );
            data_len -= SAMPLE_SKIP;
            tracker track( table, id );

            for( string::size_type i( 0 ); i < data_len; ++i )
            {
                string index( data.substr( i, SAMPLE_LENGTH ) );
                const dup_lookup_table::sample * sample( table[index] );

                if( sample != 0 )
                    track( index, seqnum, i, sample->begin(), sample->end() );
            }

            table.add_seq_info( id, data );
            ++seqnum;
        }
    }
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
 * Revision 1.3  2005/02/12 20:24:39  dicuccio
 * Dropped use of std:: (not needed)
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

