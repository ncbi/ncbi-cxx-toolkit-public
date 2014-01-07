/*
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

#ifndef __process_null__hpp__
#define __process_null__hpp__

#include <objtools/edit/struc_comm_field.hpp>
#include <objtools/edit/gb_block_field.hpp>
#include <objects/valid/Comment_rule.hpp>

//  ============================================================================
class CNullProcess
//  ============================================================================
    : public CSeqEntryProcess
{
public:
    //  ------------------------------------------------------------------------
    CNullProcess()
    //  ------------------------------------------------------------------------
        : CSeqEntryProcess()
    {};

    //  ------------------------------------------------------------------------
    ~CNullProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryProcess::ProcessInitialize( args );
    };

    static bool x_UserFieldCompare (const CRef<CUser_field>& f1, const CRef<CUser_field>& f2)
    {
        if (!f1->IsSetLabel()) return true;
        if (!f2->IsSetLabel()) return false;
        return f1->GetLabel().Compare(f2->GetLabel()) < 0;
    }

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        // empty method body
        try {

        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

};

#endif
