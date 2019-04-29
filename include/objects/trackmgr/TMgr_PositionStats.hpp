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
 */

/// @file TMgr_PositionStats.hpp
/// User-defined methods of the data storage class.
///


#ifndef OBJECTS_TRACKMGR_TMGR_POSITIONSTATS_HPP
#define OBJECTS_TRACKMGR_TMGR_POSITIONSTATS_HPP

#include <objects/trackmgr/TMgr_PositionStats_.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


/////////////////////////////////////////////////////////////////////////////
class NCBI_TRACKMGR_EXPORT CTMgr_PositionStats : public CTMgr_PositionStats_Base
{
    typedef CTMgr_PositionStats_Base Tparent;
public:
    CTMgr_PositionStats(void);
    ~CTMgr_PositionStats(void);

    void Add(TSeqPos start, TSeqPos stop, Uint8 prior_count = 0);

private:
    // Prohibit copy constructor and assignment operator
    CTMgr_PositionStats(const CTMgr_PositionStats& value);
    CTMgr_PositionStats& operator=(const CTMgr_PositionStats& value);
};


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

#endif // OBJECTS_TRACKMGR_TMGR_POSITIONSTATS_HPP

