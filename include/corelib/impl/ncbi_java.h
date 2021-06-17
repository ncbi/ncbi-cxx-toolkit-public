#ifndef CORELIB___IMPL___NCBI_JAVA__H
#define CORELIB___IMPL___NCBI_JAVA__H

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
 * Author:  Michael Kholodov
 *
 */

/// @file ncbi_java.h
///
/// Defines JNI specific macros and includes
///

#include <jni.h>

#include <corelib/ncbidiag.hpp>

#define JAVA_CURRENT_VERSION JNI_VERSION_1_6

#define JAVA_CHECK_EXCEPTION(env) \
    { \
        if(env->ExceptionCheck()) \
        { \
            env->ExceptionDescribe(); \
            env->ExceptionClear(); \
        } \
    }

#define JAVA_FATAL_ERROR(msg) \
    { \
        CJniUtil::JavaFatalError(msg); \
    }

#define JAVA_VALIDATE(val, msg) \
    { \
        if(!(val)) \
            JAVA_FATAL_ERROR(msg) \
    }

#define JAVA_ABORT(msg) \
    { \
        NcbiCerr << __FILE__ << ": " << NCBI_CURRENT_FUNCTION << ": " msg << endl; \
        abort(); \
    }


BEGIN_NCBI_SCOPE

class INcbiToolkit_LogHandler;
class CNcbiApplication;

class NCBI_DLL_EXPORT CJniUtil
{
public:
    static void SetJVM(JavaVM* vm);

    static JNIEnv* GetEnv(void);

    static void ThrowJavaException(const char *clsName, const char* msg);
    static void DescribeJavaException(void);
    static void JavaFatalError(const char* msg);

    // Returns argArray.length + 1 strings. Deallocate the array and strings after use
    static char** CreateCStyleArgs(JNIEnv *env, jstring progName, jobjectArray argArray);

    // Returns java.util.Properties object, if argument has no value, the Properties val is null
    static jobject GetArgsFromNcbiApp(JNIEnv *env, CNcbiApplication* app);

    static INcbiToolkit_LogHandler* GetLogHandler();
    static void SetLogHandler(jobject logger);

private:
    static INcbiToolkit_LogHandler* sm_logHandler;
    static JavaVM* sm_JVM;

    // Static only
    CJniUtil()
    {
    }
};

END_NCBI_SCOPE

extern "C" {
JNIEXPORT void JNICALL Java_org_ncbi_toolkit_NcbiToolkit_attachNcbiLogHandler0
  (JNIEnv *env, jclass, jobject logger);

JNIEXPORT void JNICALL Java_org_ncbi_toolkit_NcbiToolkit_init0
  (JNIEnv *env, jclass, jobjectArray argArray, jstring progName);

JNIEXPORT void JNICALL Java_org_ncbi_toolkit_NcbiToolkit_testLog
  (JNIEnv *env, jclass, jstring s);
}

#endif  /* CORELIB___IMPL___NCBI_JAVA__H */
