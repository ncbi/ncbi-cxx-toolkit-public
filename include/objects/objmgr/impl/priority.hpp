#ifndef PRIORITY__HPP
#define PRIORITY__HPP

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
* Authors:
*           Aleksey Grichenko
*
* File Description:
*           Priority record for CObjectManager and CScope
*
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDataSource;


struct SDataSourceRec {
    typedef unsigned int TPriority;

    SDataSourceRec(CDataSource* src, TPriority priority = 9)
        : m_DataSource(src), m_Priority(priority) {}

    SDataSourceRec(const SDataSourceRec& rec)
        : m_DataSource(rec.m_DataSource), m_Priority(rec.m_Priority) {}

    SDataSourceRec& operator =(const SDataSourceRec& rec)
        {
            if (&rec != this) {
                m_DataSource = rec.m_DataSource;
                m_Priority = rec.m_Priority;
            }
            return *this;
        }

    bool operator ==(const SDataSourceRec& rec) const
        {
            return m_DataSource == rec.m_DataSource
                &&  m_Priority == rec.m_Priority;
        }
    bool operator <(const SDataSourceRec& rec) const
        {
            if (m_Priority != rec.m_Priority) return m_Priority < rec.m_Priority;
            return m_DataSource < rec.m_DataSource;
        }

    CDataSource* m_DataSource;
    TPriority m_Priority;
};

typedef set<SDataSourceRec> TDataSourceSet;



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/03/11 14:14:49  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif // PRIORITY__HPP
