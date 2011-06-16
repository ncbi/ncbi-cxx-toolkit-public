#include <ncbi_pch.hpp>

#include <corelib/impl/ncbi_java.h>
#include "test_java.h"


USING_NCBI_SCOPE;

#define DEAD_PTR ((void*)0xdeadbeef)

CNcbiStrstream *s_os = NULL;

/*
 * Class:     org_ncbi_toolkit_JniTest
 * Method:    getCurThreadFromJni
 * Signature: ()Ljava/lang/Thread;
 */
JNIEXPORT jobject JNICALL Java_org_ncbi_toolkit_JniTest_getCurThreadFromJni
  (JNIEnv *, jclass)
{
	return g_GetCurrJavaThread();
}

/*
 * Class:     org_ncbi_toolkit_JniTest
 * Method:    logPost
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_ncbi_toolkit_JniTest_logPost
  (JNIEnv *env, jclass, jstring str)
{
    const char *s;
    s = env->GetStringUTFChars(str, NULL);
    if (s == NULL) {
       return; /* OutOfMemoryError already thrown */
    }
	//printf("%s %s\n", "In logPost", s);

	LOG_POST(s);
    env->ReleaseStringUTFChars(str, s);
}

/*
 * Class:     org_ncbi_toolkit_JniTest
 * Method:    initOutputBuffer
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_ncbi_toolkit_JniTest_initOutputBuffer
  (JNIEnv *env, jclass)
{
	delete s_os;
	s_os = new CNcbiStrstream;

	// Redirect diag output to string
	SetDiagStream(s_os);
}

/*
 * Class:     org_ncbi_toolkit_JniTest
 * Method:    getOutputBuffer
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_ncbi_toolkit_JniTest_getOutputBuffer
  (JNIEnv *env, jclass)
{
    (*s_os) << std::ends; // Terminate the stream properly
    jstring js = env->NewStringUTF(s_os->str());

    s_os->freeze(false);
	return js;
}
