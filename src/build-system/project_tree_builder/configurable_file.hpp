#ifndef PROJECT_TREE_BUILDER__CONFIGURABLE_FILE__HPP
#define PROJECT_TREE_BUILDER__CONFIGURABLE_FILE__HPP

/* $Id$
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
 * Author:  Vladimir Ivanov
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


// Create configurable file from file 'src_path', replace all substitute
// variables and write result to 'dst_path'.
bool CreateConfigurableFile(const string& src_path, const string& dst_path,
                            const string& config_name);

// Get suffix for configurable file, which depends from configuration
// (source files, ...)
string ConfigurableFileSuffix(const string& config_name);


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__CONFIGURABLE_FILE__HPP
