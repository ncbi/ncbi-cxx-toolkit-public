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
*   Some usefull classes and methods.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2000/12/15 21:27:11  vasilche
* Moved some typedefs/enums to serial/serialbase.hpp.
* Removed explicit keyword from AutoPtr costructor.
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
* qwebenv files splitted to smaller chunks.
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
#include <map>
#include <set>
#include <memory>
#include <list>
#include <vector>
#include <functional>

BEGIN_NCBI_SCOPE

//-------------------------------------------
// Utilities

// check for equality object pointed by pointer
template <class T>
struct p_equal_to : public binary_function
<const T*, const T*, bool>
{
    bool operator() (const T*& x, const T*& y) const
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

// default create function
template<class X>
struct Creater
{
    static X* Create(void)
        { return new X; }
};

// default delete function
template<class X>
struct Deleter
{
    static void Delete(X* object)
        { delete object; }
};

// array delete function
template<class X>
struct ArrayDeleter
{
    static void Delete(X* object)
        { delete[] object; }
};

// C structures delete function
template<class X>
struct CDeleter
{
    static void Delete(X* object)
        { free(object); }
};

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
    if ( ptr !=m.end() )
        return ptr->second;
    return string();
}

template<class Key>
inline void SetMapString(map<Key, string>& m, const Key& key, const string& data)
{
    if ( !data.empty() )
        m[key] = data;
    else
        m.erase(key);
}

// delete all elements from a container of pointers (list, vector, set, multiset)
// clear container afterwards
template<class Cnt>
inline void DeleteElements( Cnt& cnt )
{
    for ( typename Cnt::iterator i = cnt.begin(); i != cnt.end(); ++i ) {
        delete *i;
    }
	cnt.clear();
}

// delete all elements from map containing pointers
// clear container afterwards
template<class Key, class Element>
inline void DeleteElements(map<Key, Element*>& m)
{
    for ( typename map<Key, Element*>::iterator i = m.begin(); i != m.end(); ++i ) {
        delete i->second;
    }
	m.clear();
}

// delete all elements from multimap containing pointers
// clear container afterwards
template<class Key, class Element>
inline void DeleteElements(multimap<Key, Element*>& m)
{
    for ( typename map<Key, Element*>::iterator i = m.begin(); i != m.end(); ++i ) {
        delete i->second;
    }
	m.clear();
}

// Standard auto_ptr template from STL have designed in a way which does'n
// allow to put it in STL containers (list, vector, map etc.).
// The reason is absence of copy constructor and assignment operator.
// We decided that it would be useful to have analog of STL's auto_ptr
// without this restriction - AutoPtr.
// NOTE: due to nature of AutoPtr it's copy constructor and assignment
// operator modify source object - source object will hold NULL after use.
// Also we added possibility to redefine the way pointer will be deleted:
// second argument of template allows to put pointers from malloc in
// AutoPtr. By default, nevertheless, pointers will be deleted by C++
// delete operator.
template< class X, class Del = Deleter<X> >
class AutoPtr
{
public:
    typedef X element_type;

    AutoPtr(X* p = 0)
        : m_Ptr(p)
        {
        }
    AutoPtr(const AutoPtr<X>& p)
        : m_Ptr(p.x_Release())
        {
        }

    ~AutoPtr(void)
        {
            reset();
        }

    AutoPtr& operator=(const AutoPtr<X>& p)
        {
            reset(p.x_Release());
            return *this;
        }
    AutoPtr& operator=(X* p)
        {
            reset(p);
            return *this;
        }

    // bool operator is for using in if() clause
    operator bool(void) const
        {
            return m_Ptr != 0;
        }
    // standard getters
    X& operator*(void) const
        {
            return *m_Ptr;
        }
    X* operator->(void) const
        {
            return m_Ptr;
        }
    X* get(void) const
        {
            return m_Ptr;
        }

    // release will release ownership of pointer to caller
    X* release(void)
        {
            X* ret = m_Ptr;
            m_Ptr = 0;
            return ret;
        }
    // reset will delete old pointer and set content to new value
    void reset(X* p)
        {
            reset();
            m_Ptr = p;
        }

private:
    X* m_Ptr;

    // release for const object
    X* x_Release(void) const
        {
            return const_cast<AutoPtr<X>*>(this)->release();
        }

    // reset without  arguments
    void reset(void)
        {
            Del::Delete(release());
        }
};

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

// iterate is useful macro for writing 'for' statements with STL container
// iterator as an variable.
#define iterate(Type, Var, Cont) \
    for ( Type::const_iterator Var = (Cont).begin(), NCBI_NAME2(Var,_end) = (Cont).end(); Var != NCBI_NAME2(Var,_end); ++Var )
#define non_const_iterate(Type, Var, Cont) \
    for ( Type::iterator Var = Cont.begin(); Var != Cont.end(); ++Var )

END_NCBI_SCOPE

#endif /* NCBI_UTILITY__HPP */
