#ifndef NCBI_XMLSTORE__HPP
#define NCBI_XMLSTORE__HPP

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
* Author:  !!! PUT YOUR NAME(s) HERE !!!
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/11/17 20:02:07  sandomir
* generic XmlMultimap as base class for History
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <string>
#include <list>
#include <map>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

//
// CXmlMultimap
//
// General-purpose container based on multimap storing pairs of strings key/value.
// Pairs are sorted by key as multimap feature.
// Save/Load in XML format <key>value</key>

class CXmlMultimap
{
public:

    typedef multimap<string,string>::const_iterator CIT;
    typedef pair<CIT,CIT> CIT_SEARCH_PAIR;

    typedef multimap<string,string>::iterator IT;
    typedef pair<IT,IT> IT_SEARCH_PAIR;

    CXmlMultimap( void ) THROWS_NONE;
    ~CXmlMultimap( void ) THROWS_NONE;

    void Put( const string& key, const string& value );

    // throws logic_error if not found
    CIT_SEARCH_PAIR Get( const string& key );

    // returns number of elements erased 
    int Erase( const string& key )
    { return m_map.erase( key ); }

    void Clear( void )
    { m_map.clear(); }

    int Count( const string& key ) const
    { return m_map.count( key ); }

    // save as a list of <key>value</key>
    void Save( IO_PREFIX::ostream& os );
    
    // throws runtime_error
    void Open( IO_PREFIX::istream& is ); 
    
private:

    char* LoadData( IO_PREFIX::istream& is );
    void ParseData( char* data );

    string GetBodyType( void ) const
    { return "history"; }

    static const char sm_lt;
    static const char sm_gt;
    static const char* sm_lte;

    multimap<string,string> m_map;

};

//
// CHistory
//
// History prototype

class CHistory : public CXmlMultimap
{
public:

    CHistory( void ) THROWS_NONE;
    ~CHistory( void ) THROWS_NONE;
};

//
// CXmlMultimapAgent
//
// Typed-dependent access to XmlMultimap

template<class T>class CXmlMultimapAgent 
{
public:

    static void Put( CXmlMultimap& s, const string& key, const T& t )
    { 
        IO_PREFIX::ostrstream os;
        os << t << ends;
        s.Put( key, os.str() );
    }

    // throws exception if key not found or reading error; result never 0
    // client should delete resulting pointer
    static T* Get( CXmlMultimap& s, const string& key ) 
    {
        CXmlMultimap::CIT_SEARCH_PAIR pi = s.Get( key );
        IO_PREFIX::istrstream is( pi.first->second.c_str() );
        T* t = new T; 

        try {
            is >> *t;
        } catch( ... ) {
            delete t;
            throw;
        }
        return t;
    }

    // throws exception if key not found or nothing was read; skips element on reading error
    // client should delete resulting pointers in list
    static list< T*, allocator<T*> >::iterator GetList( CXmlMultimap& s, const string& key, 
                                       list< T*, allocator<T*> >& lst ) 
    { 
        CXmlMultimap::CIT_SEARCH_PAIR pi = s.Get( key );
        int sz = lst.size();

        for( CXmlMultimap::CIT iter = pi.first; iter != pi.second; iter++ ) {
            IO_PREFIX::istrstream is( iter->second.c_str() );
            T* t = new T; 
            try {
                is >> *t;
                lst.push_back( t );
            } catch( ... ) { 
                delete t; 
            }
        } // for

        if( sz == lst.size() ) {
            throw logic_error( "CXmlMultimapAgent<T>::GetList: '" + key + "' list is empty" );
        }
       
        return lst.begin();
    }

};

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif /* NCBI_XMLSTORE__HPP */

