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
 * Authors:  Denis Vakatov
 *
 * File Description:
 *   PRIVATE header -- for inclusion by "ncbiargs.cpp" only!
 *   Command-line arguments' processing:
 *      descriptions  -- CArgDescriptions,  CArgDesc
 *      constraints   -- CArgAllow;  CArgAllow_{Strings,Integers,Int8s,Doubles}
 *      parsed values -- CArgs,             CArgValue
 *      exceptions    -- CArgException, ARG_THROW()
 *
 */


#if !defined(NCBIARGS__CPP)
#  error "PRIVATE header -- for inclusion by ncbiargs.cpp only!"
#endif


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArg_***::   classes representing various types of argument value
//
//    CArg_NoValue     : CArgValue
//
//    CArg_String      : CArgValue
//
//       CArg_Alnum       : CArg_String
//       CArg_Int8        : CArg_String
//          CArg_Integer  : CArg_Int8
//       CArg_Double      : CArg_String
//       CArg_Boolean     : CArg_String
//       CArg_InputFile   : CArg_String
//       CArg_OutputFile  : CArg_String
//    


class CArg_NoValue : public CArgValue
{
public:
    CArg_NoValue(const string& name);
    virtual bool HasValue(void) const;

    virtual const string&  AsString (void) const;
    virtual Int8           AsInt8   (void) const;
    virtual int            AsInteger(void) const;
    virtual double         AsDouble (void) const;
    virtual bool           AsBoolean(void) const;

    virtual CNcbiIstream&  AsInputFile (void) const;
    virtual CNcbiOstream&  AsOutputFile(void) const;
    virtual void           CloseFile   (void) const;
};



class CArg_String : public CArgValue
{
public:
    CArg_String(const string& name, const string& value);
    virtual bool HasValue(void) const;

    virtual const string&  AsString (void) const;
    virtual Int8           AsInt8   (void) const;
    virtual int            AsInteger(void) const;
    virtual double         AsDouble (void) const;
    virtual bool           AsBoolean(void) const;

    virtual CNcbiIstream&  AsInputFile (void) const;
    virtual CNcbiOstream&  AsOutputFile(void) const;
    virtual void           CloseFile   (void) const;
private:
    // Value of the argument as passed to the constructor ("value")
    string m_String;
};



class CArg_Int8 : public CArg_String
{
public:
    CArg_Int8(const string& name, const string& value);
    virtual Int8 AsInt8(void) const;
protected:
    Int8 m_Integer;
};



class CArg_Integer : public CArg_Int8
{
public:
    CArg_Integer(const string& name, const string& value);
    virtual int AsInteger(void) const;
};



class CArg_Double : public CArg_String
{
public:
    CArg_Double(const string& name, const string& value);
    virtual double AsDouble(void) const;
private:
    double m_Double;
};



class CArg_Boolean : public CArg_String
{
public:
    CArg_Boolean(const string& name, bool value);
    CArg_Boolean(const string& name, const string& value);
    virtual bool AsBoolean(void) const;
private:
    bool m_Boolean;
};



class CArg_InputFile : public CArg_String
{
public:
    CArg_InputFile(const string& name, const string& value,
                   IOS_BASE::openmode  openmode,
                   bool                delay_open);
    virtual ~CArg_InputFile(void);

    virtual CNcbiIstream& AsInputFile(void) const;
    virtual void CloseFile(void) const;
private:
    void x_Open(void) const;
    mutable IOS_BASE::openmode m_OpenMode;
    mutable CNcbiIstream*      m_InputFile;
    mutable bool               m_DeleteFlag;
};



class CArg_OutputFile : public CArg_String
{
public:
    CArg_OutputFile(const string& name, const string& value,
                    IOS_BASE::openmode openmode,
                    bool               delay_open);
    virtual ~CArg_OutputFile(void);

    virtual CNcbiOstream& AsOutputFile(void) const;
    virtual void CloseFile(void) const;
private:
    void x_Open(void) const;
    mutable IOS_BASE::openmode m_OpenMode;
    mutable CNcbiOstream*      m_OutputFile;
    mutable bool               m_DeleteFlag;
};




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArgDesc***::   abstract base classes for argument descriptions
//
//    CArgDesc
//
//    CArgDescMandatory  : CArgDesc
//    CArgDescOptional   : virtual CArgDescMandatory
//    CArgDescDefault    : virtual CArgDescOptional
//
//    CArgDescSynopsis
//



class CArgDescMandatory : public CArgDesc
{
public:
    CArgDescMandatory(const string& name, const string& comment,
                      CArgDescriptions::EType  type,
                      CArgDescriptions::TFlags flags);
    virtual ~CArgDescMandatory(void);

    CArgDescriptions::EType  GetType (void) const { return m_Type; }
    CArgDescriptions::TFlags GetFlags(void) const { return m_Flags; }

    virtual string GetUsageSynopsis(bool name_only = false) const = 0;
    virtual string GetUsageCommentAttr(void) const;

    virtual CArgValue* ProcessArgument(const string& value) const;
    virtual CArgValue* ProcessDefault(void) const;

    virtual void SetConstraint(CArgAllow* constraint);
    virtual const CArgAllow* GetConstraint(void) const;

private:
    CArgDescriptions::EType  m_Type;
    CArgDescriptions::TFlags m_Flags;
    CConstRef<CArgAllow>     m_Constraint;
};



class CArgDescOptional : virtual public CArgDescMandatory
{
public:
    CArgDescOptional(const string& name, const string& comment,
                     CArgDescriptions::EType  type,
                     CArgDescriptions::TFlags flags);
    virtual ~CArgDescOptional(void);
    virtual CArgValue* ProcessDefault(void) const;
};



class CArgDescDefault : virtual public CArgDescOptional
{
public:
    CArgDescDefault(const string& name, const string& comment,
                    CArgDescriptions::EType  type,
                    CArgDescriptions::TFlags flags,
                    const string&            default_value);
    virtual ~CArgDescDefault(void);

    const string& GetDefaultValue(void) const { return m_DefaultValue; }

    virtual CArgValue* ProcessDefault(void) const;
    virtual void       VerifyDefault (void) const;

private:
    string m_DefaultValue;
};



class CArgDescSynopsis
{
public:
    CArgDescSynopsis(const string& synopsis);
    const string& GetSynopsis(void) const { return m_Synopsis; }
private:
    string m_Synopsis;
};




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArgDesc_***::   classes for argument descriptions
//
//    CArgDesc_Flag    : CArgDesc
//
//    CArgDesc_Pos     : virtual CArgDescMandatory
//    CArgDesc_PosOpt  : virtual CArgDescOptional, CArgDesc_Pos
//    CArgDesc_PosDef  :         CArgDescDefault,  CArgDesc_PosOpt
//
//    CArgDescSynopsis
//
//    CArgDesc_Key     : CArgDesc_Pos,    CArgDescSynopsis
//    CArgDesc_KeyOpt  : CArgDesc_PosOpt, CArgDescSynopsis
//    CArgDesc_KeyDef  : CArgDesc_PosDef, CArgDescSynopsis
//


class CArgDesc_Flag : public CArgDesc
{
public:
    CArgDesc_Flag(const string& name, const string& comment,
                  bool set_value = true);
    virtual ~CArgDesc_Flag(void);

    virtual string GetUsageSynopsis(bool name_only = false) const;
    virtual string GetUsageCommentAttr(void) const;

    virtual CArgValue* ProcessArgument(const string& value) const;
    virtual CArgValue* ProcessDefault(void) const;

private:
    bool m_SetValue;  // value to set if the arg is provided  
};



class CArgDesc_Pos : virtual public CArgDescMandatory
{
public:
    CArgDesc_Pos(const string&            name,
                 const string&            comment,
                 CArgDescriptions::EType  type,
                 CArgDescriptions::TFlags flags);
    virtual ~CArgDesc_Pos(void);
    virtual string GetUsageSynopsis(bool name_only = false) const;
};



class CArgDesc_PosOpt : virtual public CArgDescOptional,
                        public CArgDesc_Pos
{
public:
    CArgDesc_PosOpt(const string&            name,
                    const string&            comment,
                    CArgDescriptions::EType  type,
                    CArgDescriptions::TFlags flags);
    virtual ~CArgDesc_PosOpt(void);
};



class CArgDesc_PosDef : public CArgDescDefault,
                        public CArgDesc_PosOpt
{
public:
    CArgDesc_PosDef(const string&            name,
                    const string&            comment,
                    CArgDescriptions::EType  type,
                    CArgDescriptions::TFlags flags,
                    const string&            default_value);
    virtual ~CArgDesc_PosDef(void);
};



class CArgDesc_Key : public CArgDesc_Pos, public CArgDescSynopsis
{
public:
    CArgDesc_Key(const string&            name,
                 const string&            comment,
                 CArgDescriptions::EType  type,
                 CArgDescriptions::TFlags flags,
                 const string&            synopsis);
    virtual ~CArgDesc_Key(void);
    virtual string GetUsageSynopsis(bool name_only = false) const;
};



class CArgDesc_KeyOpt : public CArgDesc_PosOpt, public CArgDescSynopsis
{
public:
    CArgDesc_KeyOpt(const string&            name,
                    const string&            comment,
                    CArgDescriptions::EType  type,
                    CArgDescriptions::TFlags flags,
                    const string&            synopsis);
    virtual ~CArgDesc_KeyOpt(void);
    virtual string GetUsageSynopsis(bool name_only = false) const;
};



class CArgDesc_KeyDef : public CArgDesc_PosDef, public CArgDescSynopsis
{
public:
    CArgDesc_KeyDef(const string&            name,
                    const string&            comment,
                    CArgDescriptions::EType  type,
                    CArgDescriptions::TFlags flags,
                    const string&            synopsis,
                    const string&            default_value);
    virtual ~CArgDesc_KeyDef(void);
    virtual string GetUsageSynopsis(bool name_only = false) const;
};


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/07/22 15:26:09  vakatov
 * Allow "Int8" arguments
 *
 * Revision 1.5  2002/12/18 22:54:48  dicuccio
 * Shuffled some headers to avoid a potentially serious compiler warning
 * (deletion of incomplete type in Windows).
 *
 * Revision 1.4  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.3  2002/04/11 21:08:01  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.2  2001/01/22 23:07:15  vakatov
 * CArgValue::AsInteger() to return "int" (rather than "long")
 *
 * Revision 1.1  2000/12/24 00:05:46  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
