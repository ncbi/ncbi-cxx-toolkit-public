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
 *   CSeqMasker class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <algorithm>
#include <memory>

#include <corelib/ncbi_limits.h>

#include <algo/winmask/seq_masker_window.hpp>
#include <algo/winmask/seq_masker_window_ambig.hpp>
#include <algo/winmask/seq_masker_window_pattern.hpp>
#include <algo/winmask/seq_masker_window_pattern_ambig.hpp>
#include <algo/winmask/seq_masker_score_mean.hpp>
#include <algo/winmask/seq_masker_score_min.hpp>
#include <algo/winmask/seq_masker_score_mean_glob.hpp>
#include <algo/winmask/seq_masker.hpp>
#include <algo/winmask/seq_masker_util.hpp>
#include <algo/winmask/seq_masker_istat_factory.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//-------------------------------------------------------------------------
CSeqMasker::CSeqMasker( const string & lstat_name,
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
                        Uint4 arg_pattern )
    : ustat( CSeqMaskerIstatFactory::create( lstat_name,
                                             arg_cutoff_score,
                                             arg_textend,
                                             arg_max_score,
                                             arg_set_max_score,
                                             arg_min_score,
                                             arg_set_min_score ) ),
      score( NULL ), score_p3( NULL ), trigger_score( NULL ),
      window_size( arg_window_size ), window_step( arg_window_step ),
      unit_step( arg_unit_step ),
      merge_pass( arg_merge_pass ),
      merge_cutoff_score( arg_merge_cutoff_score ),
      abs_merge_cutoff_dist( arg_abs_merge_cutoff_dist ),
      mean_merge_cutoff_dist( arg_mean_merge_cutoff_dist ),
      merge_unit_step( arg_merge_unit_step ),
      trigger( arg_trigger == "mean" ? eTrigger_Mean
               : eTrigger_Min ),
      discontig( arg_discontig ), pattern( arg_pattern )
{
    if( window_size == 0 )
        window_size = ustat->UnitSize() + 4;

    trigger_score = score = new CSeqMaskerScoreMean( ustat );

    if( trigger == eTrigger_Min )
        trigger_score = new CSeqMaskerScoreMin( ustat, tmin_count );

    if( !score )
    {
        NCBI_THROW( CSeqMaskerException,
                    eScoreAllocFail,
                    "" );
    }

    if( arg_merge_pass )
    {
        score_p3 = new CSeqMaskerScoreMeanGlob( ustat );

        if( !score )
        {
            NCBI_THROW( CSeqMaskerException,
                        eScoreP3AllocFail,
                        "" );
        }
    }
}

//-------------------------------------------------------------------------
CSeqMasker::~CSeqMasker()
{ 
    if( trigger_score != score ) delete trigger_score;

    delete score; 
    delete score_p3;
}

//-------------------------------------------------------------------------
CSeqMasker::TMaskList *
CSeqMasker::operator()( const CSeqVector& data ) const
{
    Uint4 cutoff_score = ustat->get_threshold();
    Uint4 textend = ustat->get_textend();
    auto_ptr<TMaskList> mask(new TMaskList);

    if( window_size > data.size() )
    {
        ERR_POST( Warning 
                  << "length of data is shorter than the window size" );
    }

    Uint1 nbits = discontig ? CSeqMaskerUtil::BitCount( pattern ) : 0;
    Uint4 unit_size = ustat->UnitSize() + nbits;
    auto_ptr<CSeqMaskerWindow> window_ptr
        (discontig ? new CSeqMaskerWindowPattern( data, unit_size, 
                                                  window_size, window_step, 
                                                  pattern, unit_step )
         : new CSeqMaskerWindow( data, unit_size, 
                                 window_size, window_step, 
                                 unit_step ));
    CSeqMaskerWindow & window = *window_ptr;
    score->SetWindow( window );

    if( trigger == eTrigger_Min ) trigger_score->SetWindow( window );

    Uint4 start = 0, end = 0, cend = 0;
    Uint4 limit = textend;

    while( window )
    {
        Uint4 ts = (*trigger_score)();
        Uint4 s = (*score)();

        if( s < limit )
        {
            if( end > start )
            {
                if( window.Start() > cend )
                {
                    mask->push_back( TMaskedInterval( start, end ) );
                    start = end = cend = 0;
                }
            }
        }
        else if( ts < cutoff_score )
        {
            if( end  > start )
            {
                if( window.Start() > cend + 1 )
                {
                    mask->push_back( TMaskedInterval( start, end ) );
                    start = end = cend = 0;
                }
                else cend = window.End();
            }
        }
        else
        {
            if( end > start )
            {
                if( window.Start() > cend + 1 )
                {
                    mask->push_back( TMaskedInterval( start, end ) );
                    start = window.Start();
                }
            }
            else start = window.Start();

            cend = end = window.End();
        }

        score->PreAdvance( window_step );
        ++window;
        score->PostAdvance( window_step );
    }

    if( end > start ) mask->push_back( TMaskedInterval( start, end ) );

    window_ptr.reset();

    if( merge_pass )
    {
        if( mask->size() < 2 ) return mask.release();

        TMList masked, unmasked;
        TMaskList::iterator jtmp = mask->end();

        for( TMaskList::iterator i = mask->begin(), j = --jtmp; 
             i != j; )
        {
            masked.push_back( mitem( i->first, i->second, unit_size, 
                                     data, *this ) );
            Uint4 nstart = (i++)->second - unit_size + 2;
            unmasked.push_back( mitem( nstart, i->first + unit_size - 2, 
                                       unit_size, data, *this ) );
        }

        masked.push_back( mitem( (mask->rbegin())->first,
                                 (mask->rbegin())->second, 
                                 unit_size, data, *this ) );

        Int4 count = 0;
        TMList::iterator i = masked.begin();
        TMList::iterator j = unmasked.begin();
        TMList::iterator k = i, l = i;
        --k; ++l;

        for( ; i != masked.end(); k = l = i, --k, ++l )
        {
            Uint4 ldist = (i != masked.begin())
                ? i->start - k->end - 1 : 0;
            Uint4 rdist = (i != --masked.end())
                ? l->start - i->end - 1 : 0;
            double lavg = 0.0, ravg = 0.0;
            bool can_go_left =  count && ldist
                && ldist <= mean_merge_cutoff_dist;
            bool can_go_right =  rdist
                && rdist <= mean_merge_cutoff_dist;

            if( can_go_left )
            {
                TMList::iterator tmp = j; --tmp;
                lavg = MergeAvg( k, tmp, unit_size );
                can_go_left = can_go_left && (lavg >= merge_cutoff_score);
            }

            if( can_go_right )
            {
                ravg = MergeAvg( i, j, unit_size );
                can_go_right = can_go_right && (ravg >= merge_cutoff_score);
            }

            if( can_go_right )
            {
                if( can_go_left )
                {
                    if( ravg >= lavg )
                    {
                        ++count;
                        ++i;
                        ++j;
                    }
                    else // count must be greater than 0.
                    {
                        --count;
                        k->avg = MergeAvg( k, --j, unit_size );
                        _TRACE( "Merging " 
                                << k->start << " - " << k->end
                                << " and " 
                                << i->start << " - " << i->end );
                        Merge( masked, k, unmasked, j );

                        if( count )
                        {
                            i = --k;
                            --j;
                            --count;
                        }
                        else i = k;
                    }
                }
                else
                {
                    ++count;
                    ++i;
                    ++j;
                }
            }
            else if( can_go_left )
            {
                --count;
                k->avg = MergeAvg( k, --j, unit_size );
                _TRACE( "Merging " 
                        << k->start << " - " << k->end
                        << " and " 
                        << i->start << " - " << i->end );
                Merge( masked, k, unmasked, j );

                if( count )
                {
                    i = --k;
                    --j;
                    --count;
                }
                else i = k;
            }
            else
            {
                ++i;
                ++j;
                count = 0;
            }
        }

        for( i = masked.begin(), j = unmasked.begin(), k = i++; 
             i != masked.end(); (k = i++), j++ )
        {
            if( k->end + abs_merge_cutoff_dist >= i->start )
            {
                _TRACE( "Unconditionally merging " 
                        << k->start << " - " << k->end
                        << " and " 
                        << i->start << " - " << i->end );
                k->avg = MergeAvg( k, j, unit_size );
                Merge( masked, k, unmasked, j );
                i = k; 

                if( ++i == masked.end() ) break;
            }
        }

        mask->clear();

        for( TMList::const_iterator i = masked.begin(); i != masked.end(); ++i )
            mask->push_back( TMaskedInterval( i->start, i->end ) );
    }

    return mask.release();
}

//-------------------------------------------------------------------------
double CSeqMasker::MergeAvg( TMList::iterator mi, 
                             const TMList::iterator & umi,
                             Uint4 unit_size ) const
{
    TMList::iterator tmp = mi++;
    Uint4 n1 = (tmp->end - tmp->start - unit_size + 2)/merge_unit_step;
    Uint4 n2 = (umi->end - umi->start - unit_size + 2)/merge_unit_step;
    Uint4 n3 = (mi->end - mi->start - unit_size + 2)/merge_unit_step;
    Uint4 N = (mi->end - tmp->start - unit_size + 2)/merge_unit_step;
    double a1 = tmp->avg, a2 = umi->avg, a3 = mi->avg;
    return (a1*n1 + a2*n2 + a3*n3)/N;
}

//-------------------------------------------------------------------------
void CSeqMasker::Merge( TMList & m, TMList::iterator mi, 
                        TMList & um, TMList::iterator & umi ) const
{
    TMList::iterator tmp = mi++;
    tmp->end = mi->end;
    m.erase( mi );
    umi = um.erase( umi );
}

//----------------------------------------------------------------------------
const char * CSeqMasker::CSeqMaskerException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
    case eLstatStreamIpenFail:

        return "can not open input stream";

    case eLstatSyntax:

        return "syntax error";

    case eLstatParam:

        return  "the following parameters could not be determined"
                " from the unit frequency database or command line: ";

    case eScoreAllocFail:

        return "score function object allocation failed";

    case eScoreP3AllocFail:

        return "merge pass score function object allocation failed";

    default: 

        return CException::GetErrCodeString();
    }
}

//----------------------------------------------------------------------------
CSeqMasker::mitem::mitem( Uint4 arg_start, Uint4 arg_end, Uint1 unit_size,
                          const CSeqVector & data, const CSeqMasker & owner )
    : start( arg_start ), end( arg_end ), avg( 0.0 )
{
    const Uint1 & window_size = owner.window_size;
    const CSeqMaskerWindow::TUnit & ambig_unit = owner.ustat->AmbigUnit();
    CSeqMaskerScore * const score = owner.score_p3;
    CSeqMaskerWindow * window = NULL;

    if( owner.discontig )
        window = new CSeqMaskerWindowPatternAmbig( data, unit_size, window_size, 
                                                   owner.merge_unit_step,
                                                   owner.pattern, ambig_unit, 
                                                   start, 
                                                   owner.merge_unit_step );
    else
        window = new CSeqMaskerWindowAmbig( data, unit_size, window_size, 
                                            owner.merge_unit_step, 
                                            ambig_unit, start, 
                                            owner.merge_unit_step );

    score->SetWindow( *window );
    Uint4 step = window->Step();

    while( window->End() < end )
    {
        score->PreAdvance( step );
        ++*window;
        score->PostAdvance( step );
    }

    avg = (*score)();
    delete window;
}

//----------------------------------------------------------------------------
void CSeqMasker::MergeMaskInfo( TMaskList * dest, const TMaskList * src )
{
    if( src->empty() )
        return;

    TMaskList::const_iterator si( src->begin() );
    TMaskList::const_iterator send( src->end() );
    TMaskList::iterator di( dest->begin() );
    TMaskList::iterator dend( dest->end() );
    TMaskList res;
    TMaskedInterval seg;
    TMaskedInterval next_seg;

    if( di != dend && di->first < si->first )
        seg = *(di++);
    else seg = *(si++);

    while( true )
    {
        if( si != send ) {
            if( di != dend ) {
                if( si->first < di->first ) {
                    next_seg = *(si++);
                } else {
                    next_seg = *(di++);
                }
            } else {
                next_seg = *(si++);
            }
        } else if( di != dend ) {
            next_seg = *(di++);
        } else {
            break;
        }

        if( seg.second + 1 < next_seg.first ) {
            res.push_back( seg );
            seg = next_seg;
        } 
        else if( seg.second < next_seg.second ) {
            seg.second = next_seg.second;
        }
    }

    res.push_back( seg );
    dest->swap(res);
}


END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
 * Revision 1.12  2005/04/06 15:57:10  morgulis
 * Fix in the output fasta formatter for skipping a base in the case of two
 * adjacent masked intervals.
 * Fix in the interval merging code for not merging adjacent intervals.
 *
 * Revision 1.11  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * Revision 1.10  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.9  2005/03/17 20:21:22  morgulis
 * Only store half of the units in unit counts file.
 *
 * Revision 1.8  2005/03/11 15:08:22  morgulis
 * 1. Made -window parameter optional and be default equal to unit_size + 4;
 * 2. Changed the name of -lstat parameter to -ustat.
 * 3. Added README file.
 *
 * Revision 1.7  2005/03/08 17:02:30  morgulis
 * Changed unit counts file to include precomputed threshold values.
 * Changed masking code to pick up threshold values from the units counts file.
 * Unit size is computed automatically from the genome length.
 * Added extra option for specifying genome length.
 * Removed all experimental command line options.
 * Fixed id strings in duplicate sequence checking code.
 *
 * Revision 1.6  2005/03/01 16:07:53  ucko
 * +<memory> for auto_ptr<>
 *
 * Revision 1.5  2005/02/14 12:15:17  dicuccio
 * More typedef changes: mlist -> TMList.  Use exlicit bracing to scope if/else
 * statements
 *
 * Revision 1.4  2005/02/13 18:27:21  dicuccio
 * Formatting changes (ctor initializer list properly indented; braces added in
 * complex if/else series).  Opitmization in LStat ctor - hoist string out of
 * frequently executed loop to amortize its allocation costs
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

