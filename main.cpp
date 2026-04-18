#include"stdio.h"
#include"windows.h"
#include <shellapi.h>
#include <string>
#include <iostream>
#include <fstream> 

bool IsRunAsAdmin();
bool RestartAsAdmin();
bool EnsureAdminPrivileges();
void ModifyMainRegistryKeys();
void ModifyProfileImagePaths();
bool Check_File_Exists(std::string index);
bool Create_File(std::string Name);
bool Delete_File(std::string Name);
bool checkDriveExists(int driveNum);

int main(){
    //申请管理员权限
     if (!EnsureAdminPrivileges()) {
        // 提权失败，处理错误或退出
        return 1;
    }
    char Word=0;
    SetConsoleOutputCP(CP_UTF8);SetConsoleCP(CP_UTF8);
    printf("欢迎使用Users用户文件夹迁移工具\r\n");
    printf("Welcome to Users folder migration tool\r\n");
    printf("*****************第一次打开会自动修改注册表然后注销,注销之后再次打开软件会自动创建符号链接*****************\r\n");
    printf("*****************First time opening will automatically modify the registry and then log off, after logging off, open the software again to automatically create symbolic links*****************\r\n");
    printf("此软件功能:\r\n1.复制Users用户文件夹到D盘\r\n2.修改注册表路径使系统访问用户路径指向D盘\r\n3.设置C:Users->D:Users的符号链接\r\n");
    printf("This software's functions:\r\n1.Copy Users folder to D drive\r\n2.Modify registry path to make system access user path point to D drive\r\n3.Set up symbolic link C:Users->D:Users\r\n");
    printf("本软件访问的修改的注册表路径：\r\nHKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\\r\n");
    printf("The registry path accessed and modified by this software:\r\nHKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\\r\n");
    printf("本软件将修改此路径下的这些条目:\r\n1.键值:Default   数据修改为:D:\\Users\\Default\r\n2.键值:ProfilesDirectory   数据修改为:D:\\Users\r\n3.键值:Public   数据修改为:D:\\Users\\Public\r\n");
    printf("This software will modify the following entries under this path:\r\n1.Key:Default   Data modified to: D:\\Users\\Default\r\n2.Key:ProfilesDirectory   Data modified to: D:\\Users\r\n3.Key:Public   Data modified to: D:\\Users\\Public\r\n");
    printf("修改下级目录开头为S-1-5-21的文件夹内的键值为ProfileImagePath的数据字段的为盘符为D盘\r\n"); 
    printf("Modify the data field of the key named ProfileImagePath in the subfolders starting with S-1-5-21 to change the drive letter to D\r\n");
    if(!Check_File_Exists("index")) {
        if(!checkDriveExists(2)){
            printf("没有D盘\r\n");
            printf("No D drivern\r\n");
            return 0;
        }
        printf("是否确认修改？  Y/N"    );
        printf("\nConfirm the modification?  Y/N   ");
        scanf("%c",&Word);
        if(Word!='y'&&Word!='Y') return 0;
        system("robocopy \"C:\\Users\" \"D:\\Users\" /E /COPYALL /XJ");
        ModifyMainRegistryKeys();
        ModifyProfileImagePaths();
        Create_File("index");
        system("logoff");
    }
    else {
        system("rmdir /S /Q \"C:\\Users\"");
        system("mklink /D \"C:\\Users\" \"D:\\Users\"");
        Delete_File("index");
        printf("操作完成，按任意键退出...");
        printf("\nOperation completed, press any key to exit...");
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
        MessageBoxW(NULL, L"程序需要管理员权限才能正常运行。\nThe program requires administrator privileges to run properly.", L"权限不足\nInsufficient Privileges", MB_ICONWARNING | MB_OK);
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


bool Check_File_Exists(std::string index) {
    char exePath[MAX_PATH] = {0};
    
    // 获取当前可执行文件路径
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    
    // 去除文件名，只保留目录
    std::string dirPath = exePath;
    size_t pos = dirPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        dirPath = dirPath.substr(0, pos);
    }
    
    // 拼接目标文件路径
    std::string targetPath = dirPath + "\\"+ index;
    
    // 检查文件是否存在
    DWORD fileAttrib = GetFileAttributesA(targetPath.c_str());
    return (fileAttrib != INVALID_FILE_ATTRIBUTES && 
            !(fileAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Create_File(std::string Name) {
    char exePath[MAX_PATH] = {0};
    
    // 获取当前可执行文件路径
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    
    // 去除文件名，只保留目录
    std::string dirPath = exePath;
    size_t pos = dirPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        dirPath = dirPath.substr(0, pos);
    }
    
    // 拼接目标文件路径
    std::string targetPath = dirPath + "\\" + Name;  
    
    // 创建文件
    std::ofstream file(targetPath);
    
    // 返回是否创建成功
    return file.is_open();  
}

bool Delete_File(std::string Name) {
    char exePath[MAX_PATH] = {0};
    
    // 获取当前可执行文件路径
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    
    // 去除文件名，只保留目录
    std::string dirPath = exePath;
    size_t pos = dirPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        dirPath = dirPath.substr(0, pos);
    }
    
    // 拼接目标文件路径
    std::string targetPath = dirPath + "\\" + Name;  
     
    return DeleteFileA(targetPath.c_str()) != 0;
}

/**
 * 检测指定序号的硬盘驱动器是否存在
 * @param driveNum 驱动器序号：1对应C盘，2对应D盘，...，9对应K盘
 * @return 如果驱动器存在返回true，否则返回false
 */
bool checkDriveExists(int driveNum) {
    // 参数有效性检查
    if (driveNum < 1 || driveNum > 9) {
        return false;
    } 
    // 获取所有逻辑驱动器
    DWORD drives = GetLogicalDrives();
    if (drives == 0) {
        printf("错误：无法获取驱动器列表");
         printf("Error: Unable to retrieve the drive list");
        return false;
    }
    
    // 计算对应驱动器的位掩码
    // A盘=0, B=1, C=2, D=3,... 所以C盘=2，对应输入1
    int bitPosition = driveNum + 1;  // 1->C(2), 2->D(3), ..., 9->K(10)
    
    // 检查该位是否为1
    return (drives & (1 << bitPosition)) != 0;
}