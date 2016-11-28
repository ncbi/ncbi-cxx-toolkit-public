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
* Author:  Justin Foley
*
* File Description:
*   implicit protein matching driver code
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "run_binary.hpp"
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <connect/ncbi_pipe.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CBinRunner::CBinRunner(const string& bin_name) {
    m_pBinary.reset(new CFile(bin_name));
    m_IsSet = true;
    CheckBinary();
}


CBinRunner::~CBinRunner(void) {}


bool CBinRunner::CheckBinary(void) const
{
    if (!(m_IsSet)) {
        return false;
    }
    
    if (!m_pBinary->Exists()) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Cannot find " + m_pBinary->GetName());
    }

    if (!m_pBinary->CheckAccess(CFile::fExecute)) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            m_pBinary->GetName() + " is not executable");
    }
    return true;
}


void CBinRunner::Exec(const vector<string>& arg_vec) 
{
    CNcbiIstrstream empty_in("");

    int exit_code;
    CPipe::EFinish exec_fin = CPipe::ExecWait(
            m_pBinary->GetPath(),
            arg_vec,
            empty_in,
            cout,
            cerr,
            exit_code);


    if ( exec_fin != CPipe::eDone ) {
        NCBI_THROW(CProteinMatchException,
            eExecutionError,
            m_pBinary->GetName() + ": execution canceled");
    }

    if ( exit_code ) {
        string msg = m_pBinary->GetName() 
            + "Exit Code - " 
            + NStr::NumericToString(exit_code); 
        NCBI_THROW(CProteinMatchException,
            eExecutionError,
            msg);
    }

    return;
}


END_NCBI_SCOPE

