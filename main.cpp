#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <iostream>
#include <sstream>
#include <fstream>
#include <codecvt> 
#include <locale>  
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h> 
#include <uiautomation.h>
#include <ctime>

using namespace std;

// ==========================================
// --- STATE VARIABLES (To be updated via Qt FocusApi) ---
// ==========================================
static bool isAdultFocusActive = false;
static ULONGLONG focusEndTime = 0; 

// Settings
static int controlMode = 0;     // 0 = Self, 1 = Friend
static int adultReligion = 0;   // 0=Muslim, 1=Hindu, 2=Christian, 3=Universal
static int adultLanguage = 0;   // 0=Bangla, 1=English

// Filters
static bool cbAdultWeb = true; 
static bool cbFbReels = true;  
static bool cbYtShorts = true; 
static bool cbHardcore = true; 
static bool cbRomantic = true; 

// Advanced Features
static bool cbPeriodicPopups = false; 
static DWORD lastPeriodicPopupTime = 0; 
static bool cb24HourLock = false; 
static ULONGLONG lock24hEndTime = 0;
static int cleanStreakDays = 12; 

struct AdultCustomItem { wstring name; };
static vector<AdultCustomItem> customAdultKeywords;

static bool isPanicActive = false;
static int totalBlockedCount = 0; 

// ==========================================
// --- DATE & FILE HELPERS ---
// ==========================================
wstring GetTodayDateString() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    wchar_t buf[100];
    wcsftime(buf, 100, L"%B %d, %Y", ltm);
    return wstring(buf);
}

wstring GetSaveFilePath() {
    wstring path = L"C:\\ProgramData\\RasFocus";
    CreateDirectoryW(path.c_str(), NULL);
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    return path + L"\\rf_sys_data.dat";
}

// ==========================================
// LOAD ADULT SITES FROM RESOURCE
// ==========================================
vector<wstring> adultWebsites; 

vector<wstring> LoadAdultSitesFromResource() {
    vector<wstring> sites;
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(105), RT_RCDATA);
    if (!hRes) return sites; 

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return sites;

    DWORD size = SizeofResource(NULL, hRes);
    const char* data = (const char*)LockResource(hData);

    if (data && size > 0) {
        string fileContent(data, size);
        stringstream ss(fileContent);
        string line;
        
        while (getline(ss, line)) {
            if (!line.empty() && line[line.length() - 1] == '\r') {
                line.erase(line.length() - 1);
            }
            if (!line.empty()) {
                sites.push_back(wstring(line.begin(), line.end()));
            }
        }
    }
    return sites;
}

void SaveAdultSettings() {
    wstring filePath = GetSaveFilePath();
    std::wofstream out(filePath.c_str());
    out.imbue(std::locale(out.getloc(), new std::codecvt_utf8<wchar_t>));
    if (out.is_open()) {
        out << cbAdultWeb << L" " << cbFbReels << L" " << cbYtShorts << L" " << cbHardcore << L" " << cbRomantic << L"\n";
        out << controlMode << L" " << adultReligion << L" " << adultLanguage << L" " << totalBlockedCount << L"\n";
        out << cbPeriodicPopups << L" " << cb24HourLock << L" " << lock24hEndTime << L" " << cleanStreakDays << L"\n";
        out << isAdultFocusActive << L" " << focusEndTime << L"\n";
        out << customAdultKeywords.size() << L"\n";
        for (auto& k : customAdultKeywords) {
            out << k.name << L"\n";
        }
        out.close();
    }
}

void LoadAdultSettings() {
    if (adultWebsites.empty()) {
        adultWebsites = LoadAdultSitesFromResource();
        if (adultWebsites.empty()) {
            adultWebsites = { L"pornhub.com", L"xvideos.com", L"xnxx.com", L"xhamster.com", L"redtube.com" };
        }
    }

    wstring filePath = GetSaveFilePath();
    std::wifstream in(filePath.c_str());
    in.imbue(std::locale(in.getloc(), new std::codecvt_utf8<wchar_t>));
    if (in.is_open()) {
        in >> cbAdultWeb >> cbFbReels >> cbYtShorts >> cbHardcore >> cbRomantic;
        in >> controlMode >> adultReligion >> adultLanguage >> totalBlockedCount;
        in >> cbPeriodicPopups >> cb24HourLock >> lock24hEndTime >> cleanStreakDays;
        in >> isAdultFocusActive >> focusEndTime;

        if (cb24HourLock && GetTickCount64() >= lock24hEndTime) {
            cb24HourLock = false;
            isAdultFocusActive = false; 
        } else if (cb24HourLock) {
            isAdultFocusActive = true; 
        }

        if (isAdultFocusActive && controlMode == 0 && GetTickCount64() >= focusEndTime && !cb24HourLock) {
            isAdultFocusActive = false;
        }

        size_t kSize = 0;
        in >> kSize;
        in.ignore(); 
        customAdultKeywords.clear();
        for (size_t i = 0; i < kSize; i++) {
            std::wstring line;
            std::getline(in, line);
            if (!line.empty()) customAdultKeywords.push_back({ line });
        }
        in.close();
    }
}

// ==========================================
// --- QUOTES & KEYWORDS DATABASES ---
// ==========================================
struct Quote { wstring bn; wstring en; };

vector<Quote> muslimQuotes = {
    {L"“মুমিনদের বলুন, তারা যেন তাদের দৃষ্টি নত রাখে...”", L"Tell the believing men to reduce [some] of their vision..."},
    // ... (Add your other quotes here)
};

vector<Quote> hinduQuotes = {
    {L"“যে মনকে নিয়ন্ত্রণ করতে পারে না, তার মন তার সবচেয়ে বড় শত্রু।”", L"For him who has conquered the mind..."},
};

vector<Quote> christianQuotes = {
    {L"“খারাপ সাহচর্য ভালো চরিত্র নষ্ট করে।”", L"Bad company ruins good morals."},
};

vector<Quote> universalQuotes = {
    {L"“সফলতা আসে ফোকাস থেকে, ডিস্ট্রাকশন থেকে নয়।”", L"Success comes from focus, not from distraction."},
};

vector<wstring> hardcoreKeywords = { L"porn", L"xxx", L"sex", L"nude" /* Add rest here */ };
vector<wstring> romanticKeywords = { L"hot dance", L"seductive dance", L"kissing scene" /* Add rest here */ };

wstring toLowerW_Logic(wstring str) {
    for (auto& c : str) c = towlower(c); return str;
}

void closeActiveTab() {
    keybd_event(VK_CONTROL, 0, 0, 0); keybd_event('W', 0, 0, 0);
    keybd_event('W', 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

// ==========================================
// --- POPUP LOGIC ---
// ==========================================
void TriggerAdultPopup(bool isWarning = false, wstring customMsg = L"", bool isFullScreen = false) {
    if(!isFullScreen) {
        totalBlockedCount++; 
        cleanStreakDays = 0; 
        SaveAdultSettings(); 
    }
    
    wstring finalQuote = L"";

    if (isWarning) {
        finalQuote = customMsg;
    } else {
        int rIdx = 0;
        if (adultReligion == 0) { rIdx = rand() % muslimQuotes.size(); finalQuote = (adultLanguage == 0) ? muslimQuotes[rIdx].bn : muslimQuotes[rIdx].en; }
        else if (adultReligion == 1) { rIdx = rand() % hinduQuotes.size(); finalQuote = (adultLanguage == 0) ? hinduQuotes[rIdx].bn : hinduQuotes[rIdx].en; }
        else if (adultReligion == 2) { rIdx = rand() % christianQuotes.size(); finalQuote = (adultLanguage == 0) ? christianQuotes[rIdx].bn : christianQuotes[rIdx].en; }
        else { rIdx = rand() % universalQuotes.size(); finalQuote = (adultLanguage == 0) ? universalQuotes[rIdx].bn : universalQuotes[rIdx].en; }
    }
    
    // N.B: Since UI is in Qt now, you should ideally emit a signal to Qt to show the popup 
    // using HTML overlay or a Qt Dialog, instead of a raw Win32 Window.
    qDebug() << "Violation Blocked! Showing quote:" << QString::fromStdWString(finalQuote);
}

// ==========================================
// --- KEYLOGGER THREAD ---
// ==========================================
HHOOK hKeyboardHook = NULL;
string globalKeyBuffer = "";

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN && !isPanicActive) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = kbdStruct->vkCode;

        if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9') || vkCode == VK_SPACE || vkCode == VK_OEM_PERIOD) {
            char c = MapVirtualKey(vkCode, MAPVK_VK_TO_CHAR);
            if (vkCode == VK_OEM_PERIOD) c = '.';
            globalKeyBuffer += tolower(c);
            if (globalKeyBuffer.length() > 100) globalKeyBuffer.erase(0, 1);

            wstring wBuffer(globalKeyBuffer.begin(), globalKeyBuffer.end());
            bool shouldBlock = false;

            if (cbHardcore && !shouldBlock) { for (const auto& k : hardcoreKeywords) if (wBuffer.find(toLowerW_Logic(k)) != wstring::npos) shouldBlock = true; }
            if (cbRomantic && !shouldBlock) { for (const auto& k : romanticKeywords) if (wBuffer.find(toLowerW_Logic(k)) != wstring::npos) shouldBlock = true; }
            if (cbAdultWeb && !shouldBlock) { 
                for (const auto& w : adultWebsites) {
                    size_t dotPos = w.find(L".");
                    wstring coreName = (dotPos != wstring::npos) ? w.substr(0, dotPos) : w;
                    if (coreName.length() > 2 && wBuffer.find(coreName) != wstring::npos) { shouldBlock = true; break; }
                }
            }
            if (!customAdultKeywords.empty() && !shouldBlock) {
                for (const auto& item : customAdultKeywords) {
                    if (!item.name.empty() && wBuffer.find(toLowerW_Logic(item.name)) != wstring::npos) { shouldBlock = true; break; }
                }
            }

            if (shouldBlock) {
                globalKeyBuffer = ""; 
                closeActiveTab(); 
                TriggerAdultPopup(); 
            }
        } 
        else if (vkCode == VK_BACK) {
            if (!globalKeyBuffer.empty()) globalKeyBuffer.pop_back();
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

void StartKeyloggerThread() {
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, NULL, 0);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
}

// ==========================================
// --- SMART URL EXTRACTOR & DETECTION ---
// ==========================================
IUIAutomation* pAutomation = NULL;

wstring GetBrowserURL_Fallback(HWND hBrowser) {
    wstring url = L"";
    if (!pAutomation) return url;

    IUIAutomationElement* pElement = NULL;
    HRESULT hr = pAutomation->ElementFromHandle(hBrowser, &pElement);
    
    if (SUCCEEDED(hr) && pElement) {
        IUIAutomationCondition* pCondition = NULL;
        IUIAutomationElement* pEdit = NULL;

        VARIANT varProp;
        varProp.vt = VT_I4;
        varProp.lVal = UIA_EditControlTypeId;
        pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
        
        if (pCondition) {
            pElement->FindFirst(TreeScope_Descendants, pCondition, &pEdit);
            pCondition->Release();
        }

        if (pEdit) {
            VARIANT varValue;
            VariantInit(&varValue);
            hr = pEdit->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &varValue);
            if (SUCCEEDED(hr) && varValue.vt == VT_BSTR && varValue.bstrVal != NULL) {
                url = wstring(varValue.bstrVal);
            }
            VariantClear(&varValue);
            pEdit->Release();
        }
        pElement->Release();
    }
    return url;
}

bool IsFacebookReelsOrVideo(const wstring& url, bool checkReels, bool checkAllVideo) {
    wstring lowerUrl = toLowerW_Logic(url);
    if (checkReels) {
        if (lowerUrl.find(L"facebook.com/reel") != wstring::npos || 
            lowerUrl.find(L"facebook.com/reels") != wstring::npos ||
            lowerUrl.find(L"instagram.com/reels") != wstring::npos || 
            lowerUrl.find(L"youtube.com/shorts") != wstring::npos) {
            return true;
        }
    }
    if (checkAllVideo) {
        if (lowerUrl.find(L"facebook.com/watch") != wstring::npos || 
            lowerUrl.find(L"facebook.com/video") != wstring::npos ||
            lowerUrl.find(L"facebook.com/gaming") != wstring::npos) {
            return true;
        }
    }
    return false;
}

// ==========================================
// --- BACKGROUND MONITORING THREAD ---
// ==========================================
void AdultBackgroundThread() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
    wstring lastTitle = L"";

    lastPeriodicPopupTime = GetTickCount();

    while (true) {
        // 24 Hour Lock Check
        if (cb24HourLock) {
            if (GetTickCount64() >= lock24hEndTime) {
                cb24HourLock = false; 
                isAdultFocusActive = false;
                SaveAdultSettings();
            } else {
                isAdultFocusActive = true; 
            }
        }

        // Standard Focus Check
        if (isAdultFocusActive && controlMode == 0 && GetTickCount64() >= focusEndTime && !cb24HourLock) {
            isAdultFocusActive = false;
            SaveAdultSettings();
        }

        // Periodic Full Screen Popups
        if (cbPeriodicPopups && isAdultFocusActive) {
            if (GetTickCount() - lastPeriodicPopupTime >= 25 * 60 * 1000) {
                TriggerAdultPopup(false, L"", true); 
                lastPeriodicPopupTime = GetTickCount();
            }
        }

        if (!isPanicActive && (cbAdultWeb || cbHardcore || cbRomantic || cbFbReels || cbYtShorts || isAdultFocusActive)) {
            HWND hActive = GetForegroundWindow();
            if (hActive) {
                wchar_t t[256]; GetWindowTextW(hActive, t, 256); wstring title(t);
                
                if (!title.empty() && title != lastTitle) {
                    lastTitle = title; wstring lowerTitle = toLowerW_Logic(title);
                    bool shouldBlockTab = false;

                    if (isAdultFocusActive) {
                        if (lowerTitle.find(L"task manager") != wstring::npos || lowerTitle.find(L"taskmgr") != wstring::npos) {
                            PostMessage(hActive, WM_CLOSE, 0, 0);
                            TriggerAdultPopup(true, L"Security Alert: Task Manager is blocked!");
                            continue;
                        }
                    }

                    if (cbHardcore && !shouldBlockTab) { for (const auto& k : hardcoreKeywords) if (lowerTitle.find(toLowerW_Logic(k)) != wstring::npos) shouldBlockTab = true; }
                    if (cbRomantic && !shouldBlockTab) { for (const auto& k : romanticKeywords) if (lowerTitle.find(toLowerW_Logic(k)) != wstring::npos) shouldBlockTab = true; }
                    
                    if (!customAdultKeywords.empty() && !shouldBlockTab) {
                        for (const auto& item : customAdultKeywords) {
                            if (!item.name.empty() && lowerTitle.find(toLowerW_Logic(item.name)) != wstring::npos) { shouldBlockTab = true; break; }
                        }
                    }

                    if (shouldBlockTab) {
                        closeActiveTab(); 
                        TriggerAdultPopup(); 
                    } 
                    else if (lowerTitle.find(L"google chrome") != wstring::npos || lowerTitle.find(L"edge") != wstring::npos || lowerTitle.find(L"brave") != wstring::npos) {
                        
                        wstring url = GetBrowserURL_Fallback(hActive);
                        bool isUrlBlocked = false;
                        
                        if (cbAdultWeb) {
                            for (const auto& site : adultWebsites) {
                                if (url.find(site) != wstring::npos || lowerTitle.find(site) != wstring::npos) { isUrlBlocked = true; break; }
                            }
                        }

                        if (!isUrlBlocked && (cbFbReels || cbYtShorts)) {
                            if(IsFacebookReelsOrVideo(url, true, false)) isUrlBlocked = true;
                        }

                        if (isUrlBlocked) {
                            closeActiveTab(); Sleep(300); 
                            TriggerAdultPopup(); 
                            lastTitle = L""; 
                        }
                    }
                }
            }
        }
        Sleep(500); 
    }
}

// ==========================================
// --- INITIALIZATION ---
// ==========================================
static bool adultThreadStarted = false;

void InitAdultSystemOnBoot() {
    if (!adultThreadStarted) { 
        LoadAdultSettings(); 
        thread t(AdultBackgroundThread); t.detach(); 
        thread kl(StartKeyloggerThread); kl.detach(); 
        adultThreadStarted = true; 
    }
}

struct AdultAutoStarter {
    AdultAutoStarter() {
        thread t([]() {
            Sleep(1000); 
            InitAdultSystemOnBoot();
        });
        t.detach();
    }
} g_adultAutoStarter;
