#include "serialobject.hpp"

string Ptr(const void* p)
{
    CNcbiOstrstream b;
    b << "0x" << hex << long(p);
    return CNcbiOstrstreamToString(b);
}

void CSerialObject::Dump(CNcbiOstream& out) const
{
    out << '{' << endl;
    out << "m_Name: \"" << m_Name << "\" (*" << Ptr(&m_Name) << ")" << endl;
    out << "m_NamePtr: ";
    if ( m_NamePtr )
        out << '"' << *m_NamePtr << "\" (*" << Ptr(m_NamePtr) << ")";
    else
        out << "null";
    out << endl;
    out << "m_Size: " << m_Size << endl;

    out << "m_Attributes: {" << endl;
    for ( list<string>::const_iterator i = m_Attributes.begin();
          i != m_Attributes.end();
          ++i ) {
        out << "    \"" << *i << '"' << endl;
    }
    out << "}" << endl;

    out << "m_Offsets: {" << endl;
    for ( vector<short>::const_iterator i1 = m_Offsets.begin();
          i1 != m_Offsets.end();
          ++i1 ) {
        out << "    " << *i1 << endl;
    }
    out << "}" << endl;
    
    out << "m_Names: {" << endl;
    for ( map<long, string>::const_iterator i2 = m_Names.begin();
          i2 != m_Names.end();
          ++i2 ) {
        out << "    " << i2->first << ": \"" << i2->second << '"' << endl;
    }
    out << "}" << endl;

    out << "m_Next: ";
    if ( m_Next )
        out << Ptr(m_Next);
    else
        out << "null";
    out << endl;
    out << '}' << endl;
}
