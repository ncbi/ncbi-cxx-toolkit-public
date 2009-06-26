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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <klib/rc.h>
#include <align/align-access.h>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static const bool trace = 0;

/////////////////////////////////////////////////////////////////////////////
//  CFetchAnnotsApp::


class CFetchAnnotsApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CFetchAnnotsApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("sra", "SRA_Accession",
                            "Accession and spot to load",
                            CArgDescriptions::eString);

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)

class CBamException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that  corelib can generate.
    ///
    /// These generic error conditions can occur for corelib applications.
    enum EErrCode {
        eNullPtr,       ///< Null pointer error
        eAddRefFailed,  ///< AddRef failed
        eInvalidArg,    ///< Invalid argument error
        eOtherError
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CBamException, CException);
};


const char* CBamException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNullPtr:    return "eNullPtr";
    case eAddRefFailed: return "eAddRefFailed";
    case eInvalidArg: return "eInvalidArg";
    case eOtherError: return "eOtherError";
    default:          return CException::GetErrCodeString();
    }
}


class CBamRefBase
{
public:
    void Log(const char* msg, const void* object, const char* type_str) const;
};

void CBamRefBase::Log(const char* msg,
                      const void* object,
                      const char* type_str) const
{
    if ( trace && (object || msg[1]) ) {
        static int count = 100000;
        //const char* type_str = type.name();
        LOG_POST(type_str);
        const char* start = strstr(type_str, "AlignAccess");
        const char* end = strstr(start, "S1_XadL");
        if ( !end ) end = strstr(start, "KS0_XadL");
        LOG_POST(this<<" "<<++count<<" "<<string(start, end)<<
                 " "<<msg<<" "<<object);
    }
}


template<class Object, class FObject,
         rc_t (*FRelease)(FObject*),
         rc_t (*FAddRef)(FObject*)>
class CBamRef : public CBamRefBase
{
protected:
    typedef CBamRef<Object, FObject, FRelease, FAddRef> TSelf;
public:
    typedef Object TObject;
    void Log(const char* msg) const
        {
            CBamRefBase::Log(msg, m_Object, typeid(*this).name());
        }

    CBamRef(void)
        : m_Object(0)
        {
        }
    CBamRef(const TSelf& ref)
        : m_Object(s_AddRef(ref))
        {
            Log("+");
        }
    TSelf& operator=(const TSelf& ref)
        {
            if ( this != &ref ) {
                Release();
                m_Object = s_AddRef(ref);
                Log("+");
            }
            return *this;
        }
    ~CBamRef(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Object ) {
                Log("-");
                FRelease(m_Object);
                m_Object = 0;
            }
        }
    
    operator TObject*(void) const
        {
            if ( !m_Object ) {
                NCBI_THROW_FMT(CBamException, eNullPtr,
                               "Null SRA pointer");
            }
            return m_Object;
        }

    DECLARE_OPERATOR_BOOL(m_Object);

protected:
    TObject** x_InitPtr(void)
        {
            Release();
            Log("init");
            return &m_Object;
        }

protected:
    static TObject* s_AddRef(const TSelf& ref)
        {
            TObject* obj = ref.m_Object;
            if ( obj ) {
                if ( rc_t rc = FAddRef(obj) ) {
                    NCBI_THROW_FMT(CBamException, eAddRefFailed,
                                   "Cannot add ref: "<<rc);
                }
            }
            return obj;
        }

private:
    TObject* m_Object;
};

class CBamMgr;
class CBamDb;
class CBamAlignIterator;

class CBamMgr : public CBamRef<const AlignAccessMgr, const AlignAccessMgr,
                               AlignAccessMgrRelease,
                               AlignAccessMgrAddRef>
{
public:
    CBamMgr(void);
};

class CBamDb : public CBamRef<const AlignAccessDB, const AlignAccessDB,
                              AlignAccessDBRelease,
                              AlignAccessDBAddRef>
{
public:
    CBamDb(const CBamMgr& mgr, const string& db_name);
    CBamDb(const CBamMgr& mgr, const string& db_name, const string& idx_name);

    const string& GetDbName(void) const
        {
            return m_DbName;
        }

private:
    string m_DbName;
};


class CBamString
{
public:
    CBamString(void)
        : m_Size(0), m_Capacity(0)
        {
        }
    CBamString(size_t capacity)
        : m_Size(0)
        {
            reserve(capacity);
        }

    void clear()
        {
            m_Size = 0;
        }
    size_t capacity() const
        {
            return m_Capacity;
        }
    void reserve(size_t min_capacity)
        {
            size_t capacity = m_Capacity;
            if ( capacity == 0 ) {
                capacity = min_capacity;
            }
            else {
                while ( capacity < min_capacity ) {
                    capacity <<= 1;
                }
            }
            m_Buffer.reset(new char[capacity]);
            m_Capacity = capacity;
        }

    size_t size() const
        {
            return m_Size;
        }
    bool empty(void) const
        {
            return m_Size == 0;
        }
    const char* data() const
        {
            return m_Buffer.get();
        }
    char operator[](size_t pos) const
        {
            return m_Buffer[pos];
        }
    operator string() const
        {
            return string(data(), size());
        }

protected:
    friend class CBamAlignIterator;
    friend class CBamRefSeqIterator;

    char* x_data()
        {
            return m_Buffer.get();
        }
    void x_resize(size_t size)
        {
            m_Size = size;
        }

private:
    size_t m_Size;
    size_t m_Capacity;
    AutoArray<char> m_Buffer;
};

CNcbiOstream& operator<<(CNcbiOstream& out, const CBamString& str)
{
    return out.write(str.data(), str.size());
}


class CBamRefSeqIterator : public CBamRef<AlignAccessRefSeqEnumerator,
                                          const AlignAccessRefSeqEnumerator,
                                          AlignAccessRefSeqEnumeratorRelease,
                                          AlignAccessRefSeqEnumeratorAddRef>
{
public:
    CBamRefSeqIterator(const CBamDb& bam_db);

    CBamRefSeqIterator(const CBamRefSeqIterator& iter);
    CBamRefSeqIterator& operator=(const CBamRefSeqIterator& iter);

    DECLARE_OPERATOR_BOOL(!m_LocateRC);

    CBamRefSeqIterator& operator++(void);

    const CBamString& GetRefSeqId(void);
    TSeqPos GetRefSeqPos(void);

    const CBamString& GetShortSeqId(void);
    const CBamString& GetShortSequence(void);

    TSeqPos GetCIGARPos(void);
    const CBamString& GetCIGAR(void);

private:
    typedef rc_t (*TGetString)(const AlignAccessRefSeqEnumerator *self,
                               char *buffer, size_t bsize, size_t *size);

    void x_AllocBuffers(void);
    void x_InvalidateBuffers(void);

    void x_CheckValid(void) const;
    bool x_CheckRC(CBamString& buf, rc_t rc, size_t size, const char* msg);
    void x_GetString(CBamString& buf,
                     const char* msg, TGetString func);

    rc_t m_LocateRC;
    CBamString m_RefSeqId;
};


class CBamAlignIterator : public CBamRef<AlignAccessAlignmentEnumerator,
                                         const AlignAccessAlignmentEnumerator,
                                         AlignAccessAlignmentEnumeratorRelease,
                                         AlignAccessAlignmentEnumeratorAddRef>
{
public:
    CBamAlignIterator(const CBamDb& bam_db);
    CBamAlignIterator(const CBamDb& bam_db,
                      const string& ref_id,
                      TSeqPos ref_pos);

    CBamAlignIterator(const CBamAlignIterator& iter);
    CBamAlignIterator& operator=(const CBamAlignIterator& iter);

    DECLARE_OPERATOR_BOOL(!m_LocateRC);

    CBamAlignIterator& operator++(void);

    const CBamString& GetRefSeqId(void);
    TSeqPos GetRefSeqPos(void);

    const CBamString& GetShortSeqId(void);
    const CBamString& GetShortSequence(void);

    TSeqPos GetCIGARPos(void);
    const CBamString& GetCIGAR(void);

private:
    typedef rc_t (*TGetString)(const AlignAccessAlignmentEnumerator *self,
                               char *buffer, size_t bsize, size_t *size);
    typedef rc_t (*TGetString2)(const AlignAccessAlignmentEnumerator *self,
                                uint64_t *start_pos,
                                char *buffer, size_t bsize, size_t *size);

    void x_AllocBuffers(void);
    void x_InvalidateBuffers(void);

    void x_CheckValid(void) const;
    bool x_CheckRC(CBamString& buf, rc_t rc, size_t size, const char* msg);
    void x_GetString(CBamString& buf,
                     const char* msg, TGetString func);
    void x_GetString(CBamString& buf, uint64_t& pos,
                     const char* msg, TGetString2 func);

    rc_t m_LocateRC;
    CBamString m_RefSeqId;
    CBamString m_ShortSeqId;
    CBamString m_ShortSequence;
    uint64_t m_CIGARPos;
    CBamString m_CIGAR;
};


CBamMgr::CBamMgr(void)
{
    if ( rc_t rc = AlignAccessMgrMake(x_InitPtr()) ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Cannot create AlignAccessMgr: " << rc);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name)
    : m_DbName(db_name)
{
    if ( rc_t rc = AlignAccessMgrMakeBAMDB(mgr, x_InitPtr(),
                                           db_name.c_str()) ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Cannot open BAM DB: " << rc
                       << ": " << db_name);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name,
               const string& idx_name)
    : m_DbName(db_name)
{
    if ( rc_t rc = AlignAccessMgrMakeIndexBAMDB(mgr, x_InitPtr(),
                                                db_name.c_str(),
                                                idx_name.c_str()) ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Cannot open BAM DB: " << rc
                       << ": " << db_name);
    }
}


/////////////////////////////////////////////////////////////////////////////

CBamRefSeqIterator::CBamRefSeqIterator(const CBamDb& bam_db)
{
    m_LocateRC = AlignAccessDBEnumerateRefSequences(bam_db, x_InitPtr());
    x_AllocBuffers();
}


void CBamRefSeqIterator::x_AllocBuffers(void)
{
    m_RefSeqId.reserve(32);
}


void CBamRefSeqIterator::x_InvalidateBuffers(void)
{
    m_RefSeqId.clear();
}


CBamRefSeqIterator::CBamRefSeqIterator(const CBamRefSeqIterator& iter)
    : TSelf(iter),
      m_LocateRC(iter.m_LocateRC)
{
    x_AllocBuffers();
}


CBamRefSeqIterator& CBamRefSeqIterator::operator=(const CBamRefSeqIterator& iter)
{
    if ( this != &iter ) {
        x_InvalidateBuffers();
        TSelf::operator=(iter);
        m_LocateRC = iter.m_LocateRC;
    }
    return *this;
}


void CBamRefSeqIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CBamException, eOtherError,
                   "CBamRefSeqIterator is invalid");
    }
}


CBamRefSeqIterator& CBamRefSeqIterator::operator++(void)
{
    x_CheckValid();
    x_InvalidateBuffers();
    m_LocateRC = AlignAccessRefSeqEnumeratorNext(*this);
    return *this;
}


bool CBamRefSeqIterator::x_CheckRC(CBamString& buf,
                                  rc_t rc,
                                  size_t size,
                                  const char* msg)
{
    if ( rc == 0 ) {
        // no error, update size and finish
        if ( size > 0 && buf[size-1] == '\0' ) {
            // omit trailing zero char
            --size;
        }
        buf.x_resize(size);
        return true;
    }
    else if ( GetRCState(rc) == rcInsufficient && size > buf.capacity() ) {
        // buffer too small, realloc and repeat
        buf.reserve(size);
        return false;
    }
    else {
        // other failure
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Cannot get " << msg << ": " << rc);
    }
}


void CBamRefSeqIterator::x_GetString(CBamString& buf,
                                    const char* msg, TGetString func)
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(*this, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


const CBamString& CBamRefSeqIterator::GetRefSeqId(void)
{
    x_GetString(m_RefSeqId, "RefSeqId",
                AlignAccessRefSeqEnumeratorGetID);
    return m_RefSeqId;
}


/////////////////////////////////////////////////////////////////////////////

CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db)
{
    m_LocateRC = AlignAccessDBEnumerateAlignments(bam_db, x_InitPtr());
    x_AllocBuffers();
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db,
                                     const string& ref_id,
                                     TSeqPos ref_pos)
{
    m_LocateRC = AlignAccessDBWindowedAlignments(bam_db, x_InitPtr(),
                                                 ref_id.c_str(), ref_pos, 0);
    x_AllocBuffers();
}


void CBamAlignIterator::x_AllocBuffers(void)
{
    m_RefSeqId.reserve(32);
    m_ShortSeqId.reserve(32);
    m_ShortSequence.reserve(256);
    m_CIGAR.reserve(32);
}


void CBamAlignIterator::x_InvalidateBuffers(void)
{
    m_RefSeqId.clear();
    m_ShortSeqId.clear();
    m_ShortSequence.clear();
    m_CIGAR.clear();
}


CBamAlignIterator::CBamAlignIterator(const CBamAlignIterator& iter)
    : TSelf(iter),
      m_LocateRC(iter.m_LocateRC)
{
    x_AllocBuffers();
}


CBamAlignIterator& CBamAlignIterator::operator=(const CBamAlignIterator& iter)
{
    if ( this != &iter ) {
        x_InvalidateBuffers();
        TSelf::operator=(iter);
        m_LocateRC = iter.m_LocateRC;
    }
    return *this;
}


void CBamAlignIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CBamException, eOtherError,
                   "CBamAlignIterator is invalid");
    }
}


CBamAlignIterator& CBamAlignIterator::operator++(void)
{
    x_CheckValid();
    x_InvalidateBuffers();
    m_LocateRC = AlignAccessAlignmentEnumeratorNext(*this);
    return *this;
}


bool CBamAlignIterator::x_CheckRC(CBamString& buf,
                                  rc_t rc,
                                  size_t size,
                                  const char* msg)
{
    if ( rc == 0 ) {
        // no error, update size and finish
        if ( size > 0 && buf[size-1] == '\0' ) {
            // omit trailing zero char
            --size;
        }
        buf.x_resize(size);
        return true;
    }
    else if ( GetRCState(rc) == rcInsufficient && size > buf.capacity() ) {
        // buffer too small, realloc and repeat
        buf.reserve(size);
        return false;
    }
    else {
        // other failure
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Cannot get " << msg << ": " << rc);
    }
}


void CBamAlignIterator::x_GetString(CBamString& buf,
                                    const char* msg, TGetString func)
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(*this, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


void CBamAlignIterator::x_GetString(CBamString& buf, uint64_t& pos,
                                    const char* msg, TGetString2 func)
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(*this, &pos, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


const CBamString& CBamAlignIterator::GetRefSeqId(void)
{
    x_GetString(m_RefSeqId, "RefSeqId",
                AlignAccessAlignmentEnumeratorGetRefSeqID);
    return m_RefSeqId;
}


TSeqPos CBamAlignIterator::GetRefSeqPos(void)
{
    x_CheckValid();
    uint64_t pos = 0;
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetRefSeqPos(*this, &pos) ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Cannot get RefSeqPos: " << rc);
    }
    return TSeqPos(pos);
}


const CBamString& CBamAlignIterator::GetShortSeqId(void)
{
    x_GetString(m_ShortSeqId, "ShortSeqId",
                AlignAccessAlignmentEnumeratorGetShortSeqID);
    return m_ShortSeqId;
}



const CBamString& CBamAlignIterator::GetShortSequence(void)
{
    x_GetString(m_ShortSequence, "ShortSequence",
                AlignAccessAlignmentEnumeratorGetShortSequence);
    return m_ShortSequence;
}



TSeqPos CBamAlignIterator::GetCIGARPos(void)
{
    x_GetString(m_CIGAR, m_CIGARPos, "CIGAR",
                AlignAccessAlignmentEnumeratorGetCIGAR);
    return m_CIGARPos;
}


const CBamString& CBamAlignIterator::GetCIGAR(void)
{
    x_GetString(m_CIGAR, m_CIGARPos, "CIGAR",
                AlignAccessAlignmentEnumeratorGetCIGAR);
    return m_CIGAR;
}



/////////////////////////////////////////////////////////////////////////////


void test(CBamMgr& mgr, const char* bam_name, const char* index_name)
{
    NcbiCout << "File: " << bam_name << NcbiEndl;
    CBamDb bam_db(mgr, bam_name);//, index_name);
    if ( 1 ) {
        for ( CBamRefSeqIterator it(bam_db); it; ++it ) {
            NcbiCout << "RefSeq: " << it.GetRefSeqId()
                     << '\n';
        }
    }
    if ( 1 ) {
        size_t count = 0;
        for ( CBamAlignIterator it(bam_db); it; ++it ) {
            NcbiCout << "Ref: " << it.GetRefSeqId()
                     << " at " << it.GetRefSeqPos()
                     << '\n';
            NcbiCout << "Seq: " << it.GetShortSeqId()
                     << ": " << it.GetShortSequence() 
                     << '\n';
            NcbiCout << "CIGAR: " << it.GetCIGARPos()
                     << ": " << it.GetCIGAR()
                     << '\n';
            if ( ++count == 100000 ) break;
        }
        NcbiCout << "Loaded: " << count << " alignments." << NcbiEndl;
    }
}

#define BAM_DIR "/panfs/traces01.be-md.ncbi.nlm.nih.gov/aspera/ebi/test/april09_bam_files/trios_bam/"
#define BAM_FILE1 "NA19240.chromMT.454.SRP000032.2009_04.bam"
#define BAM_FILE2 "NA19240.chromMT.SLX.SRP000032.2009_04.bam"
#define BAM_FILE3 "NA19240.chrom3.SLX.SRP000032.2009_04.bam"

int CFetchAnnotsApp::Run(void)
{
    // Get arguments
    //const CArgs& args = GetArgs();

    CBamMgr mgr;
    test(mgr, BAM_DIR BAM_FILE1, BAM_DIR BAM_FILE1 ".bai");
    //test(mgr, BAM_DIR BAM_FILE2, BAM_DIR BAM_FILE2 ".bai");
    //test(mgr, BAM_DIR BAM_FILE3, BAM_DIR BAM_FILE3 ".bai");
    NcbiCout << "Exiting" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CFetchAnnotsApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CFetchAnnotsApp().AppMain(argc, argv);
}
