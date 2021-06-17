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

/// @file TMgr_AnnotCounts.hpp
/// User-defined methods of the data storage class.
///
/// See also: TMgr_AnnotCounts_.hpp


#ifndef OBJECTS_TRACKMGR_TMGR_ANNOTCOUNTS_HPP
#define OBJECTS_TRACKMGR_TMGR_ANNOTCOUNTS_HPP

#include <objects/trackmgr/TMgr_AnnotCounts_.hpp>
#include <objects/trackmgr/TMgr_AnnotType.hpp>
#include <objects/trackmgr/TMgr_TypeStat.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


/////////////////////////////////////////////////////////////////////////////
class NCBI_TRACKMGR_EXPORT CTMgr_AnnotCounts : public CTMgr_AnnotCounts_Base
{
    typedef CTMgr_AnnotCounts_Base Tparent;
public:
    CTMgr_AnnotCounts(void);
    ~CTMgr_AnnotCounts(void);

    void Add(ETMgr_AnnotType type, Int8 count);
    void AddPosition(ETMgr_AnnotType type, Uint8 start, Uint8 stop);
    objects::CTMgr_TypeStat::TCount GetCount(ETMgr_AnnotType type) const;

protected:
    typedef CRef<CTMgr_TypeStat> TTypeStatRef;
    typedef map<int, TTypeStatRef> TCountMap;

protected:
    TTypeStatRef x_GetCount(ETMgr_AnnotType type) const;

private:
    mutable TCountMap m_Counts;

private:
    // Prohibit copy constructor and assignment operator
    CTMgr_AnnotCounts(const CTMgr_AnnotCounts& value);
    CTMgr_AnnotCounts& operator=(const CTMgr_AnnotCounts& value);
};


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE


#endif // OBJECTS_TRACKMGR_TMGR_ANNOTCOUNTS_HPP

