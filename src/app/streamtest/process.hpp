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
* Author:
*
* File Description:
*
* ===========================================================================
*/

#ifndef __process__hpp__
#define __process__hpp__

#include <iostream>
using namespace std;

//  ============================================================================
class CSeqEntryProcess
//  ============================================================================
{
public:
    //  ------------------------------------------------------------------------
    CSeqEntryProcess()
    //  ------------------------------------------------------------------------
        : m_objectcount( 0 )
    {};

    //  ------------------------------------------------------------------------
    virtual ~CSeqEntryProcess()
    //  ------------------------------------------------------------------------
    {};

    //  ------------------------------------------------------------------------
    virtual void ProcessInitialize(
        const CArgs& )
    //  ------------------------------------------------------------------------
    {};

    //  ------------------------------------------------------------------------
    virtual void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {};

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        const CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {};

    //  ------------------------------------------------------------------------
    virtual void SeqEntryProcess(
        const CRef<CSeq_entry>& )
    //  ------------------------------------------------------------------------
    {++m_objectcount;};

    //  ------------------------------------------------------------------------
    virtual void SeqEntryFinalize(
        const CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {};

    //  ------------------------------------------------------------------------
    unsigned int GetObjectCount() const
    //  ------------------------------------------------------------------------
    {
        return m_objectcount;
    };

protected:
    unsigned int m_objectcount;
};

#endif
