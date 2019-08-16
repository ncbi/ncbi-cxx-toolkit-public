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
 * Authors:  Michael Kholodov
 *
 * File Description:   Java toolkit specifics.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/impl/ncbi_java.h>

#include <assert.h>
//#include <jni.h>
#include <cstdlib>

#include <corelib/ncbiapp_api.hpp>
#include <corelib/ncbi_toolkit.hpp>

BEGIN_NCBI_SCOPE

JavaVM* CJniUtil::sm_JVM = NULL;
INcbiToolkit_LogHandler* CJniUtil::sm_logHandler = NULL;

/////////////////////////////////////////////////////////////////////
// Custom log hanlder implementation
//
class CJavaLogHandler: public INcbiToolkit_LogHandler
{
public:

    CJavaLogHandler(jobject logger);
    virtual ~CJavaLogHandler();

    virtual void Post(const CNcbiToolkit_LogMessage& msg);

private:
    jobject GetLevel(JNIEnv* env, CNcbiToolkit_LogMessage::ESeverity severity);

private:

    const int PARAM_COUNT;

    jobject m_logger;

    jclass m_Logger_class;
    jmethodID m_Logger_logId;

    jclass m_LogRecord_class;
    jmethodID m_LogRecord_ctorId;
    jmethodID m_LogRecord_setParametersId;

    jclass m_Level_class;
    jfieldID m_Level_SevereId;
    jfieldID m_Level_WarningId;
    jfieldID m_Level_InfoId;
    jfieldID m_Level_ConfigId;
    jfieldID m_Level_FineId;
    jfieldID m_Level_FinerId;
    jfieldID m_Level_FinestId;

    jclass m_String_class;
};

CJavaLogHandler::CJavaLogHandler(jobject logger)
: PARAM_COUNT(6)
{
    JNIEnv *env = CJniUtil::GetEnv();

    // Obtain global ref to the logger
    m_logger = env->NewGlobalRef(logger);
    JAVA_VALIDATE( m_logger != NULL,
        "CJavaLogHandler ctor: could not create global logger object reference");
    
    jclass clazz;

    // Cache Logger metainfo
    clazz = env->FindClass("java/util/logging/Logger");
    JAVA_VALIDATE(clazz != NULL,
        "CJavaLogHandler ctor: java Logger class could not be found");

    m_Logger_class = (jclass)env->NewGlobalRef(clazz);
    JAVA_VALIDATE(m_Logger_class != NULL,
        "CJavaLogHandler ctor: could not create global Logger reference");


    m_Logger_logId = env->GetMethodID(clazz, "log", "(Ljava/util/logging/LogRecord;)V");
    JAVA_VALIDATE(m_Logger_logId != NULL,
        "CJavaLogHandler ctor: java Logger method log(LogRecord) ID could not be returned");

    env->DeleteLocalRef(clazz);

    // Cache LogRecord metainfo
    clazz = env->FindClass("java/util/logging/LogRecord");
    JAVA_VALIDATE(clazz != NULL,
        "CJavaLogHandler ctor: java LogRecord class could not be found");

    m_LogRecord_class = (jclass)env->NewGlobalRef(clazz);
    JAVA_VALIDATE(m_LogRecord_class != NULL,
        "CJavaLogHandler ctor: could not create global LogRecord reference");


    m_LogRecord_ctorId = env->GetMethodID(clazz, "<init>", 
        "(Ljava/util/logging/Level;Ljava/lang/String;)V");
    JAVA_VALIDATE(m_LogRecord_ctorId != NULL,
        "CJavaLogHandler ctor: java LogRecord constructor ID could not be returned");

    m_LogRecord_setParametersId = env->GetMethodID(clazz, "setParameters", 
        "([Ljava/lang/Object;)V");
    JAVA_VALIDATE(m_LogRecord_setParametersId != NULL,
        "CJavaLogHandler ctor: java LogRecord setParameters ID could not be returned");

    env->DeleteLocalRef(clazz);

    // Cache Level metainfo
    clazz = env->FindClass("java/util/logging/Level");
    JAVA_VALIDATE(clazz != NULL,
        "CJavaLogHandler ctor: java Level class could not be found");

    m_Level_class = (jclass)env->NewGlobalRef(clazz);
    JAVA_VALIDATE(m_Level_class != NULL,
        "CJavaLogHandler ctor: could not create global Level reference");

    m_Level_SevereId = env->GetStaticFieldID(m_Level_class, 
        "SEVERE", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_SevereId != NULL,
        "CJavaLogHandler ctor: Level.SEVERE not found");

    m_Level_WarningId = env->GetStaticFieldID(m_Level_class, 
        "WARNING", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_WarningId != NULL,
        "CJavaLogHandler ctor: Level.WARNING not found");

    m_Level_InfoId = env->GetStaticFieldID(m_Level_class, 
        "INFO", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_InfoId != NULL,
        "CJavaLogHandler ctor: Level.INFO not found");

    m_Level_ConfigId = env->GetStaticFieldID(m_Level_class, 
        "CONFIG", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_ConfigId != NULL,
        "CJavaLogHandler ctor: Level.CONFIG not found");

    m_Level_FineId = env->GetStaticFieldID(m_Level_class, 
        "FINE", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_FineId != NULL,
        "CJavaLogHandler ctor: Level.FINE not found");

    m_Level_FinerId = env->GetStaticFieldID(m_Level_class, 
        "FINER", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_FinerId != NULL,
        "CJavaLogHandler ctor: Level.FINER not found");

    m_Level_FinestId = env->GetStaticFieldID(m_Level_class, 
        "FINEST", "Ljava/util/logging/Level;");
    JAVA_VALIDATE(m_Level_FinestId != NULL,
        "CJavaLogHandler ctor: Level.FINEST not found");

    // Cache String metainfo
    clazz = env->FindClass("java/lang/String");
    JAVA_VALIDATE(clazz != NULL,
        "CJavaLogHandler ctor: java String class could not be found");

    m_String_class = (jclass)env->NewGlobalRef(clazz);
    JAVA_VALIDATE(m_String_class != NULL,
        "CJavaLogHandler ctor: could not create global String reference");

    env->DeleteLocalRef(clazz);
}

CJavaLogHandler::~CJavaLogHandler()
{
}

jobject CJavaLogHandler::GetLevel(JNIEnv* env, CNcbiToolkit_LogMessage::ESeverity severity)
{
    jobject level = NULL;
    switch(severity)
    {
    case eDiag_Info:
        level = env->GetStaticObjectField(m_Level_class, m_Level_InfoId);
        break;    
    case eDiag_Warning:
        level = env->GetStaticObjectField(m_Level_class, m_Level_WarningId);
        break;    
    case eDiag_Error:
    case eDiag_Critical:
    case eDiag_Fatal:
        level = env->GetStaticObjectField(m_Level_class, m_Level_SevereId);
        break;    
    case eDiag_Trace:
        level = env->GetStaticObjectField(m_Level_class, m_Level_FineId);
        break;
    default:
        level = env->GetStaticObjectField(m_Level_class, m_Level_InfoId);
        break;
    }

    return level;
}

void CJavaLogHandler::Post(const CNcbiToolkit_LogMessage& msg)
{
    JNIEnv *env = CJniUtil::GetEnv();

    // Create a LogRecord object
    jobject level = GetLevel(env, msg.Severity());
    JAVA_VALIDATE(level != NULL, "CJavaLogHandler::Post(): NULL Level returned");

    jstring jstr = env->NewStringUTF(msg.Message().c_str());    
    jobject lr = env->NewObject(m_LogRecord_class, m_LogRecord_ctorId, level, jstr);
    JAVA_VALIDATE(lr != NULL, "CJavaLogHandler::Post(): could not create LogRecord object");

    // Create parameter array and fill it out from msg
    jobjectArray params = env->NewObjectArray(PARAM_COUNT, m_String_class, NULL);
    
    jstr = env->NewStringUTF("1"); // Version of the parameter set
    env->SetObjectArrayElement(params, 0, jstr);

    jstr = env->NewStringUTF(NStr::IntToString(msg.Severity()).c_str());
    env->SetObjectArrayElement(params, 1, jstr);

    jstr = env->NewStringUTF(NStr::IntToString(msg.ErrCode()).c_str());
    env->SetObjectArrayElement(params, 2, jstr);

    jstr = env->NewStringUTF(NStr::IntToString(msg.ErrSubCode()).c_str());
    env->SetObjectArrayElement(params, 3, jstr);

    jstr = env->NewStringUTF(msg.File().c_str());
    env->SetObjectArrayElement(params, 4, jstr);

    jstr = env->NewStringUTF(NStr::Int8ToString(msg.Line()).c_str());
    env->SetObjectArrayElement(params, 5, jstr);
        
    // Call LogRecord.setParameters(Object[] params)
    env->CallVoidMethod(lr, m_LogRecord_setParametersId, params);
    JAVA_CHECK_EXCEPTION(env);


    // Call Logger.log(LogRecord lr)
    env->CallVoidMethod(m_logger, m_Logger_logId, lr);
    JAVA_CHECK_EXCEPTION(env);
}

/////////////////////////////////////////////////////////////////////
// CJniUtil implementation
//

void  CJniUtil::SetJVM(JavaVM* vm)
{
    sm_JVM = vm;
}

JNIEnv* CJniUtil::GetEnv(void)
{
    JNIEnv *env = NULL;
    switch(sm_JVM->GetEnv((void**)&env, JAVA_CURRENT_VERSION))
    {
    case JNI_EVERSION:
        JAVA_ABORT("Requested Java version not supported");
        break;
    case JNI_EDETACHED:
        if (sm_JVM->AttachCurrentThread((void **)&env, NULL) != 0 )
        {    
            JAVA_ABORT("Could not attach native thread");
        }
        break;
    case JNI_OK:
        //printf("%s\n", "In GetJNIEnv - JNI_OK");
        break;
    default:
        JAVA_ABORT("Unknown return code from g_GetJNIEnv");
        break;
    }
    assert(env != NULL);

    return env;
}

void CJniUtil::ThrowJavaException(const char *clsName, const char* msg)
{
    JNIEnv *env = GetEnv();
    jclass newExcCls = env->FindClass(clsName);
    if (newExcCls != 0)
        env->ThrowNew(newExcCls, msg);
    else
        JAVA_FATAL_ERROR("CJniUtil::ThrowJavaException(): requested exception class not found");
}

void CJniUtil::JavaFatalError(const char* msg)
{
    JNIEnv *env = GetEnv();
    env->ExceptionDescribe();
    env->FatalError(msg);
}

void CJniUtil::DescribeJavaException()
{
    JNIEnv *env = GetEnv();
    env->ExceptionDescribe();
}

INcbiToolkit_LogHandler* CJniUtil::GetLogHandler()
{
    return sm_logHandler;
}

void CJniUtil::SetLogHandler(jobject logger)
{
    sm_logHandler = new CJavaLogHandler(logger);
}

char** CJniUtil::CreateCStyleArgs(JNIEnv *env, jstring progName, jobjectArray argArray)
{
    char **argv;

    int argc = env->GetArrayLength(argArray);
    argv = new char* [argc+1];

    const char *cn = env->GetStringUTFChars(progName, 0);
    argv[0] = new char[strlen(cn) + 1];
    strcpy(argv[0], cn);

    for( int i = 0; i < argc; ++i)
    {
        jstring s = (jstring)env->GetObjectArrayElement(argArray, i);
        const char* p = env->GetStringUTFChars(s, 0);
        argv[i+1] = new char[strlen(p) + 1];
        strcpy(argv[i+1], p);
    }

    // Release java strings
    for( int i = 0; i < argc; ++i)
    {
        jstring s = (jstring)env->GetObjectArrayElement(argArray, i);
        env->ReleaseStringUTFChars(s, argv[i+1]);
    }

    env->ReleaseStringUTFChars(progName, cn);
    return argv;
}

jobject CJniUtil::GetArgsFromNcbiApp(JNIEnv *env, CNcbiApplication* app)
{
    jobject props = NULL;
    jclass propsClass;
    jmethodID midCtor;
    jmethodID midSetProp;

    // Prep java objects
    propsClass = env->FindClass("java/util/Properties");
    if( propsClass == NULL )
        return NULL;

    midCtor = env->GetMethodID(propsClass, "<init>", "()V");
    if( midCtor == NULL )
        return NULL;

    props = env->NewObject(propsClass, midCtor);
    if( props == NULL )
        return NULL;

    midSetProp = env->GetMethodID(propsClass, "setProperty", 
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;");
    if( midSetProp == NULL )
        return NULL;

    // Process arguments
    vector<CRef<CArgValue> > ncbiArgs = app->GetArgs().GetAll();
    vector<CRef<CArgValue> >::iterator i = ncbiArgs.begin();
    for( ; i != ncbiArgs.end(); ++i )
    {
        string name = (*i)->GetName();
        jstring key = env->NewStringUTF(name.c_str());
        jstring val = NULL;
        if( (*i)->HasValue() )
            val = env->NewStringUTF((*i)->AsString().c_str());

        env->CallObjectMethod(props, midSetProp, key, val);
    }
    return props;
}

END_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////// 
// Java native calls implementation
//
JNIEXPORT void JNICALL Java_org_ncbi_toolkit_NcbiToolkit_attachNcbiLogHandler0
  (JNIEnv *env, jclass, jobject logger)
{
    USING_NCBI_SCOPE;

    CJniUtil::SetLogHandler(logger);
}

JNIEXPORT void JNICALL Java_org_ncbi_toolkit_NcbiToolkit_init0
  (JNIEnv *env, jclass, jobjectArray argArray, jstring progName)
{
    USING_NCBI_SCOPE;
    
    // Get command line
    try
    {
        int argc = env->GetArrayLength(argArray);
        char **argv = CJniUtil::CreateCStyleArgs(env, progName, argArray);

        NcbiToolkit_Init(argc + 1, argv, 0, CJniUtil::GetLogHandler());

        // Release strings
        for( int i = 0; i < argc + 1; ++i)
        {
            delete argv[i];
        }

        delete argv;
    }
    catch(exception& e) 
    {
        CJniUtil::ThrowJavaException("java/lang/RuntimeException", e.what());
    }
}

JNIEXPORT void JNICALL Java_org_ncbi_toolkit_NcbiToolkit_testLog
  (JNIEnv *env, jclass, jstring s)
{
    const char* p = env->GetStringUTFChars(s, 0);
    LOG_POST(p);
    //ERR_POST(p);
    env->ReleaseStringUTFChars(s, p);
}

/////////////////////////////////////////////////////////////////////
// Jni OnLoad implementation
//
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
    USING_NCBI_SCOPE;

    CJniUtil::SetJVM(jvm);

    return JAVA_CURRENT_VERSION;
}

