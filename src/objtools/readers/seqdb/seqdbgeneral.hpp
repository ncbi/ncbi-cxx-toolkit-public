#ifndef CORELIB__SEQDB__SEQDBGENERAL_HPP
#define CORELIB__SEQDB__SEQDBGENERAL_HPP

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


/// @file seqdbgeneral.hpp
/// This file defines several SeqDB utility functions related to byte
/// order and file system portability.
/// Implemented for: UNIX, MS-Windows


#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <corelib/ncbi_bswap.hpp>
#include "seqdbatlas.hpp"
#include <map>

BEGIN_NCBI_SCOPE

// Byte-order-nonspecific (long) versions

template<typename T>
inline T SeqDB_GetStdOrdUnaligned(const T * stdord_obj)
{
#ifdef WORDS_BIGENDIAN
    unsigned char * stdord =
	(unsigned char*)(stdord_obj);
    
    unsigned char * pend = stdord + sizeof(T) - 1;
    unsigned char * pcur = stdord;
    
    T retval = *pcur;
    
    while(pcur < pend) {
	retval <<= 8;
	retval += *++pcur;
    }
    
    return retval;
#else
    if (sizeof(T) == 8) {
        return (T) CByteSwap::GetInt8((unsigned char *) stdord_obj);
    } else if (sizeof(T) == 4) {
        return (T) CByteSwap::GetInt4((unsigned char *) stdord_obj);
    } else if (sizeof(T) == 2) {
        return (T) CByteSwap::GetInt2((unsigned char *) stdord_obj);
    }
    
    _ASSERT(sizeof(T) == 1);
    
    return *stdord_obj;
#endif
}

template<typename T>
inline T SeqDB_GetBrokenUnaligned(const T * stdord_obj)
{
    unsigned char * stdord =
	(unsigned char*)(stdord_obj);
    
    unsigned char * pend = stdord;
    unsigned char * pcur = stdord + sizeof(T) - 1;
    
    T retval = *pcur;
    
    while(pcur > pend) {
	retval <<= 8;
	retval += *--pcur;
    }
    
    return retval;
}

// Macro Predicates for binary qualities

#define IS_POWER_OF_TWO(x)    (((x) & ((x)-1)) == 0)
#define ALIGNED_TO_POW2(x,y)  (! ((x) & (0-y)))

#define PTR_ALIGNED_TO_SELF_SIZE(x) \
    (IS_POWER_OF_TWO(sizeof(*x)) && ALIGNED_TO_POW2((size_t)(x), sizeof(*x)))


// Portable byte swapping from marshalled version

#ifdef WORDS_BIGENDIAN

template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    if (PTR_ALIGNED_TO_SELF_SIZE(stdord_obj)) {
        return *stdord_obj;
    } else {
        return SeqDB_GetStdOrdUnaligned(stdord_obj);
    }
}

template<typename T>
inline T SeqDB_GetBroken(const T * stdord_obj)
{
    return SeqDB_GetBrokenUnaligned(stdord_obj);
}

#else

template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    return SeqDB_GetStdOrdUnaligned(stdord_obj);
}

template<typename T>
inline T SeqDB_GetBroken(const T * stdord_obj)
{ 
    if (PTR_ALIGNED_TO_SELF_SIZE(stdord_obj)) {
        return *stdord_obj;
    } else {
        return SeqDB_GetBrokenUnaligned(stdord_obj);
    }
}

#endif


/// Higher Performance String Assignment
/// 
/// It looks like the default assignments and modifiers (ie insert,
/// operator = and operator +=) for strings do not use the capacity
/// doubling algorithm such as vector::push_back() uses.  For our
/// purposes, they sometimes should.  The following assignment
/// function provides this.

inline void
s_SeqDB_QuickAssign(string & dst, const char * bp, const char * ep)
{
    size_t length = ep - bp;
    
    if (dst.capacity() < length) {
        size_t increment = 16;
        size_t newcap = dst.capacity() ? dst.capacity() : increment;
        
        while(length > newcap) {
            newcap <<= 1;
        }
        
        dst.reserve(newcap);
    }
    
    dst.assign(bp, ep);
}


/// Higher Performance String Assignment
/// 
/// String to string version, using the above function.  I use the
/// (char*,char*) version of string::assign() since I've found that it
/// does not discard excess capacity.

inline void
s_SeqDB_QuickAssign(string & dst, const string & src)
{
    s_SeqDB_QuickAssign(dst, src.data(), src.data() + src.size());
}


/// String slicing
///
/// This class describes part of an existing (C++ or C) string as a
/// memory range, and provides a limited set of read-only string
/// operations over it.  In the common case where parts of several
/// string are found and spliced together, this class represents the
/// temporary sub-strings.  It does not deal with ownership or
/// lifetime issues, so it should not be stored in a structure that
/// will outlast the original string.  It never allocates and never
/// frees.  In writing this, I only implemented the features that are
/// used somewhere in SeqDB; adding new features is fairly trivial.

class CSeqDB_Substring {
public:
    CSeqDB_Substring()
        : m_Begin(0), m_End(0)
    {
    }
    
    explicit CSeqDB_Substring(const char * s)
        : m_Begin(s)
    {
        m_End = s + strlen(s);
    }
    
    explicit CSeqDB_Substring(const string & s)
        : m_Begin(s.data()), m_End(s.data() + s.size())
    {
    }
    
    CSeqDB_Substring(const char * b, const char * e)
        : m_Begin(b), m_End(e)
    {
    }
    
    void GetString(string & s) const
    {
        if (m_Begin != m_End) {
            s.assign(m_Begin, m_End);
        } else {
            s.clear();
        }
    }
    
    void GetStringQuick(string & s) const
    {
        if (m_Begin != m_End) {
            s_SeqDB_QuickAssign(s, m_Begin, m_End);
        } else {
            s.clear();
        }
    }
    
    int FindLastOf(char ch) const
    {
        for (const char * p = m_End - 1;  p >= m_Begin;  --p) {
            if (*p == ch) {
                return p - m_Begin;
            }
        }
        
        return -1;
    }
    
    void EraseFront(int n)
    {
        m_Begin += n;
        
        if (m_End <= m_Begin) {
            m_End = m_Begin = 0;
        }
    }
    
    void EraseBack(int n)
    {
        m_End -= n;
        
        if (m_End <= m_Begin) {
            m_End = m_Begin = 0;
        }
    }
    
    void Resize(int n)
    {
        m_End = m_Begin + n;
    }
    
    void Clear()
    {
        m_End = m_Begin = 0;
    }
    
    int Size() const
    {
        return m_End-m_Begin;
    }
    
    const char & operator[](int n) const
    {
        return m_Begin[n];
    }
    
    const char * GetBegin() const
    {
        return m_Begin;
    }
    
    const char * GetEnd() const
    {
        return m_End;
    }
    
    bool Empty() const
    {
        return m_Begin == m_End;
    }
    
    bool operator ==(const CSeqDB_Substring & other)
    {
        int sz = Size();
        
        if (other.Size() == sz) {
            return 0 == memcmp(other.m_Begin, m_Begin, sz);
        }
        
        return false;
    }
    
private:
    const char * m_Begin;
    const char * m_End;
};


/// Combine a filesystem path and file name
///
/// Combine a provided filesystem path and a file name.  This function
/// tries to avoid duplicated delimiters.  If either string is empty,
/// the other is returned.  Conceptually, the first path might be the
/// current working directory and the second path is a filename.  So,
/// if the second path starts with "/", the first path is ignored.
/// Also, care is taken to avoid duplicated delimiters.  If the first
/// path ends with the delimiter character, another delimiter will not
/// be added between the strings.  The delimiter used will vary from
/// operating system to operating system, and is adjusted accordingly.
/// If a file extension is specified, it will also be appended.
///
/// @param path
///   The filesystem path to use
/// @param file
///   The name of the file (may include path components)
/// @param extn
///   The file extension (without the "."), or NULL if none.
/// @param outp
///   A returned string containing the combined path and file name
void SeqDB_CombinePath(const CSeqDB_Substring & path,
                       const CSeqDB_Substring & file,
                       const CSeqDB_Substring * extn,
                       string                 & outp);


/// Returns a path minus filename.
///
/// Substring version of the above.  This returns the part of a file
/// path before the last path delimiter, or the whole path if no
/// delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Path minus file extension
CSeqDB_Substring SeqDB_GetDirName(CSeqDB_Substring s);


/// Returns a filename minus greedy path.
///
/// Substring version.  This returns the part of a file name after the
/// last path delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Filename portion of path
CSeqDB_Substring SeqDB_GetFileName(CSeqDB_Substring s);


/// Returns a filename minus greedy path.
///
/// This returns the part of a file name after the last path
/// delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Path minus file extension
CSeqDB_Substring SeqDB_GetBasePath(CSeqDB_Substring s);


/// Returns a filename minus greedy path.
///
/// This is just the composition of the SeqDB_GetDirName and
/// SeqDB_GetFileName functions; it returns a filename minus greedy
/// path and non-greedy extension.
///
/// @param s
///   Input path
/// @return
///   Filename portion of path, minus extension
CSeqDB_Substring SeqDB_GetBaseName(CSeqDB_Substring s);


// File and directory path classes.  This should be used across all
// CSeqDB functionality.  Phasing it in might work in this order:
//
// 1. Get this alias deal working.
// 2. Move classes to seqdbcommon.cpp.
// 3. De-alias-ify it - make the functionality work for non-alias filenames.
// 4. Use classes as types in map<> and vector<> containers here.
// 5. Start removing the GetFooS() calls from this code by passing
//    the new type to all points of interaction.
//
// DFE (dir, file, ext)
//
// 000 <n/a>
// 001 <n/a>
// 010 BaseName
// 011 FileName
// 100 DirName
// 101 <n/a>
// 110 BasePath
// 111 PathName
//
// Design uses constructors (composition) and getters (seperation).


/// CSeqDB_BaseName
/// 
/// Name of a file without extension or directories.

class CSeqDB_BaseName {
public:
    explicit CSeqDB_BaseName(const string & n)
        : m_Value(n)
    {
    }
    
    explicit CSeqDB_BaseName(const CSeqDB_Substring & n)
    {
        n.GetString(m_Value);
    }
    
    const string & GetBaseNameS() const
    {
        return m_Value;
    }
    
    CSeqDB_Substring GetBaseNameSub() const
    {
        return CSeqDB_Substring(m_Value);
    }
    
    bool operator ==(const CSeqDB_BaseName & other)
    {
        return m_Value == other.m_Value;
    }
    
private:
    string m_Value;
};


/// CSeqDB_FileName
/// 
/// Name of a file with extension but without directories.

class CSeqDB_FileName {
public:
    CSeqDB_FileName()
    {
    }
    
    explicit CSeqDB_FileName(const string & n)
        : m_Value(n)
    {
    }
    
    const string & GetFileNameS() const
    {
        return m_Value;
    }
    
    CSeqDB_Substring GetFileNameSub() const
    {
        return CSeqDB_Substring(m_Value);
    }
    
    void Assign(const CSeqDB_Substring & sub)
    {
        sub.GetStringQuick(m_Value);
    }
    
private:
    string m_Value;
};


/// CSeqDB_DirName
/// 
/// Directory name without a filename.

class CSeqDB_DirName {
public:
    explicit CSeqDB_DirName(const string & n)
        : m_Value(n)
    {
    }
    
    explicit CSeqDB_DirName(const CSeqDB_Substring & n)
    {
        n.GetString(m_Value);
    }
    
    const string & GetDirNameS() const
    {
        return m_Value;
    }
    
    CSeqDB_Substring GetDirNameSub() const
    {
        return CSeqDB_Substring(m_Value);
    }
    
    CSeqDB_DirName & operator =(const CSeqDB_DirName & other)
    {
        s_SeqDB_QuickAssign(m_Value, other.GetDirNameS());
        return *this;
    }
    
    void Assign(const CSeqDB_Substring & sub)
    {
        sub.GetStringQuick(m_Value);
    }
    
private:
    string m_Value;
};


/// CSeqDB_BasePath
/// 
/// Directory and filename without extension.

class CSeqDB_BasePath {
public:
    CSeqDB_BasePath()
    {
    }
    
    explicit CSeqDB_BasePath(const string & bp)
        : m_Value(bp)
    {
    }
    
    explicit CSeqDB_BasePath(const CSeqDB_Substring & bp)
    {
        bp.GetString(m_Value);
    }
    
    CSeqDB_BasePath(const CSeqDB_DirName  & d,
                    const CSeqDB_BaseName & b)
    {
        SeqDB_CombinePath(d.GetDirNameSub(),
                          b.GetBaseNameSub(),
                          0,
                          m_Value);
    }
    
    // Join a directory and a path, for example, "/public/blast/dbs"
    // and "primates/baboon.nal".  If the second element starts with a
    // "/", the directory argument will be discarded.
    
    CSeqDB_BasePath(const CSeqDB_DirName  & d,
                    const CSeqDB_BasePath & b)
    {
        SeqDB_CombinePath(d.GetDirNameSub(),
                          b.GetBasePathSub(),
                          0,
                          m_Value);
    }
    
    CSeqDB_Substring FindDirName() const
    {
        _ASSERT(Valid());
        return SeqDB_GetDirName( CSeqDB_Substring(m_Value) );
    }
    
    CSeqDB_Substring FindBaseName() const
    {
        _ASSERT(Valid());
        return SeqDB_GetBaseName( CSeqDB_Substring( m_Value) );
    }
    
    const string & GetBasePathS() const
    {
        return m_Value;
    }
    
    CSeqDB_Substring GetBasePathSub() const
    {
        return CSeqDB_Substring( m_Value );
    }
    
    bool operator ==(const CSeqDB_BasePath & other)
    {
        return m_Value == other.m_Value;
    }
    
    // Methods like this should be put in class of the "most complete"
    // of the two arguments.
    
    bool Valid() const
    {
        //_ASSERT(m_Value == SeqDB_GetBasePath(m_Value));
        return ! m_Value.empty();
    }
    
    CSeqDB_BasePath & operator =(const CSeqDB_BasePath & other)
    {
        s_SeqDB_QuickAssign(m_Value, other.GetBasePathS());
        return *this;
    }
    
    void Assign(const CSeqDB_Substring & sub)
    {
        sub.GetStringQuick(m_Value);
    }
    
    void Assign(const string & path)
    {
        s_SeqDB_QuickAssign(m_Value, path);
    }
    
private:
    string m_Value;
};


/// CSeqDB_Path
/// 
/// Directory and filename (with extension).  Note that the directory
/// may be empty or incomplete, if the name is a relative name.  The
/// idea is that the name is as complete as we have.  The filename
/// should probably not be trimmed off another path to build one of
/// these objects.

class CSeqDB_Path {
public:
    CSeqDB_Path()
    {
    }
    
    explicit CSeqDB_Path(const string & n)
        : m_Value(n)
    {
    }
    
    CSeqDB_Path(const CSeqDB_Substring & dir,
                const CSeqDB_Substring & file)
    {
        SeqDB_CombinePath(dir, file, 0, m_Value);
    }
    
    CSeqDB_Path(const CSeqDB_DirName  & dir,
                const CSeqDB_FileName & file)
    {
        SeqDB_CombinePath(dir.GetDirNameSub(),
                          file.GetFileNameSub(),
                          0,
                          m_Value);
    }
    
    CSeqDB_Path(const CSeqDB_BasePath & bp, char ext1, char ext2, char ext3)
    {
        const string & base = bp.GetBasePathS();
        
        m_Value.reserve(base.size() + 4);
        m_Value.assign(base.data(), base.data() + base.size());
        m_Value += '.';
        m_Value += ext1;
        m_Value += ext2;
        m_Value += ext3;
    }
    
    CSeqDB_Path(const CSeqDB_DirName  & d,
                const CSeqDB_BaseName & b,
                char                    ext1,
                char                    ext2,
                char                    ext3)
    {
        char extn[3];
        extn[0] = ext1;
        extn[1] = ext2;
        extn[2] = ext3;
        
        CSeqDB_Substring extn1(& extn[0], & extn[3]);
        
        SeqDB_CombinePath(CSeqDB_Substring(d.GetDirNameS()),
                          CSeqDB_Substring(b.GetBaseNameS()),
                          & extn1,
                          m_Value);
    }
    
    bool Valid() const
    {
        return ! m_Value.empty();
    }
    
    const string & GetPathS() const
    {
        _ASSERT(Valid());
        return m_Value;
    }
    
    CSeqDB_Substring FindDirName() const
    {
        _ASSERT(Valid());
        return SeqDB_GetDirName( CSeqDB_Substring(m_Value) );
    }
    
    CSeqDB_Substring FindBasePath() const
    {
        _ASSERT(Valid());
        return SeqDB_GetBasePath( CSeqDB_Substring(m_Value) );
    }
    
    CSeqDB_Substring FindBaseName() const
    {
        _ASSERT(Valid());
        return SeqDB_GetBaseName( CSeqDB_Substring(m_Value) );
    }
    
    CSeqDB_Substring FindFileName() const
    {
        _ASSERT(Valid());
        return SeqDB_GetFileName( CSeqDB_Substring( m_Value ) );
    }
    
    bool operator ==(const CSeqDB_Path & other)
    {
        return m_Value == other.m_Value;
    }
    
    CSeqDB_Path & operator =(const CSeqDB_Path & other)
    {
        s_SeqDB_QuickAssign(m_Value, other.GetPathS());
        return *this;
    }
    
    void Assign(const string & path)
    {
        s_SeqDB_QuickAssign(m_Value, path);
    }
    
    // Combines the directory from a path with a filename.
    
    void ReplaceFilename(const CSeqDB_Path      & dir_src,
                         const CSeqDB_Substring & fname)
    {
        SeqDB_CombinePath(dir_src.FindDirName(), fname, 0, m_Value);
    }
    
private:
    string m_Value;
};


/// Finds a file in the search path.
///
/// This function resolves the full name of a file.  It searches for a
/// file of the provided base name and returns the provided name with
/// the full path attached.  If the exact_name flag is set, the file
/// is assumed to have any extension it may need, and none is added
/// for searching or stripped from the return value.  If exact_name is
/// not set, the file is assumed to end in ".pin", ".nin", ".pal", or
/// ".nal", and if such a file is found, that extension is stripped
/// from the returned string.  Furthermore, in the exact_name == false
/// case, only file extensions relevant to the dbtype are considered.
/// Thus, if dbtype is set to 'p' for protein, only ".pin" and ".pal"
/// are checked for; if it is set to nucleotide, only ".nin" and
/// ".nal" are considered.  The places where the file may be found are
/// dependant on the search path.  The search path consists of the
/// current working directory, the contents of the BLASTDB environment
/// variable, the BLASTDB member of the BLAST group of settings in the
/// NCBI meta-registry.  This registry is an interface to settings
/// found in (for example) a ".ncbirc" file found in the user's home
/// directory (but several paths are usually checked).  Finally, if
/// the provided file_name starts with the default path delimiter
/// (which is OS dependant, but for example, "/" on Linux), the path
/// will be taken to be absolute, and the search path will not affect
/// the results.
/// 
/// @param file_name
///   File base name for which to search
/// @param dbtype
///   Input file base name
/// @param sp
///   If non-null, the ":" delimited search path is returned here
/// @param exact
///   If true, the file_name already includes any needed extension
/// @param atlas
///   The memory management layer.
/// @param locked
///   The lock holder object for this thread.
/// @return
///   Fully qualified filename and path, minus extension
string SeqDB_FindBlastDBPath(const string   & file_name,
                             char             dbtype,
                             string         * sp,
                             bool             exact,
                             CSeqDBAtlas    & atlas,
                             CSeqDBLockHold & locked);


/// Join two strings with a delimiter
///
/// This function returns whichever of two provided strings is
/// non-empty.  If both are non-empty, they are joined with a
/// delimiter placed between them.  It is intended for use when
/// combining strings, such as a space delimited list of database
/// volumes.  It is probably not suitable for joining file system
/// paths with filenames (use something like SeqDB_CombinePaths).
///
/// @param a
///   First component and returned path
/// @param b
///   Second component
/// @param delim
///   The delimiter to use when joining elements
void SeqDB_JoinDelim(string & a, const string & b, const string & delim);


/// Thow a SeqDB exception; this is seperated into a function
/// primarily to allow a breakpoint to be set.
void SeqDB_ThrowException(CSeqDBException::EErrCode code, const string & msg);


/// OID-Range type to simplify interfaces.
struct SSeqDBSlice {
    /// Default constructor
    SSeqDBSlice()
    {
        begin = end = -1;
    }
    
    /// Constructor
    SSeqDBSlice(int b, int e)
        : begin (b),
          end   (e)
    {
    }
    
    int begin;
    int end;
};


/// Simple "Copy Collector" Like Cache
///
/// This code implements a simple STL map based cache with limited LRU
/// properties.  The cache is given a size, and will maintain a size
/// somewhere between half this number and this number of entries.

template<typename TKey, typename TValue>
class CSeqDBSimpleCache {
public:
    /// Constructor
    ///
    /// Constructs a cache with the specified size limit.
    ///
    /// @param sz
    ///   Maximum size of the cache.
    CSeqDBSimpleCache(int sz)
        : m_MaxSize(sz/2)
    {
        if (m_MaxSize < 4) {
            m_MaxSize = 4;
        }
    }
    
    /// Lookup a value in the cache.
    ///
    /// Like the C++ STL's map::operator[], this method will find the
    /// specified cache mapping and return a reference to the value at
    /// that location.  If the key was not found, the key will be
    /// inserted with a null value and the reference to that null
    /// returned (the caller may assign to it to add a value).
    ///
    /// @param k
    ///     The key to find or insert in the cache.
    TValue & Lookup(const TKey & k)
    {
        TMapIter i = m_Mapping.find(k);
        
        // If we have it, just return it.
        
        if (i != m_Mapping.end()) {
            return (*i).second;
        }
        
        i = m_OldMap.find(k);
        
        // If the old version has it, transfer to the new version.
        // The erase here is optional.  It should not affect the
        // code except to reduce memory requirements and possibly
        // to speed up lookups in oldmap.
        
        if (i != m_OldMap.end()) {
            TValue & rv = m_Mapping[k] = ((*i).second);
            m_OldMap.erase(i);
            
            return rv;
        }
        
        // Otherwise, add a new value, returning a reference to it.
        // Since we are expanding the map, we may swap the containers
        // and discard the old values here.
        
        if (int(m_Mapping.size()) >= m_MaxSize) {
            m_OldMap.clear();
            m_Mapping.swap(m_OldMap);
        }
        
        return m_Mapping[k];
    }
    
private:
    /// The underlying associative array type.
    typedef std::map<TKey, TValue> TMap;
    
    /// The underlying associative array iterator type.
    typedef typename TMap::iterator TMapIter;
    
    /// Values will be added to this mapping until size is reached.
    TMap m_Mapping;
    
    /// Values from m_Mapping are moved here when size is exceeded.
    TMap m_OldMap;
    
    /// Maximum size of the m_Mapping container.
    int m_MaxSize;
};


END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBGENERAL_HPP

