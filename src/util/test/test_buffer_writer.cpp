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
 * Author:  Sergey Satskiy
 *
 * File Description:
 *   Test of buffer writer
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <ncbiconf.h>

#include <util/buffer_writer.hpp>
#include <util/simple_buffer.hpp>


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE( BufferWriter01 )
{

    // Should not produce compilation errors
    {
        typedef std::vector< signed char >  container;

        container                           low_level_buffer;
        CBufferWriter< container >          buffer(low_level_buffer,
                                                   eCreateMode_Add);
    }

    {
        typedef std::vector< unsigned char >    container;

        container                               low_level_buffer;
        CBufferWriter< container >              buffer(low_level_buffer,
                                                       eCreateMode_Add);
    }

    {
        typedef std::string             container;

        container                       low_level_buffer;
        CBufferWriter< container >      buffer(low_level_buffer,
                                               eCreateMode_Add);
    }

    {
        typedef CSimpleBuffer           container;

        container                       low_level_buffer;
        CBufferWriter< container >      buffer(low_level_buffer,
                                               eCreateMode_Add);
    }

    {
        typedef CSimpleBufferT< unsigned char >     container;

        container                                   low_level_buffer;
        CBufferWriter< container >                  buffer(low_level_buffer,
                                                           eCreateMode_Add);
    }

    {
        typedef CSimpleBufferT< signed char >       container;

        container                                   low_level_buffer;
        CBufferWriter< container >                  buffer(low_level_buffer,
                                                           eCreateMode_Add);
    }

    // Uncomment to get a compilation error
    #if 0
    {
        CSimpleBufferT< int >                   low_level_buffer;
        CBufferWriter< CSimpleBufferT< int > >  buffer(low_level_buffer);
    }
    #endif

    return;
}


template<typename T>
void CompareBuffers( const T &  container, const char *  buf, size_t  len )
{
    BOOST_CHECK( container.size() == len );
    for ( size_t  k(0); k < len; ++k )
    {
        BOOST_CHECK( container[k] == buf[k] );
    }
}


BOOST_AUTO_TEST_CASE( BufferWriter02 )
{
    std::string                     ll_buffer;
    CBufferWriter< std::string >    buffer(ll_buffer, eCreateMode_Add);

    buffer.Write( "abc", 3, NULL );
    CompareBuffers( ll_buffer, "abc", 3 );

    buffer.Write( "def", 3, NULL );
    CompareBuffers( ll_buffer, "abcdef", 6 );

    return;
}


BOOST_AUTO_TEST_CASE( BufferWriter03 )
{
    std::vector< signed char >                      ll_buffer;
    CBufferWriter< std::vector< signed char > >     buffer(ll_buffer,
                                                           eCreateMode_Add);

    buffer.Write( "abc", 3, NULL );
    CompareBuffers( ll_buffer, "abc", 3 );

    buffer.Write( "def", 3, NULL );
    CompareBuffers( ll_buffer, "abcdef", 6 );

    return;
}


BOOST_AUTO_TEST_CASE( BufferWriter04 )
{
    std::vector< unsigned char >                    ll_buffer;
    CBufferWriter< std::vector< unsigned char > >   buffer(ll_buffer,
                                                           eCreateMode_Add);

    buffer.Write( "abc", 3, NULL );
    CompareBuffers( ll_buffer, "abc", 3 );

    buffer.Write( "def", 3, NULL );
    CompareBuffers( ll_buffer, "abcdef", 6 );

    return;
}


BOOST_AUTO_TEST_CASE( BufferWriter05 )
{
    CSimpleBufferT< unsigned char >                     ll_buffer;
    CBufferWriter< CSimpleBufferT< unsigned char > >    buffer(ll_buffer,
                                                               eCreateMode_Add);

    buffer.Write( "abc", 3, NULL );
    CompareBuffers( ll_buffer, "abc", 3 );

    buffer.Write( "def", 3, NULL );
    CompareBuffers( ll_buffer, "abcdef", 6 );

    return;
}


BOOST_AUTO_TEST_CASE( BufferWriter06 )
{
    CSimpleBufferT< signed char >                       ll_buffer;
    CBufferWriter< CSimpleBufferT< signed char > >      buffer(ll_buffer,
                                                               eCreateMode_Add);

    buffer.Write( "abc", 3, NULL );
    CompareBuffers( ll_buffer, "abc", 3 );

    buffer.Write( "def", 3, NULL );
    CompareBuffers( ll_buffer, "abcdef", 6 );

    return;
}


BOOST_AUTO_TEST_CASE( BufferWriter07 )
{
    CSimpleBuffer                   ll_buffer;
    CBufferWriter< CSimpleBuffer >  buffer(ll_buffer, eCreateMode_Add);

    buffer.Write( "abc", 3, NULL );
    CompareBuffers( ll_buffer, "abc", 3 );

    buffer.Write( "def", 3, NULL );
    CompareBuffers( ll_buffer, "abcdef", 6 );

    return;
}

