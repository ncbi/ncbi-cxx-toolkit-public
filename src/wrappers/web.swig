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
 * Authors:  Josh Cherry
 *
 * File Description:  SWIG interface file for web
 *
 */


#ifdef SWIGPERL
// not clear why, but SWIG generates uncompilable wrappers
%ignore ncbi::CHTML_area;
%ignore ncbi::CHTML_map;
#endif

%%

html/commentdiag.hpp
html/components.hpp
html/error_codes.hpp
html/factory.hpp
html/html.hpp
html/html_exception.hpp
html/htmlhelper.hpp
html/indentstream.hpp
html/jsmenu.hpp
html/node.hpp
html/nodemap.hpp
html/page.hpp
html/pager.hpp
html/selection.hpp
html/writer_htmlenc.hpp

cgi/caf.hpp
//cgi/cgiapp_cached.hpp  // some ugly problem with parameters macro
cgi/cgi_exception.hpp
cgi/cgi_util.hpp
cgi/cgi_session.hpp
cgi/cgi_serial.hpp
cgi/cgiapp.hpp
cgi/cgictx.hpp
cgi/error_codes.hpp
cgi/ncbicgi.hpp
cgi/ncbicgir.hpp
cgi/ncbires.hpp
cgi/ref_args.hpp
// These rely on posix-specific headers
%ifndef WINDOWS cgi/caf_plain.hpp
%ifndef WINDOWS cgi/caf_encoded.hpp

misc/cgi_redirect/redirect.hpp

%%
