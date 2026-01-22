#ifndef CORELIB___ATOMIC_UTIL__HPP
#define CORELIB___ATOMIC_UTIL__HPP

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
 * Author:  Eugene Vasilchenko, Pavel Ivanov
 *
 *
 */

/// @file atomic_util.hpp
/// Extensions to std::atomic<>


#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>
#include <atomic>


BEGIN_NCBI_NAMESPACE;


// A version of std::atomic<> that allow copy and assignment.
template<class C>
class copyable_atomic : public std::atomic<C>
{
public:
    copyable_atomic() noexcept = default;
    constexpr copyable_atomic(C value) noexcept
        : std::atomic<C>(value)
        {}

    constexpr copyable_atomic(const copyable_atomic<C>& value) noexcept
        : copyable_atomic(value.load(std::memory_order_acquire))
        {}

    copyable_atomic& operator=(const copyable_atomic<C>& value) noexcept
        {
            this->store(value.load(std::memory_order_acquire), std::memory_order_release);
            return *this;
        }
};

// A version of std::atomic<> with relaxed memory order that allow copy and assignment.
template<class C>
class copyable_relaxed_atomic : public std::atomic<C>
{
public:
    copyable_relaxed_atomic() noexcept = default;
    constexpr copyable_relaxed_atomic(C value) noexcept
        : std::atomic<C>(value)
        {}

    constexpr copyable_relaxed_atomic(const copyable_relaxed_atomic<C>& value) noexcept
        : std::atomic<C>(value.load())
        {}

    operator C() const noexcept
        {
            return load();
        }

    C operator=(C value) noexcept
        {
            store(value);
            return value;
        }
    
    void store(C value) noexcept
        {
            std::atomic<C>::store(value, std::memory_order_relaxed);
        }

    C load() const noexcept
        {
            return std::atomic<C>::load(memory_order_relaxed);
        }
    
    copyable_relaxed_atomic& operator=(const copyable_relaxed_atomic<C>& value) noexcept
        {
            store(value.load());
            return *this;
        }
};


// Simplified reference to CObject-derived objects with atomic operations
template<class C>
class COneTimeSetRef
{
public:
    typedef C element_type;             ///< Define alias element_type
    typedef element_type TObjectType;   ///< Define alias TObjectType
    
    /// Default constructor with null initialization
    COneTimeSetRef()
        : m_Ptr(0)
        {
        }
    /// Constructor for explicit type conversion from pointer to object.
    explicit COneTimeSetRef(TObjectType* ptr)
        : m_Ptr(ptr)
        {
            if ( ptr ) {
                ptr->AddReference();
            }
        }
    /// Copy constructor
    explicit COneTimeSetRef(const COneTimeSetRef& ref)
        : COneTimeSetRef(ref.GetNCPointerOrNull())
        {
        }
    /// Assignment
    COneTimeSetRef& operator=(const COneTimeSetRef& ref)
        {
            Reset(ref.GetNonNullNCPointer());
            return *this;
        }
    
    /// Destructor.
    ~COneTimeSetRef(void)
        {
            if ( TObjectType* oldPtr = m_Ptr.exchange(nullptr, memory_order_release) ) {
                oldPtr->RemoveReference();
            }
        }

    DECLARE_OPERATOR_BOOL(NotNull());

    /// Check if pointer is null -- same effect as Empty().
    ///
    /// @sa
    ///   Empty(), operator!()
    bool IsNull(void) const noexcept
        {
            return GetPointerOrNull() == nullptr;
        }

    /// Check if pointer is not null -- same effect as NotEmpty().
    ///
    /// @sa
    ///   NotEmpty()
    bool NotNull(void) const noexcept
        {
            return !IsNull();
        }

    /// Check if CRef is empty -- not pointing to any object, which means
    /// having a null value. 
    ///
    /// @sa
    ///   IsNull(), operator!()
    bool Empty(void) const noexcept
        {
            return IsNull();
        }

    /// Check if CRef is not empty -- pointing to an object and has
    /// a non-null value. 
    bool NotEmpty(void) const noexcept
        {
            return NotNull();
        }

    /// Reset reference object to new pointer
    ///
    /// This sets the pointer to object to the new pointer.
    /// The COneTimeSetRef<> must be null before, and the newPtr must be non-null.
    inline
    void Reset(TObjectType* newPtr)
        {
            newPtr->AddReference();
            TObjectType* oldPtr = nullptr;
            if ( !m_Ptr.compare_exchange_strong(oldPtr, newPtr,
                                                memory_order_release, memory_order_relaxed) ) {
                // the new pointer wasn't assigned because the COneTimeSetRef was set already
                // rollback the reference counter
                newPtr->RemoveReference();
                // report error if new pointer is different from already assigned
                if ( oldPtr != newPtr ) {
                    NCBI_THROW(CCoreException, eInvalidArg,
                               "COneTimeSetRef::Reset: "
                               "double set with different pointers");
                }
            }
        }
    
    /// Get pointer value and throw a null pointer exception if pointer
    /// is null.
    ///
    /// Similar to GetPointerOrNull() except that this method throws a null
    /// pointer exception if pointer is null, whereas GetPointerOrNull()
    /// returns a null value.
    ///
    /// @sa
    ///   GetPointerOrNull(), GetPointer(), GetObject()
    inline
    TObjectType* GetNonNullPointer(void)
        {
            auto ptr = GetPointerOrNull();
            if ( !ptr ) {
                CObject::ThrowNullPointerException();
            }
            return ptr;
        }

    /// Get pointer value and throw a null pointer exception if pointer
    /// is null.
    ///
    /// Similar to GetPointerOrNull() except that this method throws a null
    /// pointer exception if pointer is null, whereas GetPointerOrNull()
    /// returns a null value.
    ///
    /// @sa
    ///   GetPointerOrNull(), GetPointer(), GetObject()
    inline
    const TObjectType* GetNonNullPointer(void) const
        {
            auto ptr = GetPointerOrNull();
            if ( !ptr ) {
                CObject::ThrowNullPointerException();
            }
            return ptr;
        }

    /// Get pointer value and throw a null pointer exception if pointer
    /// is null.
    ///
    /// Similar to GetPointerOrNull() except that this method throws a null
    /// pointer exception if pointer is null, whereas GetPointerOrNull()
    /// returns a null value.
    ///
    /// @sa
    ///   GetPointerOrNull(), GetPointer(), GetObject()
    inline
    TObjectType* GetNonNullNCPointer(void) const
        {
            auto ptr = GetPointerOrNull();
            if ( !ptr ) {
                CObject::ThrowNullPointerException();
            }
            return ptr;
        }

    /// Get pointer value.
    ///
    /// Similar to GetNonNullPointer() except that this method returns a null
    /// if the pointer is null, whereas GetNonNullPointer() throws a null
    /// pointer exception.
    ///
    /// @sa
    ///   GetNonNullPointer()
    inline
    TObjectType* GetPointerOrNull(void) noexcept
        {
            return x_GetPtr();
        }

    /// Get pointer value.
    ///
    /// Similar to GetNonNullPointer() except that this method returns a null
    /// if the pointer is null, whereas GetNonNullPointer() throws a null
    /// pointer exception.
    ///
    /// @sa
    ///   GetNonNullPointer()
    inline
    TObjectType* GetNCPointerOrNull(void) const noexcept
        {
            return x_GetPtr();
        }

    /// Get pointer,
    ///
    /// Same as GetPointerOrNull().
    ///
    /// @sa
    ///   GetPointerOrNull()
    inline
    TObjectType* GetNCPointer(void) const noexcept
        {
            return GetNCPointerOrNull();
        }

    /// Get object.
    ///
    /// Similar to GetNonNullPointer(), except that this method returns the
    /// object whereas GetNonNullPointer() returns a pointer to the object.
    /// 
    /// @sa
    ///   GetNonNullPointer()
    inline
    TObjectType& GetNCObject(void) const
        {
            return *GetNonNullNCPointer();
        }

    /// Get pointer value.
    ///
    /// Similar to GetNonNullPointer() except that this method returns a null
    /// if the pointer is null, whereas GetNonNullPointer() throws a null
    /// pointer exception.
    ///
    /// @sa
    ///   GetNonNullPointer()
    inline
    const TObjectType* GetPointerOrNull(void) const noexcept
        {
            return x_GetPtr();
        }

    /// Get pointer,
    ///
    /// Same as GetPointerOrNull().
    ///
    /// @sa
    ///   GetPointerOrNull()
    inline
    TObjectType* GetPointer(void) noexcept
        {
            return GetPointerOrNull();
        }

    /// Get pointer,
    ///
    /// Same as GetPointerOrNull().
    ///
    /// @sa
    ///   GetPointerOrNull()
    inline
    const TObjectType* GetPointer(void) const noexcept
        {
            return GetPointerOrNull();
        }

    /// Get object.
    ///
    /// Similar to GetNonNullPointer(), except that this method returns the
    /// object whereas GetNonNullPointer() returns a pointer to the object.
    /// 
    /// @sa
    ///   GetNonNullPointer()
    inline
    TObjectType& GetObject(void)
        {
            return *GetNonNullPointer();
        }

    /// Get object.
    ///
    /// Similar to GetNonNullPointer(), except that this method returns the
    /// object whereas GetNonNullPointer() returns a pointer to the object.
    /// 
    /// @sa
    ///   GetNonNullPointer()
    inline
    const TObjectType& GetObject(void) const
        {
            return *GetNonNullPointer();
        }

    /// Dereference operator returning object.
    ///
    /// @sa
    ///   GetObject()
    inline
    TObjectType& operator*(void)
        {
            return *GetNonNullPointer();
        }
    
    /// Dereference operator returning object.
    ///
    /// @sa
    ///   GetObject()
    inline
    const TObjectType& operator*(void) const
        {
            return *GetNonNullPointer();
        }
    
    /// Reference operator.
    ///
    /// @sa
    ///   GetNonNullPointer()
    inline
    TObjectType* operator->(void)
        {
            return GetNonNullPointer();
        }
    
    /// Reference operator.
    ///
    /// @sa
    ///   GetNonNullPointer()
    inline
    const TObjectType* operator->(void) const
        {
            return GetNonNullPointer();
        }

    operator CRef<TObjectType>() const
        {
            return CRef<TObjectType>(GetNCPointerOrNull());
        }
    
    /// Assignment operator for references.
    COneTimeSetRef& operator=(const CRef<TObjectType>& ref)
        {
            Reset(ref.GetNonNullNCPointer());
            return *this;
        }

    /// Assignment operator for references.
    COneTimeSetRef& operator=(TObjectType* ptr)
        {
            Reset(ptr);
            return *this;
        }

protected:
    TObjectType* x_GetPtr() const noexcept
        {
            return m_Ptr.load(memory_order_acquire);
        }
private:
    atomic<TObjectType*> m_Ptr;
};


END_NCBI_NAMESPACE;

#endif /* CORELIB___ATOMIC_UTIL__HPP */
