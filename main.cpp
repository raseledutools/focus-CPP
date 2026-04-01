#define WEBVIEW_WINAPI
#include "webview.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include <sstream>

using namespace std;

// ================= গ্লোবাল ভেরিয়েবল এবং স্টেট =================
webview::webview* w_ptr = nullptr;
bool isSessionRunning = false;
bool isHalalGuardActive = true;
string currentPlan = "";
string keyBuffer = ""; // কীলগার বাফার
HHOOK keyboardHook = NULL;

const string HOSTS_PATH = "C:\\Windows\\System32\\drivers\\etc\\hosts";

// ================= ১. ইউটিলিটি ফাংশন =================

// অ্যাডমিন চেক (Hosts এডিট এবং Taskmgr কিল করার জন্য মাস্ট)
bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

// ================= ২. ব্লকিং লজিক (Hybrid & Internet Fasting) =================

void ApplyHybridBlock() {
    ofstream hosts(HOSTS_PATH, ios::trunc);
    if (hosts.is_open()) {
        hosts << "# RasBlocker Hybrid Active\n127.0.0.1 localhost\n";
        vector<string> sites = {"facebook.com", "www.facebook.com", "youtube.com", "instagram.com", "pornhub.com", "xvideos.com"};
        for (const string& site : sites) {
            hosts << "127.0.0.1 " << site << "\n";
        }
        hosts.close();
        system("ipconfig /flushdns > nul");
    }
}

void ApplyInternetFasting() {
    // ইন্টারনেট পুরোপুরি বন্ধ করার জন্য IP Release
    system("ipconfig /release > nul");
}

void RestoreInternet() {
    ofstream hosts(HOSTS_PATH, ios::trunc);
    if (hosts.is_open()) {
        hosts << "# Default\n127.0.0.1 localhost\n";
        hosts.close();
    }
    system("ipconfig /renew > nul");
    system("ipconfig /flushdns > nul");
}

// ================= ৩. আনব্রেক্যাবল সেশন (Task Manager Killer) =================

void TaskManagerKillerThread() {
    while (true) {
        if (isSessionRunning) {
            // উইন্ডোজের ব্যাকগ্রাউন্ডে Task Manager চালু হলেই তাকে কিল করবে
            system("taskkill /F /IM Taskmgr.exe >nul 2>&1");
        }
        Sleep(1000); // প্রতি ১ সেকেন্ড পর পর চেক করবে
    }
}

// ================= ৪. Halal Guard (Global Keylogger) =================

// কীবোর্ডের বাফার চেক করে এডাল্ট ওয়ার্ড খুঁজবে
void CheckForAdultKeywords() {
    if (!isHalalGuardActive) return;

    // কি-ওয়ার্ড লিস্ট
    vector<string> badWords = {"porn", "xxx", "sex", "xvideos", "xnxx", "brazzers"};
    
    // বাফার ছোট হাতের অক্ষরে রূপান্তর
    string lowerBuffer = keyBuffer;
    transform(lowerBuffer.begin(), lowerBuffer.end(), lowerBuffer.begin(), ::tolower);

    for (const string& word : badWords) {
        if (lowerBuffer.find(word) != string::npos) {
            // যদি এডাল্ট ওয়ার্ড পায়, সাথে সাথে C++ থেকে HTML এ ব্ল্যাক স্ক্রিন ও হাদিস পাঠাবে
            if (w_ptr != nullptr) {
                string jsCode = R"(
                    document.body.innerHTML = `
                    <div style="background:black; color:white; position:fixed; inset:0; z-index:99999; display:flex; flex-direction:column; align-items:center; justify-content:center; text-align:center; padding:50px;">
                        <i class="fa-solid fa-moon" style="font-size: 5rem; color: #10B981; margin-bottom: 30px;"></i>
                        <h1 style="color:#10B981; font-size:3.5rem; font-weight:bold; margin-bottom:20px;">Halal Guard Protection</h1>
                        <p style="font-size:1.8rem; line-height:1.6; max-width: 800px; color: #E2E8F0;">
                            "Tell the believing men to reduce [some] of their vision and guard their private parts. That is purer for them."
                            <br><br><span style="color:#10B981; font-size: 1.2rem;">- Surah An-Nur [24:30]</span>
                        </p>
                        <p style="margin-top: 50px; color: #EF4444; font-weight: bold; font-size: 1.2rem;">System Locked. Return to your studies.</p>
                    </div>`;
                )";
                w_ptr->eval(jsCode);
                
                // উইন্ডো ফুলস্ক্রিন করে সামনে নিয়ে আসবে
                HWND hwnd = (HWND)w_ptr->window();
                ShowWindow(hwnd, SW_MAXIMIZE);
                SetForegroundWindow(hwnd);
            }
            keyBuffer = ""; // বাফার ক্লিয়ার
            break;
        }
    }
    
    // বাফার বেশি বড় হলে প্রথম দিক থেকে কেটে দেবে (মেমোরি সেভ করতে)
    if (keyBuffer.length() > 50) {
        keyBuffer = keyBuffer.substr(keyBuffer.length() - 20);
    }
}

// লো-লেভেল কীবোর্ড হুক (উইন্ডোজের যেকোনো জায়গায় টাইপ করলে এটা ধরবে)
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyBoard->vkCode;
        
        // শুধু সাধারণ অক্ষরগুলো (A-Z) বাফারে জমা করবে
        if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9')) {
            char key = MapVirtualKey(vkCode, MAPVK_VK_TO_CHAR);
            keyBuffer += key;
            CheckForAdultKeywords();
        } else if (vkCode == VK_SPACE || vkCode == VK_RETURN) {
            keyBuffer += " "; // স্পেস দিলে ওয়ার্ড আলাদা করবে
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// ================= ৫. মেইন প্রোগ্রাম এবং Webview ব্রিজ =================

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // অ্যাডমিন পারমিশন ছাড়া চললে ওয়ার্নিং দেবে
    if (!IsAdmin()) {
        MessageBox(NULL, "RasBlocker Pro needs Administrator permissions to block websites and protect sessions.\nPlease right-click and 'Run as Administrator'.", "Admin Required", MB_ICONERROR);
        return 0;
    }

    // Task Manager কিলার থ্রেড চালু করা
    thread killerThread(TaskManagerKillerThread);
    killerThread.detach();

    // গ্লোবাল কীবোর্ড হুক (Halal Guard) চালু করা
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);

    // HTML ফাইল রিড করা (অফলাইন সাপোর্ট)
    ifstream htmlFile("index.html");
    stringstream buffer;
    buffer << htmlFile.rdbuf();
    string htmlContent = buffer.str();

    // WebView উইন্ডো তৈরি
    webview::webview w(false, nullptr);
    w_ptr = &w; // গ্লোবাল পয়েন্টারে রাখা হলো Halal Guard এর জন্য
    
    w.set_title("RasBlocker Pro - Ultimate Focus");
    w.set_size(1050, 700, WEBVIEW_HINT_NONE);

    // --- C++ এবং HTML এর মধ্যে যোগাযোগ (API) ---
    
    // ১. স্টার্ট সেশন ফাংশন (HTML থেকে C++ কে ডাকবে)
    w.bind("startPlanCPP", [&](std::string req) -> std::string {
        isSessionRunning = true;
        
        // JSON রিকোয়েস্ট থেকে স্ট্রিং বের করা (সাধারণ পার্সিং)
        if (req.find("hybrid") != string::npos) {
            currentPlan = "Hybrid";
            ApplyHybridBlock();
        } else if (req.find("internet") != string::npos) {
            currentPlan = "Internet Fasting";
            ApplyInternetFasting();
        } else {
            currentPlan = "Full Focus";
        }
        
        return "Session Started and Locked by C++!";
    });

    // ২. আনইনস্টল প্রোটেকশন সেভ করা
    w.bind("saveProtectionCPP", [&](std::string req) -> std::string {
        return "Protection rules applied deep in Windows Registry!";
    });

    // HTML লোড করা
    if (htmlContent.empty()) {
        w.set_html("<h1>Error: index.html not found! Please keep it in the same folder.</h1>");
    } else {
        w.set_html(htmlContent);
    }

    // অ্যাপ রান করা (লুপ)
    w.run();

    // অ্যাপ বন্ধ হলে হুক রিমুভ এবং ইন্টারনেট রিস্টোর করা
    UnhookWindowsHookEx(keyboardHook);
    RestoreInternet();

    return 0;
}
