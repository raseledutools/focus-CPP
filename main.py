import webview
import os
import ctypes
import threading
import time
import sys
import pynput # কীবোর্ড মনিটর করার জন্য (pip install pynput)

# ================= রিসোর্স পাথ হ্যান্ডেলার (Nuitka/PyInstaller এর জন্য) =================
def get_resource_path(relative_path):
    """ গেট অ্যাবসলুট পাথ টু রিসোর্স, যা ডেভলপমেন্ট এবং .exe মোড উভয় ক্ষেত্রেই কাজ করবে """
    if hasattr(sys, '_MEIPASS'):
        # PyInstaller মোড
        return os.path.join(sys._MEIPASS, relative_path)
    
    # Nuitka বা সাধারণ রান মোড
    base_path = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base_path, relative_path)

# ================= গ্লোবাল স্টেট =================
is_strict_session = False
is_halal_guard_on = True
bad_words = ["PORN", "XXX", "SEX", "XVIDEOS", "XNXX", "BRAZZERS"]
current_typed = ""

# অ্যাডমিন চেক (Hosts ফাইল এবং Taskmgr কিল করার জন্য জরুরি)
def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

# ================= লজিক ফাংশনসমূহ =================

class Api:
    def startPlan(self, mode):
        global is_strict_session
        is_strict_session = True
        
        if mode == "hybrid":
            self.apply_hybrid_block()
        elif mode == "internet":
            os.system("ipconfig /release")
        
        return f"SUCCESS: {mode.capitalize()} mode activated and locked."

    def apply_hybrid_block(self):
        hosts_path = r"C:\Windows\System32\drivers\etc\hosts"
        redirect = "127.0.0.1"
        # আপনার লিস্ট অনুযায়ী সাইটগুলো
        sites = ["facebook.com", "www.facebook.com", "youtube.com", "instagram.com", "tiktok.com"]
        
        try:
            with open(hosts_path, "a") as f:
                f.write("\n# RasBlocker Hybrid Active\n")
                for site in sites:
                    f.write(f"{redirect} {site}\n")
            os.system("ipconfig /flushdns")
        except Exception as e:
            print(f"Error: {e}")

    def saveProtection(self):
        return "Uninstall Protection Active (Registry Locked)"

# ================= ব্যাকগ্রাউন্ড প্রোটেকশন থ্রেড =================

def protection_loop():
    """টাস্ক ম্যানেজার বন্ধ করা এবং সেশন লক রাখা"""
    while True:
        if is_strict_session:
            # Task Manager কিল করা
            os.system("taskkill /F /IM Taskmgr.exe >nul 2>&1")
        time.sleep(1)

# ================= Halal Guard (Keyboard Monitor) =================

def on_press(key):
    global current_typed, is_halal_guard_on
    if not is_halal_guard_on: return

    try:
        if hasattr(key, 'char') and key.char:
            current_typed += key.char.upper()
            
            # এডাল্ট কি-ওয়ার্ড চেক
            for word in bad_words:
                if word in current_typed:
                    trigger_halal_screen()
                    current_typed = ""
                    break
        
        # বাফার বেশি বড় হলে ছোট করা
        if len(current_typed) > 20:
            current_typed = current_typed[-10:]
            
    except:
        current_typed = ""

def trigger_halal_screen():
    """ব্ল্যাক স্ক্রিন এবং হাদিস ইনজেক্ট করা"""
    hadith_js = """
    document.body.innerHTML = `
    <div style="background:black; color:white; position:fixed; inset:0; z-index:99999; display:flex; flex-direction:column; align-items:center; justify-content:center; text-align:center; padding:50px; font-family: sans-serif;">
        <h1 style="color:#10B981; font-size:60px; margin-bottom:20px;">Halal Guard Protection</h1>
        <p style="font-size:28px; line-height:1.6; max-width: 800px;">
            "মু'মিন পুরুষদেরকে বলুন, তারা যেন তাদের দৃষ্টি সংযত রাখে এবং তাদের লজ্জাস্থানের হিফাযত করে।"
            <br><br><span style="color:#10B981;">- সূরা আন-নূর [৩০]</span>
        </p>
        <p style="margin-top:40px; color:red; font-weight:bold;">UNSAFE KEYWORD DETECTED. RETURN TO WORK.</p>
    </div>`;
    """
    if 'window' in globals():
        window.evaluate_js(hadith_js)
        window.maximize()

# ================= মেইন রানার =================

if __name__ == '__main__':
    if not is_admin():
        # অ্যাডমিন না হলে মেসেজ বক্স দেখাবে
        ctypes.windll.user32.MessageBoxW(0, "RasBlocker Pro needs Admin rights to block websites and protect sessions.", "Run as Admin", 0x10)
        sys.exit()

    # ১. ব্যাকগ্রাউন্ড প্রোটেকশন চালু
    threading.Thread(target=protection_loop, daemon=True).start()

    # ২. কীবোর্ড লিসেনার চালু (Halal Guard)
    listener = pynput.keyboard.Listener(on_press=on_press)
    listener.start()

    # ৩. উইন্ডো তৈরি
    api = Api()
    # Nuitka/EXE এর জন্য index.html এর সঠিক পাথ বের করা
    html_file_path = get_resource_path('index.html')
    
    window = webview.create_window(
        'RasBlocker Pro', 
        html_file_path, 
        js_api=api, 
        width=1050, 
        height=700,
        resizable=False
    )

    # ৪. অ্যাপ স্টার্ট
    webview.start()
    
    # অ্যাপ বন্ধ হলে ইন্টারনেট রিস্টোর করা
    os.system("ipconfig /renew")
