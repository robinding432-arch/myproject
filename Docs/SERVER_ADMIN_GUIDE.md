# StellarSystem 服务器管理指南

## 控制台命令速查

### 玩家管理

| 命令 | 参数 | 说明 |
|---|---|---|
| `ListPlayers` | 无 | 列出所有在线玩家（名称/IP/Ping/Trust） |
| `KickPlayer` | `<Name/ID>` `<Reason>` | 踢出指定玩家 |
| `BanPlayer` | `<Name/ID>` `[HWID]` | 封禁玩家（可选硬件封禁） |
| `UnbanPlayer` | `<Name/ID>` | 解除封禁 |
| `MutePlayer` | `<Name/ID>` | 禁言 |
| `UnmutePlayer` | `<Name/ID>` | 解除禁言 |
| `TeleportPlayer` | `<Name>` `<X>` `<Y>` `<Z>` | 传送玩家到坐标 |
| `SetPlayerHealth` | `<Name>` `<Value>` | 设置玩家血量（GM 用） |
| `GiveItem` | `<Name>` `<ItemID>` `<Qty>` | 给玩家物品（GM 用） |

### 服务器控制

| 命令 | 参数 | 说明 |
|---|---|---|
| `SaveNow` | 无 | 立即存档所有玩家数据 |
| `ShutdownServer` | `[DelaySeconds]` | 延迟关闭（默认 10 秒） |
| `CancelShutdown` | 无 | 取消计划中的关闭 |
| `RestartServer` | `[DelaySeconds]` | 重启服务器 |
| `SetMaxPlayers` | `<Number>` | 设置最大玩家数（1-128） |
| `SetPassword` | `<Password>` | 设置服务器密码（空=无密码） |
| `ServerSay` | `<Message>` | 发送服务器公告 |
| `BroadcastMessage` | `<Message>` | 同 ServerSay |

### 网络调优

| 命令 | 参数 | 说明 |
|---|---|---|
| `SetNetworkTickRate` | `<Hz>` | 设置网络频率（1-60） |
| `SetRelevancyDistance` | `<cm>` | 设置相关性距离 |
| `ShowNetStats` | 无 | 显示网络统计 |
| `KickLaggers` | `<PingThreshold>` | 踢出 Ping 超标的玩家 |

### 游戏设置

| 命令 | 参数 | 说明 |
|---|---|---|
| `EnablePvP` | 无 | 开启 PvP |
| `DisablePvP` | 无 | 关闭 PvP |
| `SetGalaxySeed` | `<Seed>` | 设置星系种子（重启生效） |
| `SpawnSolarSystem` | 无 | 重新生成星系 |
| `WeatherControl` | `<Type>` `<Intensity>` | 手动控制天气 |
| `GiveCredits` | `<Name>` `<Amount>` | 给玩家货币（GM） |

### 诊断

| 命令 | 参数 | 说明 |
|---|---|---|
| `ShowServerStatus` | 无 | 显示完整服务器状态（JSON） |
| `ShowPerformance` | 无 | 显示性能诊断 |
| `ShowPlayerList` | 无 | 同 ListPlayers |
| `DumpMemory` | 无 | 输出内存快照到文件 |
| `ToggleDebugLogging` | 无 | 切换详细日志 |
| `ExportStats` | `<FilePath>` | 导出统计数据到 JSON |

---

## RCON 远程管理

### 启用 RCON

编辑 `Config/Server.ini`：

```ini
[/Script/StellarSystem.StellarDedicatedServer]
; RCON 设置
RCONEnabled=true
RCONPort=27020
RCONPassword=YourSecurePassword123!
```

重启服务器生效。

### 连接 RCON

**使用 rcon-cli（推荐）：**

```bash
# 安装
npm install -g rcon-cli

# 连接
rcon -h server-ip -p 27020 -P "YourSecurePassword123!"

# 发送命令
rcon> ListPlayers
rcon> ServerSay Hello from RCON!
rcon> ShowServerStatus
```

**使用 Python：**

```python
import socket
import struct

def rcon_command(host, port, password, command):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    # 认证
    auth = struct.pack('<ii', len(password)+10, 3) + \
           b'\x00\x00\x00\x00' + password.encode() + b'\x00\x00'
    s.send(auth)

    # 发送命令
    cmd = struct.pack('<ii', len(command)+10, 2) + \
          b'\x00\x00\x00\x00' + command.encode() + b'\x00\x00'
    s.send(cmd)

    # 接收响应
    response = s.recv(4096)
    s.close()
    return response

# 使用
print(rcon_command("server-ip", 27020, "YourSecurePassword123!", "ListPlayers"))
```

**使用 Web 面板：**

```javascript
// Node.js WebSocket RCON
const net = require('net');

function rcon(host, port, password, cmd) {
    return new Promise((resolve) => {
        const socket = net.connect(port, host);
        let buffer = '';

        socket.on('data', (data) => {
            buffer += data.toString();
        });

        socket.on('connect', () => {
            // Auth
            socket.write(Buffer.from([0,0,0,0,3,0,0,0]));
            socket.write(password);
            // Command
            socket.write(Buffer.from([0,0,0,0,2,0,0,0]));
            socket.write(cmd);
        });

        setTimeout(() => {
            socket.end();
            resolve(buffer);
        }, 2000);
    });
}
```

---

## 日志管理

### 日志位置

| 日志 | 路径 |
|---|---|
| 服务器主日志 | `Saved/Logs/StellarSystem.log` |
| 崩溃转储 | `Saved/Crashes/` |
| 性能分析 | `Saved/Profiling/` |
| 反作弊日志 | `Saved/AntiCheat.log` |
| 封禁列表 | `Saved/BannedPlayers.txt` |
| 聊天记录 | `Saved/ChatLogs/` |

### 日志轮转

```bash
# /etc/logrotate.d/stellarsystem
/opt/stellarsystem/Saved/Logs/*.log {
    daily
    missingok
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 stellarsystem adm
    sharedscripts
    postrotate
        systemctl kill -s USR1 stellarsystem
    endscript
}
```

### 实时日志分析

```bash
# 玩家连接/断开
tail -f Saved/Logs/StellarSystem.log | grep --line-buffered "Player"

# 反作弊触发
tail -f Saved/Logs/StellarSystem.log | grep --line-buffered "AntiCheat"

# 错误/警告
tail -f Saved/Logs/StellarSystem.log | grep --line-buffered "ERROR\|WARNING"

# 性能
tail -f Saved/Logs/StellarSystem.log | grep --line-buffered "\[Stats\]"
```

---

## 自动化脚本

### 定时重启（防内存泄漏）

```bash
#!/bin/bash
# daily_restart.sh — 每天凌晨 4 点重启

SERVER_DIR="/opt/stellarsystem"
PID=$(pgrep StellarSystemServer)

if [ -n "$PID" ]; then
    echo "Sending shutdown command..."
    # 通过 RCON 发送
    rcon -h 127.0.0.1 -p 27020 -P "$RCON_PASS" "ShutdownServer 30"
    
    # 等待关闭
    sleep 35
fi

# 启动
cd "$SERVER_DIR"
nohup ./RunServer.sh -log > server.log 2>&1 &
echo "Server restarted at $(date)"
```

Crontab：
```bash
0 4 * * * /opt/stellarsystem/daily_restart.sh >> /var/log/stellarsystem_restart.log 2>&1
```

### 自动备份 + 上传

```bash
#!/bin/bash
# backup_and_upload.sh

DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/backup/stellarsystem"
SERVER_DIR="/opt/stellarsystem"

# 1. 触发存档
rcon -h 127.0.0.1 -p 27020 -P "$RCON_PASS" "SaveNow"
sleep 5

# 2. 打包
tar -czf "${BACKUP_DIR}/backup_${DATE}.tar.gz" \
    "${SERVER_DIR}/Saved/" \
    "${SERVER_DIR}/Config/"

# 3. 上传到 S3
aws s3 cp "${BACKUP_DIR}/backup_${DATE}.tar.gz" \
    "s3://my-stellarsystem-backups/"

# 4. 清理旧备份（保留 7 天）
find "${BACKUP_DIR}" -name "*.tar.gz" -mtime +7 -delete

echo "Backup completed: ${DATE}"
```

### 监控 + 自动恢复

```bash
#!/bin/bash
# watchdog.sh — 每 30 秒检查服务器状态

SERVER_DIR="/opt/stellarsystem"
CHECK_URL="http://127.0.0.1:9090/health"  # 如果有 HTTP 健康检查

while true; do
    # 检查进程
    if ! pgrep -x "StellarSystemServer" > /dev/null; then
        echo "[$(date)] Server down! Restarting..."
        
        # 检查崩溃日志
        LAST_CRASH=$(ls -t ${SERVER_DIR}/Saved/Crashes/ 2>/dev/null | head -1)
        if [ -n "$LAST_CRASH" ]; then
            echo "Last crash: $LAST_CRASH"
            # 上传崩溃报告
            # aws s3 cp "${SERVER_DIR}/Saved/Crashes/${LAST_CRASH}" s3://crashes/
        fi
        
        # 重启
        cd "$SERVER_DIR"
        nohup ./RunServer.sh -log > server.log 2>&1 &
        
        # 通知管理员（示例：发邮件/Discord Webhook）
        curl -X POST -H "Content-Type: application/json" \
            -d "{\"content\": \"⚠️ Server crashed and restarted at $(date)\"}" \
            "$DISCORD_WEBHOOK_URL"
    fi
    
    sleep 30
done
```

启动：
```bash
nohup ./watchdog.sh > watchdog.log 2>&1 &
```

---

## 玩家管理 Web 面板（简单版）

```python
#!/usr/bin/env python3
"""
StellarSystem Admin Panel — Flask 版
运行：pip install flask && python admin_panel.py
"""
from flask import Flask, render_template, request, redirect, url_for
import socket, struct, json, os

app = Flask(__name__)

RCON_HOST = "127.0.0.1"
RCON_PORT = 27020
RCON_PASS = os.environ.get("RCON_PASS", "changeme")

def rcon(cmd):
    """发送 RCON 命令"""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3)
    s.connect((RCON_HOST, RCON_PORT))
    
    # Auth
    auth = struct.pack('<ii', 10 + len(RCON_PASS), 3) + \
           b'\x00\x00\x00\x00' + RCON_PASS.encode() + b'\x00\x00'
    s.send(auth)
    s.recv(1024)
    
    # Command
    body = cmd.encode()
    packet = struct.pack('<ii', 10 + len(body), 2) + \
             b'\x00\x00\x00\x00' + body + b'\x00\x00'
    s.send(packet)
    resp = s.recv(4096)
    s.close()
    return resp.decode('utf-8', errors='replace')

@app.route('/')
def dashboard():
    status = rcon("ShowServerStatus")
    return f"""
    <html><body style="font-family:monospace;background:#1a1a2e;color:#eee;padding:20px">
    <h1>🌌 StellarSystem Admin</h1>
    <pre>{status}</pre>
    <form action="/say" method="post">
        <input name="msg" placeholder="Announcement..." style="width:400px">
        <button>Send</button>
    </form>
    <form action="/kick" method="post">
        <input name="name" placeholder="Player name">
        <input name="reason" placeholder="Reason">
        <button>Kick</button>
    </form>
    <a href="/players">Players</a> |
    <a href="/restart">Restart Server</a> |
    <a href="/logs">Logs</a>
    </body></html>
    """

@app.route('/say', methods=['POST'])
def say():
    rcon(f"ServerSay {request.form['msg']}")
    return redirect(url_for('dashboard'))

@app.route('/kick', methods=['POST'])
def kick():
    rcon(f"KickPlayer {request.form['name']} {request.form['reason']}")
    return redirect(url_for('dashboard'))

@app.route('/players')
def players():
    return f"<pre>{rcon('ListPlayers')}</pre>"

@app.route('/restart')
def restart():
    rcon("ShutdownServer 10")
    return "Restarting in 10 seconds..."

@app.route('/logs')
def logs():
    with open('/opt/stellarsystem/server.log', 'r') as f:
        lines = f.readlines()[-100:]
    return f"<pre>{''.join(lines)}</pre>"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080, debug=False)
```

---

## 安全清单

- [ ] 修改默认 RCON 密码（强密码 16+ 字符）
- [ ] RCON 端口不对外暴露（仅 localhost / VPN）
- [ ] SSH 使用密钥登录 + 改端口
- [ ] 防火墙只开放必要端口
- [ ] Fail2Ban 启用
- [ ] 定期更新系统（`apt upgrade`）
- [ ] 定期备份存档
- [ ] 监控异常登录
- [ ] 封禁列表定期审查
- [ ] 服务器以非 root 用户运行
