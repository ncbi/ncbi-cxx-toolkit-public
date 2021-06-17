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

using namespace std;

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

TTaxRank
CTaxon1Node::GetRank() const
{
    return ((m_ref->GetCde())&0xff)-1;
}

TTaxDivision
CTaxon1Node::GetDivision() const
{
    return (m_ref->GetCde()>>8)&0x3f;
}

TTaxGeneticCode
CTaxon1Node::GetGC() const
{
    return (m_ref->GetCde()>>(8+6))&0x3f;
}

TTaxGeneticCode
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

class PPredDbTagByName {
    const string& m_name;
public:
    PPredDbTagByName(const string& sName) : m_name(sName) {}
    bool operator()( const COrg_ref::TDb::value_type& vt ) const {
	return m_name.size()+10 == vt->GetDb().size() &&
	    NStr::StartsWith( vt->GetDb(), "taxlookup" ) &&
	    NStr::EndsWith( vt->GetDb(), m_name );
    }
};


void
COrgrefProp::SetOrgrefProp( COrg_ref& org, const string& prop_name, const string& prop_val )
{
    string sProp = "taxlookup$"+prop_name;
    CRef< CDbtag > pDb( new CDbtag );
    pDb->SetDb( sProp );
    pDb->SetTag().SetStr( prop_val );
    // Find the prop_name first
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::iterator i = find_if( org.SetDb().begin(), org.SetDb().end(), ppdb );
	if( i != org.SetDb().end() ) {
	    *i = pDb;
	    return;
	}
    }
    org.SetDb().push_back( pDb );
}

void
COrgrefProp::SetOrgrefProp( COrg_ref& org, const string& prop_name, int prop_val )
{
    string sProp = "taxlookup%"+prop_name;
    CRef< CDbtag > pDb( new CDbtag );
    pDb->SetDb( sProp );
    pDb->SetTag().SetId( prop_val );
    // Find the prop_name first
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::iterator i = find_if( org.SetDb().begin(), org.SetDb().end(), ppdb );
	if( i != org.SetDb().end() ) {
	    *i = pDb;
	    return;
	}
    }
    org.SetDb().push_back( pDb );
}

void
COrgrefProp::SetOrgrefProp( COrg_ref& org, const string& prop_name, bool prop_val )
{
    string sProp = "taxlookup?"+prop_name;
    CRef< CDbtag > pDb( new CDbtag );
    pDb->SetDb( sProp );
    pDb->SetTag().SetId( prop_val ? 1 : 0 );
    // Find the prop_name first
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::iterator i = find_if( org.SetDb().begin(), org.SetDb().end(), ppdb );
	if( i != org.SetDb().end() ) {
	    *i = pDb;
	    return;
	}
    }
    org.SetDb().push_back( pDb );
}

bool 
COrgrefProp::HasOrgrefProp( const COrg_ref& org, const string& prop_name )
{
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::const_iterator i = find_if( org.GetDb().begin(), org.GetDb().end(), ppdb );
	return i != org.GetDb().end();
    }
    return false;
}

const string& 
COrgrefProp::GetOrgrefProp( const COrg_ref& org, const string& prop_name )
{
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::const_iterator i = find_if( org.GetDb().begin(), org.GetDb().end(), ppdb );
	if( i != org.GetDb().end() && (*i)->IsSetTag() && (*i)->GetTag().IsStr() ) {
	    return (*i)->GetTag().GetStr();
	}
    }
    return CNcbiEmptyString::Get();
}

// returns default value false if not found
bool 
COrgrefProp::GetOrgrefPropBool( const COrg_ref& org, const string& prop_name )
{
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::const_iterator i = find_if( org.GetDb().begin(), org.GetDb().end(), ppdb );
	if( i != org.GetDb().end() && (*i)->IsSetTag() && (*i)->GetTag().IsId() ) {
	    return (*i)->GetTag().GetId() != 0;
	}
    }
    return false;
}

// returns default value 0 if not found
int 
COrgrefProp::GetOrgrefPropInt( const COrg_ref& org, const string& prop_name )
{
    if( org.IsSetDb() ) {
	PPredDbTagByName ppdb( prop_name );
	COrg_ref::TDb::const_iterator i = find_if( org.GetDb().begin(), org.GetDb().end(), ppdb );
	if( i != org.GetDb().end() && (*i)->IsSetTag() && (*i)->GetTag().IsId() ) {
	    return (*i)->GetTag().GetId();
	}
    }
    return 0;
}

void
COrgrefProp::RemoveOrgrefProp( COrg_ref& org, const string& prop_name )
{
    // Find the prop_name first
    if( org.IsSetDb() ) {
	for( COrg_ref::TDb::iterator i = org.SetDb().begin(); i != org.SetDb().end(); ) {
	    if( prop_name.size()+10 == (*i)->GetDb().size() &&
		NStr::StartsWith( (*i)->GetDb(), "taxlookup" ) &&
		NStr::EndsWith( (*i)->GetDb(), prop_name ) ) {
		i = org.SetDb().erase(i);
	    } else {
		++i;
	    }
	}
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
