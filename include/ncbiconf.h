#ifndef FORWARDING_NCBICONF_H
#define FORWARDING_NCBICONF_H

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
 * Authors: Denis Vakatov, Aaron Ucko
 *
 */

/** @file ncbiconf.h
 ** Front end for a platform-specific configuration summary when using
 ** project files.
 **/

#ifdef _MSC_VER
#  include <corelib/config/ncbiconf_msvc.h>
#elif defined(__MWERKS__)
#  include <corelib/config/ncbiconf_mwerks.h>
#elif defined(NCBI_XCODE_BUILD)
#  include <corelib/config/ncbiconf_xcode.h>
#else
/*
 * Otherwise, we expect to be using the Unix build system, in which case
 * configure should have generated an appropriate ncbiconf.h in a
 * build-specific .../inc directory that should have appeared in an earlier
 * -I directive.  If you're using our recommended makefile framework per
 * http://www.ncbi.nlm.nih.gov/books/bv.fcgi?call=bv.View..ShowSection&rid=toolkit.chapter.ch_build
 * this should happen automatically; otherwise, make sure your flags are as
 * ... -I.../c++/Release/inc -I.../c++/include ...
 * rather than vice versa.  (Substitute the actual configuration you're
 * using for Release, of course.)
 *
 * If you encounter this error in-house and are listing the directories in
 * the right order, it's possible that the build name is a symlink that has
 * gone missing; if so, please notify cpp-core so we can restore it.
 */
#  error Configuration-specific <ncbiconf.h> not found; check your search path.
#endif

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2004/06/18 15:39:07  ucko
 * Support using a canned ncbiconf.h with xcode.
 *
 * Revision 1.2  2003/09/23 20:44:40  ucko
 * Fix the C++-style comment that slipped in.
 * Add a long comment explaining how to avoid triggering the #error on Unix.
 *
 * Revision 1.1  2003/09/23 15:56:50  ucko
 * Added as a smart forwarding header; should do away with the need to
 * copy a version in when using project files.
 *
 * ===========================================================================
 */

#endif  /* FORWARDING_NCBICONF_H */
