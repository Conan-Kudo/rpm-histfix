/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_sleepycat_db_DbLogc */

#ifndef _Included_com_sleepycat_db_DbLogc
#define	_Included_com_sleepycat_db_DbLogc
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_sleepycat_db_DbLogc
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLogc_close
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_sleepycat_db_DbLogc
 * Method:    get
 * Signature: (Lcom/sleepycat/db/DbLsn;Lcom/sleepycat/db/Dbt;I)I
 */
JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbLogc_get
  (JNIEnv *, jobject, jobject, jobject, jint);

/*
 * Class:     com_sleepycat_db_DbLogc
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLogc_finalize
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif
