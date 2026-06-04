# AdaptixC2 v1.2

FEB, 28: [What has changed in version v1.2](https://adaptix-framework.gitbook.io/adaptix-framework/changelog-and-updates/v1.1-greater-than-v1.2)?

Adaptix is an extensible post-exploitation and adversarial emulation framework made for authorized penetration testing. The Adaptix server is written in Golang and to allow operator flexibility. The GUI Client is written in C++ QT, allowing it to be used on Linux, Windows, and MacOS operating systems. [Full documentation is available here](https://adaptix-framework.gitbook.io/adaptix-framework).

![](https://adaptix-framework.gitbook.io/adaptix-framework/~gitbook/image?url=https%3A%2F%2F2104178602-files.gitbook.io%2F%7E%2Ffiles%2Fv0%2Fb%2Fgitbook-x-prod.appspot.com%2Fo%2Fspaces%252FS8p8XLFtLmf0NkofQvoa%252Fuploads%252FB6UKkj5WzVJ4Gty9dKYD%252FScreenshot_20251226_235357.png%3Falt%3Dmedia%26token%3Ddd04b937-3f02-43b2-a7bd-4dd8c876d763&width=768&dpr=4&quality=100&sign=7ee59d4&sv=2)



## Legal Warning

This tool is designed for AUTHORIZED security testing and red team operations ONLY. Unauthorized use is strictly prohibited and may violate local and international laws. Use at your own risk.


## Getting Started

Please checkout the [wiki](https://adaptix-framework.gitbook.io/adaptix-framework/adaptix-c2/getting-starting/installation).



## Features
* Server/Client Architecture for Multiplayer Support 
* Cross-platform GUI client 
* Fully encrypted communications 
* Listener and Agents as Plugin (Extender)
* AxScript Engine
* Task and Jobs storage 
* Credentials Manager
* Targets Manager
* Remote Terminal / Shell
* Files and Process browsers
* Socks4 / Socks5 / Socks5 Auth support
* Local and Reverse port forwarding support
* BOF & Async BOF support
* Linking Agents and Sessions Graph
* Agents Health Checker
* Agents KillDate and WorkingTime control
* Windows/Linux/MacOs agents support


## Current Extenders
* HTTP/S Beacon Listener
* DNS/DoH Beacon Listener
* SMB Beacon Listener
* TCP Beacon Listener
* Beacon Agent
* TCP/mTLS Gopher Listener
* Gopher Agent



## Extension-Kit

Official [Extension-Kit](https://github.com/Adaptix-Framework/Extension-Kit) on GitHub

![](https://adaptix-framework.gitbook.io/adaptix-framework/~gitbook/image?url=https%3A%2F%2F2104178602-files.gitbook.io%2F%7E%2Ffiles%2Fv0%2Fb%2Fgitbook-x-prod.appspot.com%2Fo%2Fspaces%252FS8p8XLFtLmf0NkofQvoa%252Fuploads%252FyRPFnlkvGJr2UEE1uzA2%252FScreenshot_20260129_233929.png%3Falt%3Dmedia%26token%3D61c90128-20f4-4756-80f4-24e7122c7c10&width=768&dpr=3&quality=100&sign=f15a166a&sv=2)

![](https://adaptix-framework.gitbook.io/adaptix-framework/~gitbook/image?url=https%3A%2F%2F2104178602-files.gitbook.io%2F%7E%2Ffiles%2Fv0%2Fb%2Fgitbook-x-prod.appspot.com%2Fo%2Fspaces%252FS8p8XLFtLmf0NkofQvoa%252Fuploads%252FnAVr0nfGpuQkiSYSPQvU%252FScreenshot_20260129_233944.png%3Falt%3Dmedia%26token%3Dd98dfda1-1607-45ba-9a92-deb420293335&width=768&dpr=3&quality=100&sign=7c1d6ea&sv=2)

![](https://adaptix-framework.gitbook.io/adaptix-framework/~gitbook/image?url=https%3A%2F%2F2104178602-files.gitbook.io%2F%7E%2Ffiles%2Fv0%2Fb%2Fgitbook-x-prod.appspot.com%2Fo%2Fspaces%252FS8p8XLFtLmf0NkofQvoa%252Fuploads%252Fxxn9BnUfG0byuRamOy4y%252FScreenshot_20260129_233957.png%3Falt%3Dmedia%26token%3D053c7d47-39af-433b-a2d2-87a1b8dec7bb&width=768&dpr=3&quality=100&sign=f1581010&sv=2)

## For macOS Users (Client Build) - Very Important

After installation using the **setup-macos.sh** script, ensure the following requirements are met before building the client.

### Supported Platforms

* macOS Intel (`x86_64`)
* macOS Apple Silicon (`arm64` / M1, M2, M3, M4)

### Required Dependencies

Install the required packages using Homebrew:

```bash
brew install \
    cmake \
    openssl \
    qtbase \
    qtdeclarative \
    qtsvg \
    qtwebsockets
```

### Verify Qt Components

Ensure the following Qt6 configuration files exist:

```bash
find /opt/homebrew /usr/local -name Qt6Config.cmake 2>/dev/null
find /opt/homebrew /usr/local -name Qt6SvgConfig.cmake 2>/dev/null
find /opt/homebrew /usr/local -name Qt6WebSocketsConfig.cmake 2>/dev/null
find /opt/homebrew /usr/local -name Qt6QmlConfig.cmake 2>/dev/null
```

Apple Silicon users should typically see paths under:

```text
/opt/homebrew/Cellar/
```

Intel users should typically see paths under:

```text
/usr/local/Cellar/
```

### Configure the Client

From the repository root:

```bash
cd AdaptixClient
```

Configure CMake using the detected Qt paths:

```bash
cmake . \
  -DQt6_DIR="$(dirname $(find $(brew --prefix) -name Qt6Config.cmake | head -1))" \
  -DQt6Svg_DIR="$(dirname $(find $(brew --prefix) -name Qt6SvgConfig.cmake | head -1))" \
  -DQt6WebSockets_DIR="$(dirname $(find $(brew --prefix) -name Qt6WebSocketsConfig.cmake | head -1))" \
  -DQt6Qml_DIR="$(dirname $(find $(brew --prefix) -name Qt6QmlConfig.cmake | head -1))"
```

### Build

```bash
make -j$(sysctl -n hw.ncpu)
```

### Runtime Fix (Qt Cocoa Plugin)

If the client launches with the error:

```text
qt.qpa.plugin: Could not find the Qt platform plugin "cocoa"
```

set the Qt plugin paths before running:

```bash
export QT_PLUGIN_PATH="$(find $(brew --prefix) -path '*share/qt/plugins' | grep qtbase | head -1)"
export QT_QPA_PLATFORM_PLUGIN_PATH="$QT_PLUGIN_PATH/platforms"
```

Then launch the client:

```bash
./AdaptixClient
```

### Qt 5 Conflict

If Homebrew reports linking conflicts with `qt@5`, unlink it temporarily:

```bash
brew unlink qt@5
```

This project requires **Qt 6**. Having Qt 5 linked may prevent Qt 6 modules such as `QtSvg`, `QtWebSockets`, or `QtQml` from being discovered correctly.

### Troubleshooting

Enable Qt plugin debugging:

```bash
export QT_DEBUG_PLUGINS=1
./AdaptixClient
```

Inspect linked Qt libraries:

```bash
otool -L ./AdaptixClient | grep Qt
```

If configuration fails, verify that the required Qt modules are installed:

```bash
brew list | grep qt
```

Expected output should include:

```text
qtbase
qtdeclarative
qtsvg
qtwebsockets
```



# CONTRIBUTING

Please push сhanges to the **dev** branch. Otherwise, changes will be made manually in the dev branch.
