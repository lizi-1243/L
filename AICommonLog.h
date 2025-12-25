#pragma once
#include <afxwin.h>   // MFC核心头文件
#include <string>     // 字符串处理
#include <ctime>      // 时间戳
#include <windows.h>  // Windows API（OutputDebugString、GUID）
#include <guiddef.h>  // UUID生成
#include <objbase.h>  // COM初始化
#include <afxmt.h>    // MFC临界区（线程安全）

// AI日志等级（对应VS调试窗口的日志级别）
enum class AILogLevel {
    LOG_INFO,    // 常规运行信息
    LOG_WARN,    // 潜在风险（如参数超限）
    LOG_ERROR,   // 严重错误（如推理失败）
    LOG_FATAL    // 系统崩溃（如GPU显存不足）
};

// AI日志核心类（单例模式，避免多实例冲突）
class CAICommonLog {
public:
    // 获取单例实例（线程安全）
    static CAICommonLog* GetInstance() {
        static CAICommonLog instance;  // C++11后静态局部变量默认线程安全
        return &instance;
    }

    // 初始化日志（指定日志文件路径，默认在程序目录的AI_Logs文件夹）
    void InitLog(CString strCustomLogPath = _T("")) {
        // 1. 确定日志文件路径
        if (strCustomLogPath.IsEmpty()) {
            // 获取程序运行目录
            TCHAR szExePath[MAX_PATH] = { 0 };
            GetModuleFileName(NULL, szExePath, MAX_PATH); // 获取EXE路径
            CString strExeDir = szExePath;
            strExeDir = strExeDir.Left(strExeDir.ReverseFind(_T('\\'))); // 截取目录（去掉EXE文件名）

            // 创建AI_Logs子目录（不存在则创建）
            CString strLogDir = strExeDir + _T("\\AI_Logs");
            if (!PathFileExists(strLogDir)) { // 检查目录是否存在
                CreateDirectory(strLogDir, NULL); // 创建目录
            }
            if (!PathFileExists(strLogDir)) {
                CString errMsg;
                errMsg.Format(_T("Failed to create log directory: %s"), strLogDir);
                OutputDebugString(errMsg);
            }
            // 按日期生成日志文件名（避免单文件过大）
            SYSTEMTIME st;
            GetLocalTime(&st); // 获取本地时间
            CString strLogFileName;
            strLogFileName.Format(_T("%04d%02d%02d_AI_Log.log"), st.wYear, st.wMonth, st.wDay);

            // 最终日志路径：程序目录\AI_Logs\20251218_AI_Log.log
            m_strLogFilePath = strLogDir + _T("\\") + strLogFileName;
        }
        else {
            m_strLogFilePath = strCustomLogPath; // 使用自定义路径
        }

        // 2. 初始化COM库（生成UUID需要）
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr)) {
            OutputDebugString(_T("COM库初始化失败，日志ID生成可能异常！\n"));
        }
    }

    // 核心接口：写入结构化AI日志
    // 参数说明：
    // eLevel: 日志等级
    // strModule: AI模块名（如AudioDenoise/ImageRecognition）
    // strModelVer: 模型版本（如V1.2_Batch202512）
    // strInput: 输入参数（采样率/数据格式/推理参数等）
    // strOutput: 输出指标（信噪比/准确率/耗时等）
    // strResource: 资源占用（CPU/GPU/显存等）
    // strError: 错误信息（仅ERROR/FATAL等级需填）
    void WriteAILog(
        AILogLevel eLevel,
        CString strModule,
        CString strModelVer,
        CString strInput,
        CString strOutput,
        CString strResource,
        CString strError = _T("")
    ) {
        // 1. 生成AI日志的核心字段
        CString strLogID = GenerateUUID();          // 唯一日志ID（UUID）
        CString strTimeStamp = GetTimeStamp();      // 带时区的时间戳
        CString strLogLevel = GetLogLevelStr(eLevel);// 日志等级文本（INFO/ERROR等）

        // 2. 拼接结构化日志内容（严格对齐AI日志模板）
        CString strLogContent;
        strLogContent.Format(_T(R"(
============================================================
[日志ID] %s
[时间戳] %s
[日志等级] %s
[项目/模块名称] %s
[模型版本] %s
[输入数据参数] %s
[输出核心指标] %s
[资源占用情况] %s
[错误信息] %s
============================================================
)"),
strLogID, strTimeStamp, strLogLevel, strModule,
strModelVer, strInput, strOutput, strResource, strError);

        // 3. 输出到VS调试窗口（关键：在VS中实时查看）
        OutputDebugString(strLogContent);

        // 4. 写入本地日志文件（线程安全，避免多线程冲突）
        WriteLogToFile(strLogContent);
    }

    // 获取日志文件路径（用于后续上传GitHub）
    CString GetLogFilePath() {
        return m_strLogFilePath;
    }

    // 释放日志资源
    void UninitLog() {
        CoUninitialize(); // 释放COM库
    }

private:
    // 私有构造函数（单例模式，禁止外部实例化）
    CAICommonLog() = default;
    ~CAICommonLog() = default;
    CAICommonLog(const CAICommonLog&) = delete; // 禁止拷贝
    CAICommonLog& operator=(const CAICommonLog&) = delete; // 禁止赋值

    // 生成UUID（日志唯一标识）
    CString GenerateUUID() {
        GUID guid;
        CoCreateGuid(&guid); // 生成全局唯一ID
        TCHAR szUUID[64] = { 0 };
        StringFromGUID2(guid, szUUID, 64); // 转换为字符串
        return szUUID;
    }

    // 生成带时区的时间戳（格式：2025-12-18T10:00:00.123+08:00）
    CString GetTimeStamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        CString strTime;
        strTime.Format(_T("%04d-%02d-%02dT%02d:%02d:%02d.%03d+08:00"),
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        return strTime;
    }

    // 日志等级转文本
    CString GetLogLevelStr(AILogLevel eLevel) {
        switch (eLevel) {
        case AILogLevel::LOG_INFO:  return _T("INFO");
        case AILogLevel::LOG_WARN:  return _T("WARN");
        case AILogLevel::LOG_ERROR: return _T("ERROR");
        case AILogLevel::LOG_FATAL: return _T("FATAL");
        default: return _T("INFO");
        }
    }

    // 写入日志文件（加临界区锁，保证线程安全）
    void WriteLogToFile(CString strContent) {
        CSingleLock lock(&m_csLog, TRUE); // MFC临界区锁（阻塞直到获取锁）

        // 打开文件：创建（不存在则建）+ 写入 + 不截断（追加模式）
        CFile file;
        CFileException ex;
        if (file.Open(m_strLogFilePath,
            CFile::modeCreate | CFile::modeWrite | CFile::modeNoTruncate, &ex)) {
            file.SeekToEnd(); // 定位到文件末尾（追加写入）
            // 写入内容：CString转char*，避免Unicode乱码
            file.Write(strContent.GetString(), strContent.GetLength() * sizeof(TCHAR));
            file.Close();
        }
        else {
            // 写入失败时，输出错误到VS调试窗口
            CString strErr;
            strErr.Format(_T("日志文件写入失败！错误码：%d，路径：%s\n"), ex.m_cause, m_strLogFilePath);
            OutputDebugString(strErr);
        }
    }

    // 成员变量
    CString m_strLogFilePath;       // 日志文件路径
    CCriticalSection m_csLog;       // 临界区锁（线程安全）
};

// 简化调用的宏（避免重复写长代码）
#define AI_LOG_INIT()          CAICommonLog::GetInstance()->InitLog()
#define AI_LOG_UNINIT()        CAICommonLog::GetInstance()->UninitLog()
#define AI_LOG_GET_PATH()      CAICommonLog::GetInstance()->GetLogFilePath()
#define AI_LOG_INFO(module, ver, input, output, resource) \
    CAICommonLog::GetInstance()->WriteAILog(AILogLevel::LOG_INFO, module, ver, input, output, resource)
#define AI_LOG_ERROR(module, ver, input, output, resource, err) \
    CAICommonLog::GetInstance()->WriteAILog(AILogLevel::LOG_ERROR, module, ver, input, output, resource, err)
#define AI_LOG_WARN(module, ver, input, output, resource) \
    CAICommonLog::GetInstance()->WriteAILog(AILogLevel::LOG_WARN, module, ver, input, output, resource)
#define AI_LOG_FATAL(module, ver, input, output, resource, err) \
    CAICommonLog::GetInstance()->WriteAILog(AILogLevel::LOG_FATAL, module, ver, input, output, resource, err)