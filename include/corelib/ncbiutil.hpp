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

// get map element (pointer) or NULL if absent
template<class Key, class Element>
inline Element* GetMapElement(const map<Key, Element*>& m, const Key& key)
{
    map<Key, Element*>::const_iterator ptr = m.find(key);
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
    map<Key, string>::const_iterator ptr = m.find(key);
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
template<class Cnt>
inline void DeleteElements( Cnt& cnt )
{
    for ( typename Cnt::iterator i = cnt.begin(); i != cnt.end(); ++i ) {
        delete *i;
    }
}

// delete all elements from map containing pointers
template<class Key, class Element>
inline void DeleteElements(map<Key, Element*>& m)
{
    for ( map<Key, Element*>::iterator i = m.begin(); i != m.end(); ++i ) {
        delete i->second;
    }
}

template<class X, class Del = Deleter<X> >
class AutoPtr : public auto_ptr<X>
{
public:
    explicit AutoPtr(void)
        : auto_ptr<X>(0)
        {
        }

    explicit AutoPtr(X* p)
        : auto_ptr<X>(NotNull(p))
        {
        }

    AutoPtr(AutoPtr& a)
        : auto_ptr<X>(a)
        {
        }

    AutoPtr& operator= (AutoPtr& rhs)
        {
            reset(rhs.release());
            return *this;
        }

    ~AutoPtr(void)
        {
            reset();
        }

    void reset(void)
        {
            Del::Delete(release());
        }

    void reset(X* p)
        { 
            if ( get() != p )
                {
                    reset();
                    auto_ptr<X>::reset(p);
                }
        }
};

END_NCBI_SCOPE

#endif /* NCBI_UTILITY__HPP */
