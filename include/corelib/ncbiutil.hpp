#ifndef NCBI_UTILITY__HPP
#define NCBI_UTILITY__HPP

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
* Author: 
*   Eugene Vasilchenko
*
* File Description:
*   Some useful classes and methods.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2002/02/13 22:39:13  ucko
* Support AIX.
*
* Revision 1.18  2001/11/16 18:21:15  thiessen
* fix for Mac/CodeWarrior
*
* Revision 1.17  2001/11/09 20:04:04  ucko
* Tweak p_equal_to for portability.  (Tested with GCC, WorkShop,
* MIPSpro, and MSVC.)
*
* Revision 1.16  2001/05/17 14:54:44  lavr
* Typos corrected
*
* Revision 1.15  2000/12/24 00:01:48  vakatov
* Moved some code from NCBIUTIL to NCBISTD.
* Fixed AutoPtr to always work with assoc.containers
*
* Revision 1.13  2000/12/12 14:20:14  vasilche
* Added operator bool to CArgValue.
* Added standard typedef element_type to CRef<> and CConstRef<>.
* Macro iterate() now calls method end() only once and uses temporary variable.
* Various NStr::Compare() methods made faster.
* Added class Upcase for printing strings to ostream with automatic conversion.
*
* Revision 1.12  2000/08/29 18:32:38  vakatov
* Rollback R1.11 to R1.10
*
* Revision 1.10  2000/07/27 16:05:25  golikov
* Added DeleteElements for multimap
*
* Revision 1.9  1999/12/01 17:35:48  vasilche
* Added missing typename keyword.
*
* Revision 1.8  1999/11/19 18:24:49  vasilche
* Added ArrayDeleter and CDeleter for use in AutoPtr.
*
* Revision 1.7  1999/11/19 15:44:51  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.6  1999/09/16 15:49:36  sandomir
* DeleteElements: clear container added to avoid case when user forgets to call clear after DeleteElements
*
* Revision 1.5  1999/09/14 18:49:09  vasilche
* Added AutoMap template to convert cached maps for set/list/vector of values.
*
* Revision 1.4  1999/09/03 21:29:56  vakatov
* Fixes for AutoPtr::  specify template parameter for the base "auto_ptr"
* + #include <memory>
*
* Revision 1.3  1999/07/08 19:07:54  vakatov
* Fixed "p_equal_to" and "pair_equal_to" to pass MIPSpro 7.3 (-n32) compil
*
* Revision 1.2  1999/06/02 00:46:06  vakatov
* DeleteElements():  use "typename" for Cnt::iterator
*
* Revision 1.1  1999/05/06 20:32:52  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.10  1999/04/22 14:22:24  vasilche
* Fixed mix of report name and label.
* Fixed 'related articles' of 'related articles' bug.
* Added _TRACE_THROW() macro before every throw statement.
*
* Revision 1.9  1999/04/14 17:36:30  vasilche
* Filling of CPmResource with commands was moved to main() function.
* main() function now in separate file: querymain.cpp
* All buttons implemented as IMAGE input tags.
* Retrieve command now doesn't appear in history.
* Results with one document now have this document selected by default.
*
* Revision 1.8  1999/04/09 15:52:45  vasilche
* Fixed warning about generated extern "C" function passes as arguments.
*
* Revision 1.7  1999/04/08 19:02:38  vasilche
* Added clipboards
*
* Revision 1.6  1999/04/06 19:36:17  vasilche
* Changed hockey puck structures.
* Added defalut DB filters.
* Added DB clipboards and clipboard page.
*
* Revision 1.5  1999/03/26 22:03:28  sandomir
* Link as kind of report added (qlink.hpp); filters redesigned; new filters for nuc and protein
*
* Revision 1.4  1999/03/15 20:01:59  vasilche
* Changed query-command interaction.
* qwebenv files split to smaller chunks.
*
* Revision 1.3  1999/02/25 19:50:36  vasilche
* First implementation of history page.
*
* Revision 1.2  1999/02/18 19:28:15  vasilche
* Added WebEnvironment.
* Fixed makefiles.
*
* Revision 1.1  1999/02/11 21:36:32  vasilche
* Added storable Web environment.
* Added qutility.hpp.
* Removed some unneeded try/catch blocks by using auto_ptr.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <memory>
#include <map>


BEGIN_NCBI_SCOPE


//-------------------------------------------
// Utilities

// check for equality object pointed by pointer
template <class T>
struct p_equal_to : public binary_function
<const T*, const T*, bool>
{
#if defined(NCBI_COMPILER_MIPSPRO) || defined(NCBI_OS_MAC) || defined(NCBI_COMPILER_VISUALAGE)
    // fails to define these
    typedef const T* first_argument_type;
    typedef const T* second_argument_type;
#endif
    // Sigh.  WorkShop rejects this code without typename (but only in
    // 64-bit mode!), and GCC rejects typename without a scope.
    bool operator() (const typename p_equal_to::first_argument_type& x,
                     const typename p_equal_to::second_argument_type& y) const
    { return *x == *y; }
};

// check for equality object pointed by pointer
template <class Pair>
struct pair_equal_to : public binary_function
<Pair, typename Pair::second_type, bool>
{
    bool operator() (const Pair& x,
                     const typename Pair::second_type& y) const
    { return x.second == y; }
};

// checker for not null value (after C malloc, strdup etc.)
template<class X>
inline X* NotNull(X* object)
{
    if ( !object ) {
        _TRACE("NotNull failed: throwing bad_alloc");
        throw bad_alloc();
    }
    return object;
}

// get map element (pointer) or NULL if absent
template<class Key, class Element>
inline Element* GetMapElement(const map<Key, Element*>& m, const Key& key)
{
    typename map<Key, Element*>::const_iterator ptr = m.find(key);
    if ( ptr !=m.end() )
        return ptr->second;
    return 0;
}

template<class Key, class Element>
inline void SetMapElement(map<Key, Element*>& m, const Key& key, Element* data)
{
    if ( data )
        m[key] = data;
    else
        m.erase(key);
}

template<class Key, class Element>
inline bool InsertMapElement(map<Key, Element*>& m,
                             const Key& key, Element* data)
{
    return m.insert(map<Key, Element*>::value_type(key, data)).second;
}

// get map element or "" if absent
template<class Key>
inline string GetMapString(const map<Key, string>& m, const Key& key)
{
    typename map<Key, string>::const_iterator ptr = m.find(key);
    if ( ptr != m.end() )
        return ptr->second;
    return string();
}

template<class Key>
inline void SetMapString(map<Key, string>& m,
                         const Key& key, const string& data)
{
    if ( !data.empty() )
        m[key] = data;
    else
        m.erase(key);
}

// delete all elements from a container of pointers
// (list, vector, set, multiset);  clear the container afterwards
template<class Cnt>
inline void DeleteElements( Cnt& cnt )
{
    for ( typename Cnt::iterator i = cnt.begin(); i != cnt.end(); ++i ) {
        delete *i;
    }
    cnt.clear();
}

// delete all elements from map containing pointers;
// clear container afterwards
template<class Key, class Element>
inline void DeleteElements(map<Key, Element*>& m)
{
    for ( typename map<Key, Element*>::iterator i = m.begin();  i != m.end();
          ++i ) {
        delete i->second;
    }
    m.clear();
}

// delete all elements from multimap containing pointers;
// clear container afterwards
template<class Key, class Element>
inline void DeleteElements(multimap<Key, Element*>& m)
{
    for ( typename map<Key, Element*>::iterator i = m.begin();  i != m.end();
          ++i ) {
        delete i->second;
    }
    m.clear();
}

template<class Result, class Source, class ToKey>
inline
Result&
AutoMap(auto_ptr<Result>& cache, const Source& source, const ToKey& toKey)
{
    Result* ret = cache.get();
    if ( !ret ) {
        cache.reset(ret = new Result);
        for ( typename Source::const_iterator i = source.begin();
              i != source.end();
              ++i ) {
            ret->insert(Result::value_type(toKey.GetKey(*i), *i));
        }
    }
    return *ret;
}

template<class Value>
struct CNameGetter
{
    const string& GetKey(const Value* value) const
    {
        return value->GetName();
    }
};


END_NCBI_SCOPE

#endif /* NCBI_UTILITY__HPP */
