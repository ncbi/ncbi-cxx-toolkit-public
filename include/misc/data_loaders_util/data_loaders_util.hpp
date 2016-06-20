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
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

class CArgs;
class CArgDescriptions;

BEGIN_SCOPE(objects)

class CObjectManager;

class CDataLoadersUtil
{
public:
    enum ELoaders
    {
        fGenbank    = 1 << 0,
        fVDB        = 1 << 1,
        fSRA        = 1 << 2,
        fLDS2       = 1 << 3,
        fAsnCache   = 1 << 4,
        fBLAST      = 1 << 5,

        fDefault    = 0x0fff,

        fGenbankOffByDefault = 1<<16
    };
    typedef unsigned int TLoaders;

    /// Add a standard set of arguments used to configure the object manager
    static void AddArgumentDescriptions(CArgDescriptions& arg_desc,
                                        TLoaders loaders = fDefault);

    /// Set up just the GenBank data loader.  This is separated out from the
    /// main setup function to permit its use separately.
    static void SetupGenbankDataLoader(const CArgs& args,
                                       objects::CObjectManager& obj_mgr);

    /// Set up the standard object manager data loaders according to the
    /// arguments provided above
    static void SetupObjectManager(const CArgs& args,
                                   objects::CObjectManager& obj_mgr,
                                   TLoaders loaders = fDefault);

    // initialize object manager using SetupObjectManager, and create CScope
    // with default loaders
    static CRef<objects::CScope> GetDefaultScope(const CArgs& args);

private:
    static void x_SetupGenbankDataLoader(const CArgs& args,
                                         objects::CObjectManager& obj_mgr,
                                         int& priority);
    static void x_SetupVDBDataLoader(const CArgs& args,
                                     objects::CObjectManager& obj_mgr,
                                     int& priority);
    static void x_SetupSRADataLoader(const CArgs& args,
                                     objects::CObjectManager& obj_mgr,
                                     int& priority);
    static void x_SetupBlastDataLoader(const CArgs& args,
                                       objects::CObjectManager& obj_mgr,
                                       int& priority);
    static void x_SetupLDS2DataLoader(const CArgs& args,
                                      objects::CObjectManager& obj_mgr,
                                      int& priority);
    static void x_SetupASNCacheDataLoader(const CArgs& args,
                                          objects::CObjectManager& obj_mgr,
                                          int& priority);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // MISC_DATA_LOADERS_UTIL___DATA_LOADERS_UTIL__HPP
