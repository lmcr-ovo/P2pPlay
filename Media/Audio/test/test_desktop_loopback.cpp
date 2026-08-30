/**
 * mingw_test.cpp
 * MinGW 调用 WasapiDll 的测试程序
 *
 * 功能：
 *   1. 加载 WasapiDll.dll（MSVC 编译的 DLL）
 *   2. 以 EXCLUDE_PROCESS 模式启动捕获（排除自己进程的声音）
 *   3. 进程内发出 Beep 声音
 *   4. 将捕获到的音频保存为 WAV 文件
 *   5. 验证：WAV 文件中不应包含 Beep 声音
 *
 * 编译（MinGW）：
 *   g++ mingw_test.cpp -o mingw_test.exe -lwinmm
 *
 * 运行：
 *   mingw_test.exe
 *   （确保 WasapiDll.dll 在同一目录或 PATH 中）
 */

#include <Windows.h>
#include <stdio.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

// ============================================================================
// 从 WasapiDll.h 复制过来的类型定义（MinGW 没有头文件时直接用这些）
// ============================================================================

typedef enum {
    WASAPI_CAPTURE_INCLUDE_PROCESS = 0,
    WASAPI_CAPTURE_EXCLUDE_PROCESS = 1,
    WASAPI_CAPTURE_SYSTEM_WIDE = 2
} WasapiCaptureMode;

typedef enum {
    WASAPI_FORMAT_PCM_16BIT_44100 = 0,
    WASAPI_FORMAT_PCM_16BIT_48000 = 1,
    WASAPI_FORMAT_FLOAT_48000 = 2
} WasapiAudioFormat;

typedef void (__cdecl *WasapiAudioCallback)(
    void* userData,
    const uint8_t* buffer,
    uint32_t bytes,
    WasapiAudioFormat format
);

typedef void (__cdecl *WasapiStatusCallback)(
    void* userData,
    int errorCode,
    const wchar_t* message
);

// 函数指针类型
typedef int  (__cdecl *pfnWasapiDll_Init)(void);
typedef int  (__cdecl *pfnWasapiDll_StartCapture)(
    DWORD targetPid,
    WasapiCaptureMode captureMode,
    WasapiAudioFormat audioFormat,
    WasapiAudioCallback audioCallback,
    WasapiStatusCallback statusCallback,
    void* userData
);
typedef int  (__cdecl *pfnWasapiDll_StopCapture)(void);
typedef void (__cdecl *pfnWasapiDll_Deinit)(void);
typedef BOOL (__cdecl *pfnWasapiDll_IsCapturing)(void);
typedef const wchar_t* (__cdecl *pfnWasapiDll_GetLastError)(void);
typedef BOOL (__cdecl *pfnWasapiDll_IsProcessCaptureSupported)(void);

// ============================================================================
// WAV 文件写入辅助
// ============================================================================

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4] = {'R','I','F','F'};
    uint32_t fileSize = 0;
    char     wave[4] = {'W','A','V','E'};
    char     fmt[4]  = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;      // PCM
    uint16_t numChannels = 2;      // Stereo
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 48000 * 2 * 2;
    uint16_t blockAlign = 4;
    uint16_t bitsPerSample = 16;
    char     data[4] = {'d','a','t','a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)

struct WriteContext {
    FILE* fp = nullptr;
    WavHeader header;
    uint32_t totalDataBytes = 0;
    CRITICAL_SECTION cs;
};

static WriteContext g_writeCtx;

// 音频数据回调（在 DLL 内部线程中调用）
void __cdecl AudioCallback(void* userData, const uint8_t* buffer, uint32_t bytes, WasapiAudioFormat format) {
    WriteContext* ctx = (WriteContext*)userData;

    EnterCriticalSection(&ctx->cs);

    if (ctx->fp) {
        fwrite(buffer, 1, bytes, ctx->fp);
        ctx->totalDataBytes += bytes;
    }

    LeaveCriticalSection(&ctx->cs);
}

// 状态回调
void __cdecl StatusCallback(void* userData, int errorCode, const wchar_t* message) {
    if (message) {
        // 宽字符转多字节输出到控制台
        char buf[512];
        WideCharToMultiByte(CP_ACP, 0, message, -1, buf, sizeof(buf), nullptr, nullptr);
        printf("[DLL Status] Code=%d: %s\n", errorCode, buf);
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    printf("========================================\n");
    printf("WasapiDll MinGW 调用测试\n");
    printf("========================================\n\n");

    // ------------------------------------------------------------------------
    // 1. 加载 DLL
    // ------------------------------------------------------------------------
    HMODULE hDll = LoadLibraryA("WasapiDll.dll");
    if (!hDll) {
        printf("[ERROR] 无法加载 WasapiDll.dll，请确保 DLL 在当前目录或 PATH 中\n");
        printf("        GetLastError = %lu\n", GetLastError());
        return 1;
    }
    printf("[OK] WasapiDll.dll 加载成功\n");

    // 获取函数地址
    auto pInit = (pfnWasapiDll_Init)GetProcAddress(hDll, "WasapiDll_Init");
    auto pStart = (pfnWasapiDll_StartCapture)GetProcAddress(hDll, "WasapiDll_StartCapture");
    auto pStop = (pfnWasapiDll_StopCapture)GetProcAddress(hDll, "WasapiDll_StopCapture");
    auto pDeinit = (pfnWasapiDll_Deinit)GetProcAddress(hDll, "WasapiDll_Deinit");
    auto pIsCapturing = (pfnWasapiDll_IsCapturing)GetProcAddress(hDll, "WasapiDll_IsCapturing");
    auto pGetLastError = (pfnWasapiDll_GetLastError)GetProcAddress(hDll, "WasapiDll_GetLastError");
    auto pIsSupported = (pfnWasapiDll_IsProcessCaptureSupported)GetProcAddress(hDll, "WasapiDll_IsProcessCaptureSupported");

    if (!pInit || !pStart || !pStop || !pDeinit) {
        printf("[ERROR] 无法获取 DLL 导出函数\n");
        FreeLibrary(hDll);
        return 1;
    }
    printf("[OK] 所有导出函数获取成功\n");

    // ------------------------------------------------------------------------
    // 2. 检查系统支持
    // ------------------------------------------------------------------------
    if (pIsSupported) {
        BOOL supported = pIsSupported();
        printf("[INFO] 进程级音频捕获支持: %s\n", supported ? "是" : "否");
        if (!supported) {
            printf("[WARN] 当前系统不支持进程级捕获，将回退到系统级捕获\n");
        }
    }

    // ------------------------------------------------------------------------
    // 3. 初始化
    // ------------------------------------------------------------------------
    if (pInit() != 0) {
        const wchar_t* err = pGetLastError ? pGetLastError() : L"Unknown";
        char buf[512];
        WideCharToMultiByte(CP_ACP, 0, err, -1, buf, sizeof(buf), nullptr, nullptr);
        printf("[ERROR] WasapiDll_Init 失败: %s\n", buf);
        FreeLibrary(hDll);
        return 1;
    }
    printf("[OK] WasapiDll_Init 成功\n");

    // ------------------------------------------------------------------------
    // 4. 准备 WAV 文件
    // ------------------------------------------------------------------------
    const char* outputFile = "captured_excluded.wav";
    g_writeCtx.fp = fopen(outputFile, "wb");
    if (!g_writeCtx.fp) {
        printf("[ERROR] 无法创建输出文件: %s\n", outputFile);
        pDeinit();
        FreeLibrary(hDll);
        return 1;
    }

    // 先写入占位头，捕获结束后再回填
    fwrite(&g_writeCtx.header, sizeof(WavHeader), 1, g_writeCtx.fp);
    InitializeCriticalSection(&g_writeCtx.cs);
    printf("[OK] 输出文件已准备: %s\n", outputFile);

    // ------------------------------------------------------------------------
    // 5. 获取当前进程 PID，以 EXCLUDE_PROCESS 模式启动捕获
    // ------------------------------------------------------------------------
    DWORD myPid = GetCurrentProcessId();
    printf("[INFO] 当前进程 PID: %lu\n", myPid);
    printf("[INFO] 捕获模式: EXCLUDE_PROCESS（排除当前进程）\n");
    printf("[INFO] 音频格式: PCM 16-bit 48000Hz 立体声\n\n");

    int result = pStart(
        myPid,
        WASAPI_CAPTURE_EXCLUDE_PROCESS,   // 排除自己
        WASAPI_FORMAT_PCM_16BIT_48000,    // 16-bit PCM 48kHz
        AudioCallback,
        StatusCallback,
        &g_writeCtx
    );

    if (result != 0) {
        const wchar_t* err = pGetLastError ? pGetLastError() : L"Unknown";
        char buf[512];
        WideCharToMultiByte(CP_ACP, 0, err, -1, buf, sizeof(buf), nullptr, nullptr);
        printf("[ERROR] WasapiDll_StartCapture 失败 (code=%d): %s\n", result, buf);
        fclose(g_writeCtx.fp);
        DeleteCriticalSection(&g_writeCtx.cs);
        pDeinit();
        FreeLibrary(hDll);
        return 1;
    }

    printf("[OK] 捕获已启动！\n");
    printf("\n========================================\n");
    printf("现在进程内将发出 Beep 声音...\n");
    printf("如果过滤成功，WAV 文件中不应包含这些 Beep 声\n");
    printf("========================================\n\n");

    // ------------------------------------------------------------------------
    // 6. 进程内发出声音（这些声音应该被过滤掉）
    // ------------------------------------------------------------------------
    printf("发出声音 1: Beep 800Hz, 500ms...\n");
    Beep(800, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    printf("发出声音 2: Beep 1000Hz, 500ms...\n");
    Beep(1000, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    printf("发出声音 3: Beep 1200Hz, 500ms...\n");
    Beep(1200, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    printf("发出声音 4: MessageBeep...\n");
    MessageBeep(MB_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    printf("\n[OK] 声音播放完毕\n");
    printf("[INFO] 继续捕获 2 秒环境音...\n");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ------------------------------------------------------------------------
    // 7. 停止捕获
    // ------------------------------------------------------------------------
    printf("\n停止捕获...\n");
    pStop();

    // ------------------------------------------------------------------------
    // 8. 回填 WAV 文件头
    // ------------------------------------------------------------------------
    EnterCriticalSection(&g_writeCtx.cs);

    g_writeCtx.header.fileSize = 36 + g_writeCtx.totalDataBytes;
    g_writeCtx.header.dataSize = g_writeCtx.totalDataBytes;

    fseek(g_writeCtx.fp, 0, SEEK_SET);
    fwrite(&g_writeCtx.header, sizeof(WavHeader), 1, g_writeCtx.fp);
    fclose(g_writeCtx.fp);
    g_writeCtx.fp = nullptr;

    LeaveCriticalSection(&g_writeCtx.cs);
    DeleteCriticalSection(&g_writeCtx.cs);

    printf("[OK] WAV 文件已保存: %s\n", outputFile);
    printf("[INFO] 文件大小: %.2f KB\n", (36.0 + g_writeCtx.totalDataBytes) / 1024.0);

    // ------------------------------------------------------------------------
    // 9. 清理
    // ------------------------------------------------------------------------
    pDeinit();
    FreeLibrary(hDll);

    printf("\n========================================\n");
    printf("测试完成！\n");
    printf("========================================\n");
    printf("\n验证方法：\n");
    printf("  1. 用音频播放器打开 captured_excluded.wav\n");
    printf("  2. 如果过滤成功，文件中不应有明显的 800/1000/1200Hz 正弦波音调\n");
    printf("  3. 如果过滤失败，你会听到清晰的 Beep 声\n");
    printf("\n提示：\n");
    printf("  - 确保测试期间有其他程序在播放声音（如音乐播放器）\n");
    printf("  - 其他程序的声音应该被正常录制到 WAV 中\n");
    printf("  - 如果系统完全静音，WAV 文件可能只有很小的底噪\n");

    return 0;
}