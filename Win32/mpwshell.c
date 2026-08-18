// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "framework.h"
#include "mpwshell.h"

#define MPWSHELL_DEFAULT_STOCK_FONT ANSI_FIXED_FONT

static LPCWSTR szTitle, szWindowClass, szGoingToFar, szInitialScript,
    szCannotFindFile, szTitleWithFile, szSaveFile, szSaveContent, szSaveFilter, szSendFeedbackURL;
static NONCLIENTMETRICSW NonClientMetrics = {sizeof(NONCLIENTMETRICSW)};
static HCURSOR hWaitCursor, hArrowCursor, hCaretCursor;
static LPCWSTR registryKey = L"SOFTWARE\\rhubarb-geek-nz\\MPW Shell";
static LPCWSTR registryCommandValue = L"ToolServer";
static LPCWSTR registryTextLimit = L"TextLimit";
static LPCWSTR registryInitialScript = L"InitialScript";
static LPCWSTR registryLogFont = L"LogFont";
static DWORD dwTextLimit = 0x100000, dwInitialScript;
static UINT WM_FINDMSG;
static BYTE UTF8BOM[] = { 0xEF, 0xBB, 0xBF };
static WORD UNICODEBOM = 0xFEFF;

#define CP_UNICODE   1200

struct CHARBUF
{
    DWORD dwLen, clientId, runspaceId, msgType;
    struct CHARBUF* next;
    WCHAR buf[1];
};

struct APPDATA
{
    HINSTANCE hInstance;
    HANDLE hReadThread, hWriteThread, hWriteEvent, hPipeRead, hPipeWrite;
    PROCESS_INFORMATION processInfo;
    DWORD tidReadThread, tidWriteThread, clientSequence;
    struct WINDATA* first;
    BOOL bRunning;
    CRITICAL_SECTION crit;
    int fileCodePage;
    LOGFONTW logFont;
    BOOL hasLogFont;
    struct CHARBUF* writeQueue;
};

#define MPW_DEAD        0
#define MPW_PAIRING     1
#define MPW_PAIRED      2

struct WINDATA
{
    HWND hWnd, hEditContent, hwndFindReplace;
    HFONT hFont, hFontScale;
    LPWSTR filePath, fileName;
    struct WINDATA* next;
    struct APPDATA* app;
    DWORD runspaceId, clientId, tidAlertBox, iBusy, lUsage, obituaryReceived;
    BYTE state, wordWrap;
    HANDLE hThreadAlertBox;
    HCURSOR hLastCursor;
    int perCentScale, iFindReplace, fileCodePage;
    struct CHARBUF* messageQueue;
    struct CHARBUF* readQueue;
    FINDREPLACEW findReplace;
    WCHAR findWhat[256];
    WCHAR replaceWith[256];
};

static struct APPDATA appData;

static void QueueWriteMessage(struct APPDATA* appData, struct WINDATA* winData, struct CHARBUF* output)
{
    if (winData)
    {
        output->clientId = winData->clientId;
        output->runspaceId = winData->runspaceId;
    }

    EnterCriticalSection(&appData->crit);

    if (appData->writeQueue)
    {
        struct CHARBUF* p = appData->writeQueue;
        while (p->next) p = p->next;
        p->next = output;
    }
    else
    {
        appData->writeQueue = output;
    }

    SetEvent(appData->hWriteEvent);

    LeaveCriticalSection(&appData->crit);
}

static void WinData_Release(struct WINDATA* winData)
{
    if (!InterlockedDecrement(&winData->lUsage))
    {
        struct APPDATA* app = winData->app;

        if (app)
        {
            EnterCriticalSection(&app->crit);

            if (winData->app == app)
            {
                if (app->first == winData)
                {
                    app->first = winData->next;
                }
                else
                {
                    struct WINDATA* p = app->first;

                    while (p->next != winData) p = p->next;

                    p->next = winData->next;
                }
            }

            LeaveCriticalSection(&app->crit);
        }

        if (winData->filePath)
        {
            LocalFree(winData->filePath);
        }

        if (winData->hThreadAlertBox)
        {
            CloseHandle(winData->hThreadAlertBox);
        }

        LocalFree(winData);
    }
}

static void ShowError(DWORD err)
{
    WCHAR buf[1024];
    DWORD dw = FormatMessageW(
        FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        err,
        0,
        buf,
        (sizeof(buf) / sizeof(buf[0])) - 2,
        NULL);

    if (dw)
    {
        buf[dw] = 0;
    }
    else
    {
        _snwprintf_s(buf,
            sizeof(buf) / sizeof(buf[0]),
            (sizeof(buf) / sizeof(buf[0]))-1,
            L"Error %08lx", err);
    }

    MessageBoxW(NULL, buf, szTitle[0] ? szTitle : L"mpwshell", MB_OK | MB_ICONHAND);
}

static DWORD WINAPI WriteThread(LPVOID  pv)
{
    struct APPDATA* appData = pv;
    BOOL bRunning = TRUE;

    __try
    {
        while (bRunning)
        {
            struct CHARBUF* buf;

            EnterCriticalSection(&appData->crit);

            buf = appData->writeQueue;

            if (buf)
            {
                appData->writeQueue = buf->next;
            }

            bRunning = appData->bRunning;

            LeaveCriticalSection(&appData->crit);

            if (!bRunning)
            {
                if (buf)
                {
                    LocalFree(buf);
                }

                break;
            }

            if (buf)
            {
                int i = buf->dwLen ? WideCharToMultiByte(CP_UTF8, 0, buf->buf, buf->dwLen, NULL, 0, NULL, NULL) : 0;

                if (i >= 0)
                {
                    BYTE* p = LocalAlloc(LMEM_ZEROINIT, i + 17);

                    if (!p)
                    {
                        LocalFree(buf);
                        break;
                    }

                    int j = buf->dwLen ? WideCharToMultiByte(CP_UTF8, 0, buf->buf, buf->dwLen, p + 16, i + 1, NULL, NULL) : 0;

                    if (j >= 0)
                    {
                        DWORD* header = (DWORD*)p;
                        header[0] = buf->msgType;
                        header[1] = buf->clientId;
                        header[2] = buf->runspaceId;
                        header[3] = j;

                        j += 16;
                        i = 0;

                        while (i < j)
                        {
                            DWORD dwLen;

                            if (!WriteFile(appData->hPipeWrite, p + i, j - i, &dwLen, NULL))
                            {
                                break;
                            }

                            i += dwLen;
                        }

                        if (i < j)
                        {
                            LocalFree(p);
                            LocalFree(buf);
                            break;
                        }
                    }

                    LocalFree(p);
                }

                LocalFree(buf);
            }
            else
            {
                FlushFileBuffers(appData->hPipeWrite);
                WaitForSingleObject(appData->hWriteEvent, INFINITE);
            }
        }
    }
    __finally
    {
        HANDLE h = appData->hPipeWrite;
        appData->hPipeWrite = INVALID_HANDLE_VALUE;
        CloseHandle(h);
    }

    return 0;
}

static const char* magicPrefix = "0c63fba6-7c2a-4b72-8de0-b3bd579dedaa";
static const char* magicCRLF = "\r\n";

static BOOL ReadThreadSignature(struct APPDATA *appData)
{
	BOOL bFoundHeader = FALSE;
    char buf[4097];
    DWORD bufLen = 0;
    DWORD magicPrefixLen = (DWORD)strlen(magicPrefix);

    while (appData->bRunning && !bFoundHeader)
    {
        DWORD dwLen = 0;
        const char* lastCRLF = NULL;
        const char* p = buf;
        DWORD amountToSend = 0;

        if (!ReadFile(appData->hPipeRead, buf + bufLen, sizeof(buf) - bufLen - 1, &dwLen, NULL))
        {
            break;
        }

        if (!dwLen) break;

        bufLen += dwLen;

        if (bufLen >= (sizeof(buf)-1))
		{
			break;
		}

        buf[bufLen] = 0;

        while (p)
        {
            p = strstr(p, magicCRLF);

            if (p)
            {
                lastCRLF = p;
                p += 2;
            }
        }

        if (lastCRLF)
        {
            size_t lastOffset = lastCRLF - buf;

            if (lastOffset >= magicPrefixLen)
            {
                if (lastOffset == (bufLen - 2))
                {
                    if (!memcmp(buf + lastOffset - magicPrefixLen, magicPrefix, magicPrefixLen))
                    {
                        bFoundHeader = TRUE;
                        bufLen -= magicPrefixLen + 2;
                        amountToSend = bufLen;
                    }
                    else
                    {
                        amountToSend = (DWORD)(lastOffset + 2);
                    }
                }
                else
                {
                    amountToSend = (DWORD)(lastOffset - magicPrefixLen);
                }
            }
        }
        else
        {
            if (bufLen > (magicPrefixLen + 2))
            {
                amountToSend = bufLen - magicPrefixLen - 2;
            }
        }

        if (amountToSend)
        {
            int j = MultiByteToWideChar(CP_ACP, 0, buf, amountToSend, NULL, 0);

            if (j > 0)
            {
                struct CHARBUF* p = LocalAlloc(LMEM_ZEROINIT, sizeof(*p) + (j + 2) * sizeof(p->buf[0]));

                if (p)
                {
                    p->dwLen = MultiByteToWideChar(CP_ACP, 0, buf, amountToSend, p->buf, j);
                    p->msgType = 0x46;

                    if (amountToSend == bufLen)
                    {
                        bufLen = 0;
                    }
                    else
                    {
                        bufLen -= amountToSend;
                        memmove(buf, buf + amountToSend, bufLen);
                    }

                    EnterCriticalSection(&appData->crit);

                    struct WINDATA* winData = appData->first;

                    if (winData)
                    {
                        if (winData->readQueue)
                        {
                            struct CHARBUF* q = winData->readQueue;

                            while (q->next) q = q->next;

                            q->next = p;
                        }
                        else
                        {
                            winData->readQueue = p;
                        }

                        PostMessage(winData->hWnd, WM_USER, 0, 0);
                    }

                    LeaveCriticalSection(&appData->crit);
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }

	return bFoundHeader;
}

static DWORD WINAPI ReadThread(LPVOID  pv)
{
    struct APPDATA* appData = pv;
    BOOL bFoundHeader = ReadThreadSignature(appData);

    __try
    {
        if (bFoundHeader)
        {
            appData->hWriteThread = CreateThread(NULL, 0, WriteThread, appData, 0, &appData->tidWriteThread);

            while (appData->bRunning)
            {
                DWORD header[4];
                DWORD headerLen = 0;

                while (headerLen < sizeof(header))
                {
                    DWORD dwLen = 0;
                    BOOL b = ReadFile(appData->hPipeRead, ((BYTE*)header) + headerLen, sizeof(header) - headerLen, &dwLen, NULL);

                    if (!b)
                    {
                        break;
                    }

                    if (dwLen == 0)
                    {
                        break;
                    }

                    headerLen += dwLen;
                }

                if (headerLen != sizeof(header))
                {
                    break;
                }

                DWORD msgLen = header[3];
                BYTE* buf = msgLen ? LocalAlloc(LMEM_ZEROINIT, msgLen) : NULL;

                if (msgLen && !buf)
                {
                    break;
                }

                DWORD i = 0;

                while (i < msgLen)
                {
                    DWORD dwLen = 0;
                    BOOL b = ReadFile(appData->hPipeRead, buf + i, msgLen - i, &dwLen, NULL);

                    if (!b)
                    {
                        break;
                    }

                    if (dwLen == 0)
                    {
                        break;
                    }

                    i += dwLen;
                }

                int j = msgLen ? MultiByteToWideChar(CP_UTF8, 0, buf, msgLen, NULL, 0) : 0;

                if (j >= 0)
                {
                    struct CHARBUF* p = LocalAlloc(LMEM_ZEROINIT, sizeof(*p) + (j + 2) * sizeof(p->buf[0]));

                    if (p)
                    {
                        struct WINDATA* winData;
                        p->msgType = header[0];
                        p->runspaceId = header[1];
                        p->clientId = header[2];

                        p->dwLen = j ? MultiByteToWideChar(CP_UTF8, 0, buf, msgLen, p->buf, j) : 0;

                        switch (p->msgType)
                        {
                        case 0x40:
                        case 0x42:
                        case 0x43:
                        case 0x44:
                        case 0x45:
                            p->buf[p->dwLen++] = '\r';
                            p->buf[p->dwLen++] = '\n';
                            break;
                        }

                        EnterCriticalSection(&appData->crit);

                        winData = appData->first;

                        BOOL found = FALSE;

                        while (winData && !found)
                        {
                            if ((winData->state == MPW_PAIRED) && winData->runspaceId == p->runspaceId && p->clientId == winData->clientId)
                            {
                                found = TRUE;
                            }
                            else
                            {
                                switch (p->msgType)
                                {
                                case 0x41:
                                    if (p->clientId == winData->clientId && winData->state == MPW_PAIRING)
                                    {
                                        found = TRUE;
                                    }
                                    break;
                                case 0x47:
                                case 0x48:
                                    if (p->clientId == winData->clientId && winData->state == MPW_PAIRED)
                                    {
                                        found = TRUE;
                                    }
                                    break;
                                }
                            }

                            if (found)
                            {
                                break;
                            }

                            winData = winData->next;
                        }

                        if (winData)
                        {
                            struct CHARBUF* q = winData->readQueue;

                            if (q)
                            {
                                while (q->next) q = q->next;
                                q->next = p;
                            }
                            else
                            {
                                winData->readQueue = p;
                            }

                            PostMessage(winData->hWnd, WM_USER, 0, 0);
                        }

                        LeaveCriticalSection(&appData->crit);
                    }
                }

                LocalFree(buf);
            }
        }
    }
    __finally
    {
        EnterCriticalSection(&appData->crit);

        HANDLE h = appData->hPipeRead;

        appData->hPipeRead = INVALID_HANDLE_VALUE;

        appData->tidReadThread = 0;

        struct WINDATA* winData = appData->first;

        while (winData)
        {
            struct CHARBUF* buf = LocalAlloc(LMEM_ZEROINIT, sizeof(*buf));

            if (buf)
            {
                buf->clientId = winData->clientId;
                buf->msgType = 0x60;

                if (winData->readQueue)
                {
                    struct CHARBUF* p = winData->readQueue;
                    while (p->next) p = p->next;
                    p->next = buf;
                }
                else
                {
                    winData->readQueue = buf;
                }

                PostMessage(winData->hWnd, WM_USER, 0, 0);
            }

            winData = winData->next;
        }

        LeaveCriticalSection(&appData->crit);

        CloseHandle(h);
    }

    return 0;
}

static BOOL AppData_OpenWindow(struct APPDATA* appData, LPCWSTR fileName, int nCmdShow, struct CHARBUF *msg)
{
    WCHAR filePath[MAX_PATH];
    LPWSTR baseName = NULL;
    DWORD dw = fileName ? GetFullPathNameW(fileName, sizeof(filePath) / sizeof(filePath[0]), filePath, &baseName) : 0;
    WCHAR buffer[MAX_PATH];
    LPCWSTR pTitle = szTitle;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    struct CHARBUF* content = NULL;
    int codePage = appData->fileCodePage;

    if (fileName && (dw == 0))
    {
        return FALSE;
    }

    if (fileName && msg && msg->msgType == 0x48)
    {
        EnterCriticalSection(&appData->crit);

        struct WINDATA* winData=appData->first;

        while (winData)
        {
            if (winData->hWnd && winData->filePath && CSTR_EQUAL == CompareStringOrdinal(msg->buf, msg->dwLen, winData->filePath, -1, TRUE))
            {
                break;
            }

            winData = winData->next;
        }

        LeaveCriticalSection(&appData->crit);

        if (winData)
        {
            SetForegroundWindow(winData->hWnd);

            return FALSE;
        }
    }

    if (baseName)
    {
        hFile = CreateFileW(filePath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();

            switch (err)
            {
            case ERROR_FILE_NOT_FOUND:
                {
                    _snwprintf_s(buffer,
                        sizeof(buffer) / sizeof(buffer[0]),
                        (sizeof(buffer) / sizeof(buffer[0])) - 1,
                        szCannotFindFile, baseName);

                    int i = MessageBoxW(NULL, buffer, szTitle, MB_YESNO | MB_ICONEXCLAMATION);

                    if (i != IDYES)
                    {
                        return FALSE;
                    }

                    hFile = CreateFileW(filePath,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ,
                        NULL,
                        CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                        DWORD err = GetLastError();

                        ShowError(err);

                        return FALSE;
                    }

                    CloseHandle(hFile);
                }
                break;

            default:
                ShowError(dw);
                return FALSE;
            }
        }
        else
        {
            LARGE_INTEGER bigSize;
            BOOL b = GetFileSizeEx(hFile, &bigSize);

            if (!b)
            {
                DWORD err = GetLastError();
                CloseHandle(hFile);
                ShowError(err);
                return FALSE;
            }

            if (bigSize.HighPart)
            {
                CloseHandle(hFile);
                ShowError(ERROR_FILE_INVALID);
                return FALSE;
            }

            if (bigSize.LowPart)
            {
                struct CHARBUF* p = LocalAlloc(LMEM_ZEROINIT, sizeof(*p) + bigSize.LowPart);
                DWORD dwLen = 0;

                if (!ReadFile(hFile, p->buf, bigSize.LowPart, &dwLen, NULL))
                {
                    DWORD err = GetLastError();
                    CloseHandle(hFile);
                    ShowError(err);
                    return FALSE;
                }

                CloseHandle(hFile);

                if (dwLen != bigSize.LowPart)
                {
                    ShowError(ERROR_FILE_INVALID);
                    return FALSE;
                }

                if (dwLen >= sizeof(UTF8BOM) && !memcmp(p->buf,UTF8BOM,sizeof(UTF8BOM)))
                {
                    codePage = CP_UTF8;
                }
                else
                {
                    if (dwLen >= sizeof(UNICODEBOM) && !memcmp(p->buf, &UNICODEBOM, sizeof(UNICODEBOM)))
                    {
                        codePage = CP_UNICODE;
                    }
                }

                if (codePage == CP_UNICODE)
                {
                    content = p;

                    content->dwLen = dwLen / sizeof(p->buf[0]);
                }
                else
                {
                    int i = MultiByteToWideChar(codePage, 0, (LPVOID)p->buf, dwLen, NULL, 0);

                    if (i > 0)
                    {
                        content = LocalAlloc(LMEM_ZEROINIT, sizeof(content[0]) + i * sizeof(content->buf[0]));

                        content->dwLen = MultiByteToWideChar(codePage, 0, (LPVOID)p->buf, dwLen, content->buf, i);
                    }

                    LocalFree(p);
                }
            }
        }

        _snwprintf_s(buffer,
            sizeof(buffer) / sizeof(buffer[0]),
            (sizeof(buffer) / sizeof(buffer[0])) - 1,
            szTitleWithFile, baseName);

        pTitle = buffer;
    }
    else
    {
        content = msg;
    }

    struct WINDATA* winData = LocalAlloc(LMEM_ZEROINIT, sizeof(*winData));

    if (!winData) return FALSE;

    winData->clientId = InterlockedIncrement(&appData->clientSequence);
    winData->lUsage = 1;
    winData->readQueue = content;
    winData->fileCodePage = codePage;

    if (msg)
    {
        winData->state = MPW_PAIRED;
        winData->runspaceId = msg->runspaceId;
    }
    else
    {
        winData->iBusy = 1;
    }

    if (baseName)
    {
        size_t i = wcslen(filePath);
        winData->filePath = LocalAlloc(LMEM_ZEROINIT, (i + 1) * sizeof(winData->filePath[0]));
        if (winData->filePath)
        {
            memcpy(winData->filePath, filePath, i * sizeof(winData->filePath[0]));
            winData->fileName = winData->filePath + (baseName - filePath);
        }
    }

    EnterCriticalSection(&appData->crit);

    winData->app = appData;
    winData->next = appData->first;
    appData->first = winData;

    if (msg)
    {
        struct CHARBUF* reply = LocalAlloc(LMEM_ZEROINIT, sizeof(*reply) + dwInitialScript * sizeof(reply->buf[0]));
        if (reply)
        {
            reply->msgType = 0x55;
            reply->dwLen = dwInitialScript;

            if (dwInitialScript)
            {
                winData->iBusy++;
                memcpy(reply->buf, szInitialScript, dwInitialScript * sizeof(reply->buf[0]));
            }

            QueueWriteMessage(appData, winData, reply);
        }
    }

    LeaveCriticalSection(&appData->crit);

    HWND hWnd = CreateWindowW(szWindowClass, pTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, appData->hInstance, winData);

    if (!hWnd)
    {
        WinData_Release(winData);

        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

static BOOL InitInstance(HINSTANCE hInstance)
{
    WCHAR cmdLine[4096];
    BOOL b;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFOW startup;
    int mask = 0;
    int keyCount = 0;
    DWORD lastRegistryStatus = 0;

    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NonClientMetrics), &NonClientMetrics, 0))
    {
        return FALSE;
    }

    appData.hInstance = hInstance;

    while (((mask & 3) != 7) && (keyCount < 2))
    {
        HKEY key = NULL;
        DWORD status = RegOpenKeyEx(
            keyCount++ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            registryKey, 0, KEY_QUERY_VALUE, &key);

        if (status)
        {
            lastRegistryStatus = status;
        }
        else
        {
            if (!(mask & 1))
            {
                DWORD cmdLineLen = sizeof(cmdLine), type = 0;

                status = RegGetValueW(key,
                    NULL,
                    registryCommandValue,
                    RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ,
                    &type,
                    cmdLine,
                    &cmdLineLen);

                if (status)
                {
                    lastRegistryStatus = status;
                }
                else
                {
                    mask |= 1;
                }
            }

            if (!(mask & 2))
            {
                DWORD dwLen = sizeof(dwTextLimit), type = 0;

                if (!RegGetValueW(key, NULL, registryTextLimit, RRF_RT_DWORD, &type, &dwTextLimit, &dwLen))
                {
                    mask |= 2;
                }
            }

            if (keyCount == 1)
            {
                DWORD dwLen = sizeof(appData.logFont), type = 0;

                if (!RegGetValueW(key, NULL, registryLogFont, RRF_RT_REG_BINARY, &type, &appData.logFont, &dwLen))
                {
                    appData.hasLogFont = TRUE;
                }
            }

            if (!(mask & 4))
            {
                DWORD initialScriptLen = 0, type = 0;

                if (!RegGetValueW(key,
                    NULL,
                    registryInitialScript,
                    RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ,
                    &type,
                    NULL,
                    &initialScriptLen))
                {
                    initialScriptLen += sizeof(szInitialScript[0]);

                    LPWSTR buf = LocalAlloc(LMEM_ZEROINIT, initialScriptLen);

                    if (RegGetValueW(key,
                        NULL,
                        registryInitialScript,
                        RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ,
                        &type,
                        buf,
                        &initialScriptLen))
                    {
                        LocalFree(buf);
                    }
                    else
                    {
                        dwInitialScript = initialScriptLen/sizeof(buf[0]);

                        while (dwInitialScript)
                        {
                            if (buf[dwInitialScript - 1]) break;
                            dwInitialScript--;
                        }

                        szInitialScript = buf;

                        mask |= 4;
                    }
                }
            }

            RegCloseKey(key);
        }
    }

    if ((mask & 1) != 1)
    {
        SetLastError(lastRegistryStatus);

        return FALSE;
    }

    appData.hWriteEvent = CreateEvent(NULL, 0, 0, NULL);
    appData.bRunning = TRUE;

    InitializeCriticalSection(&appData.crit);

    memset(&startup, 0, sizeof(startup));
    memset(&saAttr, 0, sizeof(saAttr));
    startup.cb = sizeof(startup);
    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    b = CreatePipe(&appData.hPipeRead, &startup.hStdOutput, &saAttr, 4096);
    b = CreatePipe(&startup.hStdInput, &appData.hPipeWrite, &saAttr, 4096);

    b = DuplicateHandle(GetCurrentProcess(), startup.hStdOutput,
        GetCurrentProcess(), &startup.hStdError,
        DUPLICATE_SAME_ACCESS, TRUE, DUPLICATE_SAME_ACCESS);

    b = SetHandleInformation(appData.hPipeRead, HANDLE_FLAG_INHERIT, 0);
    b = SetHandleInformation(appData.hPipeWrite, HANDLE_FLAG_INHERIT, 0);

    startup.dwFlags = STARTF_USESTDHANDLES;

    b = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &appData.processInfo);

    CloseHandle(startup.hStdInput);
    CloseHandle(startup.hStdOutput);
    CloseHandle(startup.hStdError);

    if (!b)
    {
        return b;
    }

    return TRUE;
}

static BOOL CALLBACK AboutChildren(HWND hWnd, LPARAM lParam)
{
    INT_PTR id = GetWindowLongPtr(hWnd, GWLP_ID);

    if (id == (INT_PTR)-1L)
    {
        WCHAR buf[128];
        DWORD dwLen = GetWindowText(hWnd, buf, sizeof(buf) / sizeof(buf[0]));

        if (dwLen > 3)
        {
            if (!wcscmp(buf + dwLen - 4, L" 1.0"))
            {
                VS_FIXEDFILEINFO* pFileInfo = (void*)lParam;

                _snwprintf_s(buf + dwLen - 3,
                    (sizeof(buf) / sizeof(buf[0])) - dwLen,
                    ((sizeof(buf) / sizeof(buf[0]))) - dwLen,
                    L"%d.%d.%d",
                    HIWORD(pFileInfo->dwFileVersionMS),
                    LOWORD(pFileInfo->dwFileVersionMS),
                    HIWORD(pFileInfo->dwFileVersionLS));

                SetWindowText(hWnd, buf);

                return FALSE;
            }
        }
    }

    return TRUE;
}

static INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            WCHAR buf[MAX_PATH];
            DWORD len = GetModuleFileNameW(appData.hInstance, buf, sizeof(buf) / sizeof(buf[0]));
            if (len)
            {
                DWORD dwHandle, dwSize = GetFileVersionInfoSizeW(buf, &dwHandle);
                if (dwSize)
                {
                    BYTE* pBlock = LocalAlloc(LMEM_ZEROINIT, dwSize);
                    if (pBlock)
                    {
                        if (GetFileVersionInfoW(buf, dwHandle, dwSize, pBlock))
                        {
                            LPVOID pFileInfo = NULL;
                            UINT uLenFileInfo = 0;
                            if (VerQueryValueW(pBlock, L"\\", &pFileInfo, &uLenFileInfo))
                            {
                                EnumChildWindows(hDlg, AboutChildren, (LPARAM)pFileInfo);
                            }
                        }
                        LocalFree(pBlock);
                    }
                }
            }
        }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

static void WinData_EndBusy(struct WINDATA* winData)
{
    if (winData->iBusy)
    {
        if (!InterlockedDecrement(&winData->iBusy))
        {
            HCURSOR hLastCursor = winData->hLastCursor;
            winData->hLastCursor = NULL;
            SetCursor(hLastCursor);
        }
    }
}

static void StartScriptRequest(struct WINDATA* winData, struct CHARBUF* output)
{
    struct APPDATA* appData = winData->app;

    output->msgType = 0x50;

    winData->iBusy++;

    if (winData->hEditContent == GetFocus())
    {
        winData->hLastCursor = SetCursor(hWaitCursor);
    }

    QueueWriteMessage(winData->app, winData, output);
}

static void StartScriptRequestFromText(struct WINDATA* winData, const WCHAR * script)
{
    int len = (int)wcslen(script);
    struct CHARBUF* output = LocalAlloc(LMEM_ZEROINIT, sizeof(*output) + len * sizeof(output->buf[0]));
    if (output)
    {
        output->dwLen = len;
        memcpy(output->buf, script, len * sizeof(output->buf[0]));
        StartScriptRequest(winData, output);
    }
}

static void StartScriptFromEditControl(struct WINDATA* winData)
{
    HWND hWnd = winData->hEditContent;
    DWORD startPos = 0, endPos = 0;
    struct CHARBUF* output = NULL;

    SendMessageW(hWnd, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);

    if (startPos != endPos)
    {
        DWORD selectCount = endPos - startPos;
        DWORD lineCount = (DWORD)SendMessageW(hWnd, EM_GETLINECOUNT, 0, 0);
        DWORD lineStart = (DWORD)SendMessageW(hWnd, EM_LINEFROMCHAR, (WPARAM)startPos, 0);
        DWORD lineEnd = (DWORD)SendMessageW(hWnd, EM_LINEFROMCHAR, (WPARAM)endPos, 0);
        DWORD charOrigin = (DWORD)SendMessageW(hWnd, EM_LINEINDEX, (WPARAM)lineStart, 0);
        DWORD charTotal, charOffset = 0;
        DWORD lineNumber = lineStart;
        struct CHARBUF* buf;
        DWORD lineAfter = lineEnd + 1;
        DWORD lineGet = lineStart;
        WCHAR* p;

        if (lineAfter < lineCount)
        {
            charTotal = (DWORD)SendMessageW(hWnd, EM_LINEINDEX, (WPARAM)lineAfter, 0) - charOrigin;
        }
        else
        {
            charTotal = GetWindowTextLength(hWnd) - charOrigin;
        }

        buf = LocalAlloc(LMEM_ZEROINIT, sizeof(*buf) + sizeof(buf->buf[0]) * charTotal);

        buf->dwLen = charTotal;
        p = buf->buf;

        while (lineGet < lineAfter)
        {
            DWORD lineLen = (DWORD)SendMessageW(hWnd, EM_LINELENGTH, (WPARAM)(charOrigin + charOffset), 0);
            DWORD offsetNext = ((lineGet + 1) < lineCount) ?
                (DWORD)SendMessageW(hWnd, EM_LINEINDEX, lineGet + 1, 0) :
                charOrigin + charTotal;

            if (lineLen)
            {
                DWORD act;
                WORD* pw = (WORD*)p;
                *pw = (WORD)(buf->dwLen - charOffset);
                act = (DWORD)SendMessageW(hWnd, EM_GETLINE, (WPARAM)lineGet, (LPARAM)p);
                p += act;
                charOffset += act;
            }

            while (offsetNext > (charOffset + charOrigin))
            {
                DWORD gap = offsetNext - charOffset - charOrigin;

                if (gap > 1)
                {
                    *p++ = '\n';
                    charOffset++;
                }

                *p++ = '\n';
                charOffset++;
            }

            lineGet++;
        }

        output = LocalAlloc(LMEM_ZEROINIT, sizeof(*output) + (selectCount + 2) * sizeof(output->buf[0]));

        output->dwLen = selectCount;
        memcpy(output->buf, buf->buf + startPos - charOrigin, output->dwLen * sizeof(output->buf[0]));

        LocalFree(buf);

        if (output->buf[output->dwLen - 1] != '\n' && output->buf[output->dwLen - 2] != '\r')
        {
            output->buf[output->dwLen++] = '\r';
            output->buf[output->dwLen++] = '\n';
        }

        SendMessageW(hWnd, EM_SETSEL, endPos, endPos);
        SendMessageW(hWnd, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
        SendMessageW(hWnd, EM_SETSEL, endPos + 2, endPos + 2);
    }
    else
    {
        DWORD lineNumber = (DWORD)SendMessageW(hWnd, EM_LINEFROMCHAR, (WPARAM)startPos, 0);
        DWORD lineLength = (DWORD)SendMessageW(hWnd, EM_LINELENGTH, (WPARAM)startPos, 0);
        DWORD lineIndex = (DWORD)SendMessageW(hWnd, EM_LINEINDEX, (WPARAM)lineNumber, 0);

        if (lineLength)
        {
            struct CHARBUF* buf = LocalAlloc(LMEM_ZEROINIT, sizeof(*buf) + sizeof(buf->buf[0]) * (lineLength + 2));

            if (buf)
            {
                WORD* pw = (LPVOID)(buf->buf);
                *pw = (WORD)(lineLength + 1);
                buf->dwLen = (DWORD)SendMessageW(hWnd, EM_GETLINE, (WPARAM)lineNumber, (LPARAM)(buf->buf));

                if (buf->dwLen)
                {
                    buf->buf[buf->dwLen++] = '\r';
                    buf->buf[buf->dwLen++] = '\n';
                    output = buf;
                }
                else
                {
                    LocalFree(buf);
                    buf = NULL;
                }
            }
        }

        startPos = lineIndex + lineLength;
        endPos = startPos;

        SendMessageW(hWnd, EM_SETSEL, startPos, endPos);
        SendMessageW(hWnd, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
        SendMessageW(hWnd, EM_SETSEL, startPos + 2, endPos + 2);
    }

    if (output)
    {
        StartScriptRequest(winData, output);
    }
}

static INT_PTR CALLBACK GoToDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
        SetFocus(GetDlgItem(hWnd, IDC_LINENUMBER));
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            {
                WCHAR buf[48];
                if (GetDlgItemTextW(hWnd, IDC_LINENUMBER, buf, (sizeof(buf) / sizeof(buf[0])) - 1))
                {
                    int line = _wtoi(buf);
                    int actual = (int)SendMessageW(winData->hEditContent, EM_GETLINECOUNT, 0, 0);
                    if ((line > actual)||(line < 1))
                    {
                        WCHAR title[64];
                        GetWindowText(hWnd, title, sizeof(title) / sizeof(title[0]));
                        MessageBoxW(hWnd, szGoingToFar, title, MB_OK);
                    }
                    else
                    {
                        DWORD offset = (DWORD)SendMessageW(winData->hEditContent, EM_LINEINDEX, (WPARAM)(line - 1), 0);
                        SendMessageW(winData->hEditContent, EM_SETSEL, (WPARAM)offset, (LPARAM)offset);

                        EndDialog(hWnd, line);
                    }
                }
                return TRUE;
            }
            break;
        case IDCANCEL:
            EndDialog(hWnd, 0);
            return TRUE;
        }
    }

    return FALSE;
}

static int WinData_SaveFile(struct WINDATA* winData, BOOL bConfirm, BOOL bSaveAs)
{
    int response = IDYES;

    if (bConfirm)
    {
        WCHAR buf[256];
        LPCWSTR m = szSaveContent;

        if (winData->fileName)
        {
            _snwprintf_s(buf,
                sizeof(buf) / sizeof(buf[0]),
                (sizeof(buf) / sizeof(buf[0])) - 1,
                szSaveFile, winData->fileName);

            m = buf;
        }

        response = MessageBox(winData->hWnd, m, szTitle, MB_YESNOCANCEL);
    }

    switch (response)
    {
    case IDYES:
        if ((winData->fileName == NULL)||bSaveAs)
        {
            OPENFILENAMEW openFileName;
            WCHAR fileName[MAX_PATH] = { 0 };

            if (winData->fileName)
            {
                wcscpy_s(fileName, sizeof(fileName)/sizeof(fileName[0]), winData->fileName);
            }

            openFileName.hwndOwner = winData->hWnd;
            memset(&openFileName, 0, sizeof(openFileName));
            openFileName.lStructSize = sizeof(openFileName);
            openFileName.lpstrFile = fileName;
            openFileName.nMaxFile = sizeof(fileName) / sizeof(fileName[0]);
            openFileName.Flags = OFN_CREATEPROMPT| OFN_DONTADDTORECENT| OFN_NOCHANGEDIR| OFN_NONETWORKBUTTON| OFN_OVERWRITEPROMPT;
            openFileName.lpstrFilter = szSaveFilter;

            if (GetSaveFileNameW(&openFileName))
            {
                size_t fileNameLen = wcslen(fileName);

                if (winData->filePath) LocalFree(winData->filePath);

                winData->filePath = LocalAlloc(LMEM_ZEROINIT, (fileNameLen + 1) * sizeof(winData->filePath[0]));

                if (winData->filePath)
                {
                    memcpy(winData->filePath, fileName, fileNameLen * sizeof(fileName[0]));
                    winData->fileName = winData->filePath + openFileName.nFileOffset;

                    _snwprintf_s(fileName,
                        sizeof(fileName) / sizeof(fileName[0]),
                        (sizeof(fileName) / sizeof(fileName[0])) - 1,
                        szTitleWithFile, winData->fileName);

                    SetWindowText(winData->hWnd, fileName);
                }
                else
                {
                    winData->fileName = NULL;
                    SetWindowText(winData->hWnd, szTitle);
                }
            }
            else
            {
                response = IDCANCEL;
            }
        }

        if (response == IDYES)
        {
            int i = GetWindowTextLength(winData->hEditContent);
            struct CHARBUF* p = i ? LocalAlloc(LMEM_ZEROINIT, sizeof(*p) + i * sizeof(p->buf[0])) : NULL;
            struct CHARBUF* b = NULL;

            if (p)
            {
                p->dwLen = GetWindowTextW(winData->hEditContent, p->buf, i + 1);

                if (p->dwLen == i)
                {
                    int codePage = winData->fileCodePage ? winData->fileCodePage : winData->app->fileCodePage;

                    if (p->dwLen && (p->buf[0] == UNICODEBOM))
                    {
                        if (codePage != CP_UNICODE)
                        {
                            codePage = CP_UTF8;
                        }
                    }

                    if (codePage == CP_UNICODE)
                    {
                        b = p;
                        b->dwLen *= sizeof(b->buf[0]);
                        p = NULL;
                    }
                    else
                    {
                        int k = WideCharToMultiByte(codePage, 0, p->buf, p->dwLen, NULL, 0, NULL, NULL);

                        if (k > 0)
                        {
                            b = LocalAlloc(LMEM_ZEROINIT,sizeof(*b) + k);

                            if (b)
                            {
                                b->dwLen = WideCharToMultiByte(codePage, 0, p->buf, p->dwLen, (LPVOID)b->buf, k, NULL, NULL);
                            }
                        }
                    }
                }
            }

            HANDLE hFile = CreateFileW(winData->filePath,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

                if (hFile == INVALID_HANDLE_VALUE)
                {
                    ShowError(GetLastError());
                    response = IDCANCEL;
                }
                else
                {
                    if (b)
                    {
                        DWORD dwLen = 0;
                        WriteFile(hFile, b->buf, b->dwLen, &dwLen, NULL);
                    }

                    CloseHandle(hFile);
                }

                if (p)
                {
                    LocalFree(p);
                }

                if (b)
                {
                    LocalFree(b);
                }

                SendMessageW(winData->hEditContent, EM_SETMODIFY, 0, 0);
        }

        break;
    }

    return response;
}

static void WinData_OpenFile(struct WINDATA* winData)
{
    OPENFILENAMEW openFileName;
    WCHAR fileName[MAX_PATH] = { 0 };

    if (winData->fileName)
    {
        wcscpy_s(fileName, sizeof(fileName) / sizeof(fileName[0]), winData->fileName);
    }

    openFileName.hwndOwner = winData->hWnd;
    memset(&openFileName, 0, sizeof(openFileName));
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = sizeof(fileName) / sizeof(fileName[0]);
    openFileName.Flags = OFN_DONTADDTORECENT | OFN_NOCHANGEDIR | OFN_NONETWORKBUTTON;
    openFileName.lpstrFilter = szSaveFilter;

    if (GetOpenFileNameW(&openFileName))
    {
        struct WINDATA* p;
        int len = (int)wcslen(fileName);

        EnterCriticalSection(&winData->app->crit);

        p = winData->app->first;

        while (p)
        {
            if (p->filePath)
            {
                int len2 = (int)wcslen(p->filePath);

                if (len == len2)
                {
                    if (CSTR_EQUAL == CompareStringOrdinal(fileName, len, p->filePath, len2, TRUE))
                    {
                        break;
                    }
                }
            }

            p = p->next;
        }

        LeaveCriticalSection(&winData->app->crit);

        if (p && p->hWnd)
        {
            if (p != winData)
            {
                SetForegroundWindow(p->hWnd);
            }
        }
        else
        {
            AppData_OpenWindow(winData->app, fileName, SW_NORMAL, NULL);
        }
    }
}

static void WinData_SetFont(struct WINDATA* winData, HFONT hFont)
{
    int cx = GetSystemMetrics(SM_CXHSCROLL);
    int cy = GetSystemMetrics(SM_CYVSCROLL);

    if (hFont)
    {
        SendMessageW(winData->hEditContent, WM_SETFONT, (WPARAM)hFont, 0);
    }

    SendMessageW(winData->hEditContent, EM_SETMARGINS, EC_LEFTMARGIN, MAKELONG(cx >> 1, cx >> 1));
}

static void WinData_CreateEdit(struct WINDATA* winData, LONG style, struct CHARBUF *content)
{
    RECT r;
    HANDLE hFont = winData->hFont;
    LOGFONTW logFont;

    GetClientRect(winData->hWnd, &r);

    winData->hEditContent = CreateWindowW(
        L"edit", (content == NULL || content->dwLen > 0x4000) ? L"" : content->buf,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | style | ES_NOHIDESEL,
        r.left, r.top, r.right - r.left, r.bottom - r.top,
        winData->hWnd, (HMENU)IDC_CONTENT, (HINSTANCE)GetWindowLongPtr(winData->hWnd, GWLP_HINSTANCE), NULL);

    SendMessageW(winData->hEditContent, EM_SETLIMITTEXT, (WPARAM)dwTextLimit, 0);

    if (content)
    {
        if (content->dwLen > 0x4000)
        {
            SetWindowText(winData->hEditContent, content->buf);
        }

        switch (content->msgType)
        {
        case 0x47:
        case 0x48:
            break;

        default:
            LocalFree(content);
            break;
        }
    }

    if (hFont == NULL)
    {
        if (winData->app->hasLogFont)
        {
            winData->hFont = CreateFontIndirect(&winData->app->logFont);

            if (winData->hFont)
            {
                hFont = winData->hFont;
            }
        }
        else
        {
            hFont = GetStockObject(MPWSHELL_DEFAULT_STOCK_FONT);

            if (hFont)
            {
                if (GetObjectW(hFont, sizeof(logFont), &logFont))
                {
                    if (logFont.lfHeight != NonClientMetrics.lfCaptionFont.lfHeight)
                    {
                        logFont.lfHeight = NonClientMetrics.lfCaptionFont.lfHeight;

                        winData->hFont = CreateFontIndirect(&logFont);

                        if (winData->hFont)
                        {
                            hFont = winData->hFont;
                        }
                    }
                }
            }
        }
    }

    WinData_SetFont(winData, hFont);
}

static DWORD CALLBACK AlertThread(LPVOID pv)
{
    struct WINDATA* winData = pv;
    struct APPDATA* appData = winData->app;

    __try
    {
        while (appData->bRunning)
        {
            EnterCriticalSection(&appData->crit);

            struct CHARBUF* buf = winData->hWnd ? winData->messageQueue : NULL;

            if (buf)
            {
                winData->messageQueue = buf->next;
            }
            else
            {
                HWND hWnd = winData->hWnd;

                if (hWnd)
                {
                    PostMessage(hWnd, WM_USER, (WPARAM)1, (LPARAM)GetCurrentThreadId());
                }
            }

            LeaveCriticalSection(&appData->crit);

            if (!buf) break;

            MessageBoxW(winData->hWnd, buf->buf, szTitle, MB_OK | MB_ICONEXCLAMATION);

            LocalFree(buf);
        }
    }
    __finally
    {
        WinData_Release(winData);
    }

    return 0;
}

static BOOL WinData_Find(struct WINDATA* winData, BOOL bForwards)
{
    DWORD matchLen = (DWORD)wcslen(winData->findWhat);

    if (matchLen)
    {
        DWORD startPos = 0;
        DWORD endPos = 0;
        DWORD lineCount = (DWORD)SendMessageW(winData->hEditContent, EM_GETLINECOUNT, 0, 0);
        SendMessageW(winData->hEditContent, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
        DWORD currentIndex = bForwards ? endPos : startPos;
        WCHAR lineBuffer[0x1000];
        BOOL bCaseInsensitive = winData->findReplace.Flags & FR_MATCHCASE ? FALSE : TRUE;

        while (TRUE)
        {
            DWORD lineLength = (DWORD)SendMessageW(winData->hEditContent, EM_LINELENGTH, currentIndex, 0);
            DWORD lineNumber = (DWORD)SendMessageW(winData->hEditContent, EM_LINEFROMCHAR, currentIndex, 0);
            DWORD lineStart = (DWORD)SendMessageW(winData->hEditContent, EM_LINEINDEX, lineNumber, 0);

            if (lineLength)
            {
                DWORD* dwPtr = (LPVOID)lineBuffer;

                if (lineLength >= (sizeof(lineBuffer) / sizeof(lineBuffer[0])))
                {
                    lineLength = (sizeof(lineBuffer) / sizeof(lineBuffer[0]))-1;
                }

                dwPtr[0] = lineLength;

                DWORD actLen = (DWORD)SendMessageW(winData->hEditContent, EM_GETLINE, lineNumber, (LPARAM)lineBuffer);

                lineBuffer[actLen] = 0;

                DWORD offset = currentIndex - lineStart;

                while (TRUE)
                {
                    if (bForwards)
                    {
                        if ((offset + matchLen) <= actLen)
                        {
                            if (CSTR_EQUAL == CompareStringOrdinal(
                                lineBuffer + offset, matchLen,
                                winData->findWhat, matchLen,
                                bCaseInsensitive))
                            {
                                offset += lineStart;
                                SendMessageW(winData->hEditContent, EM_SETSEL, offset, offset + matchLen);
                                return TRUE;
                            }
                        }

                        offset++;
                        if ((offset + matchLen) > actLen) break;
                    }
                    else
                    {
                        if ((offset >= matchLen)&&(offset <= actLen))
                        {
                            if (CSTR_EQUAL == CompareStringOrdinal(
                                lineBuffer + offset - matchLen, matchLen,
                                winData->findWhat, matchLen,
                                bCaseInsensitive))
                            {
                                offset += lineStart;
                                SendMessageW(winData->hEditContent, EM_SETSEL, offset - matchLen, offset);
                                return TRUE;
                            }
                        }

                        if (!offset) break;
                        offset--;
                    }
                }
            }

            if (bForwards)
            {
                lineNumber++;
                if (lineNumber == lineCount) break;
                currentIndex = (DWORD)SendMessageW(winData->hEditContent, EM_LINEINDEX, lineNumber, 0);
            }
            else
            {
                if (!lineNumber) break;
                lineNumber--;
                currentIndex = (DWORD)SendMessageW(winData->hEditContent, EM_LINEINDEX, lineNumber, 0);
                lineLength = (DWORD)SendMessageW(winData->hEditContent, EM_LINELENGTH, currentIndex, 0);
                currentIndex += lineLength;
            }
        }
    }

    return FALSE;
}

static void WinData_Obituary(struct WINDATA* winData)
{
    BOOL bObituary = TRUE;

    if (SendMessageW(winData->hEditContent, EM_GETMODIFY, 0, 0))
    {
        int i = WinData_SaveFile(winData, TRUE, FALSE);

        if (i == IDCANCEL)
        {
            bObituary = FALSE;
        }
    }

    if (bObituary && winData->hWnd)
    {
        DestroyWindow(winData->hWnd);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SETCURSOR:
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);

            if (winData->iBusy)
            {
                SetCursor(hWaitCursor);
                return TRUE;
            }

            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_COMMAND:
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            int wmId = LOWORD(wParam);

            switch (wmId)
            {
            case IDM_CANCEL:
                if (winData->state == MPW_PAIRED)
                {
                    struct CHARBUF* buf = LocalAlloc(LMEM_ZEROINIT, sizeof(buf[0]));
                    if (buf)
                    {
                        buf->msgType = 0x54;
                        QueueWriteMessage(winData->app, winData, buf);
                    }
                }
                break;

            case IDM_ABOUT:
                DialogBoxW(winData->app->hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;

            case IDM_FILE_OPEN:
                WinData_OpenFile(winData);
                break;

            case IDM_FILE_SAVE:
                WinData_SaveFile(winData, FALSE, FALSE);
                break;

            case IDM_FILE_SAVEAS:
                WinData_SaveFile(winData, FALSE, TRUE);
                break;

            case IDM_EXIT:
                StartScriptRequestFromText(winData, L"Exit");
                break;

            case IDM_EDIT_TIME:
                StartScriptRequestFromText(winData, L"Get-Date");
                break;

            case IDM_EXECUTE_SCRIPT:
                StartScriptFromEditControl(winData);
                break;

            case IDM_EDIT_GOTO:
                {
                    DialogBoxParamW(winData->app->hInstance, MAKEINTRESOURCE(IDD_GOTO), hWnd, GoToDlgProc, (LPARAM)winData);
                }
                break;

            case IDM_FILE_NEW:
                {
                    struct APPDATA* appData = winData->app;
                    struct WINDATA* newData = LocalAlloc(LMEM_ZEROINIT, sizeof(*newData));

                    if (newData)
                    {
                        newData->clientId = InterlockedIncrement(&appData->clientSequence);
                        newData->lUsage = 1;
                        newData->iBusy = 1;
                        newData->hLastCursor = SetCursor(hWaitCursor);

                        EnterCriticalSection(&appData->crit);

                        newData->app = appData;
                        newData->next = appData->first;
                        appData->first = newData;

                        LeaveCriticalSection(&appData->crit);

                        HWND hOther = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, appData->hInstance, newData);

                        if (hOther)
                        {
                            ShowWindow(hOther, SW_NORMAL);
                        }
                        else
                        {
                            WinData_EndBusy(newData);
                            WinData_Release(newData);
                        }
                    }
                }
                break;

            case IDM_EDIT_UNDO:
                if (winData->hEditContent)
                {
                    SendMessageW(winData->hEditContent, EM_UNDO, 0, 0);
                }
                break;

            case IDM_EDIT_CUT:
                if (winData->hEditContent)
                {
                    SendMessageW(winData->hEditContent, WM_CUT, 0, 0);
                }
                break;

            case IDM_EDIT_COPY:
                if (winData->hEditContent)
                {
                    SendMessageW(winData->hEditContent, WM_COPY, 0, 0);
                }
                break;

            case IDM_EDIT_PASTE:
                if (winData->hEditContent)
                {
                    SendMessageW(winData->hEditContent, WM_PASTE, 0, 0);
                }
                break;

            case IDM_EDIT_DELETE:
                if (winData->hEditContent)
                {
                    SendMessageW(winData->hEditContent, WM_CLEAR, 0, 0);
                }
                break;

            case IDM_EDIT_SELECTALL:
                if (winData->hEditContent)
                {
                    DWORD len = GetWindowTextLength(winData->hEditContent);
                    SendMessageW(winData->hEditContent, EM_SETSEL, 0, len);
                }
                break;

            case IDM_FORMAT_FONT:
                if (winData->hEditContent)
                {
                    HFONT hFont = winData->hFont ? winData->hFont : GetStockObject(MPWSHELL_DEFAULT_STOCK_FONT);
                    LOGFONTW logFont;
                    CHOOSEFONTW chooser;
                    struct APPDATA* appData = winData->app;

                    if (appData->hasLogFont)
                    {
                        logFont = appData->logFont;
                    }
                    else
                    {
                        GetObjectW(hFont, sizeof(logFont), &logFont);
                    }

                    memset(&chooser, 0, sizeof(chooser));

                    chooser.lStructSize = sizeof(chooser);
                    chooser.hwndOwner = hWnd;
                    chooser.hInstance = winData->app->hInstance;
                    chooser.lpLogFont = &logFont;
                    chooser.Flags = CF_INITTOLOGFONTSTRUCT | CF_FIXEDPITCHONLY;

                    if (ChooseFontW(&chooser))
                    {
                        hFont = CreateFontIndirectW(&logFont);

                        if (hFont)
                        {
                            WinData_SetFont(winData, hFont);

                            if (winData->hFont)
                            {
                                DeleteObject(winData->hFont);
                            }

                            if (winData->hFontScale)
                            {
                                DeleteObject(winData->hFontScale);
                                winData->hFontScale = NULL;
                            }

                            winData->perCentScale = 100;

                            winData->hFont = hFont;

                            appData->logFont = logFont;

                            appData->hasLogFont = TRUE;

                            HKEY key;
                            LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, registryKey, 0, KEY_SET_VALUE, &key);

                            if (!status)
                            {
                                status = RegSetValueEx(key, registryLogFont, 0, REG_BINARY, (LPVOID)&logFont, sizeof(logFont));

                                RegCloseKey(key);
                            }
                        }
                    }
                }
                break;

            case IDM_FORMAT_WORDWRAP:
                if (winData->hEditContent)
                {
                    DWORD startPos = 0, endPos = 0;
                    SendMessageW(winData->hEditContent, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
                    DWORD oldStyle = GetWindowLong(winData->hEditContent, GWL_STYLE);
                    DWORD len = GetWindowTextLength(winData->hEditContent);
                    struct CHARBUF* content = len ? LocalAlloc(LMEM_ZEROINIT,sizeof(*content)+len*sizeof(content->buf[0])) : NULL;
                    BOOL focus = GetFocus() == winData->hEditContent;
                    DWORD dirty = (DWORD)SendMessageW(winData->hEditContent, EM_GETMODIFY, 0, 0);

                    if (len)
                    {
                        content->dwLen = GetWindowTextW(winData->hEditContent, content->buf, len + 1);
                    }

                    winData->wordWrap = !winData->wordWrap;

                    DWORD newStyle =
                        (winData->wordWrap ? 0 : WS_HSCROLL) | (oldStyle & ES_READONLY) |
                        (IsWindowEnabled(winData->hEditContent) ? 0 : WS_DISABLED);

                    DestroyWindow(winData->hEditContent);

                    WinData_CreateEdit(winData, newStyle, content);

                    SendMessageW(winData->hEditContent, EM_SETSEL, (WPARAM)startPos, (LPARAM)endPos);

                    SendMessageW(winData->hEditContent, EM_SCROLLCARET, 0, 0);

                    if (dirty)
                    {
                        SendMessageW(winData->hEditContent, EM_SETMODIFY, TRUE, 0);
                    }

                    if (focus)
                    {
                        SetFocus(winData->hEditContent);
                    }
                }
                break;

            case IDM_ZOOM_RESTOREDEFAULT:
            case IDM_ZOOM_ZOOMIN:
            case IDM_ZOOM_ZOOMOUT:
                switch (LOWORD(wParam))
                {
                case IDM_ZOOM_RESTOREDEFAULT:
                    winData->perCentScale = 100;
                    break;
                case IDM_ZOOM_ZOOMIN:
                    winData->perCentScale = (int)((winData->perCentScale * 1.5)+0.5);
                    break;
                case IDM_ZOOM_ZOOMOUT:
                    winData->perCentScale = (int)((winData->perCentScale / 1.5) + 0.5);
                    break;
                }

                if (winData->hFont)
                {
                    LOGFONTW logFont;

                    if (GetObjectW(winData->hFont, sizeof(logFont), &logFont))
                    {
                        HFONT oldFont = winData->hFontScale, hFontScale = NULL;

                        if (winData->perCentScale != 100)
                        {
                            logFont.lfHeight = ((logFont.lfHeight * winData->perCentScale) + 50) / 100;

                            hFontScale = CreateFontIndirectW(&logFont);
                        }

                        WinData_SetFont(winData, hFontScale ? hFontScale : winData->hFont);

                        if (oldFont)
                        {
                            DeleteObject(oldFont);
                        }

                        winData->hFontScale = hFontScale;
                    }
                }
                break;

            case IDM_HELP_VIEWHELP:
                {
                    WCHAR path[MAX_PATH];
                    int i = GetModuleFileName(winData->app->hInstance, path, sizeof(path) / sizeof(path[0]));

                    while (i--)
                    {
                        if (path[i] == '.')
                        {
                            StringCchCopyW(path + i + 1, (sizeof(path) / sizeof(path[0])) - (i + 2), L"chm");
                            break;
                        }
                    }

                    HtmlHelpW(NULL,path, HH_DISPLAY_TOC, 0);
                }
                break;

            case IDM_HELP_SENDFEEDBACK:
                if (szSendFeedbackURL)
                {
                    SHELLEXECUTEINFO sei = { sizeof(sei) };
                    sei.lpVerb = L"open";
                    sei.lpFile = szSendFeedbackURL;
                    sei.nShow = SW_SHOWNORMAL;

                    ShellExecuteEx(&sei);
                }
                break;

            case IDM_EDIT_FIND:
                if (!winData->iFindReplace)
                {
                    winData->iFindReplace = 1;
                    winData->findReplace.lStructSize = sizeof(winData->findReplace);
                    winData->findReplace.hwndOwner = winData->hWnd;
                    winData->findReplace.lCustData = (LPARAM)winData;
                    winData->findReplace.Flags = FR_DOWN | FR_NOWHOLEWORD;
                    winData->findReplace.wFindWhatLen = sizeof(winData->findWhat);
                    winData->findReplace.lpstrFindWhat = winData->findWhat;
                    winData->hwndFindReplace = FindTextW(&winData->findReplace);

                    if (!winData->hwndFindReplace)
                    {
                        winData->iFindReplace = 0;
                    }
                }
                break;

            case IDM_EDIT_REPLACE:
                if (!winData->iFindReplace)
                {
                    winData->iFindReplace = 2;
                    winData->findReplace.lStructSize = sizeof(winData->findReplace);
                    winData->findReplace.hwndOwner = winData->hWnd;
                    winData->findReplace.lCustData = (LPARAM)winData;
                    winData->findReplace.Flags = FR_DOWN | FR_NOWHOLEWORD;
                    winData->findReplace.wFindWhatLen = sizeof(winData->findWhat);
                    winData->findReplace.lpstrFindWhat = winData->findWhat;
                    winData->findReplace.wReplaceWithLen = sizeof(winData->replaceWith);
                    winData->findReplace.lpstrReplaceWith = winData->replaceWith;
                    winData->hwndFindReplace = ReplaceTextW(&winData->findReplace);

                    if (!winData->hwndFindReplace)
                    {
                        winData->iFindReplace = 0;
                    }
                }
                break;

            case IDM_EDIT_FINDNEXT:
                WinData_Find(winData, TRUE);
                break;

            case IDM_EDIT_FINDPREVIOUS:
                WinData_Find(winData, FALSE);
                break;

            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            struct APPDATA* appData = winData->app;
            HWND hEditContent = winData->hEditContent;
            BOOL bQuit;

            winData->hEditContent = NULL;
            winData->hWnd = NULL;

            EnterCriticalSection(&appData->crit);

            if (appData->first == winData)
            {
                appData->first = winData->next;
            }
            else
            {
                struct WINDATA* p = appData->first;

                while (p->next != winData)
                {
                    p = p->next;
                }

                p->next = winData->next;
            }

            winData->app = NULL;

            bQuit = !appData->first;

            LeaveCriticalSection(&appData->crit);

            if (bQuit)
            {
                PostQuitMessage(0);
            }
            else
            {
                if (winData->state != MPW_DEAD)
                {
                    struct CHARBUF* cb = LocalAlloc(LMEM_ZEROINIT, sizeof(*cb));

                    if (cb)
                    {
                        switch (winData->state)
                        {
                        case MPW_PAIRING:
                            cb->msgType = 0x53;
                            break;
                        case MPW_PAIRED:
                            cb->msgType = 0x52;
                            break;
                        }

                        QueueWriteMessage(appData, winData, cb);
                    }
                }
            }

            if (hEditContent)
            {
                DestroyWindow(hEditContent);

                if (winData->hFont)
                {
                    DeleteObject(winData->hFont);
                    winData->hFont = NULL;
                }

                if (winData->hFontScale)
                {
                    DeleteObject(winData->hFontScale);
                    winData->hFontScale = NULL;
                }
            }

            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);

            WinData_Release(winData);
        }
        break;

    case WM_CREATE:
        {
            LPCREATESTRUCT lpCreate = (LPCREATESTRUCT)lParam;
            struct WINDATA* winData = (void *)(lpCreate->lpCreateParams);
            struct CHARBUF* content = winData->readQueue;
            LONG style = WS_HSCROLL | (winData->state == MPW_PAIRED ? 0 : WS_DISABLED);
            if (content) winData->readQueue = content->next;
            winData->perCentScale = 100;

            winData->hWnd = hWnd;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)winData);

            WinData_CreateEdit(winData, style , content);

            if (winData->state == MPW_DEAD)
            {
                struct CHARBUF* cb = LocalAlloc(LMEM_ZEROINIT, sizeof(*cb) + dwInitialScript * sizeof(cb->buf[0]));

                if (cb)
                {
                    winData->state = MPW_PAIRING;
                    cb->msgType = 0x51;
                    cb->clientId = winData->clientId;
                    cb->dwLen = dwInitialScript;

                    if (dwInitialScript)
                    {
                        winData->iBusy++;
                        memcpy(cb->buf, szInitialScript, dwInitialScript * sizeof(cb->buf[0]));
                    }

                    QueueWriteMessage(winData->app, winData, cb);
                }
                else
                {
                    WinData_EndBusy(winData);
                }
            }
        }
        break;
    case WM_SIZE:
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd,GWLP_USERDATA);

            if (winData->hEditContent)
            {
                MoveWindow(winData->hEditContent, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
            }
        }
        break;

    case WM_SETFOCUS:
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);

            if (winData->hEditContent)
            {
                SetFocus(winData->hEditContent);
            }
        }
        break;

    case WM_INITMENUPOPUP:
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            HMENU hMenu = (HMENU)wParam;

            if (!HIWORD(lParam))
            {
                switch (LOWORD(lParam))
                {
                case 0: // FILE
                    if (winData->hEditContent)
                    {
                        BOOL bModified = (BOOL)SendMessageW(winData->hEditContent, EM_GETMODIFY, 0, 0);
                        EnableMenuItem(hMenu, IDM_FILE_NEW, winData->state != MPW_DEAD ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EXIT, winData->state == MPW_PAIRED ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_FILE_SAVE, (winData->fileName != NULL && bModified) ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_FILE_SAVEAS, MF_ENABLED);
                        EnableMenuItem(hMenu, IDM_FILE_OPEN, winData->app->tidReadThread ? MF_ENABLED : MF_DISABLED);
                    }
                    break;
                case 1: // EDIT
                    if (winData->hEditContent)
                    {
                        DWORD startPos = 0, endPos = 0;
                        int len = GetWindowTextLength(winData->hEditContent);
                        BOOL bReadOnly = (ES_READONLY & GetWindowLong(winData->hEditContent, GWL_STYLE)) ? TRUE : FALSE;
                        BOOL bEnabled = IsWindowEnabled(winData->hEditContent);
                        BOOL bCanUndo = bEnabled && (!bReadOnly) && (BOOL)SendMessageW(winData->hEditContent, EM_CANUNDO, 0, 0);
                        BOOL bSelected = FALSE, bPaste = bEnabled && (!bReadOnly) && IsClipboardFormatAvailable(CF_TEXT);
                        DWORD nLines = (bEnabled && !bReadOnly) ? (DWORD)SendMessageW(winData->hEditContent, EM_GETLINECOUNT, 0, 0) : 0;

                        if (len && bEnabled)
                        {
                            SendMessageW(winData->hEditContent, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
                            bSelected = startPos != endPos;
                        }

                        EnableMenuItem(hMenu, IDM_EDIT_UNDO, bCanUndo ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_CUT, (bSelected && !bReadOnly) ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_COPY, bSelected ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_PASTE, bPaste ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_DELETE, (bSelected && !bReadOnly) ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_SELECTALL, (len && bEnabled) ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_GOTO, (nLines > 1) ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_TIME, winData->state == MPW_PAIRED ? MF_ENABLED : MF_DISABLED);

                        BOOL bFindReplace = winData->iFindReplace == 0 && len && bEnabled;

                        EnableMenuItem(hMenu, IDM_EDIT_FIND, bFindReplace ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_FINDNEXT, bFindReplace && winData->findWhat[0] ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_FINDPREVIOUS, bFindReplace && winData->findWhat[0] ? MF_ENABLED : MF_DISABLED);
                        EnableMenuItem(hMenu, IDM_EDIT_REPLACE, (bFindReplace && !bReadOnly) ? MF_ENABLED : MF_DISABLED);
                    }
                    break;
                case 2: // FORMAT
                    if (winData->hEditContent)
                    {
                        DWORD style = GetWindowLong(winData->hEditContent, GWL_STYLE);
                        CheckMenuItem(hMenu, IDM_FORMAT_WORDWRAP, winData->wordWrap ? MF_CHECKED : MF_UNCHECKED);
                    }
                    break;
                case 3: // VIEW
                    if (winData->hEditContent)
                    {
                        EnableMenuItem(hMenu, IDM_ZOOM_RESTOREDEFAULT, winData->hFontScale ? MF_ENABLED : MF_DISABLED);
                    }
                    break;
                case 4: // HELP
                    break;
                default:
                    MessageBox(hWnd, L"Unexpected menu", szTitle, MB_OK);
                    break;
                }
            }
        }
        break;

    case WM_USER:
        switch (wParam)
        {
        case 0:
            {
                struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
                struct APPDATA* appData = winData->app;
                BOOL bObituary = FALSE;
                BOOL bScrollCaret = FALSE;

                EnterCriticalSection(&appData->crit);

                while (winData->readQueue)
                {
                    struct CHARBUF* buf = winData->readQueue;
                    winData->readQueue = buf->next;
                    buf->next = NULL;

                    if (winData->hEditContent)
                    {
                        switch (buf->msgType)
                        {
                        case 0x41:
                            if (winData->state == MPW_PAIRING)
                            {
                                winData->state = MPW_PAIRED;
                                winData->runspaceId = buf->runspaceId;

                                WinData_EndBusy(winData);

                                if (ES_READONLY & GetWindowLong(winData->hEditContent, GWL_STYLE))
                                {
                                    SendMessageW(winData->hEditContent, EM_SETREADONLY, FALSE, 0);
                                }

                                EnableWindow(winData->hEditContent, TRUE);

                                if (hWnd == GetFocus())
                                {
                                    SetFocus(winData->hEditContent);
                                }
                            }
                            break;

                        case 0x42:
                            winData->state = MPW_DEAD;
                            bObituary = TRUE;
                            SendMessageW(winData->hEditContent, EM_SETREADONLY, TRUE, 0);
                            EnableWindow(winData->hEditContent, TRUE);
                            break;

                        case 0x45:
                            WinData_EndBusy(winData);
                            break;

                        case 0x60:
                            winData->state = MPW_DEAD;
                            winData->runspaceId = 0;

                            if (winData->iBusy)
                            {
                                winData->iBusy = 0;
                                SetCursor(NULL);
                            }

                            if (winData->hEditContent)
                            {
                                SendMessageW(winData->hEditContent, EM_SETREADONLY, (WPARAM)TRUE, 0);

                                if (!IsWindowEnabled(winData->hEditContent))
                                {
                                    EnableWindow(winData->hEditContent, TRUE);
                                }
                            }

                            break;

                        case 0x47:
                        case 0x48:
                            if (!AppData_OpenWindow(winData->app, buf->msgType == 0x48 ? buf->buf : NULL, SW_NORMAL, buf))
                            {
                                struct CHARBUF* msg = LocalAlloc(LMEM_ZEROINIT, sizeof(*msg));

                                if (msg)
                                {
                                    msg->msgType = 0x52;
                                    msg->runspaceId = buf->runspaceId;
                                    msg->clientId = buf->clientId;
                                    QueueWriteMessage(winData->app, NULL, msg);
                                }
                            }
                            break;

                        case 0x49:
                            if (winData->messageQueue)
                            {
                                struct CHARBUF* p = winData->messageQueue;
                                while (p->next) p = p->next;
                                p->next = buf;
                            }
                            else
                            {
                                winData->messageQueue = buf;
                            }

                            buf = NULL;

                            if (!winData->tidAlertBox)
                            {
                                InterlockedIncrement(&winData->lUsage);
                                winData->hThreadAlertBox = CreateThread(NULL, 0, AlertThread, winData, 0, &winData->tidAlertBox);
                            }
                            break;

                        default:
                            {
                                DWORD startPos = 0, endPos = 0;

                                SendMessageW(winData->hEditContent, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);

                                if (startPos != endPos)
                                {
                                    startPos = endPos;
                                    SendMessageW(winData->hEditContent, EM_SETSEL, startPos, endPos);
                                }

                                SendMessageW(winData->hEditContent, EM_REPLACESEL, FALSE, (LPARAM)buf->buf);

                                bScrollCaret = TRUE;
                            }
                        }
                    }

                    if (buf)
                    {
                        LocalFree(buf);
                    }
                }

                LeaveCriticalSection(&appData->crit);

                if (bScrollCaret && winData->hEditContent)
                {
                    SendMessageW(winData->hEditContent, EM_SCROLLCARET, 0, 0);
                }

                if (bObituary)
                {
                    if (winData->tidAlertBox)
                    {
                        winData->obituaryReceived++;
                    }
                    else
                    {
                        WinData_Obituary(winData);
                    }
                }
            }
            break;
        case 1:
            {
                struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
                struct APPDATA* appData = winData->app;

                if (lParam == winData->tidAlertBox)
                {
                    HANDLE h = winData->hThreadAlertBox;
                    winData->hThreadAlertBox = NULL;
                    winData->tidAlertBox = 0;
                    WaitForSingleObject(h, INFINITE);
                    CloseHandle(h);
                }

                if (appData->bRunning)
                {
                    EnterCriticalSection(&appData->crit);

                    if (winData->messageQueue && !winData->tidAlertBox)
                    {
                        InterlockedIncrement(&winData->lUsage);
                        winData->hThreadAlertBox = CreateThread(NULL, 0, AlertThread, winData, 0, &winData->tidAlertBox);
                    }

                    LeaveCriticalSection(&appData->crit);
                }

                if (winData->obituaryReceived && !winData->tidAlertBox)
                {
                    winData->obituaryReceived--;

                    WinData_Obituary(winData);
                }
            }
            break;
        }
        break;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE)
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            if (SendMessageW(winData->hEditContent, EM_GETMODIFY, 0, 0))
            {
                int i = WinData_SaveFile(winData, TRUE, FALSE);

                if (i == IDCANCEL)
                {
                    return 0;
                }
            }
        }

        return DefWindowProc(hWnd, message, wParam, lParam);

    default:
        if (message == WM_FINDMSG)
        {
            struct WINDATA* winData = (LPVOID)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            FINDREPLACE* findReplace = (LPVOID)lParam;

            if (findReplace == &winData->findReplace)
            {
                BOOL bScrollCaret = FALSE;

                switch (findReplace->Flags & (FR_DIALOGTERM | FR_FINDNEXT | FR_REPLACE | FR_REPLACEALL))
                {
                case FR_DIALOGTERM:
                    winData->iFindReplace = 0;
                    winData->hwndFindReplace = NULL;
                    break;

                case FR_FINDNEXT:
                    bScrollCaret = WinData_Find(winData, findReplace->Flags & FR_DOWN ? TRUE : FALSE);
                    break;

                case FR_REPLACE:
                case FR_REPLACEALL:
                    while (TRUE)
                    {
                        if (WinData_Find(winData, findReplace->Flags & FR_DOWN ? TRUE : FALSE))
                        {
                            DWORD replaceLen = (DWORD)wcslen(winData->replaceWith);
                            DWORD startPos = 0, endPos = 0;
                            SendMessageW(winData->hEditContent, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
                            SendMessageW(winData->hEditContent, EM_REPLACESEL, TRUE, (LPARAM)winData->replaceWith);
                            SendMessageW(winData->hEditContent, EM_SETSEL, startPos, startPos + replaceLen);
                            bScrollCaret = TRUE;
                        }
                        else
                        {
                            break;
                        }

                        if (findReplace->Flags & FR_REPLACE) break;
                    }
                    break;

                default:
                    break;
                }

                if (bScrollCaret)
                {
                    SendMessageW(winData->hEditContent, EM_SCROLLCARET, 0, 0);
                }
            }

            return 0;
        }

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

static ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    WM_FINDMSG = RegisterWindowMessageW(FINDMSGSTRING);

    wcex.cbSize = sizeof(WNDCLASSEX);

    hArrowCursor = LoadCursor(NULL, IDC_ARROW);
    hWaitCursor = LoadCursor(NULL, IDC_WAIT);
    hCaretCursor = LoadCursor(NULL, IDC_IBEAM);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MPWSHELL));
    wcex.hCursor = hArrowCursor;
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MPWSHELL);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_MPWSHELL));

    return RegisterClassExW(&wcex);
}

static LPCWSTR LoadResourceString(int n)
{
    LPWSTR str[4] = { 0,0,0,0 };
    int x = LoadStringW(appData.hInstance, n, (void *)str, 0);
    if (x > 0)
    {
        LPWSTR p = LocalAlloc(LMEM_ZEROINIT, (x + 1) * sizeof(p[0]));
        memcpy(p, str[0], x * sizeof(p[0]));
        return p;
    }
    return NULL;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    int argc = 0;
    int exitCode = 0;
    LPWSTR* argv = NULL;

    appData.hInstance = hInstance;
    appData.fileCodePage = GetACP();
    appData.clientSequence = 1 + (GetTickCount() & 0xFFFFFF);

    if (!appData.fileCodePage)
    {
        appData.fileCodePage = CP_UTF8;
    }

    UNREFERENCED_PARAMETER(hPrevInstance);

    szTitle = LoadResourceString(IDS_APP_TITLE);
    szWindowClass = LoadResourceString(IDC_MPWSHELL);
    szGoingToFar = LoadResourceString(IDS_GOTO_RANGE_ERROR);
    szCannotFindFile = LoadResourceString(IDS_CANNOT_FIND_FILE);
    szTitleWithFile = LoadResourceString(IDS_TITLE_WITH_FILE);
    szSaveFile = LoadResourceString(IDS_SAVEFILE);
    szSaveContent = LoadResourceString(IDS_SAVECONTENT);
    szSaveFilter = LoadResourceString(IDS_SAVEFILTER);
    szSendFeedbackURL = LoadResourceString(IDS_SENDFEEDBACKURL);

    MyRegisterClass(hInstance);

    if (lpCmdLine && lpCmdLine[0])
    {
        argv = CommandLineToArgvW(lpCmdLine, &argc);

        if (!argv)
        {
            DWORD dwLastError = GetLastError();

            ShowError(dwLastError);

            return dwLastError;
        }

        if (argc)
        {
            int i = 0;
            int j = 0;

            while (i < argc)
            {
                LPWSTR p = argv[j++];

                if (CSTR_EQUAL == CompareStringOrdinal(p, -1, L"-WorkingDirectory", -1, TRUE))
                {
                    argc--;

                    if (argc)
                    {
                        p = argv[j++];

                        WCHAR buf[MAX_PATH];

                        if (!ExpandEnvironmentStringsW(p, buf, sizeof(buf)/sizeof(buf[0])))
                        {
                            DWORD dwLastError = GetLastError();

                            ShowError(dwLastError);

                            return 1;
                        }

                        argc--;

                        if (!SetCurrentDirectoryW(buf))
                        {
                            DWORD dwLastError = GetLastError();

                            ShowError(dwLastError);

                            return 1;
                        }
                    }
                    else
                    {
                        ShowError(ERROR_CURRENT_DIRECTORY);

                        return 1;
                    }
                }
                else
                {
                    argv[i++] = p;
                }
            }
        }
    }

    // Perform application initialization:
    if (!InitInstance(hInstance))
    {
        DWORD dwLastError = GetLastError();

        ShowError(dwLastError);

        return dwLastError;
    }

    if (argc)
    {
        int k = 0;

        while (k < argc)
        {
            AppData_OpenWindow(&appData, argv[k++], nCmdShow, NULL);
        }
    }
    else
    {
        AppData_OpenWindow(&appData, NULL, nCmdShow, NULL);
    }

    if (appData.first)
    {
        HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MPWSHELL));

        appData.hReadThread = CreateThread(NULL, 0, ReadThread, &appData, 0, &appData.tidReadThread);

        MSG msg;

        // Main message loop:
        while (GetMessage(&msg, NULL, 0, 0))
        {
            HWND hwndMain = msg.hwnd;
            BOOL bHandled = FALSE;

            while (hwndMain && (WS_CHILD & GetWindowLong(hwndMain, GWL_STYLE)))
            {
                hwndMain = GetParent(hwndMain);
            }

            if (hwndMain)
            {
                struct WINDATA* winData = appData.first;

                while (winData && winData->hWnd != hwndMain && winData->hwndFindReplace != hwndMain)
                {
                    winData = winData->next;
                }

                if (winData)
                {
                    if (hwndMain == winData->hWnd)
                    {
                        bHandled = TranslateAccelerator(hwndMain, hAccelTable, &msg);
                    }
                    else
                    {
                        if (hwndMain == winData->hwndFindReplace)
                        {
                            bHandled = IsDialogMessageW(hwndMain, &msg);
                        }
                    }
                }
            }
            
            if (!bHandled)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

        }

        exitCode = (int)msg.wParam;
    }

    EnterCriticalSection(&appData.crit);
    appData.bRunning = 0;
    SetEvent(appData.hWriteEvent);
    LeaveCriticalSection(&appData.crit);

    if (appData.hWriteThread)
    {
        WaitForSingleObject(appData.hWriteThread, INFINITE);
    }
    else
    {
        CloseHandle(appData.hPipeWrite);
    }

    if (appData.hReadThread)
    {
        WaitForSingleObject(appData.hReadThread, INFINITE);
    }
    else
    {
        CloseHandle(appData.hPipeRead);
    }

    if (appData.processInfo.hProcess)
    {
        DWORD procExitCode = 0;

        WaitForSingleObject(appData.processInfo.hProcess, INFINITE);

        if (!GetExitCodeProcess(appData.processInfo.hProcess, &procExitCode))
        {
            exitCode = (int)procExitCode;
        }
    }

    return exitCode;
}
