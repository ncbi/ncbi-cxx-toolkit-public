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
* File Name:  utils.cpp
*
* Author:  Michael Domrachev
*
* File Description:  Taxon1 object module utility functions
*
*/

#include <ncbi_pch.hpp>
#include <objects/taxon1/taxon1.hpp>

#include "ctreecont.hpp"
#include "cache.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


//////////////////////////////////
//  CTaxon1Node implementation
//
#define TXC_INH_DIV 0x4000000
#define TXC_INH_GC  0x8000000
#define TXC_INH_MGC 0x10000000
/* the following three flags are the same (it is not a bug) */
#define TXC_SUFFIX  0x20000000
#define TXC_UPDATED 0x20000000
#define TXC_UNCULTURED 0x20000000

#define TXC_GBHIDE  0x40000000
#define TXC_STHIDE  0x80000000

short
CTaxon1Node::GetRank() const
{
    return ((m_ref->GetCde())&0xff)-1;
}

short
CTaxon1Node::GetDivision() const
{
    return (m_ref->GetCde()>>8)&0x3f;
}

short
CTaxon1Node::GetGC() const
{
    return (m_ref->GetCde()>>(8+6))&0x3f;
}

short
CTaxon1Node::GetMGC() const
{
    return (m_ref->GetCde()>>(8+6+6))&0x3f;
}

bool
CTaxon1Node::IsUncultured() const
{
    return m_ref->GetCde() & TXC_UNCULTURED ? true : false;
}

bool
CTaxon1Node::IsGenBankHidden() const
{
    return m_ref->GetCde() & TXC_GBHIDE ? true : false;
}


END_objects_SCOPE
END_NCBI_SCOPE
