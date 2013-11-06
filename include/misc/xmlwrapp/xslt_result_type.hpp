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
 * Author:  Sergey Satskiy, NCBI
 * Credits: Denis Vakatov, NCBI (API design)
 *
 */


/*
 * Type to specify how to treat XSLT results
 */

#ifndef _xmlwrapp_xslt_result_type_hpp_
#define _xmlwrapp_xslt_result_type_hpp_


namespace xslt {

    // Internal library usage only.
    // Used as a flag which affects how the XSLT results are converted
    // to a string
    enum result_treat_type {
        type_treat_as_doc,      // treat the XSLT result as a document;
                                // subsequent output will be affected by
                                // local and global formatting flags.
        type_no_treat           // no treatment;
                                // libxslt is responsible for output
                                // formatting, and formatting flags
                                // will not affect subsequent output.
    };

} // xslt namespace

#endif

