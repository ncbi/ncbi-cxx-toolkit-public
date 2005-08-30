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
 *   CSeqMaskerCacheBoost class definition.
 *
 */

#ifndef C_SEQ_MASKER_CACHE_BOOST_H
#define C_SEQ_MASKER_CACHE_BOOST_H

#include <corelib/ncbiobj.hpp>

#include <algo/winmask/seq_masker_istat.hpp>
#include <algo/winmask/seq_masker_window.hpp>

BEGIN_NCBI_SCOPE

class CSeqMaskerCacheBoost
{
    public:

        CSeqMaskerCacheBoost( CSeqMaskerWindow & window,
                              const CSeqMaskerIstat::optimization_data * od )
            : window_( window ), od_( od ), last_checked_( 0 )
        { nu_ = window_.NumUnits(); }

        bool Check();

    private:

        typedef CSeqMaskerWindow::TUnit TUnit;

        Uint1 bit_at( TSeqPos pos ) const;
        bool full_check() const;
        
        CSeqMaskerWindow & window_;
        const CSeqMaskerIstat::optimization_data * od_;

        TSeqPos last_checked_;
        Uint8 nu_;
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/08/30 14:35:19  morgulis
 * NMer counts optimization using bit arrays. Performance is improved
 * by about 20%.
 *
 * ========================================================================
 */
