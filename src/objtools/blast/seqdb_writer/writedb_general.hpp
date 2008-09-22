#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GENERAL_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GENERAL_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_general.hpp
/// Implementation for general purpose utilities for WriteDB.
/// 
/// Defines classes:
///     CWriteDB_PackedStringsCompare
///     CWriteDB_PackedBuffer
///     CWriteDB_PackedStrings
///     CArrayString
///     CWriteDB_PackedSemiTree
/// 
/// Implemented for: UNIX, MS-Windows

#include <objects/seq/seq__.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// Divide by a number, rounding up to a whole integer.
inline int s_DivideRoundUp(int value, int blocksize)
{
    return ((value + (blocksize - 1)) / blocksize);
}

/// Round up to the next multiple of some number.
inline int s_RoundUp(int value, int blocksize)
{
    return s_DivideRoundUp(value, blocksize)*blocksize;
}

/// Comparator for sorting null terminated strings.
class CWriteDB_PackedStringsCompare {
public:
#ifdef NCBI_OS_SOLARIS
    /// Constructor.
    ///
    /// This definition silences a false-alarm warning on solaris.
    CWriteDB_PackedStringsCompare()
    {
    }
#endif // NCBI_OS_SOLARIS
    
    /// Compare two null terminated strings.
    bool operator()(const char * a, const char * b) const
    {
        return strcmp(a, b) < 0;
    }
};

/// Sortable packed array of null terminated strings.
///
/// This class holds NUL terminated string data for multiple strings.
/// Compared to a vector<string>, it is more efficient in terms of
/// both time and space, but it omits many general purpose features
/// such as iteration, deletion and modification of stored data.
template<int ALLOCSIZE>
class CWriteDB_PackedBuffer : public CObject {
public:
    /// Constructor
    CWriteDB_PackedBuffer()
    {
        m_Zero[0] = char(0);
    }
    
    /// Destructor
    ~CWriteDB_PackedBuffer()
    {
        Clear();
    }
    
    /// Insert a new string plus a NUL terminator.
    ///
    /// This inserts a string and returns the location of that string
    /// in the packed buffer.  The returned address will not change
    /// over the lifetime of this object.
    ///
    /// @param data The string data.
    /// @param length The length of the string data.
    /// @return A pointer to the packed string.
    const char * Insert(const char * data, int length)
    {
        if (m_Packed.empty()) {
            x_AddBlock();
        }
        
        string * back = m_Packed.back();
        
        if ((back->size() + length + 1) > back->capacity()) {
            x_AddBlock();
            back = m_Packed.back();
        }
        
        const char * rv = back->data() + back->size();
        back->append(data, length);
        back->append(& m_Zero[0], 1);
        
        return rv;
    }
    
    /// Free all held memory.
    void Clear()
    {
        vector<string*> p2;
        m_Packed.swap(p2);
        
        NON_CONST_ITERATE(vector<string*>, iter, p2) {
            string * pstr = *iter;
            delete pstr;
            *iter = NULL;
        }
    }
    
private:
    /// This object type.
    typedef CWriteDB_PackedBuffer<ALLOCSIZE> TThis;
    
    /// Prevent copy constructor.
    CWriteDB_PackedBuffer(const TThis & other);
    
    /// Prevent copy operator.
    TThis & operator=(const TThis & other);
    
    /// Add an block of ALLOCSIZE bytes to the sorted string vector.
    void x_AddBlock()
    {
        string * p = 0;
        
        try {
            p = new string;
            int blksz = ALLOCSIZE;
            p->reserve(blksz);
        }
        catch(...) {
            delete p;
            throw;
        }
        
        m_Packed.push_back(p);
    }
    
    /// Array of pointers to the start of the strings stored here.
    vector<string*> m_Packed;
    
    /// Array containing NUL byte for convenience.
    char m_Zero[1];
};

/// Sortable packed array of strings, optimized for space.
///
/// This stores a sortable packed collection of strings.  Features
/// here are minimal and targeted for the ISAM code, which follows a
/// simple (input, sort, output) data processing pattern.

template<int ALLOCSIZE>
class CWriteDB_PackedStrings : public CObject {
public:
    /// Constructor
    CWriteDB_PackedStrings(CWriteDB_PackedBuffer<ALLOCSIZE> & buffer)
        : m_Buffer(buffer)
    {
    }
    
    /// Destructor
    ~CWriteDB_PackedStrings()
    {
    }
    
    /// Insert string data - must be null terminated.
    void Insert(const char * x, int length)
    {
        m_KeyLoc.push_back(m_Buffer.Insert(x, length));
    }
    
    /// Sort the keyloc array (in place).
    void Sort()
    {
        CWriteDB_PackedStringsCompare cmp;
        std::sort(m_KeyLoc.begin(), m_KeyLoc.end(), cmp);
    }
    
    /// Sort the keyloc array (in place).
    int Size()
    {
        return m_KeyLoc.size();
    }
    
    /// Get the list of null terminated strings.
    const vector<const char *> & GetList()
    {
        return m_KeyLoc;
    }
    
private:
    /// This object type.
    typedef CWriteDB_PackedStrings<ALLOCSIZE> TThis;
    
    /// Prevent copy constructor.
    CWriteDB_PackedStrings(const TThis & other);
    
    /// Prevent assignment.
    TThis & operator=(const TThis & other);
    
    /// Reference to shared packed-buffer object.
    CWriteDB_PackedBuffer<ALLOCSIZE> & m_Buffer;
    
    /// Pointers to the beginning of the strings stored here.
    vector<const char *> m_KeyLoc;
};

/// Fixed-buffer string type.
///
/// This type stores a string in a fixed buffer.  The string length
/// may be up to the specific size (the string is not NUL terminated
/// in the internal array).  Compared to std::string, this type is
/// compact, more cache-aware, and does not use dynamic allocation.
/// It cannot handle embedded NULs or strings larger than STR_SIZE.

template<int STR_SIZE>
class CArrayString {
public:
    /// Constructor.
    CArrayString()
    {
        memset(m_Data, 0, STR_SIZE);
    }
    
    /// Copy constructor.
    CArrayString(const CArrayString<STR_SIZE> & other)
    {
        memcpy(m_Data, other.m_Data, STR_SIZE);
    }
    
    /// Construct from a string in a memory location.
    CArrayString(const char * x, int L)
    {
        assert(L <= STR_SIZE);
        memcpy(m_Data, x, L);
        if (L < STR_SIZE) {
            m_Data[L] = 0;
        }
    }
    
    /// Compare two strings (lexicographically).
    /// @param other Another string against which to compare.
    /// @return -1, 0, or 1 if string is less, equal, or greater than other.
    int Cmp(const CArrayString<STR_SIZE> & other) const
    {
        for(int i = 0; i<STR_SIZE; i++) {
            char ch1 = m_Data[i], ch2 = other.m_Data[i];
            
            if (ch1 < ch2)
                return -1;
            
            if (ch1 > ch2)
                return 1;
            
            if ((! ch1) && (! ch2))
                break;
        }
        return 0;
    }
    
    /// Return true if this string is less than 'other'.
    /// @param other Another string against which to compare.
    /// @return True if this string is less than other.
    bool operator <(const CArrayString<STR_SIZE> & other) const
    {
        return Cmp(other) < 0;
    }
    
    /// Return true if this string is less than 'other'.
    /// @param other Another string against which to compare.
    /// @return True if this string is equal to other.
    bool operator==(const CArrayString<STR_SIZE> & other) const
    {
        return Cmp(other) == 0;
    }
    
    /// Assign this string from another.
    /// @param other Another string to assign from.
    CArrayString<STR_SIZE>& operator=(const CArrayString<STR_SIZE> & other)
    {
        memcpy(m_Data, other.m_Data, STR_SIZE);
        return *this;
    }
    
    /// Get a pointer to the start of this string's data.
    /// @return A pointer to the start of this string's data.
    const char * Data() const
    {
        return & m_Data[0];
    }
    
    /// Get this string's length.
    /// @return This string's length.
    int Size() const
    {
        for(int i = 0; i<STR_SIZE; i++) {
            if (! m_Data[i])
                break;
        }
        return STR_SIZE;
    }
    
private:
    /// Data for this string, NUL terminated iff less than STR_SIZE bytes.
    char m_Data[STR_SIZE];
};


/// Packed string data container with sorting and iteration.
/// 
/// This class efficiently stores a packed array of string data.
/// Strings can be added, sorted, and iterated over.  The actual data
/// is stored in packed buffers, but the beginning of each string is
/// removed and used as a first level index to a packed list of the
/// strings (minus this first part).  This reduces the total string
/// storage required, because each string is shorter, and makes the
/// final sorting easier (because each subset of the string table is
/// partially sorted and comparisons are done with shorter strings).
class CWriteDB_PackedSemiTree {
public:
    // The capacity() of a string is not necessarily what was provided
    // to std::string::reserve(int).  Instead, reserve can provide
    // extra space to shift the capacity to something that matches the
    // underlying new or malloc implementation.  The value 65000 here
    // is designed to get approximately 64K.  Since C++ string buffers
    // are normally allocated with metadata prefixes, asking for 2^16
    // bytes is unlikely to result in clean 64k block allocations.
    
    enum {
        /// This is the number of bytes for the first level index.
        PREFIX = 6,
        
        /// This is the amount of string capacity to request.
        BLOCK = 65000
    };
    
    /// A packed list of buffers.
    typedef CWriteDB_PackedStrings<BLOCK> TPacked;
    
    /// A map from the string prefixes to a packed buffer.
    typedef map< CArrayString<PREFIX>, CRef<TPacked> > TPackedMap;
    
    /// Constructor.
    CWriteDB_PackedSemiTree()
        : m_Size(0)
    {
    }
    
    /// Destructor.
    ~CWriteDB_PackedSemiTree()
    {
        Clear();
    }
    
    /// Insert string data into the container.
    void Insert(const char * x, int L);
    
    /// Sort all contained data.
    void Sort();
    
    /// Return the number of contained entries.
    int Size() const
    {
        return m_Size;
    }
    
    /// Class providing iteration over string data.
    ///
    /// This class assumes all PackedStrings objects are non-empty.
    class Iterator {
    public:
        /// Create an iterator.
        ///
        /// The specified TPacked iterator will normally point to the
        /// first TPacked object (in which case iteration will start
        /// there) or to pile::end(), in which case this is the end.
        ///
        /// @param The total string collection.
        /// @param i The PackedString object to start with.
        Iterator(TPackedMap & pile, TPackedMap::iterator i)
            : m_Packed (pile),
              m_Pos1 (i),
              m_Pos2 (0)
        {
        }
        
        /// Move forward one item (prefix).
        Iterator & operator++()
        {
            if (m_Pos1 != m_Packed.end()) {
                m_Pos2 ++;
                
                if (m_Pos2 >= m_Pos1->second->Size()) {
                    m_Pos1 ++;
                    m_Pos2 = 0;
                }
            }
            return *this;
        }
        
        /// Compare two iterators for equality.
        /// @param Iterator to compare this iterator to.
        /// @return true If the iterators are equal.
        bool operator ==(Iterator & other)
        {
            return (m_Pos1 == other.m_Pos1 &&
                    m_Pos2 == other.m_Pos2);
        }
        
        /// Compare two iterators for inequality.
        /// @param Iterator to compare this iterator to.
        /// @return true If the iterators are unequal.
        bool operator !=(Iterator & other)
        {
            return ! ((*this) == other);
        }
        
        /// Get the string pointed to by this iterator.
        ///
        /// The C++ tradition of using operator*() for this task is
        /// not followed here, because this string needs to be pieced
        /// together, and that would require a string allocation for
        /// each returned item.  Instead, I allow the user to pass in
        /// a string, allowing the calling code to use a single string
        /// for all allocations.
        ///
        /// @param data The returned string.
        void Get(string & data)
        {
            _ASSERT(m_Pos1 != m_Packed.end());
            
            data.resize(0);
            data.append(m_Pos1->first.Data(), m_Pos1->first.Size());
            data.append(m_Pos1->second->GetList()[m_Pos2]);
        }
        
    private:
        /// The CWriteDB_PackedString container.
        TPackedMap & m_Packed;
        
        /// The iterator for the current TPacked object.
        TPackedMap::iterator m_Pos1;
        
        /// An integer to iterate within the TPacked object.
        int m_Pos2;
    };
    
    /// Get an iterator to the beginning of this collection.
    Iterator Begin()
    {
        return Iterator(m_Packed, m_Packed.begin());
    }
    
    /// Get an iterator to the end of this collection.
    Iterator End()
    {
        return Iterator(m_Packed, m_Packed.end());
    }
    
    /// Clear all objects from this container.
    void Clear();
    
private:
    /// Number of elements stored in this container.
    int m_Size;
    
    /// Map of string prefixes to packed string lists.
    TPackedMap m_Packed;
    
    /// Shared list of packed string buffers.
    CWriteDB_PackedBuffer<BLOCK> m_Buffer;
};

/// Compute length of sequence from raw packing.
/// @param protein Specify true for protein formats, false for nucleotide.
/// @param seq Sequence data (in na2 format for nucletide).
int WriteDB_FindSequenceLength(bool protein, const string & seq);

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GENERAL_HPP


