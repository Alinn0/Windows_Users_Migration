// Ensure Unicode is used
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include"stdio.h"
#include"windows.h"
#include <shellapi.h>
#include <string>
#include <iostream>

bool IsRunAsAdmin();
bool RestartAsAdmin();
bool EnsureAdminPrivileges();
bool ExecuteCommand(const std::wstring& command, bool waitForCompletion = true, bool showWindow = false);
void ModifyMainRegistryKeys();
void ModifyProfileImagePaths();

int main(){
    // Request administrator privileges
     if (!EnsureAdminPrivileges()) {
        // Failed to elevate privileges, handle error or exit
        return 1;
    }
    int index=0;
    printf("Welcome to the Users Folder Migration Tool\r\n");
    printf("***********************************************You must select step 1 first, then step 2******************************************************\r\n");
    printf("This tool's functions:\r\n1. Copy Users folder to D drive\r\n2. Modify registry paths to point system user paths to D drive\r\n3. Create a symbolic link from C:Users to D:Users\r\n");
    printf("Registry paths modified by this tool:\r\nHKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\\r\n");
    printf("This tool will modify the following entries under this path:\r\n1. Key: Default, data modified to: D:\\Users\\Default\r\n2. Key: ProfilesDirectory, data modified to: D:\\Users\r\n3. Key: Public, data modified to: D:\\Users\\Public\r\n");
    printf("Modify the ProfileImagePath data field in subdirectories starting with S-1-5-21 to change the drive letter to D drive\r\n"); 
    printf("Please select function:\r\n1. Copy Users folder to D drive and modify registry\r\n2. Delete old Users folder and create symbolic link\r\n");
    scanf("%d",&index);
    if(index == 1) {
        ExecuteCommand(L"robocopy \"C:\\Users\" \"D:\\Users\" /E /COPYALL /XJ", 1, 0);
        ModifyMainRegistryKeys();
        ModifyProfileImagePaths();
        ExecuteCommand(L"logoff", 1, 0);
    }
    if(index == 2) {
        system("rmdir /S /Q \"C:\\Users\"");
        system("mklink /D \"C:\\Users\" \"D:\\Users\"");
        printf("Operation completed, press any key to exit...");
        system("pause");
    }
    
    return 0;
}

bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroupSid = NULL;
    
    // Create SID for the Administrators group
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroupSid)) {
        
        // Check if the token contains the Administrators SID
        if (!CheckTokenMembership(NULL, adminGroupSid, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(adminGroupSid);
    }
    
    return isAdmin == TRUE;
}

// Restart the current program with administrator privileges
bool RestartAsAdmin() {
    wchar_t szPath[MAX_PATH];
    
    // Get the path of the current executable
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH) == 0) {
        return false;
    }
    
    // Build command line parameters, add "/restart" flag to prevent infinite loop
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    std::wstring parameters = L"";
    for (int i = 1; i < argc; i++) {
        // Avoid adding restart flag repeatedly
        if (wcscmp(argv[i], L"/restart") != 0) {
            parameters += L" \"";
            parameters += argv[i];
            parameters += L"\"";
        }
    }
    // Add restart flag to indicate this is a restarted instance
    parameters += L" /restart";
    
    LocalFree(argv);
    
    // Use ShellExecute to start the program with administrator privileges
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";  // Request administrator privileges
    sei.lpFile = szPath;
    sei.lpParameters = parameters.c_str();
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;
    
    if (ShellExecuteExW(&sei)) {
        return true;
    }
    
    DWORD err = GetLastError();
    if (err == ERROR_CANCELLED) {
        // User cancelled the UAC prompt
        MessageBoxW(NULL, L"The program requires administrator privileges to run properly.", L"Insufficient Privileges", MB_ICONWARNING | MB_OK);
    }
    
    return false;
}

bool EnsureAdminPrivileges() {
    // If already restarted (contains /restart parameter), skip check
    bool hasRestartFlag = false;
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"/restart") == 0) {
            hasRestartFlag = true;
            break;
        }
    }
    LocalFree(argv);
    
    // Check privileges
    if (!hasRestartFlag && !IsRunAsAdmin()) {
        // Try to restart with administrator privileges
        if (RestartAsAdmin()) {
            // Restart successful, exit current process
            ExitProcess(0);
        }
        return false;  // Failed to elevate privileges
    }
    
    return true;  // Already has administrator privileges or restarted
}

// Note: There are no default parameters here!
bool ExecuteCommand(const std::wstring& command, bool waitForCompletion, bool showWindow) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    // Set window display state
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOW : SW_HIDE;
    
    // Copy command string (CreateProcessW requires modifiable buffer)
    wchar_t* cmdLine = _wcsdup(command.c_str());
    
    if (!CreateProcessW(
        NULL,               // Application name
        cmdLine,            // Command line
        NULL,              // Process security attributes
        NULL,              // Thread security attributes
        FALSE,             // Do not inherit handles
        0,                 // Creation flags
        NULL,              // Environment variables
        NULL,              // Current directory
        &si,               // STARTUPINFO
        &pi                // PROCESS_INFORMATION
    )) {
        std::wcerr << L"Failed to execute command, error code: " << GetLastError() << std::endl;
        free(cmdLine);
        return false;
    }
    
    free(cmdLine);
    
    if (waitForCompletion) {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;  // Return whether command execution succeeded
}

// Modify main registry path key values
void ModifyMainRegistryKeys() {
    HKEY hKey;
    const char* subKey = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        // 1. Modify Default
        RegSetValueExA(hKey, "Default", 0, REG_SZ, (BYTE*)"D:\\Users\\Default", strlen("D:\\Users\\Default") + 1);
        
        // 2. Modify ProfilesDirectory
        RegSetValueExA(hKey, "ProfilesDirectory", 0, REG_SZ, (BYTE*)"D:\\Users", strlen("D:\\Users") + 1);
        
        // 3. Modify Public
        RegSetValueExA(hKey, "Public", 0, REG_SZ, (BYTE*)"D:\\Users\\Public", strlen("D:\\Users\\Public") + 1);
        
        RegCloseKey(hKey);
    }
}

// Modify drive letter in ProfileImagePath
void ModifyProfileImagePaths() {
    HKEY hKey;
    const char* subKey = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char subKeyName[256];
        DWORD subKeyNameSize = sizeof(subKeyName);
        
        for (DWORD i = 0; RegEnumKeyExA(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS; i++) {
            subKeyNameSize = sizeof(subKeyName);
            
            // Find subkeys starting with "S-1-5-21"
            if (strncmp(subKeyName, "S-1-5-21", 8) == 0) {
                HKEY hSubKey;
                std::string fullPath = std::string(subKey) + "\\" + subKeyName;
                
                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hSubKey) == ERROR_SUCCESS) {
                    char oldPath[MAX_PATH] = {0};
                    DWORD type, size = sizeof(oldPath);
                    
                    // Get current ProfileImagePath
                    if (RegQueryValueExA(hSubKey, "ProfileImagePath", NULL, &type, (BYTE*)oldPath, &size) == ERROR_SUCCESS) {
                        // Change drive letter to D:
                        std::string newPath = "D:";
                        newPath += (oldPath + 2); // Keep the part after the colon in the original path
                        
                        RegSetValueExA(hSubKey, "ProfileImagePath", 0, REG_SZ, (BYTE*)newPath.c_str(), newPath.length() + 1);
                    }
                    
                    RegCloseKey(hSubKey);
                }
            }
        }
        
        RegCloseKey(hKey);
    }
}