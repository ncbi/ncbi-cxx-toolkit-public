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
 *   Header file for CSymDustMasker class.
 *
 */

#ifndef C_SYM_DUST_MASKER_HPP
#define C_SYM_DUST_MASKER_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <deque>
#include <list>

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XALGODUSTMASK_EXPORT CSymDustMasker
{
    private:

        struct CIupac2Ncbi2na_converter
        {
            Uint1 operator()( Uint1 r ) const
            {
                switch( r )
                {
                    case 67: return 1;
                    case 71: return 2;
                    case 84: return 3;
                    default: return 0;
                }
            }
        };

        typedef objects::CSeqVector seq_t;
        typedef CIupac2Ncbi2na_converter convert_t;

    public:

        typedef seq_t sequence_type;
        typedef sequence_type::size_type size_type;
        typedef std::pair< size_type, size_type > TMaskedInterval;
        typedef std::vector< TMaskedInterval > TMaskList;

        static const Uint4 DEFAULT_LEVEL  = 20;
        static const Uint4 DEFAULT_WINDOW = 64;
        static const Uint4 DEFAULT_LINKER = 1;

        // These (up to constructor) are public to work around the bug in SUN C++ compiler.
        struct lcr
        {
            TMaskedInterval bounds_;
            Uint4 score_;
            size_type len_;

            lcr( size_type start, size_type stop, Uint4 score, size_type len )
                : bounds_( start, stop ), score_( score ), len_( len )
            {}
        };

        typedef std::list< lcr > lcr_list_type;
        typedef std::vector< Uint4 > thres_table_type;
        typedef Uint1 triplet_type;

        static const triplet_type TRIPLET_MASK = 0x3F;

        CSymDustMasker( Uint4 level = DEFAULT_LEVEL, 
                        size_type window 
                            = static_cast< size_type >( DEFAULT_WINDOW ),
                        size_type linker 
                            = static_cast< size_type >( DEFAULT_LINKER ) );

        std::auto_ptr< TMaskList > operator()( const sequence_type & seq );

        void GetMaskedLocs( 
            objects::CSeq_id & seq_id,
            const sequence_type & seq,
            std::vector< CConstRef< objects::CSeq_loc > > & locs );

    private:

        typedef sequence_type::const_iterator seq_citer_type;

        class triplets
        {
            public:
                
                triplets( const sequence_type & seq, 
                          size_type window, 
                          lcr_list_type & lcr_list,
                          thres_table_type & thresholds );

                size_type start() const { return start_; }
                size_type stop() const { return stop_; }
                size_type size() const { return triplet_list_.size(); }

                bool add( sequence_type::value_type n );
                          
            private:
                
                typedef std::deque< triplet_type > impl_type;
                typedef impl_type::const_iterator impl_citer_type;

                impl_type triplet_list_;

                size_type start_;
                size_type stop_;
                size_type max_size_;

                lcr_list_type & lcr_list_;
                thres_table_type & thresholds_;
        };

        Uint4 level_;
        size_type window_;
        size_type linker_;

        lcr_list_type lcr_list_;
        thres_table_type thresholds_;

        convert_t converter_;
};

END_NCBI_SCOPE

#endif

