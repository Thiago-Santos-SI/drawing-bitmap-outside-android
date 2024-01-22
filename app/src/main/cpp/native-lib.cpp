#include <jni.h>
#include <string>
#include <sstream>
#include <android/log.h>
#include <pthread.h>
#include <iomanip>
#include <unistd.h>
#include <random>

#include "Timer.h"
#include "Laplacian.h"

#include <jni.h>
#include <android/bitmap.h>
#include <vector>
#include <unordered_set>
#include <omp.h>

static const char* kTAG = "c++Logger";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

  // save all the JVM stuff so we don't have to suffer JNI overhead every call
typedef struct jni_context {
    JavaVM *javaVM;
    jobject mainActivityObj;
    jclass  mainActivityClass;
    pthread_mutex_t  lock;
    AndroidBitmapInfo info;
    void *pixels;
    int      isThreadAvailable;
} jni_context;
jni_context g_ctx;
bool isContextSet = false;


void* castedTest(void* context) {
    jni_context *pctx = (jni_context*) context;

    // I only want one test running at a  -> check isThreadAvailable with mutex
    pthread_mutex_lock(&pctx->lock);
    if(!pctx->isThreadAvailable) {
        pthread_mutex_unlock(&pctx->lock);
        LOGI("Thread already running.");
        return context;
    } else {
        pctx->isThreadAvailable = 0;
        pthread_mutex_unlock(&pctx->lock);
    }
    LOGI("Thread started (castedTest).");

    // JNI stuff: attaching this c++ thread to the JVM so we can make calls to java
    JavaVM *javaVM = pctx->javaVM;
    JNIEnv *env;
    jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = javaVM->AttachCurrentThread(&env, NULL);
        if (JNI_OK != res) {
            LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
            pthread_mutex_lock(&pctx->lock);
            pctx->isThreadAvailable = 1;
            pthread_mutex_unlock(&pctx->lock);
            return NULL;
        }
    }

    // now that everything is set up, get references to the java methods we will be using
    jmethodID getThreadCount = env->GetMethodID(pctx->mainActivityClass, "getThreadCount", "()I");
    jmethodID textAppender = env->GetMethodID(pctx->mainActivityClass, "appendToView", "(Ljava/lang/String;)V");
    jmethodID dataDisplayer = env->GetMethodID(pctx->mainActivityClass, "displayData", "([DI)V");

    // do the method call to get desired number of threads to use
    jint numThreads = env->CallIntMethod(pctx->mainActivityObj, getThreadCount);
    int nt = (int)numThreads;

    // android redirects stdout/stderr to /dev/nul so I'll just use stringstream and send that to
    // the java method that puts text on the screen
    std::stringstream cout;
    cout << "Executing (cast layout) with XDIM:" << XDIM << " and YDIM:" << YDIM << " on " << nt << " threads";
    jstring jstr = env->NewStringUTF(cout.str().c_str());
    env->CallVoidMethod(pctx->mainActivityObj, textAppender, jstr);
    env->DeleteLocalRef(jstr);  // explicit release to it is immediately available for GC by JVM

    // we can finally do the ComputeLaplacian task
    double resutls[TESTITRS];  // save results to send back all at once (reduce work for UI until test done)

    using array_t = float (&) [XDIM][YDIM];

    float *uRaw = new float [XDIM*YDIM];
    float *LuRaw = new float [XDIM*YDIM];
    array_t u = reinterpret_cast<array_t>(*uRaw);
    array_t Lu = reinterpret_cast<array_t>(*LuRaw);
    Timer timer;
    int contadorPixels = 0;

    std::vector<uint32_t> coresUnicas;

    for(int test = 0; test < TESTITRS; test++)
    {
        timer.Start();
        omp_set_num_threads(nt);
        #pragma omp parallel for
            for (int y = 0; y < pctx->info.width; ++y) {
                for (int x = 0; x < pctx->info.height; ++x) {

                    uint32_t *pixel = (uint32_t *)((char *)pctx->pixels + y * pctx->info.stride + x * 4);
                    uint32_t cor = *pixel;


                    if (std::find(coresUnicas.begin(), coresUnicas.end(), cor) == coresUnicas.end()) {
                        coresUnicas.push_back(cor);
                    }
                    __android_log_print(ANDROID_LOG_INFO, "C++", "cor.", cor);
                    contadorPixels++; // Incrementa o contador de pixels

                    __android_log_print(ANDROID_LOG_INFO, "C++", "--", cor);
                }
            }
        //ComputeLaplacian(u, Lu, nt);
        timer.Stop();
        resutls[test] = timer.mostRecentElapsed;
    }

    jintArray resultado = env->NewIntArray(coresUnicas.size());
    // DONE

    // prepare to send back data
    jdoubleArray jdblarr= env->NewDoubleArray(TESTITRS);
    env->SetDoubleArrayRegion(jdblarr, 0, TESTITRS, resutls);
    // send back data
    env->CallVoidMethod(pctx->mainActivityObj, dataDisplayer, jdblarr, nt);
    env->DeleteLocalRef(jdblarr);

    // some cleanup before returning
    free(uRaw);
    free(LuRaw);
    pthread_mutex_lock(&pctx->lock);
    pctx->isThreadAvailable = 1;
    __android_log_print(ANDROID_LOG_INFO, "C++", "contadorPixels.", contadorPixels);

    LOGI("Thread finished ------------->.");
    pthread_mutex_unlock(&pctx->lock);

    return context;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVM = vm;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    g_ctx.isThreadAvailable = 1;
    g_ctx.mainActivityObj = NULL;

    return  JNI_VERSION_1_6;
}


extern "C" JNIEXPORT void JNICALL
Java_com_example_CS639Playground_MainActivity_executeCPP(
        JNIEnv *env,
        jobject thiz,
        jint mode,
        jobject bitmap) {

    AndroidBitmapInfo info;
    void *pixels;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
    }

    int m = (int)mode;

    pthread_t threadInfo_;
    pthread_attr_t threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);

    if (!isContextSet) {
        g_ctx.info = info;
        g_ctx.pixels = pixels;
        pthread_mutex_init(&g_ctx.lock, NULL);

        jclass clz = env->GetObjectClass(thiz);
        // global ref guaranteed valid until explicitly released
        g_ctx.mainActivityObj = env->NewGlobalRef(thiz);
        g_ctx.mainActivityClass = (jclass) env->NewGlobalRef(clz);

        isContextSet = true;
    }

    int result;
    switch(m) {
        case 0 : result = pthread_create( &threadInfo_, &threadAttr_, castedTest, &g_ctx); break;
    }
    pthread_attr_destroy(&threadAttr_);
    (void)result;
}

extern "C" {

JNIEXPORT jintArray JNICALL
Java_com_example_CS639Playground_MainActivity_executeBitmap(JNIEnv *env, jobject thiz, jobject bitmap) {
    AndroidBitmapInfo info;
    void *pixels;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        return nullptr;
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        return nullptr;
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        return nullptr;
    }

    std::chrono::steady_clock::time_point inicio = std::chrono::steady_clock::now();

    std::vector<uint32_t> coresUnicas;
    int contadorPixels = 0;

    for (int y = 0; y < info.height; ++y) {
        for (int x = 0; x < info.width; ++x) {
            uint32_t *pixel = (uint32_t *)((char *)pixels + y * info.stride + x * 4);
            uint32_t cor = *pixel;

            if (std::find(coresUnicas.begin(), coresUnicas.end(), cor) == coresUnicas.end()) {
                coresUnicas.push_back(cor);
            }
            __android_log_print(ANDROID_LOG_INFO, "C++", "cor.", cor);
            contadorPixels++; // Incrementa o contador de pixels
        }
    }

    std::chrono::steady_clock::time_point fim = std::chrono::steady_clock::now();
    auto duracao = std::chrono::duration_cast<std::chrono::milliseconds>(fim - inicio).count();

    AndroidBitmap_unlockPixels(env, bitmap);

    __android_log_print(ANDROID_LOG_INFO, "C++", "duracao.", duracao);

    jintArray resultado = env->NewIntArray(coresUnicas.size());
    env->SetIntArrayRegion(resultado, 0, coresUnicas.size(), reinterpret_cast<jint*>(coresUnicas.data()));

    // Você pode acessar o contadorPixels para obter o número total de pixels processados
    return resultado;
}

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_com_example_CS639Playground_MainActivity_extractColorsNative(JNIEnv *env, jobject instance, jobject bitmap) {

    auto start = std::chrono::high_resolution_clock::now();

    // Obtém informações sobre o Bitmap
    AndroidBitmapInfo info;
    void *pixels;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        return NULL;
    }

    // Verifica o formato do Bitmap (deve ser ARGB_8888)
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        return NULL;
    }

    // Bloqueia os pixels para leitura
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        return NULL;
    }

    // Obtém largura e altura do Bitmap
    int width = info.width;
    int height = info.height;

    // Inicializa um array de inteiros para armazenar os pixels
    jintArray resultArray = env->NewIntArray(width * height);

    // Obtém um ponteiro para os elementos do array
    jint *resultPixels = env->GetIntArrayElements(resultArray, NULL);

    int pixelCount = 0;

    // Itera sobre os pixels do Bitmap
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t *pixel = (uint32_t *)((char *)pixels + y * info.stride + x * 4);
            int pixelColor = static_cast<int>(*pixel);
            resultPixels[pixelCount++] = pixelColor;
        }
    }

    // Desbloqueia os pixels
    AndroidBitmap_unlockPixels(env, bitmap);

    // Libera o array de pixels
    env->ReleaseIntArrayElements(resultArray, resultPixels, 0);

    // Imprime informações sobre a execução no console de log do Android
    __android_log_print(ANDROID_LOG_INFO, "JNI", "Número total de pixels: %d", pixelCount);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    long long microseconds = duration.count();

    __android_log_print(ANDROID_LOG_INFO, "JNI", "Tempo de execução: %lld microssegundos", microseconds);


    return reinterpret_cast<jobjectArray>(resultArray);
} }}