# 网络功能状态报告

## 当前已有的网络相关功能

### SAL-BOF (Situational Awareness & Lateral) - 网络信息收集
1. **netstat** - 显示网络连接、路由表和网络接口统计
2. **ipconfig** - 显示 IP 配置信息
3. **arp** - 显示和修改 ARP 缓存
4. **routeprint** - 显示路由表
5. **listdns** - 列出 DNS 服务器
6. **nslookup** - DNS 查询工具

### 隧道和代理功能
1. **SOCKS5 隧道** - 通过 beacon 建立 SOCKS5 代理
2. **WebSocket 隧道** - HTTP beacon 的 WebSocket 隧道（新开发）
3. **端口转发** - Link/Unlink 功能

### 横向移动功能 (LateralMovement-BOF)
1. **psexec** - SMB 远程执行
2. **WinRM** - Windows 远程管理

## ❌ 缺失的功能

### 端口扫描
**状态**: 未找到
- 搜索关键词: `portscan`, `port-scan`, `smartportscan`, `SYN scan`
- 结果: 项目中不存在端口扫描功能

## 建议的实现方案

### 方案 1: BOF 端口扫描模块 (推荐)
创建一个新的 BOF 模块 `Extension-Kit/SAL-BOF/portscan/`

**特性**:
- TCP Connect 扫描 (最隐蔽)
- SYN 扫描 (需要 RAW socket 权限)
- 多线程扫描
- 智能速率控制
- 支持单主机或 CIDR 范围
- 常见端口预设

**实现文件**:
```
SAL-BOF/portscan/
├── portscan.c          # 主扫描逻辑
├── tcp_connect.c       # TCP connect 扫描
├── syn_scan.c          # SYN 扫描 (可选)
└── common_ports.h      # 常见端口定义
```

**集成到 sal.axs**:
```javascript
Command("portscan", {
    cmd: {
        description: "TCP 端口扫描",
        args: {
            target: "目标 IP 或 CIDR",
            ports: "端口范围 (如: 80,443,1-1024)",
            threads: "扫描线程数 (默认: 10)",
            timeout: "超时时间/ms (默认: 1000)"
        },
        code: "portscan.x64.o"
    }
});
```

### 方案 2: 内置 C++ 端口扫描
在 beacon agent 中添加内置命令

**位置**: `Extenders/agent_beacon/src_beacon/beacon/Scanner.cpp`

**优点**:
- 更好的性能
- 更多控制选项
- 可以集成到现有的网络功能

**缺点**:
- 增加 beacon 体积
- 修改核心代码

### 方案 3: 利用现有工具
通过 `execute-assembly` 或 BOF 调用现有的扫描工具:
- Nmap (通过 execute-assembly)
- Masscan (需要编译为 BOF)
- 自定义轻量级扫描器

## 推荐实现: SmartPortScan BOF

### 功能规格
1. **扫描模式**:
   - Quick Scan (常见端口: 21,22,23,25,53,80,110,139,143,443,445,3389,etc.)
   - Full Scan (1-65535)
   - Custom Scan (指定端口列表)

2. **扫描技术**:
   - TCP Connect (默认，最隐蔽)
   - TCP SYN (可选，需要权限)
   - Banner Grabbing (服务识别)

3. **智能特性**:
   - 自适应速率控制
   - 结果过滤 (只显示开放端口)
   - 服务版本检测
   - 输出格式化

4. **使用示例**:
```bash
# 快速扫描常见端口
portscan 192.168.1.100 quick

# 扫描指定端口
portscan 192.168.1.0/24 80,443,8080

# 全端口扫描
portscan 10.0.0.5 full --threads 50

# 带服务检测
portscan 192.168.1.100 1-1000 --banner
```

## 下一步行动

1. ✅ 确认需求: 需要哪种扫描方式？
2. ⏳ 创建 BOF 模块骨架
3. ⏳ 实现 TCP Connect 扫描
4. ⏳ 添加多线程支持
5. ⏳ 集成到 sal.axs
6. ⏳ 测试和优化

---

**注意**: 端口扫描可能触发 IDS/IPS 告警，建议:
- 使用慢速扫描模式
- 分批次扫描
- 通过 SOCKS 隧道进行扫描以隐藏来源

