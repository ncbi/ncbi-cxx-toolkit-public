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
 *   Implementation of CSeqMaskerUStatFactory class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_ostat_factory.hpp>
#include <algo/winmask/seq_masker_ostat_ascii.hpp>
#include <algo/winmask/seq_masker_ostat_bin.hpp>
#include <algo/winmask/seq_masker_ostat_opt_ascii.hpp>
#include <algo/winmask/seq_masker_ostat_opt_bin.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * 
CSeqMaskerOstatFactory::CSeqMaskerOstatFactoryException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadName:      return "bad name";
        case eCreateFail:   return "creation failure";
        default:            return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
CSeqMaskerOstat * CSeqMaskerOstatFactory::create( 
    const string & ustat_type, CNcbiOstream & os, bool use_ba )
{
    try
    {
        if( ustat_type.substr( 0, 5 ) == "ascii" )
            return new CSeqMaskerOstatAscii( os );
        else if( ustat_type.substr( 0, 6 ) == "binary" )
            return new CSeqMaskerOstatBin( os );
        else if( ustat_type.substr( 0, 6 ) == "oascii" )
        {
            Uint4 size = atoi( ustat_type.substr( 6 ).c_str() );
            return new CSeqMaskerOstatOptAscii( os, size );
        }
        else if( ustat_type.substr( 0, 7 ) == "obinary" )
        {
            Uint4 size = atoi( ustat_type.substr( 7 ).c_str() );
            return new CSeqMaskerOstatOptBin( os, size, use_ba );
        }
        else NCBI_THROW( CSeqMaskerOstatFactoryException,
                         eBadName,
                         "unkown unit counts format" );
    }
    catch( CException & e ) {
        NCBI_RETHROW( e, CSeqMaskerOstatFactoryException, eCreateFail,
                      "could not create a unit counts container" );
    }
    catch( std::exception & e )
    {
        NCBI_THROW( CSeqMaskerOstatFactoryException,
                    eCreateFail,
                    std::string( "could not create a unit counts container" ) +
                        e.what() );
    }
}
    
//------------------------------------------------------------------------------
CSeqMaskerOstat * CSeqMaskerOstatFactory::create( 
    const string & ustat_type, const string & name, bool use_ba )
{
    try
    {
        if( ustat_type.substr( 0, 5 ) == "ascii" )
            return new CSeqMaskerOstatAscii( name );
        else if( ustat_type.substr( 0, 6 ) == "binary" )
            return new CSeqMaskerOstatBin( name );
        else if( ustat_type.substr( 0, 6 ) == "oascii" )
        {
            Uint4 size = atoi( ustat_type.substr( 6 ).c_str() );
            return new CSeqMaskerOstatOptAscii( name, size );
        }
        else if( ustat_type.substr( 0, 7 ) == "obinary" )
        {
            Uint4 size = atoi( ustat_type.substr( 7 ).c_str() );
            return new CSeqMaskerOstatOptBin( name, size, use_ba );
        }
        else NCBI_THROW( CSeqMaskerOstatFactoryException,
                         eBadName,
                         "unkown unit counts format" );
    }
    catch( CException & e ) {
        NCBI_RETHROW( e, CSeqMaskerOstatFactoryException, eCreateFail,
                      "could not create a unit counts container" );
    }
    catch( std::exception & e )
    {
        NCBI_THROW( CSeqMaskerOstatFactoryException,
                    eCreateFail,
                    std::string( "could not create a unit counts container" ) +
                        e.what() );
    }
}
    
END_NCBI_SCOPE
