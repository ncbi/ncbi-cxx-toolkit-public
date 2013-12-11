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

#ifndef __process_scoped__hpp__
#define __process_scoped__hpp__

//  ============================================================================
class CScopedProcess
//  ============================================================================
    : public CSeqEntryProcess
{
public:
    //  ------------------------------------------------------------------------
    CScopedProcess()
    //  ------------------------------------------------------------------------
        : CSeqEntryProcess()
    {};

    //  ------------------------------------------------------------------------
    ~CScopedProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryProcess::ProcessInitialize( args );

        m_objmgr = CObjectManager::GetInstance();
        if ( m_objmgr ) {
            if ( args["gbload"] ) {
                CGBDataLoader::RegisterInObjectManager(*m_objmgr);
            }
            if ( args["wgsload"] ) {
                CWGSDataLoader::RegisterInObjectManager(*m_objmgr, CObjectManager::eDefault);
            }
        }
    };

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryProcess::SeqEntryInitialize( se );

        m_objmgr = CObjectManager::GetInstance();
        if ( !m_objmgr ) {
            /* raise hell */;
        }

        m_scope.Reset( new CScope( *m_objmgr ) );
        if ( !m_scope ) {
            /* raise hell */;
        }

        m_scope->AddDefaults();
        m_topseh = m_scope->AddTopLevelSeqEntry( *m_entry );
        if ( !m_topseh ) {
            /* raise hell */;
        }
    };

    //  ------------------------------------------------------------------------
    virtual void SeqEntryFinalize()
    //  ------------------------------------------------------------------------
    {
        m_scope->ResetHistory();
        m_scope.Reset();
        CSeqEntryProcess::SeqEntryFinalize();
    };

protected:
    CRef<CObjectManager> m_objmgr;
    CRef<CScope> m_scope;
    CSeq_entry_Handle m_topseh;
};

#endif
