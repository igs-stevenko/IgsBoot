#include <windows.h>
#include <stdio.h>

BOOL EnableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // 取得當前程序的token
    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    // 取得 shutdown privilege 的 LUID
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 啟用 privilege
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

    return (GetLastError() == ERROR_SUCCESS);
}

int Reboot() {

    if (!EnableShutdownPrivilege()) {
        printf("Enable privilege failed\n");
        return -1;
    }

    // 重新開機
    if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0)) {
        printf("Reboot failed, error=%lu\n", GetLastError());
        return -2;
    }

    return 0;
}