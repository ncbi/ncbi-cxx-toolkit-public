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
 * Author:  Anton Lavrentiev
 *
 * File dscription:
 *   C++->C conversion functions for basic corelib stuff:
 *     Registry
 *     Logging
 *     Locking
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/01/11 23:08:16  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_core_cxx.hpp>
#include <string.h>

BEGIN_NCBI_SCOPE


extern "C" {
    static void s_REG_Get(void* user_data,
                          const char* section, const char* name,
                          char* value, size_t value_size);
    static void s_REG_Set(void* user_data,
                          const char* section, const char* name,
                          const char* value, EREG_Storage storage);
    static void s_REG_Cleanup(void* user_data);
}


static void s_REG_Get(void* user_data,
                      const char* section, const char* name,
                      char* value, size_t value_size)
{
    CNcbiRegistry* reg = static_cast<CNcbiRegistry*>(user_data);
    string result;
    try {
        result = reg->Get(section, name);
    } STD_CATCH_ALL("s_REG_Get() failed");
    if (result.length()) {
        strncpy(value, result.c_str(), value_size - 1);
        value[value_size - 1] = '\0';
    }
}


static void s_REG_Set(void* user_data,
                      const char* section, const char* name,
                      const char* value, EREG_Storage storage)
{
    CNcbiRegistry* reg = static_cast<CNcbiRegistry*>(user_data);
    try {
        reg->Set(section, name, value,
                 (storage == eREG_Persistent
                  ? CNcbiRegistry::ePersistent : 0) |
                 CNcbiRegistry::eOverride | CNcbiRegistry::eTruncate);
    } STD_CATCH_ALL("s_REG_Set() failed");
}


static void s_REG_Cleanup(void* user_data)
{
    CNcbiRegistry* reg = static_cast<CNcbiRegistry*>(user_data);
    delete reg;
}


REG REG_cxx2c(CNcbiRegistry* reg, bool pass_ownership)
{
    return REG_Create(static_cast<void*>(reg),
                      s_REG_Get, s_REG_Set,
                      pass_ownership ? s_REG_Cleanup : 0,
                      0);
}


END_NCBI_SCOPE
