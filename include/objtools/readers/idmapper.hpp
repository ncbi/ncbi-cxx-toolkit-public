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
 * Author:  Frank Ludwig
 *
 * File Description: Definition of the IIdMapper interface and its 
 *          implementation
 *
 */

#ifndef OBJTOOLS_IDMAPPER___IDMAPPER__HPP
#define OBJTOOLS_IDMAPPER___IDMAPPER__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

/// Functor for CSeq_id_Handle comparision
///
/// Will test given CSeq_id_Handles for equality- for the purpose of ID mapping,
/// which is more lenient than strict identity.
///
struct HandleComparator
{
    bool operator()(
        CSeq_id_Handle,
        CSeq_id_Handle ) const;
};


/// General IdMapper interface
///
/// This interface should suffice for typical IdMapper use, regardless of the 
/// actual inplementation.
///
class NCBI_XOBJREAD_EXPORT IIdMapper
{
public:
    virtual ~IIdMapper() {};
    
    /// Map a single given CSeq_id_Handle to another.
    /// @return 
    ///   the mapped handle, or an invalid handle if a mapping is not possible. 
    virtual CSeq_id_Handle
    Map(
        const CSeq_id_Handle& ) =0;

    /// Map all embedded IDs in a given object at once.
    virtual void 
    MapObject(
        CSerialObject& ) =0;
    
    /// Retrieve a catalog of all mapping contexts supported by this class.    
    static void
    GetSupportedContexts(
        std::vector<std::string>& contexts ) { contexts.clear(); }; 

};


/// IdMapper base class implementation
///
/// Provides the means to set up and maintain an internal table of mappings
/// and to use such table for actual ID mapping.
/// Actual initialization of the internal table is left for derived classes to 
/// implement.
///  
class NCBI_XOBJREAD_EXPORT CIdMapper: public IIdMapper
{
protected:
    typedef std::map< CSeq_id_Handle, CSeq_id_Handle, HandleComparator > CACHE;
    
public:
    /// Constructor specifying the mapping context, direction, and error handling.
    /// @param strContext
    ///   the mapping context or genome source IDs will belong to. Something like
    ///   "mm6" or "hg18".
    /// @param bInvert
    ///   Mapping direction. "true" will map in reverse direction.
    /// @param pErrors
    ///   Optional error container. If specified, mapping errors will be passed to
    ///   the error container for further processing. If not specified, mapping
    ///   errors result in exceptions that need to be handled.
    CIdMapper(
        const std::string& strContext = "",
        bool bInvert = false,
        IErrorContainer* pErrors = 0 )
    : 
        m_strContext(strContext), 
        m_bInvert(bInvert),
        m_pErrors( pErrors ) 
    {};
    
    /// Add a mapping to the internal mapping table.
    /// @param from
    ///   source handle, or target handle in the case of reverse mapping
    /// @param to
    ///   target handle, or source handle in the case of reverse mapping
    virtual void
    AddMapping(
        const CSeq_id_Handle& from,
        const CSeq_id_Handle& to ) 
    {
        if (!m_bInvert) {
            m_Cache[ from ] = to;
        } else {
            m_Cache[ to ] = from;
        }
    };
        
    virtual CSeq_id_Handle
    Map(
        const CSeq_id_Handle& ); 
        
    virtual void 
    MapObject(
        CSerialObject& ); 

protected:
    static std::string
    MapErrorString(
        const CSeq_id_Handle& );
    
    virtual void
    InitializeCache() {};
    
    const std::string m_strContext;
    const bool m_bInvert;
    CACHE m_Cache;
    IErrorContainer* m_pErrors;   
};

            
/// IdMapper implementation using hardcoded values
///
/// Mapping targets are fixed at compile time and cannot be modified later. 
/// Useful for self contained applications that should work without external
/// configuration files or databases.
///  
class NCBI_XOBJREAD_EXPORT CIdMapperBuiltin: public CIdMapper
{

public:
    /// Constructor specifying the mapping context, direction, and error handling.
    /// @param strContext
    ///   the mapping context or genome source IDs will belong to. Something like
    ///   "mm6" or "hg18".
    /// @param bInvert
    ///   Mapping direction. "true" will map in reverse direction.
    /// @param pErrors
    ///   Optional error container. If specified, mapping errors will be passed to
    ///   the error container for further processing. If not specified, mapping
    ///   errors result in exceptions that need to be handled.
    CIdMapperBuiltin(
        const std::string& strContext,
        bool bInvert = false,
        IErrorContainer* pErrors = 0 ): CIdMapper(strContext, bInvert, pErrors)
    {
        InitializeCache();
    };
        
    static void
    GetSupportedContexts(
        std::vector<std::string>& ); 

protected:
    virtual void
    InitializeCache();
    
    void
    AddMapEntry(
        const std::string&,
        int );
};


/// IdMapper implementation using an external configuration file
///
/// The internal mapping table will be initialized during IdMapper construction
/// from a given input stream (typically, an open configuration file).
///  
class NCBI_XOBJREAD_EXPORT CIdMapperConfig: public CIdMapper
{

public:
    /// Constructor specifying the content of the mapping table, mapping context, 
    /// direction, and error handling.
    /// @param istr
    ///   open input stream containing tabbed data specifying map sources and 
    ///   targets.
    /// @param strContext
    ///   the mapping context or genome source IDs will belong to. Something like
    ///   "mm6" or "hg18".
    /// @param bInvert
    ///   Mapping direction. "true" will map in reverse direction.
    /// @param pErrors
    ///   Optional error container. If specified, mapping errors will be passed to
    ///   the error container for further processing. If not specified, mapping
    ///   errors result in exceptions that need to be handled.
    CIdMapperConfig(
        CNcbiIstream& istr,
        const std::string& strContext = "",
        bool bInvert = false,
        IErrorContainer* pErrors = 0)
        : CIdMapper(strContext, bInvert, pErrors), m_Istr(istr)
    {
        InitializeCache();
    };
    
protected:
    virtual void
    InitializeCache();
    
    void
    AddMapEntry(
        const std::string& );

    void
    SetCurrentContext(
        const std::string&,
        std::string& );
    
    CSeq_id_Handle
    SourceHandle(
        const std::string& );
        
    CSeq_id_Handle
    TargetHandle(
        const std::string& );
            
    CNcbiIstream& m_Istr;
};


/// IdMapper implementation using an external database
///
/// Mappings will be retrived from an external database, then cached internally
/// for future reuse.
///  
class NCBI_XOBJREAD_EXPORT CIdMapperDatabase: public CIdMapper
{
public:
    /// Constructor specifying a database containing the actual mapping, the mapping 
    /// context, direction, and error handling.
    /// @param strServer
    ///   server on which the mapping database resides.
    /// @param strDatabase
    ///   the actual database on the specified server.
    /// @param strContext
    ///   the mapping context or genome source IDs will belong to. Something like
    ///   "mm6" or "hg18".
    /// @param bInvert
    ///   Mapping direction. "true" will map in reverse direction.
    /// @param pErrors
    ///   Optional error container. If specified, mapping errors will be passed to
    ///   the error container for further processing. If not specified, mapping
    ///   errors result in exceptions that need to be handled.
    CIdMapperDatabase(
        const std::string& strServer,
        const std::string& strDatabase,
        const std::string& strContext,
        bool bInvert = false,
        IErrorContainer* pErrors = 0)
        : CIdMapper(strContext, bInvert, pErrors), 
          m_strServer(strServer), 
          m_strDatabase(strDatabase)
    {};
        
    virtual CSeq_id_Handle
    Map(
        const CSeq_id_Handle& from );
        
protected:
    const std::string m_strServer;
    const std::string m_strDatabase;
};
           
END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_IDMAPPER___IDMAPPER__HPP
