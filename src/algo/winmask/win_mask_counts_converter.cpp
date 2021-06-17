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
#include <algo/winmask/seq_masker_ostat_factory.hpp>
#include <algo/winmask/seq_masker_ostat.hpp>

#include <algo/winmask/win_mask_counts_converter.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
CWinMaskCountsConverter::CWinMaskCountsConverter(
    const string & input_fname, const string & output_fname,
    const string & counts_oformat, string const & metadata )
    : istat( 0 ), ofname( output_fname ), oformat( counts_oformat ), os( 0 ),
      metadata( metadata )
{
    if( input_fname == "-" ) {
        NCBI_THROW( 
                Exception, eBadOption, "input file name must be non-empty" );
    }

    if( output_fname == "-" ) {
        NCBI_THROW( 
                Exception, eBadOption, "output file name must be non-empty" );
    }

    LOG_POST( "reading counts..." );
    istat = CSeqMaskerIstatFactory::create( 
            input_fname, 0, 0, 0, 0, 0, 0, true );
}

//------------------------------------------------------------------------------
CWinMaskCountsConverter::CWinMaskCountsConverter(
    const string & input_fname, CNcbiOstream & out_stream,
    const string & counts_oformat, string const & metadata )
    : istat( 0 ), ofname( "" ), oformat( counts_oformat ), os( &out_stream ),
      metadata( metadata )
{
    if( input_fname == "-" ) {
        NCBI_THROW( 
                Exception, eBadOption, "input file name must be non-empty" );
    }

    LOG_POST( "reading counts..." );
    istat = CSeqMaskerIstatFactory::create( 
            input_fname, 0, 0, 0, 0, 0, 0, true );
}

//------------------------------------------------------------------------------
int CWinMaskCountsConverter::operator()()
{
    CRef< CSeqMaskerOstat > ostat( 0 );
    string md( metadata );

    if( md.empty() ) md = istat->GetMetaData();

    if( os == 0 ) {
        ostat = CSeqMaskerOstatFactory::create( 
                oformat, ofname, true, md );
    }
    else ostat = CSeqMaskerOstatFactory::create( oformat, *os, true, md );

    Uint4 unit_size = istat->UnitSize();
    _TRACE( "set unit size to " << unit_size );
    ostat->setUnitSize( unit_size );
    Uint8 num_units = (unit_size < 16) ? (1ULL<<(2*unit_size))
                                       : 0x100000000ULL;
    LOG_POST( "converting counts..." );

    for( Uint8 i = 0; i < num_units; ++i ) {
        Uint4 ri = CSeqMaskerUtil::reverse_complement( i, unit_size );
        
        if( i <= ri ) {
            Uint4 count = istat->trueat( i );
            if( count != 0 ) ostat->setUnitCount( i, count );
        }
    }

    LOG_POST( "converting parameters..." );

    Uint4 t_low       = istat->get_min_count();
    Uint4 t_extend    = istat->get_textend();
    Uint4 t_threshold = istat->get_threshold();
    Uint4 t_high      = istat->get_max_count();
    ostat->setParam( "t_low      ", t_low );
    ostat->setParam( "t_extend   ", t_extend );
    ostat->setParam( "t_threshold", t_threshold );
    ostat->setParam( "t_high     ", t_high );
    LOG_POST( "final processing..." );
    ostat->SetStatAlgoVersion( istat->GetStatAlgoVersion() );
    ostat->finalize();
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

