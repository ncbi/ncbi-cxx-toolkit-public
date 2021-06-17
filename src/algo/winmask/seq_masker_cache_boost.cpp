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
 *   CSeqMaskerCacheBoost member and method definitions.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_cache_boost.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
inline Uint1 CSeqMaskerCacheBoost::bit_at( TUnit pos ) const
{
    pos /= od_->divisor_;
    TSeqPos word = pos/(8*sizeof( Uint4 ));
    TSeqPos bit = pos%(8*sizeof( Uint4 ));
    Uint1 res = (((od_->cba_[word])>>bit)&0x1) == 0 ? 0 : 1;
    return res;
}

//------------------------------------------------------------------------------
inline bool CSeqMaskerCacheBoost::full_check() const
{
    for( unsigned int i = 0; i < nu_; ++i )
        if( bit_at( window_[i] ) != 0 )
            return false;

    return true;
}

//------------------------------------------------------------------------------
bool CSeqMaskerCacheBoost::Check() 
{
    if( od_ == 0 || od_->cba_ == 0 )
        return true;

    while( window_ )
    {
        if( last_checked_ + 1 != window_.End() )
        {
            if( !full_check() )
                break;
        }
        else if( bit_at( window_[nu_-1] ) != 0 )
                break;

        last_checked_ = window_.End();
        ++window_;
    }

    return bool( window_ );
}

END_NCBI_SCOPE
