#ifndef STREAM_PUSHBACK__HPP
#define STREAM_PUSHBACK__HPP

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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Push an arbitrary block of data back to a C++ input stream.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/12/09 06:28:59  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


// Push the block of data [buf, buf+buf_size) back to the input stream "is".
// If "del_ptr" is not NULL, then `delete [] (char*)del_ptr' will be called
// some time between the moment you call this function and when either the
// passed data is all read from the stream, or if the stream is destroyed.
// Until all of the passed data is read from the stream, this block of
// data may be used by the input stream.
// NOTE 1:  at the function discretion, a copy of the data can be created and
//          used instead of the original [buf, buf+buf_size) area.
// NOTE 2:  this data does not go to the original streambuf of "is".
// NOTE 3:  it's okay if "is" is actually a duplex stream (iostream).
extern void UTIL_StreamPushback(istream&      is,
                                CT_CHAR_TYPE* buf,
                                streamsize    buf_size,
                                void*         del_ptr);


// Acts just like its counterpart with 4 args (above), but this variant always
// copies the "pushback data" into internal buffer, so you can do whatever you
// want to with the [buf, buf+buf_size) area after calling this function.
extern void UTIL_StreamPushback(istream&      is,
                                CT_CHAR_TYPE* buf,
                                streamsize    buf_size);


END_NCBI_SCOPE


#endif  /* STREAM_PUSHBACK__HPP */
