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
//       CArg_IOFile      : CArg_String
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
    virtual const CDir&    AsDirectory(void) const;

    virtual CNcbiIstream&  AsInputFile (TFileFlags flags = 0) const;
    virtual CNcbiOstream&  AsOutputFile(TFileFlags flags = 0) const;
    virtual CNcbiIostream& AsIOFile(TFileFlags flags = 0) const;
    virtual void           CloseFile   (void) const;
};


// Generates error (like CArg_NoValue) for excluded arguments.
class CArg_ExcludedValue : public CArgValue
{
public:
    CArg_ExcludedValue(const string& name);
    virtual bool HasValue(void) const;

    virtual const string&  AsString (void) const;
    virtual Int8           AsInt8   (void) const;
    virtual int            AsInteger(void) const;
    virtual double         AsDouble (void) const;
    virtual bool           AsBoolean(void) const;
    virtual const CDir&    AsDirectory(void) const;

    virtual CNcbiIstream&  AsInputFile (TFileFlags flags = 0) const;
    virtual CNcbiOstream&  AsOutputFile(TFileFlags flags = 0) const;
    virtual CNcbiIostream& AsIOFile(TFileFlags flags = 0) const;
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
    virtual const CDir&    AsDirectory(void) const;

    virtual CNcbiIstream&  AsInputFile (TFileFlags flags = 0) const;
    virtual CNcbiOstream&  AsOutputFile(TFileFlags flags = 0) const;
    virtual CNcbiIostream& AsIOFile(TFileFlags flags = 0) const;
    virtual void           CloseFile   (void) const;

    virtual const TStringArray& GetStringList() const;
    virtual TStringArray& SetStringList();

private:
    /// Value of the argument as passed to the constructor ("value")
    /// becomes the first element in the value list
    /// AsString() and other methods then use it 
    TStringArray  m_StringList;
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



class CArg_Flag : public CArg_Boolean
{
public:
    CArg_Flag(const string& name, bool value);
    virtual bool HasValue(void) const;
};


class CArg_Dir : public CArg_String
{
public:
    CArg_Dir(const string& name, const string& value,
                    CArgDescriptions::TFlags flags);
    virtual ~CArg_Dir(void);
    virtual const CDir&  AsDirectory() const;
protected:
    CDir         m_Dir;
    TFileFlags   m_DescriptionFlags;
};


class CArg_Ios : public CArg_String
{
public:
    CArg_Ios(const string& name, const string& value,
                    CArgDescriptions::TFlags flags);
    virtual ~CArg_Ios(void);

    virtual CNcbiIstream&  AsInputFile(  TFileFlags flags = 0) const;
    virtual CNcbiOstream&  AsOutputFile( TFileFlags flags = 0) const;
    virtual CNcbiIostream& AsIOFile(     TFileFlags flags = 0) const;

    virtual void CloseFile(void) const;
    static IOS_BASE::openmode IosMode(TFileFlags flags);

protected:
    virtual void x_Open(TFileFlags flags) const;
    bool x_CreatePath(TFileFlags flags) const;

    TFileFlags             m_DescriptionFlags;
    mutable TFileFlags     m_CurrentFlags;
    mutable CNcbiIos*      m_Ios;
    mutable bool           m_DeleteFlag;
    mutable CFastMutex     m_AccessMutex;
};


class CArg_InputFile : public CArg_Ios
{
public:
    CArg_InputFile(const string& name, const string& value,
                    CArgDescriptions::TFlags flags);
protected:
    virtual void x_Open(TFileFlags flags) const;
};



class CArg_OutputFile : public CArg_Ios
{
public:
    CArg_OutputFile(const string& name, const string& value,
                    CArgDescriptions::TFlags flags);
protected:
    virtual void x_Open(TFileFlags flags) const;
};



class CArg_IOFile : public CArg_Ios
{
public:
    CArg_IOFile(const string& name, const string& value,
                    CArgDescriptions::TFlags flags);
protected:
    virtual void x_Open(TFileFlags flags) const;
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
    CArgDescMandatory(const string&            name,
                      const string&            comment,
                      CArgDescriptions::EType  type,
                      CArgDescriptions::TFlags flags);
    virtual ~CArgDescMandatory(void);

    CArgDescriptions::EType  GetType (void) const { return m_Type; }
    virtual CArgDescriptions::TFlags GetFlags(void) const { return m_Flags; }

    virtual string GetUsageSynopsis(bool name_only = false) const = 0;
    virtual string GetUsageCommentAttr(void) const;

    virtual CArgValue* ProcessArgument(const string& value) const;
    virtual CArgValue* ProcessDefault(void) const;

    virtual 
    void SetConstraint(const CArgAllow*                    constraint, 
                       CArgDescriptions::EConstraintNegate negate);
    virtual const CArgAllow* GetConstraint(void) const;
    virtual bool IsConstraintInverted() const;

    virtual const CArgErrorHandler* GetErrorHandler(void) const
        { return m_ErrorHandler.GetPointerOrNull(); }
    virtual void SetErrorHandler(CArgErrorHandler* err_handler)
        { m_ErrorHandler.Reset(err_handler); }

private:
    CArgDescriptions::EType              m_Type;
    CArgDescriptions::TFlags             m_Flags;
    CConstRef<CArgAllow>                 m_Constraint;
    CArgDescriptions::EConstraintNegate  m_NegateConstraint;
    CRef<CArgErrorHandler>               m_ErrorHandler;
};


class CArgDescOptional : virtual public CArgDescMandatory
{
public:
    CArgDescOptional(const string&            name,
                     const string&            comment,
                     CArgDescriptions::EType  type,
                     CArgDescriptions::TFlags flags);
    virtual ~CArgDescOptional(void);
    virtual CArgValue* ProcessDefault(void) const;
    virtual size_t GetGroup(void) const { return m_Group; }
    virtual void SetGroup(size_t group) { m_Group = group; }

private:
    size_t m_Group;
};



class CArgDescDefault : virtual public CArgDescOptional
{
public:
    CArgDescDefault(const string&            name,
                    const string&            comment,
                    CArgDescriptions::EType  type,
                    CArgDescriptions::TFlags flags,
                    const string&            default_value,
                    const string&            env_var);
    virtual ~CArgDescDefault(void);

    const string& GetDefaultValue(void) const;

    virtual CArgValue* ProcessDefault(void) const;
    virtual void       VerifyDefault (void) const;

private:
    string m_DefaultValue;
    string m_EnvVar;
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
    CArgDesc_Flag(const string& name,
                  const string& comment,
                  bool          set_value = true);
    virtual ~CArgDesc_Flag(void);

    virtual string GetUsageSynopsis(bool name_only = false) const;
    virtual string GetUsageCommentAttr(void) const;

    virtual CArgValue* ProcessArgument(const string& value) const;
    virtual CArgValue* ProcessDefault(void) const;
    virtual size_t GetGroup(void) const { return m_Group; }
    virtual void SetGroup(size_t group) { m_Group = group; }
    
    bool GetSetValue(void) const { return m_SetValue;}

private:
    size_t  m_Group;
    bool    m_SetValue;  // value to set if the arg is provided  
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



class CArgDesc_Opening : virtual public CArgDescMandatory
{
public:
    CArgDesc_Opening(const string&            name,
                 const string&            comment,
                 CArgDescriptions::EType  type,
                 CArgDescriptions::TFlags flags);
    virtual ~CArgDesc_Opening(void);
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
                    const string&            default_value,
                    const string&            env_var);
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
                    const string&            default_value,
                    const string&            env_var);
    virtual ~CArgDesc_KeyDef(void);
    virtual string GetUsageSynopsis(bool name_only = false) const;
};


// Special case - arg synonym. Can be used e.g. to create short and
// long argument names.

class CArgDesc_Alias : public CArgDesc
{
public:
    // Create an argument alias.
    // alias is a new name for the existing argument, arg_name is
    // its original name. Any search functions will return the original
    // argument rather than the alias.
    CArgDesc_Alias(const string& alias,
                   const string& arg_name,
                   const string& comment);
    virtual ~CArgDesc_Alias(void);

    const string& GetAliasedName(void) const;

    // Dummy methods - to make the class not abstract
    virtual string GetUsageSynopsis(bool name_only) const;
    virtual string GetUsageCommentAttr(void) const;
    virtual CArgValue* ProcessArgument(const string& value) const;
    virtual CArgValue* ProcessDefault(void) const;

    void SetNegativeFlag(bool value) { m_NegativeFlag = value; }
    bool GetNegativeFlag(void) const { return m_NegativeFlag; }
private:
    string m_ArgName;
    bool   m_NegativeFlag;
};
