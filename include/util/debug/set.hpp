#ifndef OBJMGR_SET__HPP
#define OBJMGR_SET__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbicntr.hpp>
#include <set>

BEGIN_NCBI_SCOPE

#define R_WRAP(Type, Call, Declaration) \
    Type Declaration \
        { \
            Type ret; \
            RLock(); \
            ret = parent_type::Call; \
            RUnlock(); \
            return ret; \
        }
#define R_WRAP_VOID(Call, Declaration) \
    void Declaration \
        { \
            RLock(); \
            parent_type::Call; \
            RUnlock(); \
        }
#define W_WRAP(Type, Call, Declaration) \
    Type Declaration \
        { \
            Type ret; \
            WLock(); \
            ret = parent_type::Call; \
            WUnlock(); \
            return ret; \
        }
#define W_WRAP_VOID(Call, Declaration) \
    void Declaration \
        { \
            WLock(); \
            parent_type::Call; \
            WUnlock(); \
        }

template<typename Key, typename Compare = less<Key> >
class set : std::set<Key, Compare>
{
    typedef std::set<Key, Compare> parent_type;
    typedef checked_set<Key, Compare> this_type;
    typedef pair<typename parent_type::const_iterator,
                 typename parent_type::const_iterator> const_iterator_pair;
    typedef pair<typename parent_type::iterator,
                 typename parent_type::iterator> iterator_pair;
    typedef pair<typename parent_type::iterator, bool> iterator_bool;

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

public:
    typedef typename parent_type::size_type size_type;
    typedef typename parent_type::key_type key_type;
    typedef typename parent_type::value_type value_type;
    typedef typename parent_type::const_iterator const_iterator;
    typedef typename parent_type::iterator iterator;

    checked_set()
        {
        }
    ~checked_set()
        {
            WLock();
        }
    checked_set(const this_type& m)
        {
            *this = m;
        }
    this_type& operator=(const this_type& m)
        {
            WLock();
            m.RLock();
            parent_type::operator=(m);
            m.RUnlock();
            WUnlock();
            return *this;
        }
    void swap(this_type& m)
        {
            WLock();
            m.WLock();
            parent_type::swap(m);
            m.WUnlock();
            WUnlock();
        }

    bool operator==(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = parent_type::operator==(m);
            m.RUnlock();
            RUnlock();
            return ret;
        }
    bool operator<(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = parent_type::operator<(m);
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
    W_WRAP(iterator, insert(pos, val), insert(iterator pos, const value_type& val));
    W_WRAP_VOID(erase(pos), erase(iterator pos));
    W_WRAP(size_type, erase(key), erase(const key_type& key));
    W_WRAP_VOID(clear(), clear());

private:
    mutable CAtomicCounter_WithAutoInit m_WCounter;
    mutable CAtomicCounter_WithAutoInit m_RCounter;
};


template<typename Key, typename Compare = less<Key> >
class multiset : std::multiset<Key, Compare>
{
    typedef std::multiset<Key, Compare> parent_type;
    typedef checked_multiset<Key, Compare> this_type;
    typedef pair<typename parent_type::const_iterator,
                 typename parent_type::const_iterator> const_iterator_pair;
    typedef pair<typename parent_type::iterator,
                 typename parent_type::iterator> iterator_pair;
    typedef pair<typename parent_type::iterator, bool> iterator_bool;

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

public:
    typedef typename parent_type::size_type size_type;
    typedef typename parent_type::key_type key_type;
    typedef typename parent_type::value_type value_type;
    typedef typename parent_type::const_iterator const_iterator;
    typedef typename parent_type::iterator iterator;

    checked_multiset()
        {
        }
    ~checked_multiset()
        {
            WLock();
        }
    checked_multiset(const this_type& m)
        {
            *this = m;
        }
    this_type& operator=(const this_type& m)
        {
            WLock();
            m.RLock();
            parent_type::operator=(m);
            m.RUnlock();
            WUnlock();
            return *this;
        }
    void swap(this_type& m)
        {
            WLock();
            m.WLock();
            parent_type::swap(m);
            m.WUnlock();
            WUnlock();
        }

    bool operator==(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = parent_type::operator==(m);
            m.RUnlock();
            RUnlock();
            return ret;
        }
    bool operator<(const this_type& m) const
        {
            bool ret;
            RLock();
            m.RLock();
            ret = parent_type::operator<(m);
            m.RUnlock();
            RUnlock();
            return ret;
        }

#define R_WRAP(Type, Call, Declaration) \
    Type Declaration \
        { \
            Type ret; \
            RLock(); \
            ret = parent_type::Call; \
            RUnlock(); \
            return ret; \
        }
#define R_WRAP_VOID(Call, Declaration) \
    void Declaration \
        { \
            RLock(); \
            parent_type::Call; \
            RUnlock(); \
        }
#define W_WRAP(Type, Call, Declaration) \
    Type Declaration \
        { \
            Type ret; \
            WLock(); \
            ret = parent_type::Call; \
            WUnlock(); \
            return ret; \
        }
#define W_WRAP_VOID(Call, Declaration) \
    void Declaration \
        { \
            WLock(); \
            parent_type::Call; \
            WUnlock(); \
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
    W_WRAP(iterator, insert(pos, val), insert(iterator pos, const value_type& val));
    W_WRAP_VOID(erase(pos), erase(iterator pos));
    W_WRAP(size_type, erase(key), erase(const key_type& key));
    W_WRAP_VOID(clear(), clear());

private:
    mutable CAtomicCounter_WithAutoInit m_WCounter;
    mutable CAtomicCounter_WithAutoInit m_RCounter;
};

#undef R_WRAP
#undef R_WRAP_VOID
#undef W_WRAP
#undef W_WRAP_VOID

END_NCBI_SCOPE

#endif//OBJMGR_SET__HPP
