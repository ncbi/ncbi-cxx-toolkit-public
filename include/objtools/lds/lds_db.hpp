#ifndef LDS_DB_HPP__
#define LDS_DB_HPP__
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
 * File Description:   Local data storage, database description.
 *
 */
#include <bdb/bdb_file.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



struct NCBI_LDS_EXPORT SLDS_FileDB : public CBDB_File
{
    CBDB_FieldInt4     file_id;
    CBDB_FieldString   file_name;
    CBDB_FieldInt4     format;
    CBDB_FieldInt4     time_stamp;
    CBDB_FieldInt4     CRC;
    CBDB_FieldInt8     file_size;   

    SLDS_FileDB();
};


struct NCBI_LDS_EXPORT SLDS_FileNameIDX : public CBDB_File
{
    CBDB_FieldString   file_name;
    CBDB_FieldInt4     file_id;

    SLDS_FileNameIDX();
};


struct NCBI_LDS_EXPORT SLDS_AnnotDB : public CBDB_File
{
    CBDB_FieldInt4    annot_id;
    CBDB_FieldInt4    file_id;
    CBDB_FieldInt4    annot_type;
    CBDB_FieldInt8    file_pos;
    CBDB_FieldInt4    TSE_object_id;     // TOP level seq entry object id;
    CBDB_FieldInt4    parent_object_id;  // Parent SeqEntry object id

    SLDS_AnnotDB();
};


struct NCBI_LDS_EXPORT SLDS_SeqId_List : public CBDB_File
{
    CBDB_FieldInt4    object_id;
    CBDB_FieldString  seq_id;

    SLDS_SeqId_List();
};

struct NCBI_LDS_EXPORT SLDS_ObjectTypeDB : public CBDB_File
{
    CBDB_FieldInt4    object_type;
    CBDB_FieldString  type_name;

    SLDS_ObjectTypeDB();
};


struct NCBI_LDS_EXPORT SLDS_ObjectDB : public CBDB_File
{
    CBDB_FieldInt4    object_id;

    CBDB_FieldInt4    file_id; 
    CBDB_FieldString  primary_seqid;
    CBDB_FieldInt4    seqlist_id; 
    CBDB_FieldInt4    object_type;
    CBDB_FieldInt8    file_pos;
    CBDB_FieldInt4    TSE_object_id;     // TOP level seq entry object id
    CBDB_FieldInt4    parent_object_id;  // Parent SeqEntry object id


    CBDB_FieldString  object_title;
    CBDB_FieldString  organism;
    CBDB_FieldString  keywords;
    CBDB_FieldString  seq_ids;

    SLDS_ObjectDB();
};


struct NCBI_LDS_EXPORT SLDS_Annot2ObjectDB : public CBDB_File
{
    CBDB_FieldInt4  object_id;
    CBDB_FieldInt4  annot_id;

    SLDS_Annot2ObjectDB();
};

/// Persistent index for text based ids (like GenBank id)
struct NCBI_LDS_EXPORT SLDS_TxtIdIDX : public CBDB_File
{
    CBDB_FieldString  id;      ///< accession in most cases
    CBDB_FieldInt4    row_id;  ///< Object id (raw id)

    SLDS_TxtIdIDX();
};


/// Persistent index for numeric ids (like GI)
struct NCBI_LDS_EXPORT SLDS_IntIdIDX : public CBDB_File
{
    CBDB_FieldInt4    id;      ///< molecule id
    CBDB_FieldInt4    row_id;  ///< Object id (raw id)

    SLDS_IntIdIDX();
};


//////////////////////////////////////////////////////////////////
//
// 
// Structure puts together all tables used in LDS
//
struct NCBI_LDS_EXPORT SLDS_TablesCollection
{
    SLDS_FileDB          file_db;
    SLDS_ObjectTypeDB    object_type_db;
    SLDS_ObjectDB        object_db;
    SLDS_AnnotDB         annot_db;
    SLDS_Annot2ObjectDB  annot2obj_db;
    SLDS_SeqId_List      seq_id_list;

    /// object_db index for seq id
    SLDS_TxtIdIDX        obj_seqid_txt_idx; 
    SLDS_IntIdIDX        obj_seqid_int_idx;
    /// file name index
    SLDS_FileNameIDX     file_filename_idx;
};


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



inline 
SLDS_FileDB::SLDS_FileDB()
{
    BindKey("file_id", &file_id);

    BindData("file_name", &file_name, 2048);
    BindData("format", &format);
    BindData("time_stamp", &time_stamp, 64);
    BindData("CRC", &CRC);
    BindData("file_size", &file_size);
}

inline
SLDS_FileNameIDX::SLDS_FileNameIDX()
{
    BindKey("file_name", &file_name, 2048);
    BindData("file_id", &file_id);

    DisableNull();
}

inline 
SLDS_AnnotDB::SLDS_AnnotDB()
{
    BindKey("annot_id", &annot_id);

    BindData("file_id", &file_id);
    BindData("annot_type", &annot_type);
    BindData("file_pos", &file_pos);
    BindData("TSE_object_id", &TSE_object_id);
    BindData("parent_object_id", &parent_object_id);
}

inline
SLDS_SeqId_List::SLDS_SeqId_List()
: CBDB_File(CBDB_File::eDuplicatesEnable)
{
    BindKey("object_id", &object_id);

    BindData("seq_id", &seq_id);
}

inline 
SLDS_ObjectTypeDB::SLDS_ObjectTypeDB()
{
    BindKey("object_type", &object_type);

    BindData("type_name",  &type_name);
}


inline 
SLDS_ObjectDB::SLDS_ObjectDB()
{
    BindKey("object_id", &object_id);

    BindData("file_id", &file_id);
    BindData("primary_seqid", &primary_seqid);
    BindData("seqlist_id", &seqlist_id);
    BindData("object_type", &object_type);
    BindData("file_pos", &file_pos);
    BindData("TSE_object_id", &TSE_object_id);
    BindData("parent_object_id", &parent_object_id);

    BindData("object_title", &object_title, 1024, eNullable);
    BindData("organism", &organism, 256, eNullable);
    BindData("keywords", &keywords, 2048, eNullable);
    BindData("seq_ids", &seq_ids, 65536, eNullable);
}



inline 
SLDS_Annot2ObjectDB::SLDS_Annot2ObjectDB()
{
    BindKey("object_id", &object_id);
    BindKey("annot_id", &annot_id);
}


inline
SLDS_TxtIdIDX::SLDS_TxtIdIDX() 
    : CBDB_File(CBDB_RawFile::eDuplicatesEnable)
{
    BindKey("id", &id, 256);
    BindData("row_id", &row_id, 0, eNotNullable);

    DisableNull();
}

inline
SLDS_IntIdIDX::SLDS_IntIdIDX()
    : CBDB_File(CBDB_RawFile::eDuplicatesEnable)
{
    BindKey("id", &id);
    BindData("row_id", &row_id, 0, eNotNullable);

    DisableNull();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
