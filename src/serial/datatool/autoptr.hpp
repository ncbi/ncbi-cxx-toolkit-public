#ifndef AUTOPTR_HPP
#define AUTOPTR_HPP

template<typename T>
class AutoPtr {
public:
    AutoPtr(T* p = 0)
        : ptr(p)
        {
        }
    AutoPtr(const AutoPtr& p)
        : ptr(p.ptr)
        {
            p.ptr = 0;
        }

    ~AutoPtr()
        {
            if ( ptr )
                delete ptr;
        }

    AutoPtr& operator=(const AutoPtr& p)
        {
            if ( ptr != p.ptr ) {
                delete ptr;
                ptr = p.ptr;
                p.ptr = 0;
            }
            return *this;
        }
    AutoPtr& operator=(T* p)
        {
            if ( ptr != p ) {
                delete ptr;
                ptr = p;
            }
            return *this;
        }

    T& operator*() const
        {
            return *ptr;
        }
    T* operator->() const
        {
            return ptr;
        }
    
    T* get() const
        {
            return ptr;
        }
    T* release() const
        {
            T* ret = ptr;
            ptr = 0;
            return ret;
        }

    operator bool() const
        {
            return ptr != 0;
        }

private:
    mutable T* ptr;
};

template<typename T1, typename T2>
inline
void Assign(AutoPtr<T1>& t1, const AutoPtr<T2>& t2)
{
    t1 = t2.release();
}

#define iterate(Type, Var, Cont) \
    for ( typename Type::const_iterator Var = Cont.begin(); Var != Cont.end(); ++Var )
#define non_const_iterate(Type, Var, Cont) \
    for ( typename Type::iterator Var = Cont.begin(); Var != Cont.end(); ++Var )

#endif
