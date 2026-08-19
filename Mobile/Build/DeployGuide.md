# Mobile Deployment Guide

## Android

### Prerequisites
- Unreal Engine 5.3+ compiled from source
- Android Studio + SDK 34 + NDK r25b
- JDK 17
- 8+ GB RAM, 100+ GB disk

### Setup
1. `Edit → Project Settings → Platforms → Android`
2. Configure SDK paths (NDK, SDK, JDK)
3. Set Minimum SDK = 24 (Android 7.0)
4. Set Target SDK = 34
5. Enable **ARM64** only (disable ARMv7, x86, x86_64)
6. Enable **Vulkan** (disable OpenGL ES)
7. Package name: `com.yourstudio.stellarsystem`

### Build
```bash
# Linux/macOS
chmod +x Mobile/Build/PackageAndroid.sh
./Mobile/Build/PackageAndroid.sh Shipping StellarSystemAndroid

# Windows
Mobile\Build\PackageAndroid.bat Shipping StellarSystemAndroid
```

Output: `Binaries/Android/StellarSystem-Shipping-arm64.apk`

### Install on Device
```bash
adb install -r Binaries/Android/StellarSystem-Shipping-arm64.apk
adb shell am start -n com.yourstudio.stellarsystem/.StellarSystemActivity
adb logcat -s UE4
```

### Google Play Upload
- Generate AAB: add `-bundle` to RunUAT command
- Upload to Play Console → Production track
- Set age rating (PEGI 16 / ESRB T)
- Add data safety disclosure (network permissions)

---

## iOS

### Prerequisites
- **macOS** required
- Xcode 15+
- Apple Developer Account ($99/year)
- Unreal Engine 5.3+ for Mac

### Setup
1. `Edit → Project Settings → Platforms → iOS`
2. Set Bundle Identifier: `com.yourstudio.stellarsystem`
3. Import Distribution Certificate
4. Import Provisioning Profile
5. Enable **Metal** (disable OpenGL ES)
6. Minimum iOS = 15.0
7. Target iOS = 17.x

### Build
```bash
chmod +x Mobile/Build/PackageIOS.sh
./Mobile/Build/PackageIOS.sh Shipping StellarSystemIOS
```

Output: `Binaries/IOS/StellarSystem-Shipping.ipa`

### TestFlight
1. Open Xcode → Window → Organizer
2. Import the .ipa
3. Distribute → TestFlight → Upload
4. Add internal testers (team) or external testers (up to 10,000)
5. Test on device via TestFlight app

### App Store
1. Prepare screenshots (6.7", 6.5", 5.5" sizes)
2. Write description, keywords, privacy manifest
3. Submit for review (average 24-48h)
4. **Important**: Add Network Security Config (NSAllowsArbitraryLoads=false)

---

## Network Permissions

### Android (`AndroidManifest.xml` — auto-generated)
```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.WAKE_LOCK" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
```

### iOS (`Info.plist` — auto-generated)
```xml
<key>NSAppTransportSecurity</key>
<dict>
  <key>NSAllowsArbitraryLoads</key>
  <false/>
</dict>
<key>UIBackgroundModes</key>
<array>
  <string>voip</string>
  <string>remote-notification</string>
</array>
```

---

## Performance Checklist

- [ ] Tested on Low-end (Snapdragon 6 series / Dimensity 700)
- [ ] Tested on Mid-range (Snapdragon 7 series / iPhone 13)
- [ ] Tested on High-end (Snapdragon 8 Gen 3 / iPhone 15 Pro)
- [ ] 30+ min play session without thermal throttling
- [ ] Memory stable (<1.5GB on low-end)
- [ ] Battery drain <15%/hour during active play
- [ ] Background → Foreground reconnect <3 seconds
- [ ] Data saver mode <100KB/min
- [ ] No frame drops below 25fps on target devices

---

## Known Limitations

- **No Steam/WeGame on mobile** — account system uses server-auth instead
- **Reduced physics** — sub-stepping at 20-30Hz (vs 60Hz on PC)
- **No mod support** — mobile sandbox prevents Lua loading
- **Smaller worlds** — planet view distance reduced 50-70%
- **Simplified shaders** — no ray tracing, simplified PBR
- **Touch only** — no mouse/keyboard support on mobile builds
