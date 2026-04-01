#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <windows.h>
#include <tlhelp32.h>

using namespace std;

const string HOSTS_PATH = "C:\\Windows\\System32\\drivers\\etc\\hosts";

class RasBlockerPro {
private:
    vector<string> blockedWebsites;
    vector<string> blockedApps;
    string friendPassword;
    bool isBlockingActive;
    bool isPomodoroMode;

    // [Premium Feature] Windows Registry-তে অ্যাপটি যুক্ত করা (Auto-Start)
    void enableRunOnStartup() {
        HKEY hKey;
        const char* path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        
        // বর্তমান .exe ফাইলের লোকেশন বের করা
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);

        if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueEx(hKey, "RasBlockerPro", 0, REG_SZ, (BYTE*)exePath, strlen(exePath) + 1);
            RegCloseKey(hKey);
            cout << "[+] Strict Mode ON: পিসি রিস্টার্ট করলেও এটি চালু হবে!\n";
        }
    }

    // Auto-Start বন্ধ করা
    void disableRunOnStartup() {
        HKEY hKey;
        const char* path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValue(hKey, "RasBlockerPro");
            RegCloseKey(hKey);
        }
    }

    // [Premium Feature] Active Window Tracking (কী ব্যবহার করছে তা ট্র্যাক করা)
    void trackUserActivity() {
        while (isBlockingActive) {
            HWND foreground = GetForegroundWindow();
            if (foreground) {
                char windowTitle[256];
                GetWindowText(foreground, windowTitle, sizeof(windowTitle));
                // রিয়েল প্রোজেক্টে এই ডেটাগুলো একটি ফাইলে বা ডাটাবেসে সেভ করতে হয়
                // cout << "[Tracking] বর্তমানে ব্যবহার করছেন: " << windowTitle << "\n";
            }
            this_thread::sleep_for(chrono::seconds(5)); // ৫ সেকেন্ড পর পর চেক
        }
    }

    void enforceAppBlock() {
        while (isBlockingActive) {
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe;
                pe.dwSize = sizeof(PROCESSENTRY32);
                if (Process32First(hSnap, &pe)) {
                    do {
                        string currentProcess = pe.szExeFile;
                        for (const auto& app : blockedApps) {
                            if (currentProcess == app) {
                                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                                if (hProcess != NULL) {
                                    TerminateProcess(hProcess, 9);
                                    CloseHandle(hProcess);
                                }
                            }
                        }
                    } while (Process32Next(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
            this_thread::sleep_for(chrono::seconds(2));
        }
    }

public:
    RasBlockerPro() : isBlockingActive(false), isPomodoroMode(false) {}

    void setPassword(string pass) { friendPassword = pass; }
    void addWebsite(string site) { blockedWebsites.push_back(site); }
    void addApp(string appExe) { blockedApps.push_back(appExe); }

    // [Premium Feature] Firewall দিয়ে পুরো ইন্টারনেট ব্লক করা
    void enableStrictFirewallBlock() {
        // ফায়ারওয়ালের মাধ্যমে থার্ড-পার্টি কানেকশন ব্লক করা
        system("netsh advfirewall set allprofiles firewallpolicy blockinbound,blockoutbound > nul");
        cout << "[!] Firewall Strict Mode: সমস্ত ইন্টারনেট ট্রাফিক ব্লক করা হয়েছে!\n";
    }

    void disableStrictFirewallBlock() {
        system("netsh advfirewall set allprofiles firewallpolicy blockinbound,allowoutbound > nul");
        cout << "[+] Firewall Strict Mode বন্ধ করা হয়েছে।\n";
    }

    // [Premium Feature] Pomodoro System (২৫ মিনিট কাজ, ৫ মিনিট আনব্লক)
    void startPomodoro(int cycles) {
        isBlockingActive = true;
        isPomodoroMode = true;
        
        // ব্যাকগ্রাউন্ড ট্র্যাকিং এবং ব্লকিং চালু
        thread appBlockerThread(&RasBlockerPro::enforceAppBlock, this);
        thread trackerThread(&RasBlockerPro::trackUserActivity, this);

        enableRunOnStartup();

        for (int i = 1; i <= cycles; ++i) {
            cout << "\n--- Pomodoro Cycle " << i << " শুরু ---\n";
            
            // ২৫ মিনিট ফোকাস (Focus Mode)
            cout << "[-] Focus Mode: ওয়েবসাইট এবং অ্যাপ ব্লক করা হয়েছে (২৫ মিনিট)।\n";
            system("ipconfig /release > nul"); // ইন্টারনেট অফ
            this_thread::sleep_for(chrono::minutes(25)); // রিয়েল টাইমে 25 হবে
            
            // ৫ মিনিট ব্রেক (Break Mode)
            cout << "[+] Break Mode: ৫ মিনিটের জন্য আনব্লক করা হলো!\n";
            system("ipconfig /renew > nul"); // ইন্টারনেট অন
            this_thread::sleep_for(chrono::minutes(5)); // রিয়েল টাইমে 5 হবে
        }

        stopBlocking("POMODORO_DONE");
        
        isBlockingActive = false;
        appBlockerThread.join();
        trackerThread.join();
    }

    void stopBlocking(string passAttempt) {
        if (passAttempt == friendPassword || passAttempt == "POMODORO_DONE") {
            isBlockingActive = false;
            disableRunOnStartup();
            disableStrictFirewallBlock();
            system("ipconfig /renew > nul");
            cout << "\n[+] ব্লকিং সফলভাবে বন্ধ করা হয়েছে!\n";
        } else {
            cout << "\n[!] ভুল পাসওয়ার্ড! আনইনস্টল প্রটেকশন চালু আছে।\n";
        }
    }
};

int main() {
    RasBlockerPro proBlocker;

    proBlocker.setPassword("adminpass123");
    
    // ডিস্ট্রাক্টিং অ্যাপস ব্লক লিস্ট
    proBlocker.addApp("chrome.exe");
    proBlocker.addApp("Taskmgr.exe"); // [Premium] টাস্ক ম্যানেজার ব্লক যাতে প্রসেস কিল করতে না পারে!

    cout << "=======================================\n";
    cout << "       RasBlocker Pro - Activated      \n";
    cout << "=======================================\n";

    // Pomodoro স্টার্ট (উদাহরণ: ২ সাইকেল বা ১ ঘণ্টা)
    // সতর্কতা: এটি চালু করলে ইন্টারনেট অফ হয়ে যাবে এবং টাস্ক ম্যানেজার ওপেন হবে না!
    // proBlocker.startPomodoro(2); 

    // ম্যানুয়ালি ফায়ারওয়াল ব্লকিং টেস্ট করতে চাইলে নিচের কমান্ডটি আনকমেন্ট করো:
    /*
    proBlocker.enableStrictFirewallBlock();
    this_thread::sleep_for(chrono::seconds(10));
    proBlocker.disableStrictFirewallBlock();
    */

    cout << "\nপ্রোগ্রামটি বন্ধ করতে যেকোনো কি (Key) চাপুন...";
    system("pause>0");
    
    return 0;
}
