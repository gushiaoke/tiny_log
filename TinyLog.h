#pragma once
#include <string>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <mutex>

#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <atlbase.h>
#include <atlfile.h>

namespace TinyLog {
enum class LogLevel : int {
    LogLevelTrace,
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarn,
    LogLevelError,
    LogLevelFatal,
};

inline const wchar_t* LogLevelStr(LogLevel level) {
    switch (level) {
        case LogLevel::LogLevelTrace:
            return L"[TRACE]";
        case LogLevel::LogLevelDebug:
            return L"[DEBUG]";
        case LogLevel::LogLevelInfo:
            return L"[INFO]";
        case LogLevel::LogLevelWarn:
            return L"[WARN]";
        case LogLevel::LogLevelError:
            return L"[ERROR]";
        case LogLevel::LogLevelFatal:
            return L"[FATAL]";
        default:
            return L"[UNKNOWN]";
    }
}

class CTinyLog {
 public:
    CTinyLog() {
		std::wostringstream stream;
		wchar_t szFilePath[MAX_PATH]{};
		::GetModuleFileName(nullptr, szFilePath, MAX_PATH);
		::PathRemoveExtension(szFilePath);
		const wchar_t* lpFileName = ::PathFindFileName(szFilePath);
		DWORD dwPid = ::GetCurrentProcessId();

		stream << lpFileName
			<< L"-" << dwPid;

        BOOL bAlreadyExisted{};
		auto hr = m_sharedMemory.MapSharedMem(sizeof(ShareData), stream.str().c_str(), &bAlreadyExisted);
		if (SUCCEEDED(hr)) {
			m_pShareData = m_sharedMemory;
            if (!bAlreadyExisted) {
				::InitializeCriticalSection(&m_pShareData->cs);
				m_pShareData->bInit = TRUE;
                m_pShareData->hFile = INVALID_HANDLE_VALUE;
            }
        }
    }

    ~CTinyLog() {
        if (m_pShareData) {
            if (m_pShareData->hFile != INVALID_HANDLE_VALUE) {
                ::CloseHandle(m_pShareData->hFile);
                m_pShareData->hFile = INVALID_HANDLE_VALUE;
            }
            ::DeleteCriticalSection(&m_pShareData->cs);
        }
        
    }

    void Write(LogLevel level, const wchar_t* lpFileName, int nLine, const wchar_t* lpFunc, LPCWSTR szFormat, ...) {
        if (!EnsureCreateLogFile()) {
            return;
        }

        va_list arglist;
        constexpr int MAX_LOG_BUFFER = 2048;
        wchar_t logBuffer[MAX_LOG_BUFFER] = { 0 };

        va_start(arglist, szFormat);
        _vsnwprintf_s(logBuffer, MAX_LOG_BUFFER - 1, _TRUNCATE, szFormat, arglist);
        va_end(arglist);

        wchar_t szTime[60]{};

        SYSTEMTIME st{};
        ::GetLocalTime(&st);
        wsprintf(szTime, L"%d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

		wchar_t szModule[MAX_PATH]{};
		::GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase), szModule, MAX_PATH);
		::PathRemoveExtension(szModule);

        std::wostringstream stream;
        stream << L"[" << szTime << L"]" 
            << LogLevelStr(level) 
            << L"[" << ::PathFindFileName(szModule) << L"!" << lpFunc << L"(" << ::PathFindFileName(lpFileName) << L":" << nLine << L")]" 
            << logBuffer << std::endl;

        std::string content(ATL::CW2A(stream.str().c_str(), CP_UTF8));

        ::EnterCriticalSection(&m_pShareData->cs);
        DWORD dwWritten{};
        ::WriteFile(m_pShareData->hFile, content.data(), static_cast<DWORD>(content.size()), &dwWritten, nullptr);
        ::LeaveCriticalSection(&m_pShareData->cs);
    }

private:
    bool EnsureCreateLogFile() {
        if (m_pShareData == nullptr) {
            return FALSE;
        }

        if (m_pShareData->hFile != INVALID_HANDLE_VALUE) {
            return true;
        }

        std::error_code ec;
        auto p = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return false;
        }

        std::wostringstream stream;
        wchar_t szFilePath[MAX_PATH]{};
        ::GetModuleFileName(nullptr, szFilePath, MAX_PATH);
        ::PathRemoveExtension(szFilePath);
        const wchar_t* lpFileName = ::PathFindFileName(szFilePath);
        DWORD dwPid = ::GetCurrentProcessId();
        std::time_t t = std::time(nullptr);
        wchar_t szTime[60]{};
        std::tm tm{};
        localtime_s(&tm, &t);
        std::wcsftime(szTime, 60, L"%F %T", &tm);
        std::transform(szTime, szTime + wcslen(szTime), szTime, [](wchar_t n) {
            return n == L':' ? L'-' : n;
            });

        p.append(L"tiny_log");
        std::filesystem::create_directories(p, ec);
        if (ec) {
            return false;
        }

        stream << lpFileName
            //<< L"-" << szTime
            << L"-" << dwPid << L".log";

        p.append(stream.str());

        m_pShareData->hFile = ::CreateFile(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_pShareData->hFile != INVALID_HANDLE_VALUE) {
            ::SetFilePointer(m_pShareData->hFile, 0, 0, FILE_END);
        }

        return m_pShareData->hFile != INVALID_HANDLE_VALUE;
    }

 private:
    struct ShareData {
        HANDLE hFile{};
        CRITICAL_SECTION cs;
        BOOL bInit = FALSE;
    };

    CAtlFileMapping<ShareData> m_sharedMemory;
    ShareData* m_pShareData{};
};

#ifdef ENABLE_TINY_LOG
inline CTinyLog g_tinyLog;
#endif


class FuncRecord {
public:
    FuncRecord(const wchar_t* lpFileName, int nLine, const wchar_t* lpFunc) : m_strFileName(lpFileName), m_strFunc(lpFunc), m_nLine(nLine) {
#ifdef ENABLE_TINY_LOG
		TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelTrace, m_strFileName.c_str(), m_nLine, m_strFunc.c_str(), L"enter");
#endif
    }

    ~FuncRecord() {
#ifdef ENABLE_TINY_LOG
		TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelTrace, m_strFileName.c_str(), m_nLine, m_strFunc.c_str(), L"leave");
#endif
    }

private:
    std::wstring m_strFileName;
    std::wstring m_strFunc;
    int m_nLine{};
};

}

#ifdef ENABLE_TINY_LOG
#define TINY_LOG_TRACE(format, ...) TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelTrace, __FILEW__, __LINE__, __FUNCTIONW__, format, __VA_ARGS__)
#define TINY_LOG_DEBUG(format, ...) TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelDebug, __FILEW__, __LINE__, __FUNCTIONW__, format, __VA_ARGS__)
#define TINY_LOG_INFO(format, ...) TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelInfo, __FILEW__, __LINE__, __FUNCTIONW__, format, __VA_ARGS__)
#define TINY_LOG_WARN(format, ...) TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelWarn, __FILEW__, __LINE__, __FUNCTIONW__, format, __VA_ARGS__)
#define TINY_LOG_ERROR(format, ...) TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelError, __FILEW__, __LINE__, __FUNCTIONW__, format, __VA_ARGS__)
#define TINY_LOG_FATAL(format, ...) TinyLog::g_tinyLog.Write(TinyLog::LogLevel::LogLevelFatal, __FILEW__, __LINE__, __FUNCTIONW__, format, __VA_ARGS__)
#define TINY_LOG_FUNC_RECORD() TinyLog::FuncRecord __func_record__(__FILEW__, __LINE__, __FUNCTIONW__)
#else
#define TINY_LOG_TRACE
#define TINY_LOG_DEBUG
#define TINY_LOG_INFO
#define TINY_LOG_WARN
#define TINY_LOG_ERROR
#define TINY_LOG_FATAL
#define TINY_LOG_FUNC_RECORD
#endif  // TINY_LOG



