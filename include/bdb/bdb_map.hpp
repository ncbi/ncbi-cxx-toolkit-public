#ifndef BDB_MAP__HPP
#define BDB_MAP__HPP

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Templates implementing Berkeley DB based maps with syntax
 *                   close to standard associative containers
 */

#include <bdb/bdb_cursor.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Map
 *
 * @{
 */


/// Internal template used for compiler based type mapping  
/// (C++ templates specialization emplyed here)
///
template<class T> struct CBDB_TypeMapper
{
    typedef T  TFieldType;
};

template<> struct CBDB_TypeMapper<int>
{
    typedef CBDB_FieldInt4 TFieldType;
};

template<> struct CBDB_TypeMapper<string>
{
    typedef CBDB_FieldLString TFieldType;
};

/// db_map_base 
/// 

template<class K, class T> class db_map_base
{
public:

    typedef typename CBDB_TypeMapper<K>::TFieldType  db_first_type;
    typedef typename CBDB_TypeMapper<T>::TFieldType  db_second_type;

    typedef std::pair<K, T>   value_type;

    struct File : public CBDB_File
    {
        db_first_type  key;
        db_second_type value;

        File(CBDB_File::EDuplicateKeys dup_keys)
        : CBDB_File(dup_keys)
        {
            BindKey("Key", &key);
            BindData("Value", &value, 1024);
        }
    };

    typedef typename db_map_base<K, T>::File  map_file_type;
// protected:

    // Base class for all db_map iterators
    // Class opens its own copy of BerkeleyDB file and cursor on it.
    class iterator_base
    {        
    protected:
        enum EIteratorStatus {
            eUnknown,
            eEnd,
            eBeforeBegin,
            eInFile
        };

        iterator_base(map_file_type* db_file)
        : m_ParentFile(db_file),
          m_CursorFile(0),
          m_Cur(0),
          m_OpenMode(CBDB_RawFile::eReadOnly),
          m_IStatus(eUnknown)
        {
        }

        ~iterator_base()
        {
            delete m_Cur;
            delete m_CursorFile;
        }

        iterator_base(const iterator_base& it)
        {
            init_from(it);
        }

        iterator_base& operator=(const iterator_base& it)
        {
            return init_from(it);
        }

        iterator_base& init_from(const iterator_base& it)
        {
            m_ParentFile = it.m_ParentFile;
            m_CursorFile = 0;
            m_Cur = 0;
            m_OpenMode = it.m_OpenMode;
            m_SearchFlag = it.m_SearchFlag;
            m_pair.first = it.m_pair.first;
            m_pair.second = it.m_pair.second;
            m_IStatus = it.m_IStatus;

            return *this;
        }

        void open_cursor(CBDB_RawFile::EOpenMode open_mode, 
                         CBDB_FileCursor::ECondition cursor_condition) const
        {
            delete m_Cur; m_Cur = 0;
            bool attached = m_CursorFile->IsAttached();
            if (!attached) {
                m_CursorFile->Attach(*m_ParentFile);
            }
            m_Cur = new CBDB_FileCursor(*m_CursorFile);
            m_Cur->SetCondition(cursor_condition);
        }

        void open_cursor(CBDB_RawFile::EOpenMode open_mode, 
                         CBDB_FileCursor::ECondition cursor_condition,
                         const K& key) const
        {
            open_cursor(open_mode, cursor_condition);
            m_Cur->From << key;
        }

        // db_map iterators are quite heavy (opens BDB cursors and stuff) and
        // to simplify the iteratros coping and assignments we use lazy initilization
        // It means actual db operation is postponed until the first iterator access.
        void check_open_cursor() const
        {
            if (m_CursorFile == 0) {
                _ASSERT(m_ParentFile);
                m_CursorFile = new map_file_type(m_ParentFile->GetDupKeysMode());

                _ASSERT(m_Cur == 0);
                if (m_SearchFlag) {
                    open_cursor(m_OpenMode, CBDB_FileCursor::eGE, m_pair.first);
                } else {
                    if (m_IStatus == eEnd) {
                        open_cursor(m_OpenMode, CBDB_FileCursor::eLast);
                        m_Cur->ReverseFetchDirection();
                    } else {
                        open_cursor(m_OpenMode, CBDB_FileCursor::eFirst);
                    }
                }
                if (m_Cur->Fetch() ==  eBDB_Ok) {
                    m_SearchFlag = true;
                    m_pair.first = m_CursorFile->key;
                    m_pair.second= m_CursorFile->value;

                    if (m_IStatus == eEnd) {
                    } else {
                        m_IStatus = eInFile;
                    }
                } else { // No data...
                    // go to the end
                    open_cursor(m_OpenMode, CBDB_FileCursor::eLast);
                    m_IStatus = eEnd;
                }
            }
        }

        void go_end()
        {
            if (m_IStatus == eEnd) return;

            // if it's "end()" iterartor, no need to open any files here
            if (m_CursorFile == 0) {
                m_IStatus = eEnd;
                return;
            }
            open_cursor(m_OpenMode, CBDB_FileCursor::eLast);
            m_Cur->ReverseFetchDirection();

            if (m_Cur->Fetch() == eBDB_Ok) {
                m_pair.first = (K)m_CursorFile->key;
                m_pair.second= (T)m_CursorFile->value;
            }
            m_IStatus = eEnd;
        }

        void fetch_next()
        {
            _ASSERT(m_IStatus != eEnd);
            check_open_cursor();

            if (m_Cur->Fetch() == eBDB_Ok) {
                m_pair.first = m_CursorFile->key;
                m_pair.second= m_CursorFile->value;
                m_IStatus = eInFile;
            } else {
                m_IStatus = eEnd;
            }
        }

        void fetch_prev()
        {
            _ASSERT(m_IStatus != eBeforeBegin);
            check_open_cursor();

            if (m_Cur->Fetch(CBDB_FileCursor::eBackward) == eBDB_Ok) {
                m_pair.first = m_CursorFile->key;
                m_pair.second= m_CursorFile->value;
                m_IStatus = eInFile;
            } else {
                m_IStatus = eBeforeBegin;
            }
        }

        bool is_equal(const iterator_base& it) const
        {
            if (m_ParentFile != it.m_ParentFile) 
                return false;
            if (m_IStatus == eUnknown) {
                check_open_cursor();
            }
            if (m_IStatus == it.m_IStatus) {
                if (m_IStatus == eEnd && it.m_IStatus == eEnd) 
                    return true;
                if (m_IStatus == eBeforeBegin && it.m_IStatus == eBeforeBegin)
                    return true;
                return (m_pair.first == it.m_pair.first);
            }
            return false;
        }

    public: 

        // Return TRUE while iterator is in the correct range
        bool valid() const
        {
            check_open_cursor();

            if (m_IStatus == eEnd) return false;
            if (m_IStatus == eBeforeBegin) return false;
            return true;
        }
    protected:
        mutable map_file_type*             m_ParentFile;
        mutable map_file_type*             m_CursorFile;
        mutable CBDB_FileCursor*           m_Cur;

        // Cursor open mode
        CBDB_RawFile::EOpenMode m_OpenMode;
        // TRUE if iterator searches for the specific key
        mutable bool                    m_SearchFlag;
         // Current key-value pair
        mutable value_type              m_pair;
        // Iterator "navigation" status
        mutable EIteratorStatus         m_IStatus;
    };
public:

    class const_iterator : public iterator_base
    {
    public:
        const_iterator(map_file_type* db_file)
        : iterator_base(db_file)
        {
            this->m_SearchFlag = false;
        }

        const_iterator(map_file_type* db_file, const K& key)
        : iterator_base(db_file)
        {
            this->m_SearchFlag = true;
            this->m_pair.first = key;
        }

        const_iterator& go_end()
        {
            iterator_base::go_end();
            return *this;
        }

        const_iterator() : iterator_base(0) 
        {}
        const_iterator(const const_iterator& it) : iterator_base(it)
        {}
        
        const_iterator& operator=(const const_iterator& it) 
        { 
            init_from(it);return *this;
        }

        const value_type& operator*() const
        {
            this->check_open_cursor();
            return this->m_pair;
        }

        const value_type* operator->() const
        {
            this->check_open_cursor();
            return &this->m_pair;
        }
        
        const_iterator& operator++() 
        { 
            this->fetch_next();
            return *this; 
        }

        const_iterator operator++(int) 
        {
            const_iterator tmp(*this);
            this->fetch_next();
            return tmp; 
        }

        const_iterator& operator--() 
        { 
            this->fetch_prev();
            return *this;
        }

        const_iterator operator--(int) 
        {
            const_iterator tmp(*this);    
            this->fetch_prev(); 
            return tmp;
        }

        bool operator==(const const_iterator& it) 
        { 
            return is_equal(it); 
        }
        bool operator!=(const const_iterator& it) 
        { 
            return !is_equal(it);
        }

    };

public:

    db_map_base(CBDB_File::EDuplicateKeys dup_keys) : m_Dbf(dup_keys) {}

    void open(const char* fname, 
              IOS_BASE::openmode mode=IOS_BASE::in|IOS_BASE::out,
              unsigned int cache_size=0);
    void open(const char* fname, 
              CBDB_RawFile::EOpenMode db_mode,
              unsigned int cache_size=0);

    const_iterator begin() const;
    const_iterator end() const;
    const_iterator find(const K& key) const;

    size_t  size() const;
    void clear();

    void erase(const K& key);

protected:
    mutable File       m_Dbf;
};



/// db_map template, mimics std::map<> using BerkeleyDB as the underlying
/// Btree mechanism.
///
/// NOTE: const methods of this template are conditionally thread safe due 
/// to the use of mutable variables.
/// If you access to access one instance of db_map from more than one thread,
/// external syncronization is required.

template<class K, class T> class db_map : public db_map_base<K, T>
{
public:
    typedef typename CBDB_TypeMapper<K>::TFieldType  db_first_type;
    typedef typename CBDB_TypeMapper<T>::TFieldType  db_second_type;

    typedef K                                        key_type;
    typedef T                                        referent_type;
    typedef std::pair<const K, T>                          value_type;

    typedef db_map<K, T>                             self_type;
    typedef typename self_type::iterator_base        iterator_base_type;

public:

    db_map() : db_map_base<K, T>(CBDB_File::eDuplicatesDisable) {}

    //
    // Determines whether an element y exists in the map whose key matches that of x.
    // If not creates such an element and returns TRUE. Otherwise returns FALSE.
    bool insert(const value_type& x);

    referent_type operator[](const K& key);

};



/// db_multimap template, mimics std::multimap<> using BerkeleyDB as 
/// the underlying Btree mechanism.
///
/// NOTE: const methods of this template are conditionally thread safe due 
/// to the use of mutable variables.
/// If you access to access one instance of db_map from more than one thread,
/// external syncronization is required.
///
template<class K, class T> class db_multimap : public db_map_base<K, T>
{
public:
    typedef typename CBDB_TypeMapper<K>::TFieldType  db_first_type;
    typedef typename CBDB_TypeMapper<T>::TFieldType  db_second_type;

    typedef K                       key_type;
    typedef T                       referent_type;
    typedef std::pair<const K, T>   value_type;

    typedef db_multimap<K, T>       self_type;
    typedef typename self_type::iterator_base iterator_base_type;
public:
    db_multimap() : db_map_base<K, T>(CBDB_File::eDuplicatesEnable) {}

    void insert(const value_type& x);

};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

inline 
CBDB_RawFile::EOpenMode iosbase2BDB(IOS_BASE::openmode mode)
{
    CBDB_RawFile::EOpenMode db_mode = CBDB_RawFile::eReadWriteCreate;

    // Mapping iostream based open mode into BDB open mode
    //
    if (mode & IOS_BASE::in) {
        db_mode = 
          (mode & IOS_BASE::out) 
            ? CBDB_RawFile::eReadWriteCreate : CBDB_RawFile::eReadOnly;
    } else {
        if (mode & IOS_BASE::out) {
            db_mode = CBDB_RawFile::eReadWriteCreate;
            if (mode & IOS_BASE::trunc) {
                db_mode = CBDB_RawFile::eCreate;
            }
        }
    }
    return db_mode;
}


/////////////////////////////////////////////////////////////////////////////
//
//  db_map_base
//

template<class K, class T>
void db_map_base<K, T>::open(const char* fname,
                             IOS_BASE::openmode mode,
                             unsigned int cache_size)
{
    CBDB_RawFile::EOpenMode db_mode = iosbase2BDB(mode);
    open(fname, db_mode, cache_size);
}

template<class K, class T>
void db_map_base<K, T>::open(const char* fname, 
                             CBDB_RawFile::EOpenMode db_mode,
                             unsigned int cache_size)
{
    if (cache_size)
        m_Dbf.SetCacheSize(cache_size);
    m_Dbf.Open(fname, db_mode);
    m_Dbf.SetLegacyStringsCheck(true);
}

template<class K, class T>
typename db_map_base<K, T>::const_iterator  db_map_base<K, T>::begin() const
{
    return const_iterator(&m_Dbf);
}


template<class K, class T>
typename db_map_base<K, T>::const_iterator  db_map_base<K, T>::end() const
{
    const_iterator it(&m_Dbf);
    return it.go_end();
}


template<class K, class T>
typename db_map_base<K, T>::const_iterator db_map_base<K, T>::find(const K& key) const
{
    return const_iterator(&m_Dbf, key);
}

template<class K, class T>
size_t db_map_base<K, T>::size() const
{
    return m_Dbf.CountRecs();
}

template<class K, class T>
void db_map_base<K, T>::clear()
{
    m_Dbf.Truncate();
}

template<class K, class T>
void db_map_base<K, T>::erase(const K& key)
{
    m_Dbf.key = key;
    m_Dbf.Delete();
}


/////////////////////////////////////////////////////////////////////////////
//
//  db_map
//

template<class K, class T>
bool db_map<K, T>::insert(const value_type& x)
{
    this->m_Dbf.key = x.first;
    this->m_Dbf.value = x.second;

    EBDB_ErrCode ret = this->m_Dbf.Insert();

    return (ret == eBDB_Ok);
}

template<class K, class T>
typename db_map<K, T>::referent_type  db_map<K, T>::operator[](const K& key)
{
    this->m_Dbf.key = key;
    EBDB_ErrCode ret = this->m_Dbf.Fetch();
    if (ret == eBDB_Ok) {
        return (T) this->m_Dbf.value;
    }
    return (T) this->m_Dbf.value;
}


/////////////////////////////////////////////////////////////////////////////
//
//  db_multimap
//

template<class K, class T>
void db_multimap<K, T>::insert(const value_type& x)
{
    this->m_Dbf.key = x.first;
    this->m_Dbf.value = x.second;

    EBDB_ErrCode ret = this->m_Dbf.Insert();
    BDB_CHECK(ret, this->m_Dbf.GetFileName().c_str());
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/03/07 21:42:16  ucko
 * Make iterator_base public because some compilers (MIPSpro at least)
 * otherwise won't expose it to other inner classes.
 *
 * Revision 1.12  2004/10/14 17:45:43  vasilche
 * Use IOS_BASE instead of non-portable ios_base.
 *
 * Revision 1.11  2004/04/26 16:43:59  ucko
 * Qualify inherited dependent names with this-> where needed by GCC 3.4.
 *
 * Revision 1.10  2004/02/11 17:59:05  kuznets
 * Set legacy strings checking by default
 *
 * Revision 1.9  2004/02/04 17:04:58  kuznets
 * Make use of LString type in map objects to handle non-ASCIIZ strings
 *
 * Revision 1.8  2003/12/16 17:47:09  kuznets
 * minor code cleanup
 *
 * Revision 1.7  2003/09/29 14:30:22  kuznets
 * Comments doxygenification
 *
 * Revision 1.6  2003/07/24 15:43:25  kuznets
 * Fixed SUN compilation problems
 *
 * Revision 1.5  2003/07/23 20:22:50  kuznets
 * Implemened:  clean(), erase()
 *
 * Revision 1.4  2003/07/23 18:09:51  kuznets
 * + "cache size" parameter for bdb_map bdb_multimap
 *
 * Revision 1.3  2003/07/22 16:38:00  kuznets
 * Fixing compilation problems
 *
 * Revision 1.2  2003/07/22 15:15:46  kuznets
 * Work in progress, multiple changes, added db_multimap template.
 *
 * Revision 1.1  2003/07/18 20:48:29  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif
