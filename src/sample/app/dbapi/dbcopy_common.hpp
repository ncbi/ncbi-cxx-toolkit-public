#ifndef SAMPLE__DBCOPY_COMMON__HPP
#define SAMPLE__DBCOPY_COMMON__HPP

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
 * Authors: Sergey Sikorskiy
 *
 */

 /// @dbcopy_common.hpp
 /// CNcbiSample_DbCopy class definition, to be used by
 /// 'dbcopy' test and the NEW_PROJECT-class utilities.


#include <common/ncbi_sample_api.hpp>
#include <dbapi/dbapi.hpp>


/** @addtogroup Sample
 *
 * @{
 */

USING_NCBI_SCOPE;


/// CNcbiSample_DbCopy - class describing 'dbcopy' test business logic.
/// 
class CNcbiSample_DbCopy : public CNcbiSample
{
public:
    CNcbiSample_DbCopy() {}

    /// Sample description
    virtual string Description(void) { return "DBCOPY"; }

    /// Initialization
    virtual void Init(void) {}

    /// Sample's business logic
    virtual int Run(void)
    {
        // Demo copying tables
        int exit_code = DemoDbCopyTable();
        return exit_code;
    }

    /// Cleanup
    void Exit(void) {}

public:
    int DemoDbCopyTable(void);
};


/* @} */

#endif // SAMPLE__DBCOPY_COMMON__HPP
