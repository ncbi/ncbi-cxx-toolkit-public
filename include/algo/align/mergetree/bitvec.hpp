#ifndef BIT_VEC__HPP
#define BIT_VEC__HPP
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
* Authors: Natha Bouk
*
* File Description:
*   vector for bits
*
* ===========================================================================
*/

#include <vector>

template<class T>
class bitvec
{
    typedef std::vector<T> back_type;
    back_type backend;
public:
    bitvec() : backend(sizeof(T)*CHAR_BIT, 0) { ; } 
    bitvec(size_t length) : backend(length,0) { ; }


    bool get(size_t index) const {
        size_t back_index  = index / (sizeof(T)*CHAR_BIT);
        size_t shift_index = index % (sizeof(T)*CHAR_BIT);
        
        if(back_index >= backend.size()) {
            return false;
        }
        //std::cerr << "get: " << index << "(" << sizeof(T)*CHAR_BIT << ")" << " => " 
        //<< back_index << "," << shift_index << " => "
        //<< backend[back_index] << " & ";

        T mask = (1 << shift_index);
        bool value = ((backend[back_index] & mask) > 0);
        
        //std::cerr << mask << " => " << (backend[back_index]&mask) << " => " << value  << std::endl;
        return value;
    }
    

    void set(size_t index) {
        set(index, true);
    }
    void unset(size_t index) {
        set(index, false);
    }
    void set(size_t index, bool value) {
        size_t back_index  = index / (sizeof(T)*CHAR_BIT);
        size_t shift_index = index % (sizeof(T)*CHAR_BIT);

        if(back_index >= backend.size()) {
            size_t newsize = back_index;
            if(backend.size() > newsize)
                newsize = backend.size();
            newsize *= 2;
            backend.reserve(newsize);
            backend.resize(newsize, 0);
        }
        //std::cerr << "set: " << index << "(" << sizeof(T)*CHAR_BIT << ")" << " => " 
        //<< back_index << "," << shift_index << " => "
        //<< backend[back_index] << " => ";

        T mask = (1 << shift_index);
   
        if(value) {
            backend[back_index] |= mask;
        } else {
            mask = ~mask;
            backend[back_index] &= mask;
        }        
        //std::cerr << backend[back_index] << std::endl;
    }
   
    void clear() { 
        typename back_type::iterator i = backend.begin();
        while(i != backend.end()) { 
            *i = 0;
            ++i;
        }
    }

    size_t size() {
        return (backend.size() * CHAR_BIT);
    }

private:

};
//typedef bitvec<unsigned int> bitvec;

#endif // end BIT_VEC__HPP

