// UT2004 Android process-prep JNI bridge — adapted from UTA (Andiweli/UT99-Android)
// android_bridge.cpp. Tiny, self-contained (JNI + POSIX only; no engine/SDL headers).
//
// GameActivity loads this in a static block and calls nativePrepareProcess() BEFORE
// super.onCreate() (which starts SDL_main), so the basedir env + cwd are in place before
// the engine's appBaseDir()/appInit() read them. UT2004 reads its ~11 GB game tree in
// place from external storage (no copy), so this only sets env + chdir + ensures the
// writable subdirs exist.
#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstring>

#define LOG_TAG "UT2004Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool mkdir_p(const char* path) {
    if (!path || !*path) {
        return false;
    }

    char tmp[1024];
    ::snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = ::strlen(tmp);
    if (len == 0 || len >= sizeof(tmp)) {
        return false;
    }

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (::mkdir(tmp, 0775) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }

    return (::mkdir(tmp, 0775) == 0 || errno == EEXIST);
}

static bool make_child_path(char* out, size_t outSize, const char* root, const char* child) {
    if (!out || outSize == 0 || !root || !child) {
        return false;
    }

    int written = ::snprintf(out, outSize, "%s/%s", root, child);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

static jboolean prepare_process_common(
        JNIEnv* env,
        jstring dataRootString,
        jstring homeDirString) {
    if (!env || !dataRootString || !homeDirString) {
        LOGE("nativePrepareProcess called with null JNI arguments");
        return JNI_FALSE;
    }

    const char* dataRoot = env->GetStringUTFChars(dataRootString, nullptr);
    const char* homeDir = env->GetStringUTFChars(homeDirString, nullptr);

    if (!dataRoot || !homeDir) {
        LOGE("GetStringUTFChars failed: dataRoot=%p homeDir=%p", dataRoot, homeDir);
        if (dataRoot) {
            env->ReleaseStringUTFChars(dataRootString, dataRoot);
        }
        if (homeDir) {
            env->ReleaseStringUTFChars(homeDirString, homeDir);
        }
        return JNI_FALSE;
    }

    bool ok = true;

    // UT2004 runs from <dataRoot>/System; Cache/Save/Logs are writable siblings. The read-only
    // game tree (Maps/Textures/... ~11 GB) already lives under dataRoot and is not touched here.
    char systemDir[1024];
    char cacheDir[1024];
    char saveDir[1024];
    char logsDir[1024];
    if (!make_child_path(systemDir, sizeof(systemDir), dataRoot, "System") ||
        !make_child_path(cacheDir, sizeof(cacheDir), dataRoot, "Cache") ||
        !make_child_path(saveDir, sizeof(saveDir), dataRoot, "Save") ||
        !make_child_path(logsDir, sizeof(logsDir), dataRoot, "Logs")) {
        LOGE("Failed to build child paths from data root: %s", dataRoot ? dataRoot : "<null>");
        ok = false;
    }

    // The home dir is writable app-internal storage; System already exists in the user's data
    // tree but mkdir_p is a harmless no-op if present. Cache/Save/Logs are created if missing.
    if (!mkdir_p(homeDir)) {
        LOGE("Failed to create home dir: %s", homeDir);
        ok = false;
    }
    if (ok && !mkdir_p(cacheDir)) {
        LOGE("Failed to create Cache dir: %s", cacheDir);
        ok = false;
    }
    if (ok && !mkdir_p(saveDir)) {
        LOGE("Failed to create Save dir: %s", saveDir);
        ok = false;
    }
    if (ok && !mkdir_p(logsDir)) {
        LOGE("Failed to create Logs dir: %s", logsDir);
        ok = false;
    }

    if (::setenv("HOME", homeDir, 1) != 0) {
        LOGE("setenv(HOME) failed: %s", ::strerror(errno));
        ok = false;
    }

    if (::setenv("UT2004_ANDROID_DATA", dataRoot, 1) != 0) {
        LOGE("setenv(UT2004_ANDROID_DATA) failed: %s", ::strerror(errno));
        ok = false;
    }

    // appBaseDir() (Core/Src/UnUnix.cpp, __ANDROID__ branch) reads this and chdir's to it, so the
    // engine's POSIX fopen layer resolves System/*.ini + ../Maps, ../Textures in place.
    if (::setenv("UT2004_ANDROID_BASEDIR", systemDir, 1) != 0) {
        LOGE("setenv(UT2004_ANDROID_BASEDIR) failed: %s", ::strerror(errno));
        ok = false;
    }

    // Run from System/ so relative ../Maps, ../Textures and local *.ini resolve (matches desktop).
    if (::chdir(systemDir) != 0) {
        LOGE("chdir(%s) failed: %s", systemDir, ::strerror(errno));
        ok = false;
    } else {
        LOGI("Working directory set to %s (dataRoot=%s)", systemDir, dataRoot);
    }

    env->ReleaseStringUTFChars(dataRootString, dataRoot);
    env->ReleaseStringUTFChars(homeDirString, homeDir);

    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ut2004_android_GameActivity_nativePrepareProcess(
        JNIEnv* env,
        jclass,
        jstring dataRootString,
        jstring homeDirString) {
    return prepare_process_common(env, dataRootString, homeDirString);
}
