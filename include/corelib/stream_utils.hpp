#ifndef CORELIB___STREAM_UTILS__HPP
#define CORELIB___STREAM_UTILS__HPP

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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Stream utilities:
 *   1. Push an arbitrary block of data back to C++ input stream.
 *   2. Non-blocking read from a stream.
 *
 */

#include <corelib/ncbistre.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup StreamSupport
 *
 * @{
 */


struct NCBI_XNCBI_EXPORT CStreamUtils {

// Push the block of data [buf, buf+buf_size) back to the input stream "is".
// If "del_ptr" is not NULL, then `delete[] (CT_CHAR_TYPE*) del_ptr' is called
// some time between the moment you call this function and when either the
// passed data is all read from the stream, or if the stream is destroyed.
// Until all of the passed data is read from the stream, this block of
// data may be used by the input stream.
// NOTE 1:  at the function's discretion, a copy of the data can be created
//          and used instead of the original [buf, buf+buf_size) area.
// NOTE 2:  this data does not go to the original streambuf of "is".
// NOTE 3:  it's okay if "is" is actually a duplex stream (iostream).
// NOTE 4:  after a pushback "is" is not allowed to do stream positioning
//          relative to current position (ios::cur); only direct-access
//          (ios::beg, ios::end) seeks are okay (if permitted by "is").
// NOTE 5:  stream re-positioning made after pushback clears all pushback data.
    static void       Pushback(CNcbiIstream&       is,
                               CT_CHAR_TYPE*       buf,
                               streamsize          buf_size,
                               void*               del_ptr);

// Acts just like its counterpart with 4 args (above), but this variant always
// copies the "pushback data" into internal buffer, so you can do whatever you
// want to with the [buf, buf+buf_size) area after calling this function.
    static void       Pushback(CNcbiIstream&       is,
                               const CT_CHAR_TYPE* buf,
                               streamsize          buf_size);


// Read at most "buf_size" bytes from the stream "is" into a buffer pointed
// to by "buf". This call tries its best to be non-blocking.
// Return the number of bytes actually read (or 0 if nothing was read
// in case of either an error or EOF).
    static streamsize Readsome(CNcbiIstream&       is,
                               CT_CHAR_TYPE*       buf,
                               streamsize          buf_size);

};


/* @} */


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.21  2006/05/02 16:09:53  lavr
 * Moved from util to here
 *
 * Revision 1.20  2006/02/15 17:39:22  lavr
 * Readsome-tolerant MIPSPro-specific streambuf moved to <corelib/ncbistre.hpp>
 *
 * Revision 1.19  2006/01/27 16:58:42  lavr
 * Note that Readsome() to block for at least one char be read
 *
 * Revision 1.18  2004/01/20 20:33:52  lavr
 * Remove HAVE_BUGGY_IOS_CALLBACKS from this header
 *
 * Revision 1.17  2003/12/29 15:15:10  lavr
 * Class CShowmanycStreambuf removed
 *
 * Revision 1.16  2003/12/18 13:23:40  ucko
 * It seems MIPSpro also lacks showmanyc, so extend our
 * streambuf-with-showmanyc class to cover it as well and make it
 * CMIPSPRO_ReadsomeTolerantStreambuf's base class.
 *
 * Revision 1.15  2003/12/18 03:43:28  ucko
 * Add CGCC_ShowmanycStreambuf as a layer between streambuf and
 * CConn_Streambuf for the benefit of Readsome.
 *
 * Revision 1.14  2003/10/23 16:16:46  vasilche
 * Added Windows export modifiers.
 *
 * Revision 1.13  2003/10/22 18:14:16  lavr
 * Change base class of CPushback_Streambuf into CNcbiStreambuf
 *
 * Revision 1.12  2003/09/22 20:26:56  lavr
 * Undef HAVE_BUGGY_IOS_CALLBACKS before defining
 *
 * Revision 1.11  2003/04/17 17:50:34  siyan
 * Added doxygen support
 *
 * Revision 1.10  2003/04/14 21:09:07  lavr
 * Define HAVE_BUGGY_IOS_CALLBACKS for Sun Workshop compiler in MT mode
 *
 * Revision 1.9  2003/03/30 06:59:50  lavr
 * MIPS-specific workaround for lame-designed stream read ops
 *
 * Revision 1.8  2002/12/19 14:51:00  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 1.7  2002/11/28 03:27:19  lavr
 * File description updated
 *
 * Revision 1.6  2002/11/27 21:07:19  lavr
 * Rename "stream_pushback" -> "stream_utils" and enclose utils in a class
 *
 * Revision 1.5  2002/11/06 03:50:39  lavr
 * Correct duplicated NOTE 4; move log to end; change guard macro
 *
 * Revision 1.4  2002/10/29 22:06:22  ucko
 * Make *buf const in the copying version of UTIL_StreamPushback.
 *
 * Revision 1.3  2002/02/05 19:37:04  lavr
 * List of included header files revised
 *
 * Revision 1.2  2002/02/04 20:20:28  lavr
 * Notes about stream repositioning added
 *
 * Revision 1.1  2001/12/09 06:28:59  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB___STREAM_UTILS__HPP */
