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

string CConfigPosition::GetKeyName(const string& value) const
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
    string keyName = GetKeyName(value);
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
