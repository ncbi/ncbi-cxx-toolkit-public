#ifndef OBJMGR_MAP__HPP
#define OBJMGR_MAP__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbicntr.hpp>
#include <map>

BEGIN_NCBI_SCOPE

#define R_WRAP(Type, Call, Declaration) \
    Type Declaration \
        { \
            Type ret; \
            RLock(); \
            ret = m_Container.Call; \
            RUnlock(); \
            return ret; \
        }
#define R_WRAP_VOID(Call, Declaration) \
    void Declaration \
        { \
            RLock(); \
            m_Container.Call; \
            RUnlock(); \
        }
#define W_WRAP(Type, Call, Declaration) \
    Type Declaration \
        { \
            Type ret; \
            WLock(); \
            ret = m_Container.Call; \
            WUnlock(); \
            return ret; \
        }
#define W_WRAP_VOID(Call, Declaration) \
    void Declaration \
        { \
            WLock(); \
            m_Container.Call; \
            WUnlock(); \
        }

template<class Container>
class map_checker
{
    typedef Container container_type;
    typedef map_checker<Container> this_type;

public:
    typedef typename container_type::size_type size_type;
    typedef typename container_type::key_type key_type;
    typedef typename container_type::value_type value_type;
    typedef typename container_type::const_iterator const_iterator;
    typedef typename container_type::iterator iterator;

protected:
    typedef pair<const_iterator, const_iterator> const_iterator_pair;
    typedef pair<iterator, iterator> iterator_pair;
    typedef pair<iterator, bool> iterator_bool;

    container_type m_Container;
    mutable CAtomicCounter_WithAutoInit m_WCounter;
    mutable CAtomicCounter_WithAutoInit m_RCounter;

public:

    void RLock() const
        {
            if ( m_RCounter.Add(1) <= 0 || m_WCounter.Get() != 0 )
                abort();
        }
    void RUnlock() const
        {
            if ( m_WCounter.Get() != 0 || m_RCounter.Add(-1) < 0 )
                abort();
        }
    void WLock() const
        {
            if ( m_WCounter.Add(1) != 1 || m_RCounter.Get() != 0 )
                abort();
        }
    void WUnlock() const
        {
            if ( m_RCounter.Get() != 0 || m_WCounter.Add(-1) != 0 )
                abort();
        }

    iterator check(iterator pos)
        {
            if ( pos != m_Container.end() ) {
                iterator it = m_Container.find(pos->first);
                if ( it != pos )
                    abort();
            }
            return pos;
        }

    map_checker()
        {
        }
    ~map_checker()
        {
            WLock();
        }
    map_checker(const this_type& m)
        {
            *this = m;
        }
    this_type& operator=(const this_type& m)
        {
            WLock();
            m.RLock();
            m_Container = m.m_Container;
            m.RUnlock();
            WUnlock();
            return *this;
        }
    void swap(this_type& m)
        {
            WLock();
            m.WLock();
            m_Container.swap(m);
            m.WUnlock();
            WUnlock();
        }

    bool operator==(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = m_Container == m.m_Container;
            m.RUnlock();
            RUnlock();
            return ret;
        }
    bool operator<(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = m_Container < m.m_Container;
            m.RUnlock();
            RUnlock();
            return ret;
        }

    R_WRAP(size_type, size(), size() const);
    R_WRAP(bool, empty(), empty() const);

    R_WRAP(const_iterator, begin(), begin() const);
    R_WRAP(const_iterator, end(), end() const);
    R_WRAP(const_iterator, find(key), find(const key_type& key) const);
    R_WRAP(const_iterator, lower_bound(key), lower_bound(const key_type& key) const);
    R_WRAP(const_iterator, upper_bound(key), upper_bound(const key_type& key) const);
    R_WRAP(const_iterator_pair, equal_range(key), equal_range(const key_type& key) const);

    R_WRAP(iterator, begin(), begin());
    R_WRAP(iterator, end(), end());
    R_WRAP(iterator, find(key), find(const key_type& key));
    R_WRAP(iterator, lower_bound(key), lower_bound(const key_type& key));
    R_WRAP(iterator, upper_bound(key), upper_bound(const key_type& key));
    R_WRAP(iterator_pair, equal_range(key), equal_range(const key_type& key));

    W_WRAP(iterator_bool, insert(val), insert(const value_type& val));
    W_WRAP(iterator, insert(check(pos), val), insert(iterator pos, const value_type& val));
    W_WRAP_VOID(erase(check(pos)), erase(iterator pos));
    W_WRAP(size_type, erase(key), erase(const key_type& key));
    W_WRAP_VOID(clear(), clear());

    typename container_type::mapped_type& operator[](const key_type& key)
        {
            WLock();
            typename container_type::mapped_type& ret = m_Container[key];
            WUnlock();
            return ret;
        }
};


template<class Container>
class multimap_checker
{
    typedef Container container_type;
    typedef multimap_checker<Container> this_type;

public:
    typedef typename container_type::size_type size_type;
    typedef typename container_type::key_type key_type;
    typedef typename container_type::value_type value_type;
    typedef typename container_type::const_iterator const_iterator;
    typedef typename container_type::iterator iterator;

protected:
    typedef pair<const_iterator, const_iterator> const_iterator_pair;
    typedef pair<iterator, iterator> iterator_pair;
    typedef pair<iterator, bool> iterator_bool;

    container_type m_Container;
    mutable CAtomicCounter_WithAutoInit m_WCounter;
    mutable CAtomicCounter_WithAutoInit m_RCounter;

public:

    void RLock() const
        {
            if ( m_RCounter.Add(1) <= 0 || m_WCounter.Get() != 0 )
                abort();
        }
    void RUnlock() const
        {
            if ( m_WCounter.Get() != 0 || m_RCounter.Add(-1) < 0 )
                abort();
        }
    void WLock() const
        {
            if ( m_WCounter.Add(1) != 1 || m_RCounter.Get() != 0 )
                abort();
        }
    void WUnlock() const
        {
            if ( m_RCounter.Get() != 0 || m_WCounter.Add(-1) != 0 )
                abort();
        }

    iterator check(iterator pos)
        {
            if ( pos != m_Container.end() ) {
                iterator it = m_Container.find(pos->first);
                while ( it != m_Container.end() && it != pos && it->first == pos->first )
                    ++it;
                if ( it != pos )
                    abort();
            }
            return pos;
        }

    multimap_checker()
        {
        }
    ~multimap_checker()
        {
            WLock();
        }
    multimap_checker(const this_type& m)
        {
            *this = m;
        }
    this_type& operator=(const this_type& m)
        {
            WLock();
            m.RLock();
            m_Container = m.m_Container;
            m.RUnlock();
            WUnlock();
            return *this;
        }
    void swap(this_type& m)
        {
            WLock();
            m.WLock();
            m_Container.swap(m);
            m.WUnlock();
            WUnlock();
        }

    bool operator==(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = m_Container == m.m_Container;
            m.RUnlock();
            RUnlock();
            return ret;
        }
    bool operator<(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = m_Container < m.m_Container;
            m.RUnlock();
            RUnlock();
            return ret;
        }

    R_WRAP(size_type, size(), size() const);
    R_WRAP(bool, empty(), empty() const);

    R_WRAP(const_iterator, begin(), begin() const);
    R_WRAP(const_iterator, end(), end() const);
    R_WRAP(const_iterator, find(key), find(const key_type& key) const);
    R_WRAP(const_iterator, lower_bound(key), lower_bound(const key_type& key) const);
    R_WRAP(const_iterator, upper_bound(key), upper_bound(const key_type& key) const);
    R_WRAP(const_iterator_pair, equal_range(key), equal_range(const key_type& key) const);

    R_WRAP(iterator, begin(), begin());
    R_WRAP(iterator, end(), end());
    R_WRAP(iterator, find(key), find(const key_type& key));
    R_WRAP(iterator, lower_bound(key), lower_bound(const key_type& key));
    R_WRAP(iterator, upper_bound(key), upper_bound(const key_type& key));
    R_WRAP(iterator_pair, equal_range(key), equal_range(const key_type& key));

    W_WRAP(iterator, insert(val), insert(const value_type& val));
    W_WRAP(iterator, insert(check(pos), val), insert(iterator pos, const value_type& val));
    W_WRAP_VOID(erase(check(pos)), erase(iterator pos));
    W_WRAP(size_type, erase(key), erase(const key_type& key));
    W_WRAP_VOID(clear(), clear());
};


template<class Container>
class rangemultimap_checker : public multimap_checker<Container>
{
    typedef Container container_type;
public:
    typedef typename container_type::key_type key_type;
    typedef typename container_type::range_type range_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator constiterator;

    R_WRAP(iterator, begin(), begin());
    R_WRAP(const_iterator, begin(), begin() const);
    R_WRAP(iterator, begin(range), begin(const range_type& range));
    R_WRAP(const_iterator, begin(range), begin(const range_type& range) const);
};



#undef R_WRAP
#undef R_WRAP_VOID
#undef W_WRAP
#undef W_WRAP_VOID

template<typename Key, typename T, typename Compare = less<Key> >
class map : public map_checker< std::map<Key, T, Compare> >
{
};


template<typename Key, typename T, typename Compare = less<Key> >
class multimap : public multimap_checker< std::multimap<Key, T, Compare> >
{
};


END_NCBI_SCOPE

#endif//OBJMGR_MAP__HPP
