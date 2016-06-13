#ifndef MISC_DATA_LOADERS_UTIL___DATA_LOADERS_UTIL__HPP
#define MISC_DATA_LOADERS_UTIL___DATA_LOADERS_UTIL__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/serial.hpp>

BEGIN_NCBI_SCOPE

class CArgs;
class CArgDescriptions;

BEGIN_SCOPE(objects)

class CObjectManager;

class CDataLoadersUtil
{
public:

    /// Add a standard set of arguments used to configure the object manager
    static void AddArgumentDescriptions(CArgDescriptions& arg_desc);

    /// Set up the standard object manager data loaders according to the
    /// arguments provided above
    static void SetupObjectManager(const CArgs& args,
                                   objects::CObjectManager& obj_mgr);

    // initialize object manager using SetupObjectManager, and create CScope
    // with default loaders
    static CRef<objects::CScope> GetDefaultScope(const CArgs& args);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // MISC_DATA_LOADERS_UTIL___DATA_LOADERS_UTIL__HPP
