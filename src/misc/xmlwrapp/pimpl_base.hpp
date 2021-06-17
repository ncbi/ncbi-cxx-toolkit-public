/*
 * Copyright (C) 2008 Vaclav Slavik (vslavik@fastmail.fm)
 * All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$ 
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements. 
 */

#ifndef _xmlwrapp_pimpl_base_h_
#define _xmlwrapp_pimpl_base_h_

#ifdef HAVE_BOOST_POOL_SINGLETON_POOL_HPP
    #include <cassert>
    #include <boost/pool/singleton_pool.hpp>
#endif // HAVE_BOOST_POOL_SINGLETON_POOL_HPP

namespace xml
{

namespace impl
{

// Base class for all pimpl classes. Uses custom pool allocator for better
// performance. Usage: derive your class FooImpl from pimpl_base<FooImpl>.
template<typename T>
class pimpl_base
{
#ifdef HAVE_BOOST_POOL_SINGLETON_POOL_HPP
public:
    struct xmlwrapp_pool_tag {};

    // NB: we can't typedef the pool type as pimpl_base<T> subtype,
    //     because sizeof(T) is unknown (incomplete type) at this point,
    //     but it's OK to use the type in implementation of operators
    //     (compiled only when T, and so sizeof(T), is known)
    #define XMLWRAPP_PIMPL_ALLOCATOR_TYPE(T) \
        boost::singleton_pool<xmlwrapp_pool_tag, sizeof(T)>

    static void* operator new(size_t size)
    {
        assert( size == sizeof(T) );
        return XMLWRAPP_PIMPL_ALLOCATOR_TYPE(T)::malloc();
    }

    static void operator delete(void *ptr, size_t size)
    {
        assert( size == sizeof(T) );
        if ( ptr )
            XMLWRAPP_PIMPL_ALLOCATOR_TYPE(T)::free(ptr);
    }
#endif // HAVE_BOOST_POOL_SINGLETON_POOL_HPP
};

} // namespace impl

} // namespace xml

#endif // _xmlwrapp_pimpl_base_h_
