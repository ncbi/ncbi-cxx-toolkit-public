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
 *   Implementation of counts format converter class.
 *
 */

#include <ncbi_pch.hpp>

#include <sstream>

#include <algo/winmask/seq_masker_util.hpp>
#include <algo/winmask/seq_masker_istat_factory.hpp>
#include "algo/winmask/seq_masker_ostat_factory.hpp"
#include "algo/winmask/seq_masker_ostat.hpp"

#include "win_mask_counts_converter.hpp"

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
CWinMaskCountsConverter::CWinMaskCountsConverter(
    const string & input_fname, const string & output_fname,
    const string & counts_oformat )
    : istat( 0 ), ofname( output_fname ), oformat( counts_oformat )
{
    if( input_fname == "" ) {
        NCBI_THROW( 
                Exception, eBadOption, "input file name must be non-empty" );
    }

    if( output_fname == "" ) {
        NCBI_THROW( 
                Exception, eBadOption, "output file name must be non-empty" );
    }

    NcbiCout << "reading counts..." << flush;
    istat = CSeqMaskerIstatFactory::create( 
            input_fname, 0, 0, 0, 0, 0, 0, true );
    NcbiCout << "done" << endl;
}

//------------------------------------------------------------------------------
int CWinMaskCountsConverter::operator()()
{
    CRef< CSeqMaskerOstat > ostat(
        CSeqMaskerOstatFactory::create( oformat, ofname, true ) );
    Uint4 unit_size = istat->UnitSize();
    NcbiCout << "set unit size to " << unit_size << endl;
    ostat->setUnitSize( unit_size );
    Uint8 num_units = (unit_size < 16) ? (1ULL<<(2*unit_size))
                                       : 0x100000000ULL;
    NcbiCout << "converting counts..." << flush;

    for( Uint8 i = 0; i < num_units; ++i ) {
        Uint4 ri = CSeqMaskerUtil::reverse_complement( i, unit_size );
        
        if( i <= ri ) {
            Uint4 count = istat->trueat( i );
            if( count != 0 ) ostat->setUnitCount( i, count );
        }
    }

    NcbiCout << "done" << endl;
    NcbiCout << "converting parameters..." << flush;
    ostat->setBlank();

    ostat->setBlank();
    Uint4 t_low       = istat->get_min_count();
    Uint4 t_extend    = istat->get_textend();
    Uint4 t_threshold = istat->get_threshold();
    Uint4 t_high      = istat->get_max_count();
    ostat->setParam( "t_low      ", t_low );
    ostat->setParam( "t_extend   ", t_extend );
    ostat->setParam( "t_threshold", t_threshold );
    ostat->setParam( "t_high     ", t_high );
    ostat->setBlank();
    NcbiCout << "done" << endl;
    NcbiCout << "final processing..." << flush;
    ostat->finalize();
    NcbiCout << "done" << endl;
    return 0;
}

//------------------------------------------------------------------------------
const char * 
CWinMaskCountsConverter::Exception::GetErrCodeString() const
{
    switch( GetErrCode() ) {
        case eBadOption: return "argument error";
        default: return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE

