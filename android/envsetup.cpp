/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Marcus Comstedt <marcus@mc.pp.se>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <jni.h>
#include <stdlib.h>

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv* env;

	(void)reserved;

	if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		return -1;
	}

	jclass envc = env->FindClass("org/sigrok/androidutils/Environment");
	jmethodID getEnv =  env->GetStaticMethodID(envc, "getEnvironment",
						   "()[Ljava/lang/String;");
	jobjectArray envs =
		(jobjectArray)env->CallStaticObjectMethod(envc, getEnv);
	jsize i, envn = env->GetArrayLength(envs);
	for (i=0; i<envn; i+=2) {
		jstring key = (jstring)env->GetObjectArrayElement(envs, i);
		jstring value = (jstring)env->GetObjectArrayElement(envs, i+1);
		const char *utfkey = env->GetStringUTFChars(key, 0);
		const char *utfvalue = env->GetStringUTFChars(value, 0);
		setenv(utfkey, utfvalue, 1);
		env->ReleaseStringUTFChars(value, utfvalue);
		env->ReleaseStringUTFChars(key, utfkey);
		env->DeleteLocalRef(value);
		env->DeleteLocalRef(key);
	}
	env->DeleteLocalRef(envs);
	env->DeleteLocalRef(envc);

	return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM *vm, void *reserved)
{
	(void)vm;
	(void)reserved;
}
