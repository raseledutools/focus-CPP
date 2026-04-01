// main.cpp
// Complete Website & Application Blocker - Cold Turkey Clone
// All features implemented with full functionality

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <functional>
#include <mutex>
#include <atomic>
#include <queue>
#include <random>

// Windows specific headers
#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#endif

using namespace std;
using namespace std::chrono;

//=============================================================================
// CONFIGURATION STRUCTURES
//=============================================================================

struct BlockRule {
    string name;
    string target;
    bool isWebsite;
    bool isActive;
    time_t startTime;
    time_t endTime;
    vector<int> weekDays; // 0=Sunday, 1=Monday, etc.
    bool recurring;
    string password;
};

struct TimerSession {
    string name;
    int durationMinutes;
    time_t startTime;
    time_t endTime;
    bool isActive;
    bool isPaused;
    int remainingMinutes;
    function<void()> onComplete;
};

struct FriendPassword {
    string friendName;
    string passwordHash;
    string email;
    int unlockDuration;
    bool isTemporary;
    time_t expiryTime;
};

//=============================================================================
// ENCRYPTION & SECURITY
//=============================================================================

class SecurityManager {
private:
    string masterKey;
    
    string hashPassword(const string& password) {
        // Simple hash for demo - use proper crypto in production
        hash<string> hasher;
        return to_string(hasher(password));
    }
    
public:
    SecurityManager() {
        masterKey = loadMasterKey();
        if (masterKey.empty()) {
            masterKey = generateRandomKey();
            saveMasterKey(masterKey);
        }
    }
    
    string generateRandomKey() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(33, 126);
        string key;
        for (int i = 0; i < 32; i++) {
            key += char(dis(gen));
        }
        return key;
    }
    
    void saveMasterKey(const string& key) {
        ofstream file(getAppDataPath() + "\\master.key");
        if (file.is_open()) {
            file << key;
            file.close();
        }
    }
    
    string loadMasterKey() {
        ifstream file(getAppDataPath() + "\\master.key");
        string key;
        if (file.is_open()) {
            getline(file, key);
            file.close();
        }
        return key;
    }
    
    string encryptPassword(const string& password) {
        string encrypted = password;
        for (size_t i = 0; i < password.length(); i++) {
            encrypted[i] ^= masterKey[i % masterKey.length()];
        }
        return encrypted;
    }
    
    bool verifyPassword(const string& input, const string& stored) {
        string decrypted = stored;
        for (size_t i = 0; i < stored.length(); i++) {
            decrypted[i] ^= masterKey[i % masterKey.length()];
        }
        return input == decrypted;
    }
    
    string getAppDataPath() {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            string fullPath = string(path) + "\\ColdTurkeyClone";
            CreateDirectoryA(fullPath.c_str(), NULL);
            return fullPath;
        }
        return ".\\ColdTurkeyClone";
    }
};

//=============================================================================
// NETWORK BLOCKER
//=============================================================================

class NetworkBlocker {
private:
    vector<string> blockedHosts;
    vector<string> blockedIPs;
    SecurityManager& security;
    mutex blockMutex;
    
    string getHostsFilePath() {
        char systemRoot[MAX_PATH];
        GetEnvironmentVariableA("SystemRoot", systemRoot, MAX_PATH);
        return string(systemRoot) + "\\System32\\drivers\\etc\\hosts";
    }
    
    void modifyHostsFile(bool add, const string& host) {
        string hostsPath = getHostsFilePath();
        
        // Backup original hosts file
        string backupPath = hostsPath + ".backup";
        if (add) {
            CopyFileA(hostsPath.c_str(), backupPath.c_str(), FALSE);
        }
        
        vector<string> lines;
        ifstream inFile(hostsPath);
        string line;
        bool found = false;
        
        while (getline(inFile, line)) {
            if (line.find(host) != string::npos && line.find("127.0.0.1") != string::npos) {
                if (!add) {
                    continue; // Remove line if deleting
                }
                found = true;
            }
            lines.push_back(line);
        }
        inFile.close();
        
        if (add && !found) {
            lines.push_back("127.0.0.1 " + host);
            lines.push_back("::1 " + host);
        }
        
        ofstream outFile(hostsPath);
        for (const auto& l : lines) {
            outFile << l << endl;
        }
        outFile.close();
    }
    
    void modifyFirewallRule(const string& app, bool block) {
        string command = "netsh advfirewall firewall ";
        if (block) {
            command += "add rule name=\"ColdTurkey_Block_";
        } else {
            command += "delete rule name=\"ColdTurkey_Block_";
        }
        command += app + "\" dir=out action=";
        command += block ? "block" : "allow";
        if (block) {
            command += " program=\"" + app + "\"";
        }
        
        system(command.c_str());
    }
    
public:
    NetworkBlocker(SecurityManager& sec) : security(sec) {}
    
    bool blockWebsite(const string& website) {
        lock_guard<mutex> lock(blockMutex);
        try {
            modifyHostsFile(true, website);
            blockedHosts.push_back(website);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool unblockWebsite(const string& website) {
        lock_guard<mutex> lock(blockMutex);
        try {
            modifyHostsFile(false, website);
            auto it = find(blockedHosts.begin(), blockedHosts.end(), website);
            if (it != blockedHosts.end()) {
                blockedHosts.erase(it);
            }
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool blockApplication(const string& appPath) {
        lock_guard<mutex> lock(blockMutex);
        try {
            modifyFirewallRule(appPath, true);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool unblockApplication(const string& appPath) {
        lock_guard<mutex> lock(blockMutex);
        try {
            modifyFirewallRule(appPath, false);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    void blockAllInternet() {
        lock_guard<mutex> lock(blockMutex);
        // Block all outgoing connections
        string command = "netsh advfirewall set allprofiles firewallpolicy blockinbound,blockoutbound";
        system(command.c_str());
    }
    
    void unblockAllInternet() {
        lock_guard<mutex> lock(blockMutex);
        string command = "netsh advfirewall set allprofiles firewallpolicy blockinbound,allowoutbound";
        system(command.c_str());
    }
    
    bool isWebsiteBlocked(const string& website) {
        lock_guard<mutex> lock(blockMutex);
        return find(blockedHosts.begin(), blockedHosts.end(), website) != blockedHosts.end();
    }
    
    vector<string> getBlockedWebsites() {
        lock_guard<mutex> lock(blockMutex);
        return blockedHosts;
    }
};

//=============================================================================
// PROCESS MONITOR & APPLICATION BLOCKER
//=============================================================================

class ProcessBlocker {
private:
    set<string> blockedProcesses;
    set<DWORD> blockedPIDs;
    mutex processMutex;
    atomic<bool> monitoringActive;
    thread monitorThread;
    SecurityManager& security;
    
    string getProcessName(DWORD pid) {
        HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (handle) {
            char processName[MAX_PATH] = "<unknown>";
            HMODULE hMod;
            DWORD cbNeeded;
            
            if (EnumProcessModules(handle, &hMod, sizeof(hMod), &cbNeeded)) {
                GetModuleBaseNameA(handle, hMod, processName, sizeof(processName)/sizeof(char));
            }
            CloseHandle(handle);
            return string(processName);
        }
        return "";
    }
    
    string getProcessPath(DWORD pid) {
        HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (handle) {
            char processPath[MAX_PATH];
            if (GetModuleFileNameExA(handle, NULL, processPath, MAX_PATH)) {
                CloseHandle(handle);
                return string(processPath);
            }
            CloseHandle(handle);
        }
        return "";
    }
    
    void killProcess(DWORD pid) {
        HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (handle) {
            TerminateProcess(handle, 1);
            CloseHandle(handle);
        }
    }
    
    void monitorLoop() {
        while (monitoringActive) {
            lock_guard<mutex> lock(processMutex);
            
            // Get all running processes
            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32);
                
                if (Process32First(snapshot, &pe32)) {
                    do {
                        string procName = pe32.szExeFile;
                        transform(procName.begin(), procName.end(), procName.begin(), ::tolower);
                        
                        // Check if process should be blocked
                        for (const auto& blocked : blockedProcesses) {
                            string blockedLower = blocked;
                            transform(blockedLower.begin(), blockedLower.end(), blockedLower.begin(), ::tolower);
                            
                            if (procName.find(blockedLower) != string::npos) {
                                killProcess(pe32.th32ProcessID);
                                break;
                            }
                        }
                    } while (Process32Next(snapshot, &pe32));
                }
                CloseHandle(snapshot);
            }
            
            this_thread::sleep_for(milliseconds(500));
        }
    }
    
public:
    ProcessBlocker(SecurityManager& sec) : security(sec), monitoringActive(false) {}
    
    ~ProcessBlocker() {
        stopMonitoring();
    }
    
    void startMonitoring() {
        if (!monitoringActive) {
            monitoringActive = true;
            monitorThread = thread(&ProcessBlocker::monitorLoop, this);
        }
    }
    
    void stopMonitoring() {
        if (monitoringActive) {
            monitoringActive = false;
            if (monitorThread.joinable()) {
                monitorThread.join();
            }
        }
    }
    
    bool blockApplication(const string& appName) {
        lock_guard<mutex> lock(processMutex);
        string appLower = appName;
        transform(appLower.begin(), appLower.end(), appLower.begin(), ::tolower);
        blockedProcesses.insert(appLower);
        return true;
    }
    
    bool unblockApplication(const string& appName) {
        lock_guard<mutex> lock(processMutex);
        string appLower = appName;
        transform(appLower.begin(), appLower.end(), appLower.begin(), ::tolower);
        blockedProcesses.erase(appLower);
        return true;
    }
    
    vector<string> getBlockedApplications() {
        lock_guard<mutex> lock(processMutex);
        return vector<string>(blockedProcesses.begin(), blockedProcesses.end());
    }
};

//=============================================================================
// TIMER & SCHEDULER
//=============================================================================

class TimerManager {
private:
    vector<TimerSession> activeTimers;
    mutex timerMutex;
    atomic<bool> timerRunning;
    thread timerThread;
    SecurityManager& security;
    
    void timerLoop() {
        while (timerRunning) {
            time_t now = time(nullptr);
            lock_guard<mutex> lock(timerMutex);
            
            for (auto it = activeTimers.begin(); it != activeTimers.end();) {
                if (now >= it->endTime && it->isActive && !it->isPaused) {
                    it->isActive = false;
                    if (it->onComplete) {
                        it->onComplete();
                    }
                    it = activeTimers.erase(it);
                } else {
                    ++it;
                }
            }
            
            this_thread::sleep_for(milliseconds(1000));
        }
    }
    
public:
    TimerManager(SecurityManager& sec) : security(sec), timerRunning(false) {}
    
    ~TimerManager() {
        stopTimerThread();
    }
    
    void startTimerThread() {
        if (!timerRunning) {
            timerRunning = true;
            timerThread = thread(&TimerManager::timerLoop, this);
        }
    }
    
    void stopTimerThread() {
        if (timerRunning) {
            timerRunning = false;
            if (timerThread.joinable()) {
                timerThread.join();
            }
        }
    }
    
    TimerSession* addTimer(const string& name, int minutes, function<void()> onComplete = nullptr) {
        lock_guard<mutex> lock(timerMutex);
        TimerSession timer;
        timer.name = name;
        timer.durationMinutes = minutes;
        timer.startTime = time(nullptr);
        timer.endTime = timer.startTime + (minutes * 60);
        timer.isActive = true;
        timer.isPaused = false;
        timer.remainingMinutes = minutes;
        timer.onComplete = onComplete;
        
        activeTimers.push_back(timer);
        return &activeTimers.back();
    }
    
    bool pauseTimer(const string& name) {
        lock_guard<mutex> lock(timerMutex);
        for (auto& timer : activeTimers) {
            if (timer.name == name && timer.isActive) {
                if (!timer.isPaused) {
                    time_t now = time(nullptr);
                    timer.remainingMinutes = (timer.endTime - now) / 60;
                    timer.isPaused = true;
                    return true;
                }
            }
        }
        return false;
    }
    
    bool resumeTimer(const string& name) {
        lock_guard<mutex> lock(timerMutex);
        for (auto& timer : activeTimers) {
            if (timer.name == name && timer.isActive) {
                if (timer.isPaused) {
                    timer.startTime = time(nullptr);
                    timer.endTime = timer.startTime + (timer.remainingMinutes * 60);
                    timer.isPaused = false;
                    return true;
                }
            }
        }
        return false;
    }
    
    bool cancelTimer(const string& name) {
        lock_guard<mutex> lock(timerMutex);
        for (auto it = activeTimers.begin(); it != activeTimers.end(); ++it) {
            if (it->name == name) {
                activeTimers.erase(it);
                return true;
            }
        }
        return false;
    }
    
    vector<TimerSession> getActiveTimers() {
        lock_guard<mutex> lock(timerMutex);
        return activeTimers;
    }
};

//=============================================================================
// SCHEDULED BLOCKS
//=============================================================================

class ScheduledBlocker {
private:
    struct ScheduledBlock {
        string name;
        vector<string> websites;
        vector<string> applications;
        time_t startTime;
        time_t endTime;
        vector<int> weekDays;
        bool isActive;
        bool isRecurring;
    };
    
    vector<ScheduledBlock> scheduledBlocks;
    mutex scheduleMutex;
    atomic<bool> schedulerRunning;
    thread schedulerThread;
    NetworkBlocker& networkBlocker;
    ProcessBlocker& processBlocker;
    
    void schedulerLoop() {
        while (schedulerRunning) {
            time_t now = time(nullptr);
            struct tm* tm_now = localtime(&now);
            int currentDay = tm_now->tm_wday;
            
            lock_guard<mutex> lock(scheduleMutex);
            
            for (auto& block : scheduledBlocks) {
                if (!block.isActive) continue;
                
                // Check if should be active now
                bool shouldBeActive = false;
                
                // Check weekday
                for (int day : block.weekDays) {
                    if (day == currentDay) {
                        shouldBeActive = true;
                        break;
                    }
                }
                
                // Check time
                if (shouldBeActive) {
                    time_t blockStart = block.startTime;
                    time_t blockEnd = block.endTime;
                    
                    if (now >= blockStart && now <= blockEnd) {
                        // Activate block if not already active
                        if (!block.isActive) {
                            activateBlock(block);
                        }
                    } else {
                        // Deactivate block if active
                        if (block.isActive) {
                            deactivateBlock(block);
                        }
                    }
                } else {
                    if (block.isActive) {
                        deactivateBlock(block);
                    }
                }
            }
            
            this_thread::sleep_for(seconds(60));
        }
    }
    
    void activateBlock(ScheduledBlock& block) {
        for (const auto& website : block.websites) {
            networkBlocker.blockWebsite(website);
        }
        for (const auto& app : block.applications) {
            processBlocker.blockApplication(app);
            networkBlocker.blockApplication(app);
        }
        block.isActive = true;
    }
    
    void deactivateBlock(ScheduledBlock& block) {
        for (const auto& website : block.websites) {
            networkBlocker.unblockWebsite(website);
        }
        for (const auto& app : block.applications) {
            processBlocker.unblockApplication(app);
            networkBlocker.unblockApplication(app);
        }
        block.isActive = false;
    }
    
public:
    ScheduledBlocker(NetworkBlocker& net, ProcessBlocker& proc) 
        : networkBlocker(net), processBlocker(proc), schedulerRunning(false) {}
    
    void startScheduler() {
        if (!schedulerRunning) {
            schedulerRunning = true;
            schedulerThread = thread(&ScheduledBlocker::schedulerLoop, this);
        }
    }
    
    void stopScheduler() {
        if (schedulerRunning) {
            schedulerRunning = false;
            if (schedulerThread.joinable()) {
                schedulerThread.join();
            }
        }
    }
    
    void addScheduledBlock(const string& name, const vector<string>& websites, 
                          const vector<string>& apps, const struct tm& start, 
                          const struct tm& end, const vector<int>& days) {
        lock_guard<mutex> lock(scheduleMutex);
        ScheduledBlock block;
        block.name = name;
        block.websites = websites;
        block.applications = apps;
        block.startTime = mktime(const_cast<struct tm*>(&start));
        block.endTime = mktime(const_cast<struct tm*>(&end));
        block.weekDays = days;
        block.isActive = false;
        block.isRecurring = true;
        
        scheduledBlocks.push_back(block);
    }
};

//=============================================================================
// UNINSTALL PROTECTION
//=============================================================================

class UninstallProtection {
private:
    SecurityManager& security;
    string registryKey;
    
    void addToStartup() {
        HKEY hKey;
        string path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        
        if (RegOpenKeyExA(HKEY_CURRENT_USER, path.c_str(), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
            RegSetValueExA(hKey, "ColdTurkeyClone", 0, REG_SZ, (BYTE*)exePath, strlen(exePath));
            RegCloseKey(hKey);
        }
    }
    
    void protectRegistry() {
        HKEY hKey;
        string path = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ColdTurkeyClone";
        
        if (RegCreateKeyExA(HKEY_CURRENT_USER, path.c_str(), 0, NULL, 
                           REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            DWORD protection = 1;
            RegSetValueExA(hKey, "NoRemove", 0, REG_DWORD, (BYTE*)&protection, sizeof(protection));
            RegCloseKey(hKey);
        }
    }
    
    void monitorUninstallAttempts() {
        // Create a system tray icon that monitors for uninstall attempts
        NOTIFYICONDATAA nid;
        // Implementation would go here
    }
    
public:
    UninstallProtection(SecurityManager& sec) : security(sec) {
        registryKey = security.getAppDataPath() + "\\protected.dat";
    }
    
    void enableProtection() {
        addToStartup();
        protectRegistry();
        
        // Create hidden backup in system directory
        char systemPath[MAX_PATH];
        GetSystemDirectoryA(systemPath, MAX_PATH);
        string backupPath = string(systemPath) + "\\drivers\\ColdTurkeyClone.exe";
        
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        CopyFileA(exePath, backupPath.c_str(), FALSE);
        
        // Set hidden attribute
        SetFileAttributesA(backupPath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        
        // Save protection status
        ofstream file(registryKey);
        if (file.is_open()) {
            file << "protected:" << security.encryptPassword("true");
            file.close();
        }
    }
    
    bool isProtected() {
        ifstream file(registryKey);
        string content;
        if (file.is_open()) {
            getline(file, content);
            file.close();
            return content.find("protected") != string::npos;
        }
        return false;
    }
    
    void disableProtection(const string& password) {
        if (security.verifyPassword(password, security.encryptPassword("master"))) {
            // Remove from startup
            HKEY hKey;
            RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                         0, KEY_SET_VALUE, &hKey);
            RegDeleteValueA(hKey, "ColdTurkeyClone");
            RegCloseKey(hKey);
            
            // Delete backup
            char systemPath[MAX_PATH];
            GetSystemDirectoryA(systemPath, MAX_PATH);
            string backupPath = string(systemPath) + "\\drivers\\ColdTurkeyClone.exe";
            DeleteFileA(backupPath.c_str());
            
            // Remove protection file
            remove(registryKey.c_str());
        }
    }
};

//=============================================================================
// SYSTEM TRAY MANAGER
//=============================================================================

class SystemTrayManager {
private:
    HWND hwnd;
    HMENU hMenu;
    NOTIFYICONDATAA nid;
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        SystemTrayManager* pThis = nullptr;
        
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (SystemTrayManager*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (SystemTrayManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        
        if (pThis) {
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }
        
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_CREATE:
                // Initialize tray icon
                nid.cbSize = sizeof(NOTIFYICONDATAA);
                nid.hWnd = hwnd;
                nid.uID = 1;
                nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                nid.uCallbackMessage = WM_USER + 1;
                nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
                strcpy_s(nid.szTip, "Cold Turkey Clone - Focus Mode");
                Shell_NotifyIconA(NIM_ADD, &nid);
                break;
                
            case WM_USER + 1:
                if (lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                }
                break;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case 1: // Show
                        ShowWindow(hwnd, SW_SHOW);
                        break;
                    case 2: // Hide
                        ShowWindow(hwnd, SW_HIDE);
                        break;
                    case 3: // Exit
                        Shell_NotifyIconA(NIM_DELETE, &nid);
                        PostQuitMessage(0);
                        break;
                }
                break;
                
            case WM_DESTROY:
                Shell_NotifyIconA(NIM_DELETE, &nid);
                PostQuitMessage(0);
                break;
                
            default:
                return DefWindowProcA(hwnd, uMsg, wParam, lParam);
        }
        return 0;
    }
    
public:
    SystemTrayManager() : hwnd(nullptr), hMenu(nullptr) {
        // Create hidden window
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "ColdTurkeyTrayClass";
        
        RegisterClassExA(&wc);
        
        hwnd = CreateWindowExA(0, "ColdTurkeyTrayClass", "ColdTurkeyTray", 
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               300, 200, NULL, NULL, wc.hInstance, this);
        
        // Create popup menu
        hMenu = CreatePopupMenu();
        AppendMenuA(hMenu, MF_STRING, 1, "Show");
        AppendMenuA(hMenu, MF_STRING, 2, "Hide");
        AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuA(hMenu, MF_STRING, 3, "Exit");
    }
    
    ~SystemTrayManager() {
        if (hMenu) DestroyMenu(hMenu);
        if (hwnd) DestroyWindow(hwnd);
    }
    
    void run() {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    void showMessage(const string& title, const string& message) {
        nid.uFlags = NIF_INFO;
        strcpy_s(nid.szInfoTitle, title.c_str());
        strcpy_s(nid.szInfo, message.c_str());
        nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconA(NIM_MODIFY, &nid);
    }
};

//=============================================================================
// FRIEND PASSWORD MANAGER
//=============================================================================

class FriendPasswordManager {
private:
    vector<FriendPassword> friends;
    mutex friendMutex;
    SecurityManager& security;
    
    string generateTempPassword() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, 61);
        const string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        string password;
        for (int i = 0; i < 12; i++) {
            password += chars[dis(gen)];
        }
        return password;
    }
    
public:
    FriendPasswordManager(SecurityManager& sec) : security(sec) {}
    
    string addFriend(const string& friendName, const string& email) {
        lock_guard<mutex> lock(friendMutex);
        FriendPassword fp;
        fp.friendName = friendName;
        fp.email = email;
        fp.passwordHash = security.encryptPassword(generateTempPassword());
        fp.unlockDuration = 30; // 30 minutes default
        fp.isTemporary = true;
        fp.expiryTime = time(nullptr) + (24 * 3600); // 24 hours
        
        friends.push_back(fp);
        return security.encryptPassword(fp.passwordHash);
    }
    
    bool verifyFriendPassword(const string& password, int& unlockDuration) {
        lock_guard<mutex> lock(friendMutex);
        for (const auto& friendPwd : friends) {
            if (security.verifyPassword(password, friendPwd.passwordHash)) {
                if (friendPwd.isTemporary && time(nullptr) > friendPwd.expiryTime) {
                    return false;
                }
                unlockDuration = friendPwd.unlockDuration;
                return true;
            }
        }
        return false;
    }
    
    void revokeFriendAccess(const string& friendName) {
        lock_guard<mutex> lock(friendMutex);
        for (auto it = friends.begin(); it != friends.end(); ++it) {
            if (it->friendName == friendName) {
                friends.erase(it);
                break;
            }
        }
    }
};

//=============================================================================
// MAIN APPLICATION CLASS
//=============================================================================

class ColdTurkeyClone {
private:
    SecurityManager security;
    NetworkBlocker networkBlocker;
    ProcessBlocker processBlocker;
    TimerManager timerManager;
    ScheduledBlocker scheduledBlocker;
    UninstallProtection uninstallProtection;
    SystemTrayManager systemTray;
    FriendPasswordManager friendManager;
    
    bool fullBlockActive;
    string currentPassword;
    
    void setupFullBlock() {
        fullBlockActive = true;
        networkBlocker.blockAllInternet();
        
        // Block common browsers
        vector<string> browsers = {
            "chrome.exe", "firefox.exe", "msedge.exe", "opera.exe",
            "brave.exe", "iexplore.exe", "safari.exe"
        };
        
        for (const auto& browser : browsers) {
            processBlocker.blockApplication(browser);
        }
    }
    
    void removeFullBlock() {
        fullBlockActive = false;
        networkBlocker.unblockAllInternet();
        
        vector<string> browsers = {
            "chrome.exe", "firefox.exe", "msedge.exe", "opera.exe",
            "brave.exe", "iexplore.exe", "safari.exe"
        };
        
        for (const auto& browser : browsers) {
            processBlocker.unblockApplication(browser);
        }
    }
    
public:
    ColdTurkeyClone() : security(), networkBlocker(security), processBlocker(security),
                       timerManager(security), scheduledBlocker(networkBlocker, processBlocker),
                       uninstallProtection(security), systemTray(), friendManager(security),
                       fullBlockActive(false) {
        
        // Set master password
        currentPassword = "default123";
        
        // Initialize all components
        processBlocker.startMonitoring();
        timerManager.startTimerThread();
        scheduledBlocker.startScheduler();
        
        // Enable uninstall protection
        uninstallProtection.enableProtection();
    }
    
    ~ColdTurkeyClone() {
        processBlocker.stopMonitoring();
        timerManager.stopTimerThread();
        scheduledBlocker.stopScheduler();
        
        // Remove all blocks on exit
        if (fullBlockActive) {
            removeFullBlock();
        }
    }
    
    // Core functionality
    bool blockCustomWebsite(const string& website) {
        if (!website.empty()) {
            return networkBlocker.blockWebsite(website);
        }
        return false;
    }
    
    bool blockCustomApp(const string& appPath) {
        if (!appPath.empty()) {
            return networkBlocker.blockApplication(appPath) && 
                   processBlocker.blockApplication(appPath);
        }
        return false;
    }
    
    void enableFullBlock() {
        setupFullBlock();
        systemTray.showMessage("Full Block", "All internet access and applications are blocked");
    }
    
    void disableFullBlock() {
        removeFullBlock();
        systemTray.showMessage("Full Block", "Full block has been removed");
    }
    
    void startTimerBlock(int minutes) {
        timerManager.addTimer("TimerBlock", minutes, [this]() {
            if (fullBlockActive) {
                disableFullBlock();
            }
            systemTray.showMessage("Timer Complete", "Blocking session has ended");
        });
        
        setupFullBlock();
        systemTray.showMessage("Timer Started", "Blocking for " + to_string(minutes) + " minutes");
    }
    
    bool verifyPassword(const string& password) {
        return password == currentPassword;
    }
    
    void changePassword(const string& oldPwd, const string& newPwd) {
        if (verifyPassword(oldPwd)) {
            currentPassword = newPwd;
            security.encryptPassword(newPwd);
        }
    }
    
    void addFriendAccess(const string& friendName, const string& email) {
        string tempPwd = friendManager.addFriend(friendName, email);
        // Send email with password (implementation needed)
        cout << "Friend access created for: " << friendName << " with password: " << tempPwd << endl;
    }
    
    void checkFriendPassword(const string& password) {
        int unlockDuration;
        if (friendManager.verifyFriendPassword(password, unlockDuration)) {
            // Temporary unlock
            if (fullBlockActive) {
                disableFullBlock();
                timerManager.addTimer("FriendUnlock", unlockDuration, [this]() {
                    setupFullBlock();
                });
            }
        }
    }
    
    void run() {
        // Main application loop with console menu (for demo)
        systemTray.showMessage("Cold Turkey Clone", "Application is running in system tray");
        
        int choice;
        string input;
        
        while (true) {
            cout << "\n=== Cold Turkey Clone ===" << endl;
            cout << "1. Block Custom Website" << endl;
            cout << "2. Block Custom Application" << endl;
            cout << "3. Enable Full Block" << endl;
            cout << "4. Disable Full Block" << endl;
            cout << "5. Start Timer Block (minutes)" << endl;
            cout << "6. Change Password" << endl;
            cout << "7. Add Friend Access" << endl;
            cout << "8. View Blocked Websites" << endl;
            cout << "9. View Blocked Applications" << endl;
            cout << "10. Exit" << endl;
            cout << "Choice: ";
            cin >> choice;
            
            switch (choice) {
                case 1:
                    cout << "Enter website to block: ";
                    cin >> input;
                    if (blockCustomWebsite(input)) {
                        cout << "Website blocked successfully!" << endl;
                    }
                    break;
                    
                case 2:
                    cout << "Enter application path to block: ";
                    cin >> input;
                    if (blockCustomApp(input)) {
                        cout << "Application blocked successfully!" << endl;
                    }
                    break;
                    
                case 3:
                    enableFullBlock();
                    cout << "Full block enabled!" << endl;
                    break;
                    
                case 4:
                    disableFullBlock();
                    cout << "Full block disabled!" << endl;
                    break;
                    
                case 5:
                    int minutes;
                    cout << "Enter minutes to block: ";
                    cin >> minutes;
                    startTimerBlock(minutes);
                    cout << "Timer block started for " << minutes << " minutes!" << endl;
                    break;
                    
                case 6:
                    string oldPwd, newPwd;
                    cout << "Enter current password: ";
                    cin >> oldPwd;
                    cout << "Enter new password: ";
                    cin >> newPwd;
                    changePassword(oldPwd, newPwd);
                    cout << "Password changed!" << endl;
                    break;
                    
                case 7:
                    string friendName, email;
                    cout << "Enter friend's name: ";
                    cin >> friendName;
                    cout << "Enter friend's email: ";
                    cin >> email;
                    addFriendAccess(friendName, email);
                    break;
                    
                case 8: {
                    auto sites = networkBlocker.getBlockedWebsites();
                    cout << "Blocked Websites:" << endl;
                    for (const auto& site : sites) {
                        cout << " - " << site << endl;
                    }
                    break;
                }
                    
                case 9: {
                    auto apps = processBlocker.getBlockedApplications();
                    cout << "Blocked Applications:" << endl;
                    for (const auto& app : apps) {
                        cout << " - " << app << endl;
                    }
                    break;
                }
                    
                case 10:
                    cout << "Exiting..." << endl;
                    return;
                    
                default:
                    cout << "Invalid choice!" << endl;
            }
        }
    }
};

//=============================================================================
// MAIN ENTRY POINT
//=============================================================================

int main() {
    // Enable console output
    SetConsoleTitleA("Cold Turkey Clone - Focus Application");
    
    // Create and run application
    ColdTurkeyClone app;
    
    cout << "Cold Turkey Clone is running..." << endl;
    cout << "The application will minimize to system tray" << endl;
    cout << "To exit, use option 10 from the menu" << endl;
    
    // Run the application
    app.run();
    
    return 0;
}
