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
    return (m_ref->GetCde() & TXC_UNCULTURED);
}

bool
CTaxon1Node::IsGBHidden() const
{
    return (m_ref->GetCde() & TXC_GBHIDE);
}


END_objects_SCOPE
END_NCBI_SCOPE

