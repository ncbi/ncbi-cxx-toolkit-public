#include "typecontext.hpp"
#include <corelib/ncbireg.hpp>

CNcbiOstream& operator<<(CNcbiOstream& out, const CFilePosition& filePos)
{
    return out << '"' << filePos.GetFileName() << "\", line " <<
        filePos.GetFileLine();
}

string CFilePosition::ToString(void) const
{
    return '"' + GetFileName() + "\", line " + NStr::IntToString(GetFileLine());
}

CConfigPosition::CConfigPosition(const CConfigPosition& base,
                                 const string& member)
    : m_ParentType(base.GetParentType()), m_CurrentMember(member)
{
    if ( GetParentType() ) {
        // new member
        m_Section = base.GetSection();
        m_KeyPrefix = base.AppendKey(member);
    }
    else {
        // new type
        m_Section = member;
    }
}

CConfigPosition::CConfigPosition(const CConfigPosition& base,
                                 const ASNType* type, const string& member)
    : m_ParentType(type), m_CurrentMember(member)
{
    m_Section = base.GetSection();
    if ( member.empty() )
        m_KeyPrefix = base.GetKeyPrefix();
    else
        m_KeyPrefix = base.AppendKey(member);
}

string CConfigPosition::AppendKey(const string& value) const
{
    if ( GetKeyPrefix().empty() )
        return value;
    else
        return GetKeyPrefix() + '.' + value;
}

const string& CConfigPosition::GetVar(const CNcbiRegistry& registry,
                                      const string& defaultSection,
                                      const string& value) const
{
    _ASSERT(!GetSection().empty());
    _ASSERT(!value.empty());
    string keyName = AppendKey(value);
    if ( !defaultSection.empty() ) {
        const string& s1 = registry.Get(defaultSection + '.' + GetSection(),
                                        keyName);
        if ( !s1.empty() )
            return s1;
    }
    const string& s2 = registry.Get(GetSection(), keyName);
    if ( !s2.empty() )
        return s2;
    if ( !defaultSection.empty() )
        return registry.Get(defaultSection, keyName);
    return NcbiEmptyString;
}
