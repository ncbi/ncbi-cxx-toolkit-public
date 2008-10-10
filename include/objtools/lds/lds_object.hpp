#ifndef LDS_OBJECT_HPP__
#define LDS_OBJECT_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Different operations on LDS Object table
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/format_guess.hpp>

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_expt.hpp>
#include <objtools/lds/lds_coreobjreader.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CObjectManager;
class CScope;
class CSeq_entry;
class CLDS_Database;
class CLDS_Seq_idsCollector;

//////////////////////////////////////////////////////////////////
///
/// SLDS_ObjectDB and SLDS_AnnotDB related methods.
///

class NCBI_LDS_EXPORT CLDS_Object
{
public:
    CLDS_Object(CLDS_Database& db, const map<string, int>& obj_map);
    ~CLDS_Object();


    /// Turn id duplication control.
    /// When turned ON LDS will throw an excaption if files 
    /// contain sequences with duplicate ids
    void ControlDuplicateIds(bool flag = true) { m_ControlDupIds = flag; }

    enum EGBReleaseMode {
        eNoGBRelease,
        eGuessGBRelease,
        eForceGBRelease,
        eDefaultGBReleaseMode = eGuessGBRelease
    };
    /// Control GBRelease file detection
    void ControlGBRelease(EGBReleaseMode mode) { m_GBReleaseMode = mode; }
    EGBReleaseMode GetGBReleasMode(void) const { return m_GBReleaseMode; }

    /// Delete all objects living in the specified set of files.
    /// All deleted ids are added to deleted set.
    void DeleteCascadeFiles(const CLDS_Set& file_ids, 
                            CLDS_Set* objects_deleted,
                            CLDS_Set* annotations_deleted);

    /// Reload all objects in given set of files
    void UpdateCascadeFiles(const CLDS_Set& file_ids);

    void UpdateFileObjects(int file_id,
                           const string& file_name,
                           CFormatGuess::EFormat format);

    /// Load and index one object from binary ASN.1 stream.
    /// Returns false if no more objects found.
    bool UpdateBinaryASNObject(CObjectIStream& in,
                               CLDS_CoreObjectsReader& objects,
                               CObjectTypeInfo type);
    void UpdateBinaryASNObjects(int file_id, const string& file_name);

    /// Return max record id in "object" and "annotation" tables. 
    /// Both tables share the same objects sequence numbering.
    /// Function returns 0 if no record were found.
    int FindMaxObjRecId();

    int SaveObjects(CLDS_CoreObjectsReader& objects,
                    bool internal);
    
public:

    /// Checks if parsed object is a "bio object" (returns TRUE) 
    /// or an annotation. if applicable object_str_id parameter receives
    /// objject or annotation id. (fasta format).
    /// If possible function extracts objects' title and molecule type
    /// (0 - unknown, 1 - NA, 2 - protein)
    bool IsObject(const CLDS_CoreObjectsReader::SObjectDetails& parse_info,
                  string* object_str_id, 
                  string* object_title);

    /// Save object to the database, return record id.
    /// NOTE: This function recursively finds all objects' parents and saves
    /// the whole genealogy tree (not only the immediate argument).
    int SaveObject(CLDS_CoreObjectsReader* objects,
                   CLDS_CoreObjectsReader::SObjectDetails* obj_info);

    /// Save object information, return record id. This function is specific 
    /// for fasta format.
    ///
    /// @param seq_ids
    ///    List of all ids (space delimited string)
    int SaveObject(int            file_id,
                   const string&  seq_id,
                   const string&  description,
                   const string&  seq_ids,
                   CNcbiStreampos pos,
                   int            type_id);

    /// Rebuild sequence id index
    void BuildSeqIdIdx();

    CScope* GetScope(void);

private:
    CLDS_Object(const CLDS_Object&);
    CLDS_Object& operator=(const CLDS_Object&);

    friend class CLDS_FastaScanner;
private:
    CLDS_Database&          m_DataBase;
    SLDS_TablesCollection&  m_db;
    const map<string, int>& m_ObjTypeMap;

    /// Max. id in "object" and "annotation" table
    int                     m_MaxObjRecId;
    CRef<CObjectManager>    m_TSE_Manager;   ///< OM for top level seq entry
    CObjectInfo             m_TSE_Info;
    CRef<CSeq_entry>        m_TSE;
    CRef<CScope>            m_Scope;         ///< OM Scope
    bool                    m_ControlDupIds; ///< Control duplicates
    EGBReleaseMode          m_GBReleaseMode;
    CRef<CLDS_Seq_idsCollector> m_Seq_idsCollector;
};


class CSeq_id;

struct SLDS_SeqIdBase;


/// Extract indexing base out of sequence id
NCBI_LDS_EXPORT
void LDS_GetSequenceBase(const CSeq_id&   seq_id, 
                         SLDS_SeqIdBase*  seqid_base);

/// Extract indexing base out of sequence id
///
/// @return 
///    TRUE if string conversion successful
///
NCBI_LDS_EXPORT
bool LDS_GetSequenceBase(const string&   seq_id_str, 
                         SLDS_SeqIdBase* seqid_base,
                         CSeq_id*        conv_seq_id = 0);




END_SCOPE(objects)
END_NCBI_SCOPE

#endif
