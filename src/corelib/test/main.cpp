#include "xmlstore.hpp"

/*
 *
 * This is to test XMLStorage
 *
 */

using namespace std;

//
// test
//

namespace Test 
{

class AStorable
{
    friend ostream& operator<<( ostream&, const AStorable& );
    friend istream& operator>>( istream&, AStorable& ); // throw( exception )

public:

    AStorable()
    {}// cout << "+++ cout ccc\n"; }

    ~AStorable()
    {}// cout << "--- dtor ccc\n"; }
};

ostream& operator<<( ostream& os, const AStorable& a )
{
    os << "AStorable";
    return os;
}

istream& operator>>( istream& is, AStorable& a ) // throw( exception )
{
    //static i = 0;

    //if( i == 2 )
    //    throw logic_error( ">> not found " );

    //i++;

    char str[ 32 ];
    is >> str;
    return is;
}

} // namespace Test 

using namespace Test;

//
// main
//

USING_NCBI_SCOPE;

typedef CXmlMultimapAgent<int> Xml_int;

int main()
{
  char endl[] = "\n";

  try {

    CHistory s; 

    IO_PREFIX::fstream fs( "tt.txt", ios::in );

    IO_PREFIX::cout << "Loading..." << endl;
    s.Open( fs );
    IO_PREFIX::cout << "Printing..." << endl;
    s.Save( cout );
    IO_PREFIX::cout << "Done" << endl;

    CXmlMultimapAgent<int> ai;

    ai.Put( s, "integer", 5 );
    int* iii = ai.Get( s, "integer" );

    CXmlMultimapAgent<int>::Put( s, "integer", 5 );
    
    CXmlMultimapAgent<float>::Put( s, "fff", 7.7 );
    float* fff = CXmlMultimapAgent<float>::Get( s, "fff" );

    int* i = CXmlMultimapAgent<int>::Get( s, "integer" );
    IO_PREFIX::cout << "integer: " << *i << "\n"; 
    delete i;

    list<int*> iv; 
    CXmlMultimapAgent<int>::GetList( s, "integer", iv );

    for( list<int*>::iterator it = iv.begin(); it != iv.end(); it++ ) {
        IO_PREFIX::cout << "integer vector: " << **it << "\n"; 
        delete *it;
    }
        
    AStorable a0;
    CXmlMultimapAgent<AStorable>::Put( s, "AStorable", a0 );
    CXmlMultimapAgent<AStorable>::Put( s, "AStorable", a0 );
    CXmlMultimapAgent<AStorable>::Put( s, "AStorable", a0 );

    AStorable* a1 = CXmlMultimapAgent<AStorable>::Get( s, "AStorable" );
    IO_PREFIX::cout << "get a1: " << *a1 << "\n";
    
    CXmlMultimapAgent<AStorable>::Put( s, "AStorable id=\"1\"", a0 );
    AStorable* a2 = CXmlMultimapAgent<AStorable>::Get( s, "AStorable id=\"1\"" );
    cout << "get a2: " << *a2 << "\n";
    delete a2;

    list<AStorable*> av; 
    CXmlMultimapAgent<AStorable>::GetList( s, "AStorable", av );

    for( list<AStorable*>::iterator ita = av.begin(); ita != av.end(); ita++ ) {
        IO_PREFIX::cout << "AStorable vector: " << **ita << "\n"; 
        delete *ita;
    }
    
    IO_PREFIX::cout << "count int: " << s.Count( "integer" ) << "\n";
    IO_PREFIX::cout << "count AStorable: " << s.Count( "AStorable" ) << "\n";

    s.Save( cout );

  } catch( exception& e ) {
    IO_PREFIX::cout << "exception: " << e.what();
  }
  
  return 0;
}
