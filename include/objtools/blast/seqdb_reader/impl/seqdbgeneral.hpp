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


#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// Byte-order-nonspecific (long) versions

/// Reads a network order integer and returns a value.
///
/// Integer types stored in platform-independent blast database files
/// usually have network byte order.  This template builds a function
/// which reads such an integer and returns its value. It may or may
/// not need to swap the integer, depending on the endianness of the
/// platform.  If the integer is not aligned to a multiple of the size
/// of the data type, it will still be read byte-wise rather than
/// word-wise.  This is done to avoid bus errors on some platforms.

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

/// Read an unaligned integer into memory.
///
/// This template builds a function that reads an integer (on any
/// platform) by reading one byte at a time and assembling the value.
/// The word "Broken" refers to fact that the integer in question is
/// in the opposite of network byte order, and this function is called
/// in those cases.  (Currently, this only happens for the 8 byte
/// volume size stored in the index file.)
///
/// @param stdord_obj Location of non-network-order object.
/// @return Value of that object.

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

// These macros are for positive numbers.

/// Discretely tests whether an integer is a power of two.
#define IS_POWER_OF_TWO(x)    (((x) & ((x)-1)) == 0)

/// Checks if a number is congruent to zero, modulo a power of 2.
#define ALIGNED_TO_POW2(x,y)  (! ((x) & (0-y)))

/// Is the provided pointer aligned to the size (which must be a power
/// of two) of the type to which it points?
#define PTR_ALIGNED_TO_SELF_SIZE(x) \
    (IS_POWER_OF_TWO(sizeof(*x)) && ALIGNED_TO_POW2((size_t)(x), sizeof(*x)))


// Portable byte swapping from marshalled version

#ifdef WORDS_BIGENDIAN

/// Read a network order integer value.
/// @param stdord_obj Location in memory of network order integer.
/// @return Value of the integer read from memory.
template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    if (PTR_ALIGNED_TO_SELF_SIZE(stdord_obj)) {
        return *stdord_obj;
    } else {
        return SeqDB_GetStdOrdUnaligned(stdord_obj);
    }
}

/// Read a non-network-order integer value.
/// @param stdord_obj Location in memory of integer.
/// @return Value of the integer read from memory.
template<typename T>
inline T SeqDB_GetBroken(const T * stdord_obj)
{
    return SeqDB_GetBrokenUnaligned(stdord_obj);
}

#else

/// Read a network order integer value.
/// @param stdord_obj Location in memory of network order integer.
/// @return Value of the integer read from memory.
template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    return SeqDB_GetStdOrdUnaligned(stdord_obj);
}

/// Read a non-network-order integer value.
/// @param stdord_obj Location in memory of integer.
/// @return Value of the integer read from memory.
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


/// Macro for EOL chars.
#define SEQDB_ISEOL(x) (((x) == '\n') || ((x) == '\r'))


/// Higher Performance String Assignment
/// 
/// Gcc's default assignment and modifier methods (insert, operator =
/// and operator += for instance) for strings do not use the capacity
/// doubling technique (i.e. as used by vector::push_back()) until the
/// length is about the size of a disk block.  For our purposes, they
/// often should use doubling.  The following assignment function
/// provides the doubling functionality for assignment.  I use the
/// assign(char*,char*) overload because it does not discard excess
/// capacity.
///
/// @param dst Destination of assigned data.
/// @param bp Start of memory containing new value.
/// @param ep Start of memory containing new value.
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
/// String to string assignment, using the above function.
/// @param dst Destination string.
/// @param src Input string.
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
    /// Constructor, builds empty substring.
    CSeqDB_Substring()
        : m_Begin(0), m_End(0)
    {
    }
    
    /// Construct a substring for a string literal.
    /// @param s A null terminated string literal.
    explicit CSeqDB_Substring(const char * s)
        : m_Begin(s)
    {
        m_End = s + strlen(s);
    }
    
    /// Construct a substring for a C++ std::string.
    /// @param s An existing std::string object.
    explicit CSeqDB_Substring(const string & s)
        : m_Begin(s.data()), m_End(s.data() + s.size())
    {
    }
    
    /// Construct a substring for a range of memory.
    /// @param b Start of the range of memory.
    /// @param e End of the memory range.
    CSeqDB_Substring(const char * b, const char * e)
        : m_Begin(b), m_End(e)
    {
    }
    
    /// Return the data by assigning it to a string.
    /// @param s The substring data is returned here.
    void GetString(string & s) const
    {
        if (m_Begin != m_End) {
            s.assign(m_Begin, m_End);
        } else {
            s.erase();
        }
    }
    
    /// Return the data by quick-assigning it to a string.
    /// @param s The substring data is returned here.
    void GetStringQuick(string & s) const
    {
        if (m_Begin != m_End) {
            s_SeqDB_QuickAssign(s, m_Begin, m_End);
        } else {
            s.erase();
        }
    }
    
    /// Find last instance of a character in the substring.
    /// @param ch The character for which to search.
    /// @return The offset of the last instance of ch or -1.
    int FindLastOf(char ch) const
    {
        for (const char * p = m_End - 1;  p >= m_Begin;  --p) {
            if (*p == ch) {
                return int(p - m_Begin);
            }
        }
        
        return -1;
    }
    
    /// Disinclude data from the beginning of the string.
    ///
    /// Unlike std::string versions of this type of functionality,
    /// this version works in constant time without reallocating or
    /// copying data.  Removing more bytes than the string contains
    /// empties the substring, and is not an error.
    ///
    /// @param n The number of characters to remove.
    void EraseFront(int n)
    {
        m_Begin += n;
        
        if (m_End <= m_Begin) {
            m_End = m_Begin = 0;
        }
    }
    
    /// Disinclude data from the end of the string.
    ///
    /// Unlike std::string versions of this type of functionality,
    /// this version works in constant time without reallocating or
    /// copying data.  Removing more bytes than the string contains
    /// empties the substring, and is not an error.
    ///
    /// @param n The number of characters to remove.
    void EraseBack(int n)
    {
        m_End -= n;
        
        if (m_End <= m_Begin) {
            m_End = m_Begin = 0;
        }
    }
    
    /// Change the length of the string.
    ///
    /// The substring will be increased or reduced.  Normally this is
    /// used to reduce the string length; increasing it is legal, but
    /// requires the calling code to understand more about the "real"
    /// underlying data in order to be useful.  Note that newly added
    /// bytes are not zeroed out by this method.
    ///
    /// @param n The number of characters to remove.
    void Resize(int n)
    {
        m_End = m_Begin + n;
    }
    
    /// Reset the string to an empty state.
    void Clear()
    {
        m_End = m_Begin = 0;
    }
    
    /// Return the length of the string in bytes.
    int Size() const
    {
        return int(m_End-m_Begin);
    }
    
    /// Returns a reference to a specific character of the string.
    const char & operator[](int n) const
    {
        return m_Begin[n];
    }
    
    /// Returns a pointer to the start of the string.
    const char * GetBegin() const
    {
        return m_Begin;
    }
    
    /// Returns a pointer to the end of the string, which is always a
    /// pointer to the character past the last included character.
    const char * GetEnd() const
    {
        return m_End;
    }
    
    /// Returns true iff the string is empty.
    bool Empty() const
    {
        return m_Begin == m_End;
    }
    
    /// Compares the contents of the string to another substring.
    bool operator ==(const CSeqDB_Substring & other) const
    {
        int sz = Size();
        
        if (other.Size() == sz) {
            return 0 == memcmp(other.m_Begin, m_Begin, sz);
        }
        
        return false;
    }
    
private:
    /// Points to the beginning of the string's data or null.
    const char * m_Begin;
    
    /// Points to the end of the string (post notation) or null.
    const char * m_End;
};


/// Parse a prefix from a substring.
///
/// The `buffer' argument is searched for a character.  If found, the
/// region before the delimiter is returned in `front' and the region
/// after the delimiter is returned in `buffer', and true is returned.
/// If not found, neither argument changes and false is returned.
///
/// @param buffer Source data to search and remainder if found. [in|out]
/// @param front Region before delim if found. [out]
/// @param delim Character for which to search. [in]
/// @return true if the character was found, false otherwise.
bool SeqDB_SplitString(CSeqDB_Substring & buffer,
                       CSeqDB_Substring & front,
                       char               delim);


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
CSeqDB_Substring SeqDB_RemoveFileName(CSeqDB_Substring s);


/// Returns a filename minus greedy path.
///
/// Substring version.  This returns the part of a file name after the
/// last path delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Filename portion of path
CSeqDB_Substring SeqDB_RemoveDirName(CSeqDB_Substring s);


/// Returns a filename minus greedy path.
///
/// This returns the part of a file name after the last path
/// delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Path minus file extension
CSeqDB_Substring SeqDB_RemoveExtn(CSeqDB_Substring s);


/// Change path delimiters to platform preferred kind in-place.
///
/// The path is modified in place.  The 'Convert' interface is more
/// efficient for cases where the new path would be assigned to the
/// same string object.  Delimiter conversion should be called by
/// SeqDB at least once on any path received from the user, or via
/// filesystem sources such as alias files.
///
/// @param dbs This string will be changed in-place.
void SeqDB_ConvertOSPath(string & dbs);


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

// Theory:
//
// Each class wraps an ordinary string object.  The methods written
// for each class are simple enough to be self documenting.  One might
// ask, what is the purpose of these classes?
//
// These types represent a kind of object oriented "immune system".
// The goal of an immune system is to recognize and catalog the
// acceptable elements in a complex system and reject all other
// elements.  In this case, these classes enumerate the kinds of
// operations that SeqDB uses on directory / filename paths.
//
// Each of these types represents part of a pathname.  Many of the
// possible combinations of methods and types here would not make any
// sense - you could not find a filename from a directory name for
// instance - so we can omit that method on that class.  It is a
// common logical mistake to try to get a piece of information from a
// given path sub-element, that does not have that information.  For
// example, trying to find the directory of a path means removing a
// filename, but if this were done twice, the last directory component
// would be removed by the second attempt.
//
// The main reason for using these types is to convert such errors
// into compile time errors, or remove them entirely.  A second
// purpose of this design is to catalog all the types of operations
// that CSeqDB uses on path components and to aggregate that code
// here.  Thirdly, by providing names to the methods, it is possible
// to find locations where a given operation is done by searching the
// code base for those names.


/// CSeqDB_BaseName
/// 
/// Name of a file without extension or directories.

class CSeqDB_BaseName {
public:
    /// Constructor taking a string.
    explicit CSeqDB_BaseName(const string & n)
        : m_Value(n)
    {
    }
    
    /// Constructor taking a substring.
    explicit CSeqDB_BaseName(const CSeqDB_Substring & n)
    {
        n.GetString(m_Value);
    }
    
    /// Returns the base name as a string.
    const string & GetBaseNameS() const
    {
        return m_Value;
    }
    
    /// Returns the base name as a substring.
    CSeqDB_Substring GetBaseNameSub() const
    {
        return CSeqDB_Substring(m_Value);
    }
    
    /// Compares the basename to another basename.
    bool operator ==(const CSeqDB_BaseName & other) const
    {
        return m_Value == other.m_Value;
    }
    
    /// Change any internal delimiters to the platform specific kind.
    void FixDelimiters()
    {
        SeqDB_ConvertOSPath(m_Value);
    }
    
private:
    /// The base name.
    string m_Value;
};


/// CSeqDB_FileName
/// 
/// Name of a file with extension but without directories.

class CSeqDB_FileName {
public:
    /// Default constructor.
    CSeqDB_FileName()
    {
    }
    
    /// Construct a filename using the given string.
    explicit CSeqDB_FileName(const string & n)
        : m_Value(n)
    {
    }
    
    /// Get the filename as a string.
    const string & GetFileNameS() const
    {
        return m_Value;
    }
    
    /// Get the filename as a substring.
    CSeqDB_Substring GetFileNameSub() const
    {
        return CSeqDB_Substring(m_Value);
    }
    
    /// Assign a new filename to this object.
    /// @param sub The filename to assign here.
    void Assign(const CSeqDB_Substring & sub)
    {
        sub.GetStringQuick(m_Value);
    }
    
    /// Change any internal delimiters to the platform specific kind.
    void FixDelimiters()
    {
        SeqDB_ConvertOSPath(m_Value);
    }
    
private:
    /// The filename.
    string m_Value;
};


/// CSeqDB_DirName
/// 
/// Directory name without a filename.

class CSeqDB_DirName {
public:
    /// Constructor taking a string.
    explicit CSeqDB_DirName(const string & n)
        : m_Value(n)
    {
    }
    
    /// Constructor taking a substring.
    explicit CSeqDB_DirName(const CSeqDB_Substring & n)
    {
        n.GetString(m_Value);
    }
    
    /// Get the directory name as a string.
    const string & GetDirNameS() const
    {
        return m_Value;
    }
    
    /// Get the directory name as a substring.
    CSeqDB_Substring GetDirNameSub() const
    {
        return CSeqDB_Substring(m_Value);
    }
    
    /// Assign a new directory name from a string.
    CSeqDB_DirName & operator =(const CSeqDB_DirName & other)
    {
        s_SeqDB_QuickAssign(m_Value, other.GetDirNameS());
        return *this;
    }
    
    /// Assign a new directory name from a substring.
    void Assign(const CSeqDB_Substring & sub)
    {
        sub.GetStringQuick(m_Value);
    }
    
private:
    /// The directory name.
    string m_Value;
};


/// CSeqDB_BasePath
/// 
/// Directory and filename without extension.

class CSeqDB_BasePath {
public:
    /// Construct an empty path.
    CSeqDB_BasePath()
    {
    }
    
    /// Constructor taking a string.
    explicit CSeqDB_BasePath(const string & bp)
        : m_Value(bp)
    {
    }
    
    /// Constructor taking a substring.
    explicit CSeqDB_BasePath(const CSeqDB_Substring & bp)
    {
        bp.GetString(m_Value);
    }
    
    /// Constructor taking a directory and filename.
    ///
    /// The given directory and filename will be combined to form the
    /// basepath.
    ///
    /// @param d The directory.
    /// @param b The base name.
    CSeqDB_BasePath(const CSeqDB_DirName  & d,
                    const CSeqDB_BaseName & b)
    {
        SeqDB_CombinePath(d.GetDirNameSub(),
                          b.GetBaseNameSub(),
                          0,
                          m_Value);
    }
    
    /// Constructor taking a directory and basepath.
    ///
    /// The given directory and basepath will be combined to form the
    /// basepath.  If the provided base path is absolute (its first
    /// character is a path delimiter) it will be used unmodified.
    ///
    /// @param d The directory.
    /// @param b The base path.
    CSeqDB_BasePath(const CSeqDB_DirName  & d,
                    const CSeqDB_BasePath & b)
    {
        SeqDB_CombinePath(d.GetDirNameSub(),
                          b.GetBasePathSub(),
                          0,
                          m_Value);
    }
    
    /// Return the portion of this path representing the directory.
    CSeqDB_Substring FindDirName() const
    {
        _ASSERT(Valid());
        return SeqDB_RemoveFileName( CSeqDB_Substring(m_Value) );
    }
    
    /// Return the portion of this path representing the base name.
    CSeqDB_Substring FindBaseName() const
    {
        _ASSERT(Valid());
        return SeqDB_RemoveDirName( CSeqDB_Substring( m_Value) );
    }
    
    /// Return this path as a string.
    const string & GetBasePathS() const
    {
        return m_Value;
    }
    
    /// Return this path as a substring.
    CSeqDB_Substring GetBasePathSub() const
    {
        return CSeqDB_Substring( m_Value );
    }
    
    /// Compare this value to another base path.
    bool operator ==(const CSeqDB_BasePath & other) const
    {
        return m_Value == other.m_Value;
    }
    
    /// Return true if the value is not empty.
    bool Valid() const
    {
        return ! m_Value.empty();
    }
    
    /// Assign the value from another base path object.
    CSeqDB_BasePath & operator =(const CSeqDB_BasePath & other)
    {
        s_SeqDB_QuickAssign(m_Value, other.GetBasePathS());
        return *this;
    }
    
    /// Assign the value from a substring.
    void Assign(const CSeqDB_Substring & sub)
    {
        sub.GetStringQuick(m_Value);
    }
    
    /// Assign the value from a string.
    void Assign(const string & path)
    {
        s_SeqDB_QuickAssign(m_Value, path);
    }
    
    /// Change any internal delimiters to the platform specific kind.
    void FixDelimiters()
    {
        SeqDB_ConvertOSPath(m_Value);
    }
    
private:
    /// The value stored here.
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
    /// Construct an empty path.
    CSeqDB_Path()
    {
    }
    
    /// Constructor taking a string.
    explicit CSeqDB_Path(const string & n)
        : m_Value(n)
    {
    }
    
    /// Constructor taking a directory and filename.
    ///
    /// The given directory and filename will be combined to form the
    /// path.  This version takes substrings.
    ///
    /// @param dir The directory.
    /// @param file The base name.
    CSeqDB_Path(const CSeqDB_Substring & dir,
                const CSeqDB_Substring & file)
    {
        SeqDB_CombinePath(dir, file, 0, m_Value);
    }
    
    /// Constructor taking a directory and filename.
    ///
    /// The given directory and filename will be combined to form the
    /// path.  This version takes encapsulated types.
    ///
    /// @param dir The directory.
    /// @param file The base name.
    CSeqDB_Path(const CSeqDB_DirName  & dir,
                const CSeqDB_FileName & file)
    {
        SeqDB_CombinePath(dir.GetDirNameSub(),
                          file.GetFileNameSub(),
                          0,
                          m_Value);
    }
    
    /// Constructor taking a basepath and extension.
    ///
    /// The given basepath will be combined with the provided
    /// extension.
    ///
    /// @param bp The base path.
    /// @param ext1 First character of the file extension.
    /// @param ext2 Second character of the file extension.
    /// @param ext3 Third character of the file extension.
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
    
    /// Constructor taking a directory, filename, and extension.
    ///
    /// The given directory and basename will be combined, along with
    /// the provided extension.
    ///
    /// @param d The directory name.
    /// @param b The base name.
    /// @param ext1 First character of the file extension.
    /// @param ext2 Second character of the file extension.
    /// @param ext3 Third character of the file extension.
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
    
    /// Returns true if this object has a value.
    bool Valid() const
    {
        return ! m_Value.empty();
    }
    
    /// Get the path as a string.
    const string & GetPathS() const
    {
        _ASSERT(Valid());
        return m_Value;
    }
    
    /// Returns the portion of this path containing the directory.
    CSeqDB_Substring FindDirName() const
    {
        _ASSERT(Valid());
        return SeqDB_RemoveFileName( CSeqDB_Substring(m_Value) );
    }
    
    /// Returns the portion of this path containing the base path.
    CSeqDB_Substring FindBasePath() const
    {
        _ASSERT(Valid());
        return SeqDB_RemoveExtn(CSeqDB_Substring(m_Value));
    }
    
    /// Returns the portion of this path containing the base name.
    CSeqDB_Substring FindBaseName() const
    {
        _ASSERT(Valid());
        return SeqDB_RemoveExtn(SeqDB_RemoveDirName(CSeqDB_Substring(m_Value)));
    }
    
    /// Returns the portion of this path containing the file name.
    CSeqDB_Substring FindFileName() const
    {
        _ASSERT(Valid());
        return SeqDB_RemoveDirName( CSeqDB_Substring( m_Value ) );
    }
    
    /// Returns true if the paths are equal.
    bool operator ==(const CSeqDB_Path & other) const
    {
        return m_Value == other.m_Value;
    }
    
    /// Assigns the provided value to this path.
    CSeqDB_Path & operator =(const CSeqDB_Path & other)
    {
        s_SeqDB_QuickAssign(m_Value, other.GetPathS());
        return *this;
    }
    
    /// Assigns the provided value to this path.
    void Assign(const string & path)
    {
        s_SeqDB_QuickAssign(m_Value, path);
    }
    
    /// Combines the directory from a path with a filename.
    ///
    /// Removes the filename from the provided path and attaches the
    /// given filename in its place, assigning the result to this
    /// object.
    ///
    /// @param dir_src The directory of this path will be used.
    /// @param fname This filename will be used.
    void ReplaceFilename(const CSeqDB_Path      & dir_src,
                         const CSeqDB_Substring & fname)
    {
        SeqDB_CombinePath(dir_src.FindDirName(), fname, 0, m_Value);
    }
    
private:
    /// The string containing this path.
    string m_Value;
};

/// Forward declaration.
class CSeqDBAtlas;

/// Forward declaration.
class CSeqDBLockHold;

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


/// Report file corruption by throwing an eFile CSeqDBException.
///
/// This function is only called in the case of validation failure,
/// and is used in code paths where the validation failure may be
/// related to file corruption or filesystem problems.  File data is
/// considered a user input, so checks for corrupt file are treated as
/// input validation.  This means that (1) checks that may be caused
/// by file corruption scenarios are not disabled in debug mode, and
/// (2) an exception (rather than an abort) is used.  Note that this
/// function does not check the assert, so it should only be called in
/// case of failure.
///
/// @param file Name of the file containing the assert.
/// @param line The line the assert in on.
/// @param text The text version of the asserted condition.
void SeqDB_FileIntegrityAssert(const string & file,
                               int            line,
                               const string & text);

#define SEQDB_FILE_ASSERT(YESNO)                                        \
    do {                                                                \
        if (! (YESNO)) {                                                \
            SeqDB_FileIntegrityAssert(__FILE__, __LINE__, (#YESNO));    \
        }                                                               \
    } while(0)


/// OID-Range type to simplify interfaces.
struct SSeqDBSlice {
    /// Default constructor
    SSeqDBSlice()
    {
        begin = end = -1;
    }
    
    /// Constructor
    /// @param b The beginning of the range.
    /// @param e The end of the range.
    SSeqDBSlice(int b, int e)
        : begin (b),
          end   (e)
    {
    }
    
    /// First oid in range.
    int begin;
    
    /// OID after last included oid.
    int end;
};


/// Simple int-keyed cache.
///
/// This code implements a simple vector based cache, mapping OIDs to
/// objects of some type.  The cache has a fixed size (which must be a
/// power of two), and uses the OID mod the cache size to select a
/// cache slot.

template<typename TValue>
class CSeqDBIntCache {
public:
    /// Constructor
    ///
    /// Constructs a cache with the specified number of entries.
    ///
    /// @param sz Number of cache slots.
    CSeqDBIntCache(int sz)
    {
        _ASSERT(IS_POWER_OF_TWO(sz));
        m_Slots.resize(sz);
    }
    
    /// Find a value in the cache.
    ///
    /// This method find the specified item, returning a reference to
    /// it.  An existing entry in the slot will be cleared if the key
    /// does not match.
    ///
    /// @param key The integer key to find.
    TValue & Lookup(int key)
    {
        _ASSERT(IS_POWER_OF_TWO(m_Slots.size()));
        
        TSlot & slot = m_Slots[key & (m_Slots.size()-1)];
        
        if (slot.first != key) {
            slot.first = key;
            slot.second = TValue();
        }
        
        return slot.second;
    }
    
private:
    /// Type used for cache slots.
    typedef std::pair<int, TValue> TSlot;
    
    /// Values are stored here.
    vector<TSlot> m_Slots;
};


/// Combine and quote list of database names.
///
/// @param dbs Database names to combine.
/// @param dbname Combined database name.
void SeqDB_CombineAndQuote(const vector<string> & dbs,
                           string               & dbname);

/// Combine and quote list of database names.
///
/// @param dbname Combined database name.
/// @param dbs Database names to combine.
void SeqDB_SplitQuoted(const string             & dbname,
                       vector<CSeqDB_Substring> & dbs,
                       bool keep_quote = false);

/// Fence Sentry value, which is placed at either end of ranges of
/// data that are included in partially fetched sequences; this only
/// applies to CSeqDBExpert objects, where SetOffsetRanges() has been
/// called.
/// @note this value is repeated in blast_util.h
#define FENCE_SENTRY 201


/// Find a map value or return a default.
///
/// This is similar to operator[], except that it works for constant
/// maps, and takes an arbitrary default value when the value is not
/// found (for std::map, the default value is always TValue()).
///
/// @param m The map from which to read values.
/// @param k The key for which to search.
/// @param dflt The value to return if the key was not found.
/// @return The value corresponding to k or a reference to dflt.
template<class T, class U>
const U & SeqDB_MapFind(const std::map<T,U> & m, const T & k, const U & dflt)
{
    typename map<T,U>::const_iterator iter = m.find(k);
    
    if (iter == m.end()) {
        return dflt;
    }
    
    return iter->second;
}

/// Copy into a vector efficiently.
///
/// This copies data into a vector which may not be empty beforehand.
/// It is more efficient than freeing the vector for cases like
/// vector<string>, where the existing string buffers may be large
/// enough to hold the new elements.  The vector is NOT resized
/// downward but the caller may do a resize() if needed.  This design
/// was chosen because for some types (such as vector<string>), more
/// efficient code can be written if element destruction/construction
/// is avoided.  The number of elements assigned is returned.
///
/// @param data Data source usable by ITERATE and *iter.
/// @param v Vector to copy the data into.
/// @return The number of elements copied.
template<class T, class U>
int SeqDB_VectorAssign(const T & data, vector<U> & v)
{
    size_t i = 0;
    
    ITERATE(typename T, iter, data) {
        if (i < v.size()) {
            v[i] = (*iter);
        } else {
            v.push_back(*iter);
        }
        i++;
    }
    
    return i;
}

END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBGENERAL_HPP

