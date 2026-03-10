// 确保使用Unicode
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
void ModifyMainRegistryKeys();
void ModifyProfileImagePaths();

int main(){
    //申请管理员权限
     if (!EnsureAdminPrivileges()) {
        // 提权失败，处理错误或退出
        return 1;
    }
    int index=0;
    printf("欢迎使用Users用户文件夹迁移工具\r\n");
    printf("***********************************************必须先选择第一步，再选择第二步******************************************************\r\n");
    printf("此软件功能:\r\n1.复制Users用户文件夹到D盘\r\n2.修改注册表路径使系统访问用户路径指向D盘\r\n3.设置C:Users->D:Users的符号链接\r\n");
    printf("本软件访问的修改的注册表路径：\r\nHKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\\r\n");
    printf("本软件将修改此路径下的这些条目:\r\n1.键值:Default   数据修改为:D:\\Users\\Default\r\n2.键值:ProfilesDirectory   数据修改为:D:\\Users\r\n3.键值:Public   数据修改为:D:\\Users\\Public\r\n");
    printf("修改下级目录开头为S-1-5-21的文件夹内的键值为ProfileImagePath的数据字段的为盘符为D盘\r\n"); 
    printf("请选择功能:\r\n1.复制Users用户文件夹到D盘并修改注册表\r\n2.删除旧Users文件夹并创建符号链接\r\n");
    scanf("%d",&index);
    if(index == 1) {
        system("robocopy \"C:\\Users\" \"D:\\Users\" /E /COPYALL /XJ");
        ModifyMainRegistryKeys();
        ModifyProfileImagePaths();
        system("logoff");
    }
    if(index == 2) {
        system("rmdir /S /Q \"C:\\Users\"");
        system("mklink /D \"C:\\Users\" \"D:\\Users\"");
        printf("操作完成，按任意键退出...");
        system("pause");
    }
    
    return 0;
}

bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroupSid = NULL;
    
    // 创建管理员组的SID
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroupSid)) {
        
        // 检查Token中是否包含管理员SID
        if (!CheckTokenMembership(NULL, adminGroupSid, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(adminGroupSid);
    }
    
    return isAdmin == TRUE;
}

// 以管理员权限重新启动当前程序
bool RestartAsAdmin() {
    wchar_t szPath[MAX_PATH];
    
    // 获取当前可执行文件的路径
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH) == 0) {
        return false;
    }
    
    // 构建命令行参数，添加"/restart"标记防止无限循环
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    std::wstring parameters = L"";
    for (int i = 1; i < argc; i++) {
        // 避免重复添加restart标记
        if (wcscmp(argv[i], L"/restart") != 0) {
            parameters += L" \"";
            parameters += argv[i];
            parameters += L"\"";
        }
    }
    // 添加restart标记，表示这是重启后的实例
    parameters += L" /restart";
    
    LocalFree(argv);
    
    // 使用ShellExecute以管理员权限启动程序
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";  // 请求管理员权限
    sei.lpFile = szPath;
    sei.lpParameters = parameters.c_str();
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;
    
    if (ShellExecuteExW(&sei)) {
        return true;
    }
    
    DWORD err = GetLastError();
    if (err == ERROR_CANCELLED) {
        // 用户取消了UAC提示
        MessageBoxW(NULL, L"程序需要管理员权限才能正常运行。", L"权限不足", MB_ICONWARNING | MB_OK);
    }
    
    return false;
}

bool EnsureAdminPrivileges() {
    // 如果已重启过（包含/restart参数），跳过检查
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
    
    // 检查权限
    if (!hasRestartFlag && !IsRunAsAdmin()) {
        // 尝试以管理员权限重启
        if (RestartAsAdmin()) {
            // 重启成功，退出当前进程
            ExitProcess(0);
        }
        return false;  // 提权失败
    }
    
    return true;  // 已有管理员权限或已重启
}

// 修改主注册表路径的键值
void ModifyMainRegistryKeys() {
    HKEY hKey;
    const char* subKey = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        // 1. 修改Default
        RegSetValueExA(hKey, "Default", 0, REG_SZ, (BYTE*)"D:\\Users\\Default", strlen("D:\\Users\\Default") + 1);
        
        // 2. 修改ProfilesDirectory
        RegSetValueExA(hKey, "ProfilesDirectory", 0, REG_SZ, (BYTE*)"D:\\Users", strlen("D:\\Users") + 1);
        
        // 3. 修改Public
        RegSetValueExA(hKey, "Public", 0, REG_SZ, (BYTE*)"D:\\Users\\Public", strlen("D:\\Users\\Public") + 1);
        
        RegCloseKey(hKey);
    }
}

// 修改ProfileImagePath的盘符
void ModifyProfileImagePaths() {
    HKEY hKey;
    const char* subKey = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char subKeyName[256];
        DWORD subKeyNameSize = sizeof(subKeyName);
        
        for (DWORD i = 0; RegEnumKeyExA(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS; i++) {
            subKeyNameSize = sizeof(subKeyName);
            
            // 查找以"S-1-5-21"开头的子项
            if (strncmp(subKeyName, "S-1-5-21", 8) == 0) {
                HKEY hSubKey;
                std::string fullPath = std::string(subKey) + "\\" + subKeyName;
                
                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hSubKey) == ERROR_SUCCESS) {
                    char oldPath[MAX_PATH] = {0};
                    DWORD type, size = sizeof(oldPath);
                    
                    // 获取当前ProfileImagePath
                    if (RegQueryValueExA(hSubKey, "ProfileImagePath", NULL, &type, (BYTE*)oldPath, &size) == ERROR_SUCCESS) {
                        // 修改盘符为D:
                        std::string newPath = "D:";
                        newPath += (oldPath + 2); // 保留原路径的冒号之后的部分
                        
                        RegSetValueExA(hSubKey, "ProfileImagePath", 0, REG_SZ, (BYTE*)newPath.c_str(), newPath.length() + 1);
                    }
                    
                    RegCloseKey(hSubKey);
                }
            }
        }
        
        RegCloseKey(hKey);
    }
}