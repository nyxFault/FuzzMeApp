#include <jni.h>
#include <android/log.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <deque>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr const char *kTag = "FuzzMeNative";
constexpr const char *kCrashNeedle = "FuzzMe@123";
constexpr size_t kCrashNeedleLen = 10;
constexpr size_t kMaxPayload = 8192;
constexpr size_t kEventQueueLimit = 50;
constexpr useconds_t kReadTimeoutUs = 500000;

std::atomic<bool> gRunning{false};
int gListenFd = -1;
std::thread gServerThread;
std::mutex gDataMutex;
std::string gLastReceived;
std::deque<std::string> gConnectionEvents;

void logInfo(const std::string &msg) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "%s", msg.c_str());
}

void triggerCrash() {
    // Intentional crash for fuzzing target behavior.
    volatile int *ptr = nullptr;
    *ptr = 0x1337;
}

bool hasCrashPrefix(const uint8_t *buffer, uint64_t length) {
    static constexpr uint8_t kPattern[kCrashNeedleLen] = {
            'F', 'u', 'z', 'z', 'M', 'e', '@', '1', '2', '3'
    };
    return buffer != nullptr && length >= kCrashNeedleLen
           && memcmp(buffer, kPattern, kCrashNeedleLen) == 0;
}

extern "C" void fuzzMe(const uint8_t *buffer, uint64_t length) {
    if (hasCrashPrefix(buffer, length)) {
        triggerCrash();
    }
}

void closeFd(int &fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void serverLoop(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        logInfo("socket() failed");
        gRunning.store(false);
        return;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::string err = std::string("bind() failed: ") + strerror(errno);
        logInfo(err);
        gRunning.store(false);
        close(fd);
        return;
    }

    if (listen(fd, 8) < 0) {
        std::string err = std::string("listen() failed: ") + strerror(errno);
        logInfo(err);
        gRunning.store(false);
        close(fd);
        return;
    }

    gListenFd = fd;
    logInfo("Listening on port " + std::to_string(port));

    while (gRunning.load()) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int client = accept(fd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (client < 0) {
            if (!gRunning.load()) {
                break;
            }
            continue;
        }
        char ipBuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
        int peerPort = ntohs(clientAddr.sin_port);
        {
            std::lock_guard<std::mutex> lock(gDataMutex);
            gConnectionEvents.emplace_back(
                    std::string("Connection from ")
                    + ipBuf
                    + ":"
                    + std::to_string(peerPort)
                    + " -> 0.0.0.0:"
                    + std::to_string(port));
            if (gConnectionEvents.size() > kEventQueueLimit) {
                gConnectionEvents.pop_front();
            }
        }

        std::string payload;
        payload.reserve(256);
        {
            std::lock_guard<std::mutex> lock(gDataMutex);
            gLastReceived.clear();
        }

        // Avoid blocking forever waiting for peer close.
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = kReadTimeoutUs;  // 500ms
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        char buf[512];
        ssize_t n;
        while ((n = read(client, buf, sizeof(buf))) > 0) {
            payload.append(buf, static_cast<size_t>(n));
            if (payload.size() >= kMaxPayload) {
                break;
            }
            {
                std::lock_guard<std::mutex> lock(gDataMutex);
                gLastReceived = payload;
            }
            // Trigger as soon as pattern is observed; do not wait for disconnect.
            if (payload.find(kCrashNeedle) != std::string::npos) {
                logInfo("Crash trigger matched");
                fuzzMe(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
            }
            // Treat a newline as end-of-message for interactive nc usage.
            if (payload.find('\n') != std::string::npos) {
                break;
            }
        }
        {
            std::lock_guard<std::mutex> lock(gDataMutex);
            gLastReceived = payload;
        }
        close(client);
    }

    closeFd(gListenFd);
    gRunning.store(false);
}
}  // namespace

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_fuzzmeapp_MainActivity_startNativeServer(JNIEnv *env, jobject /*thiz*/, jint port) {
    if (gRunning.load()) {
        return env->NewStringUTF("Native listener already running");
    }

    if (gServerThread.joinable()) {
        gServerThread.join();
    }
    gRunning.store(true);
    gServerThread = std::thread(serverLoop, static_cast<int>(port));
    return env->NewStringUTF("Listening on port 4444");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_fuzzmeapp_MainActivity_stopNativeServer(JNIEnv * /*env*/, jobject /*thiz*/) {
    gRunning.store(false);
    closeFd(gListenFd);
    if (gServerThread.joinable()) {
        gServerThread.join();
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_fuzzmeapp_MainActivity_isServerRunning(JNIEnv * /*env*/, jobject /*thiz*/) {
    return gRunning.load() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_fuzzmeapp_MainActivity_getLastReceived(JNIEnv *env, jobject /*thiz*/) {
    std::string value;
    {
        std::lock_guard<std::mutex> lock(gDataMutex);
        value = gLastReceived;
    }
    return env->NewStringUTF(value.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_fuzzmeapp_MainActivity_consumeConnectionEvent(JNIEnv *env, jobject /*thiz*/) {
    std::string event;
    {
        std::lock_guard<std::mutex> lock(gDataMutex);
        if (!gConnectionEvents.empty()) {
            event = gConnectionEvents.front();
            gConnectionEvents.pop_front();
        }
    }
    return env->NewStringUTF(event.c_str());
}
