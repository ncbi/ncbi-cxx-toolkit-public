#ifndef UTIL_SEQUTIL___SEQUTIL_TABLES__HPP
#define UTIL_SEQUTIL___SEQUTIL_TABLES__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   Conversion tables
 */   
#include <util/sequtil/sequtil.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// Conversion Tables

// IUPACna -> ....
//===========================================================================

// IUPACna to IUPACna
// Size: 256 (1 column)
// each IUPACna is mapped to itself, lower case letters are mapped
// to upper case, U / u are mapped to T

class CIupacnaToIupacna
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacnaToIupacna(void);

    static const Uint1 scm_Table[256];
};

// IUPACna to NCBI2na
// Size: 1024 (256 rows * 4 columns)

class CIupacnaTo2na
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacnaTo2na(void);

    static const Uint1 scm_Table[1024];
};


// IUPACna to NCBI2na_expand
// Size: 256

class CIupacnaTo2naExpand
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacnaTo2naExpand(void);

    static const Uint1 scm_Table[256];
};


// IUPACna to NCBI4na
// Size: 512 (256 rown * 2 columns)

class CIupacnaTo4na
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacnaTo4na(void);

    static void Init(void);

    static const Uint1 scm_Table[512];
};


// IUPACna to NCBI8na
// Size: 256

class CIupacnaTo8na
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacnaTo8na(void);

    static const Uint1 scm_Table[256];
};


// NCBI2na -> ....
//===========================================================================

// NCBI2na to IUPACna
// Size: 1024 (256 rows * 4 columns)

class C2naToIupacna
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C2naToIupacna(void);

    static const Uint1    scm_Table[1024];
};


// NCBI2na to NCBI2na_expand
// Size: 1024 (256 rows * 4 columns)

class C2naTo2naExpand
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C2naTo2naExpand(void);

    static const Uint1    scm_Table[1024];
};


// NCBI2na to NCBI4na
// We use 2 tables for this conversion; a straight forward table in 
// the case when the starting position of the conversion (within a 
// ncbi2na byte) is 0 or 2. this table maps a single ncbi2na byte to
// 2 ncbi4na bytes.
// a second table is used when the offset is 1 or 3. this table consist 
// of 3 columns. for a given ncbi2na byte the first column is the mapping of
// the lower 2 bits (1st base) in the ncbi2na byte, the second is the 
// mapping of bases 2 and 3 and the 3rd column is the mapping of the 4th base.

class C2naTo4na
{
public:
    static const Uint1* GetTable(bool boundry) { 
        return boundry ? scm_Table0 : scm_Table1;
    }

private:
    C2naTo4na(void);

    static const Uint1    scm_Table0[512];
    static const Uint1    scm_Table1[768];
};


// NCBI2na to NCBI8na (NCBI4na_expand)
// each ncbi2na byte is mapped to 4 ncbi8na bytes

class C2naTo8na
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C2naTo8na(void);

    static const Uint1    scm_Table[1024];
};


// NCBI2na_expand -> ....
//===========================================================================

// NCBI2na_expand to IUPACna
// 0 -> A
// 1 -> C
// 2 -> G
// 3 -> T

class C2naExpandToIupacna
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C2naExpandToIupacna(void);

    static const Uint1    scm_Table[256];
};


// NCBI4na -> ....
//===========================================================================


// NCBI4na to IUPACna
// each ncbi4na byte maps to 2 iupacna bytes

class C4naToIupacna
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C4naToIupacna(void);

    static const Uint1    scm_Table[512];
};


// NCBI4na to NCBI2na
// depending on the initial offset 4 bases that are packed in
// a single ncbi2na byte might come from either 2 or 3 ncbi4na bytes.
// we use 2 different tables to accomodate to 2 cases.
// 0  -> 3  (gap -> T)
// 1  -> 0  (A -> A)
// 2  -> 1  (C -> C)
// 3  -> 1  (M -> C)
// 4  -> 2  (G -> G)
// 5  -> 2  (R -> G)
// 6  -> 1  (S -> C)
// 7  -> 0  (V -> A)
// 8  -> 3  (T -> T)
// 9  -> 3  (W -> T)
// 10 -> 3  (Y -> T)
// 11 -> 0  (H -> A)
// 12 -> 2  (K -> G)
// 13 -> 2  (D -> G)
// 14 -> 1  (B -> C)
// 15 -> 0  (N -> A)

class C4naTo2na
{
public:
    static const Uint1* GetTable(size_t offset) {
        return (offset == 0) ? scm_Table0 : scm_Table1;
    }

private:
    C4naTo2na(void);

    static const Uint1    scm_Table0[512];
    static const Uint1    scm_Table1[768];
};


// NCBI4na to NCBI2na_expand
// each ncbi4na byte maps to 2 iupacna bytes

// gap -> 3 T
// A   -> 0 A
// C   -> 1 C
// M   -> 1 C
// G   -> 2 G
// R   -> 2 G
// S   -> 1 C
// V   -> 0 A
// T   -> 3 T
// W   -> 3 T
// Y   -> 3 T
// H   -> 0 A
// K   -> 2 G
// D   -> 2 G
// B   -> 1 C
// N   -> 0 A

class C4naTo2naExpand
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C4naTo2naExpand(void);

    static const Uint1    scm_Table[512];
};


// NCBI4na to NCBI8na (NCBI4na_expand)
// expand a single ncbi4na byte to 2 ncbi8na bytes

class C4naTo8na
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C4naTo8na(void);

    static const Uint1    scm_Table[512];
};


// NCBI8na -> ....
//===========================================================================

// NCBI8na to IUPACna
// map ncbi8na byte to an iupacna one.

class C8naToIupacna
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C8naToIupacna(void);

    static const Uint1    scm_Table[256];
};


// NCBI8na to NCBI2na
// map ncbi8na byte to the corresponding ncbi2na 2 bits based on the offset
// of the byte in the 4 bytes comprising the single ncbi2na byte.

class C8naTo2na
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C8naTo2na(void);

    static const Uint1    scm_Table[1024];
};


// IUPACaa to NCBIstdaa

class CIupacaaToStdaa
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacaaToStdaa(void);

    static const Uint1    scm_Table[256];
};


// NCBIeaa to IUPACaa

class CEaaToIupacaa
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CEaaToIupacaa(void);

    static const Uint1    scm_Table[256];
};


// NCBIeaa to NCBIstdaa

class CEaaToStdaa
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CEaaToStdaa(void);

    static const Uint1    scm_Table[256];
};


// NCBIstdaa to IUPACaa

class CStdaaToIupacaa
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CStdaaToIupacaa(void);

    static const Uint1    scm_Table[256];
};


// NCBIstdaa to NCBIeaa

class CStdaaToEaa
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CStdaaToEaa(void);

    static const Uint1    scm_Table[256];
};


/////////////////////////////////////////////////////////////////////////////
//
// Reverse Tables

// NCBI2na

class C2naReverse
{
public:
    static const Uint1* GetTable(size_t offset) { return scm_Tables[offset]; }

private:
    C2naReverse(void);

    static const Uint1    scm_Table3[256];  // offset 3 - byte boundry
    static const Uint1    scm_Table2[512];  // offset 2
    static const Uint1    scm_Table1[512];  // offset 1
    static const Uint1    scm_Table0[512];  // offset 0
    static const Uint1*   scm_Tables[4];
};



// NCBI4na

class C4naReverse
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C4naReverse(void);

    static const Uint1    scm_Table[256];
};


/////////////////////////////////////////////////////////////////////////////
//
// Complement Tables

// IUPACna

class CIupacnaCmp
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    CIupacnaCmp(void);

    static const Uint1    scm_Table[256];
};


// NCBI2na

class C2naCmp
{
public:
    static const Uint1* GetTable(size_t offset) { return scm_Tables[offset]; }

private:
    C2naCmp(void);

    static const Uint1    scm_Table1[512];
    static const Uint1    scm_Table2[512];
    static const Uint1    scm_Table3[512];
    static const Uint1*   scm_Tables[4];
};


// NCBI4na

// 0  -> 0
// 1  -> 8
// 2  -> 4
// 3  -> 12
// 4  -> 2
// 5  -> 10
// 6  -> 9
// 7  -> 14
// 8  -> 1
// 9  -> 6
// 10 -> 5
// 11 -> 13
// 12 -> 3
// 13 -> 11
// 14 -> 7
// 15 -> 15

class C4naCmp
{
public:
    static const Uint1* GetTable(size_t offset) {
        return (offset == 0) ? scm_Table0 : scm_Table1;
    }

private:
    C4naCmp(void);

    static const Uint1    scm_Table0[256];
    static const Uint1    scm_Table1[512];
};


// NCBI8na

class C8naCmp
{
public:
    static const Uint1* GetTable(void) { return scm_Table; }

private:
    C8naCmp(void);

    static const Uint1    scm_Table[256];
};



/////////////////////////////////////////////////////////////////////////////
//
// ReverseComplement Tables

// NCBI2na
class C2naRevCmp
{
public:
    static const Uint1* GetTable(size_t offset) { return scm_Tables[offset]; }

private:
    C2naRevCmp(void);

    static const Uint1*   scm_Tables[4];
    static const Uint1    scm_Table0[512];
    static const Uint1    scm_Table1[512];
    static const Uint1    scm_Table2[512];
    static const Uint1    scm_Table3[256];
};


class C4naRevCmp
{
public:
    static const Uint1* GetTable(size_t offset) { 
        return (offset == 0) ? scm_Table0 : scm_Table1; 
    }

private:
    C4naRevCmp(void);

    static const Uint1*   scm_Tables[4];
    static const Uint1    scm_Table0[512];
    static const Uint1    scm_Table1[256];
};

/////////////////////////////////////////////////////////////////////////////
//
// Ambiguity Tables

// IUPACna

class CIupacnaAmbig
{
public:
    static const bool* GetTable(void) { return scm_Table; }

private:
    CIupacnaAmbig(void);

    static const bool    scm_Table[256];
};


// NCBI4na

class CNcbi4naAmbig
{
public:
    static const bool* GetTable(void) { return scm_Table; }

private:
    CNcbi4naAmbig(void);

    static const bool    scm_Table[256];
};


// NCBI8na

class CNcbi8naAmbig
{
public:
    static const bool* GetTable(void) { return scm_Table; }

private:
    CNcbi8naAmbig(void);

    static const bool    scm_Table[256];
};


/////////////////////////////////////////////////////////////////////////////
//
// Residue classification

struct SBestCodings {
    typedef CSeqUtil::ECoding TCoding;

    TCoding iupacna[256];
    TCoding ncbi4na[256];
    TCoding ncbi8na[256];
    TCoding ncbieaa[256];
    TCoding ncbi8aa[256];
};

extern const SBestCodings kBestCodingsWithGaps, kBestCodingsWithoutGaps;

END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL_TABLES__HPP */
