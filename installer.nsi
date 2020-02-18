Name "Sweet Tea"

InstallDir $PROGRAMFILES\Sweet-Tea
DirText "Choose install directory."

Section

SetOutPath $INSTDIR
File Sweet-Tea.exe
File libcrypto-1_1-x64.dll
File libssl-1_1-x64.dll

createDirectory "$SMPROGRAMS\Thunderspy Gaming"
createShortCut "$SMPROGRAMS\Thunderspy Gaming\Sweet Tea.lnk" "$INSTDIR\Sweet-Tea.exe"

WriteRegStr HKCU "Software\Thunderspy Gaming\Sweet Tea" "manifests" "https://www.thunderspygaming.net/styles/freedom/manifest.xml https://join.cityofheroesrebirth.com/Manifests/Rebirth.xml http://slanter.sytes.net/ultramanifest.xml"

SectionEnd