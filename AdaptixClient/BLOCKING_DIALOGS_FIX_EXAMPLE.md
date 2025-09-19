# 阻塞性对话框修复示例

## 问题描述
客户端在调用文件对话框时会阻塞主线程，导致WebSocket连接断开。

## 解决方案
使用NonBlockingDialogs工具类替换所有阻塞性对话框调用。

## 修改示例

### 1. DialogAgent.cpp 修改示例

#### 原代码：
```cpp
void DialogAgent::onButtonGenerate()
{
    // ... 前面的代码 ...
    
    QString filePath = QFileDialog::getSaveFileName( nullptr, "Save File", filename, "All Files (*.*)" );
    if ( filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        MessageError("Failed to open file for writing");
        return;
    }

    file.write( content );
    file.close();
    
    // ... 后续代码 ...
}
```

#### 修改后：
```cpp
#include <Utils/NonBlockingDialogs.h>  // 添加头文件

void DialogAgent::onButtonGenerate()
{
    // ... 前面的代码 ...
    
    NonBlockingDialogs::getSaveFileName(this, "Save File", filename, "All Files (*.*)",
        [this, content](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(content);
            file.close();
            
            // ... 后续代码 ...
        });
}
```

### 2. DialogListener.cpp 修改示例

#### 原代码：
```cpp
void DialogListener::onButtonLoad()
{
    QString filePath = QFileDialog::getOpenFileName( nullptr, "Select file", QDir::homePath(), "JSON files (*.json)" );
    if ( filePath.isEmpty())
        return;

    // 处理文件加载逻辑
    QFile file(filePath);
    // ...
}
```

#### 修改后：
```cpp
#include <Utils/NonBlockingDialogs.h>  // 添加头文件

void DialogListener::onButtonLoad()
{
    NonBlockingDialogs::getOpenFileName(this, "Select file", QDir::homePath(), "JSON files (*.json)",
        [this](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            // 处理文件加载逻辑
            QFile file(filePath);
            // ...
        });
}
```

### 3. 其他需要修改的文件

以下文件都需要类似的修改：

#### 文件对话框调用：
- `DialogExtender.cpp` - getOpenFileName
- `BrowserFilesWidget.cpp` - getOpenFileName  
- `ScreenshotsWidget.cpp` - getSaveFileName
- `TargetsWidget.cpp` - getSaveFileName
- `CredentialsWidget.cpp` - getSaveFileName
- `DownloadsWidget.cpp` - getSaveFileName
- `CustomElements.cpp` - getOpenFileName
- `BridgeApp.cpp` - 所有文件对话框方法

#### 输入对话框调用：
- `CustomElements.cpp` - getInt
- `TargetsWidget.cpp` - getText
- `CredentialsWidget.cpp` - getText
- `ScreenshotsWidget.cpp` - getText
- `TunnelsWidget.cpp` - getText
- `SessionsTableWidget.cpp` - getText
- `TerminalWidget.cpp` - getInt
- `GraphScene.cpp` - getText

#### 消息对话框调用：
- `BridgeApp.cpp` - question, information
- `SessionsTableWidget.cpp` - question
- `GraphScene.cpp` - question
- `DialogDownloader.cpp` - critical
- `DialogUploader.cpp` - critical

## 修改步骤

1. 在每个需要修改的文件顶部添加：
   ```cpp
   #include <Utils/NonBlockingDialogs.h>
   ```

2. 将所有阻塞性对话框调用替换为NonBlockingDialogs的对应方法

3. 将原来的同步代码逻辑包装在回调函数中

4. 确保所有错误处理和后续逻辑都在回调函数内部

## 效果

修改完成后，客户端在打开文件对话框时将不再阻塞主线程，WebSocket连接将保持活跃，避免断线问题。