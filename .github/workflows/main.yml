name: Build RasBlocker Pro

on:
  push:
    branches: [ "main" ]
  workflow_dispatch: 

jobs:
  build:
    runs-on: windows-latest

    steps:
    - name: Checkout Repository
      uses: actions/checkout@v4

    - name: Download webview.h (Single Header Version)
      run: curl -L -o webview.h https://raw.githubusercontent.com/webview/webview/0.10.0/webview.h

    - name: Setup Official MSYS2 (C++ Compiler)
      uses: msys2/setup-msys2@v2  # একদম অফিসিয়াল এবং লেটেস্ট টুল
      with:
        msystem: MINGW64
        install: mingw-w64-x86_64-gcc

    - name: Compile C++ Code
      shell: msys2 {0} # MSYS2 এনভায়রনমেন্টে রান করার জন্য
      run: |
        g++ main.cpp -o RasBlockerPro.exe -static -static-libgcc -static-libstdc++ -mwindows -ladvapi32 -lole32 -lshell32 -lshlwapi -luser32 -lversion -loleaut32

    - name: Package App into Zip
      shell: powershell # জিপ করার জন্য উইন্ডোজের পাওয়ারশেল ব্যবহার
      run: |
        Compress-Archive -Path "RasBlockerPro.exe", "index.html" -DestinationPath "RasBlockerPro_App.zip"

    - name: Upload Final Artifact
      uses: actions/upload-artifact@v4
      with:
        name: RasBlockerPro-Release
        path: RasBlockerPro_App.zip
