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
 * Authors:  Josh Cherry
 *
 * File Description:  Classes for representing and finding restriction sites
 *
 */


#include <corelib/ncbistd.hpp>
#include "restriction.hpp"

BEGIN_NCBI_SCOPE

void CREnzyme::Reset(void)
{
    m_Name.erase();
    m_Specs.clear();
}


void CRSpec::Reset(void)
{
    m_Seq.erase();
    m_PlusCuts.clear();
    m_MinusCuts.clear();
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/08/17 19:25:30  jcherry
 * Changed member variable names to follow convention
 *
 * Revision 1.1  2003/08/12 18:52:58  jcherry
 * Initial version
 *
 * ===========================================================================
 */

