# StellarSystem 服务器部署指南

## 快速开始（5 分钟）

### 方案 A：本地测试（同一台电脑）

```bash
# 终端 1：启动服务器
cd Server/Build/Packaged
./RunServer.sh -port=7777 -maxplayers=32 -log

# 终端 2：启动客户端（或编辑器 Play）
# 在游戏中 ConnectToServer("127.0.0.1", 7777)
```

### 方案 B：云服务器（推荐）

**推荐云厂商：**
- AWS EC2（c6i.2xlarge 起步）
- Google Cloud（n2-standard-8）
- 阿里云 ECS（计算型 c7）
- DigitalOcean（CPU-Optimized）

**推荐配置：**
- 8 vCPU / 16 GB RAM / 100 GB SSD / Ubuntu 22.04
- 带宽：1 Gbps 端口（实际用量 50-100 Mbps）

---

## 详细部署步骤（Ubuntu 22.04）

### Step 1：准备系统

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y wget curl unzip libvulkan1 \
    libxcb-keysyms1 libxcb-icccm4 libxcb-image0 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 \
    libxcb-sync1 libxcb-xfixes0 libxcb-xkb1 \
    libxkbcommon0 libxkbcommon-x11-0 libfontconfig1 \
    ca-certificates htop iotop
```

### Step 2：上传服务器包

```bash
# 本地
scp -r Server/Build/Packaged/ user@server-ip:/opt/stellarsystem/

# 服务器
ssh user@server-ip
cd /opt/stellarsystem
chmod +x StellarSystemServer RunServer.sh
```

### Step 3：配置

```bash
nano Config/Server.ini
```

关键参数：
```ini
ServerName="My Awesome Server"
MOTD="Welcome! PvP enabled. Have fun!"
MaxPlayers=32
Port=7777
EnableAntiCheat=true
RequiredClientVersion="6.7.0"
NetworkTickRate=30
RelevancyDistance=50000
AutoSaveInterval=300
```

### Step 4：防火墙

```bash
# UFW
sudo ufw allow 7777/udp
sudo ufw allow 7777/tcp
sudo ufw allow 27015/udp  # Steam

# 或 iptables
sudo iptables -A INPUT -p udp --dport 7777 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 7777 -j ACCEPT
```

### Step 5：启动

```bash
# 前台（调试用）
./RunServer.sh -log

# 后台（生产用）
nohup ./RunServer.sh -log > server.log 2>&1 &
echo $! > server.pid

# 查看日志
tail -f server.log
```

### Step 6：监控

```bash
# 内存/CPU
htop

# 网络
sudo iotop -a
watch -n 1 "cat /proc/$(pgrep StellarSystemServer)/status | grep -E 'VmRSS|VmSize'"

# 日志过滤
grep "Players=" server.log | tail -20
grep "WARNING\|ERROR" server.log
```

---

## systemd 服务（推荐生产环境）

创建 `/etc/systemd/system/stellarsystem.service`：

```ini
[Unit]
Description=StellarSystem Dedicated Server
After=network.target

[Service]
Type=simple
User=stellarsystem
Group=stellarsystem
WorkingDirectory=/opt/stellarsystem
ExecStart=/opt/stellarsystem/RunServer.sh -log
Restart=on-failure
RestartSec=10
KillSignal=SIGTERM
TimeoutStopSec=30

# 安全加固
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=/opt/stellarsystem/Saved

# 资源限制
LimitNOFILE=65535
LimitNPROC=4096
MemoryMax=12G
CPUQuota=800%

[Install]
WantedBy=multi-user.target
```

启用：
```bash
sudo useradd -r -s /bin/false stellarsystem
sudo chown -R stellarsystem:stellarsystem /opt/stellarsystem
sudo systemctl daemon-reload
sudo systemctl enable stellarsystem
sudo systemctl start stellarsystem

# 查看状态
sudo systemctl status stellarsystem
sudo journalctl -u stellarsystem -f
```

---

## Docker 部署

### Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libvulkan1 libxcb-keysyms1 libxcb-icccm4 \
    libxcb-image0 libxcb-randr0 libxcb-render-util0 \
    libxcb-shape0 libxcb-sync1 libxcb-xfixes0 \
    libxcb-xkb1 libxkbcommon0 libxkbcommon-x11-0 \
    libfontconfig1 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

COPY Packaged/ /app/
WORKDIR /app
RUN chmod +x StellarSystemServer RunServer.sh

EXPOSE 7777/udp 7777/tcp 27015/udp

# 健康检查
HEALTHCHECK --interval=30s --timeout=5s --start-period=20s \
    CMD pgrep StellarSystemServer || exit 1

CMD ["./RunServer.sh", "-log", "-port=7777", "-maxplayers=32"]
```

### docker-compose.yml

```yaml
version: '3.8'

services:
  stellarsystem:
    build: .
    container_name: stellarsystem
    restart: unless-stopped
    ports:
      - "7777:7777/udp"
      - "7777:7777/tcp"
      - "27015:27015/udp"
    volumes:
      - ./Saved:/app/Saved
      - ./Config:/app/Config
    deploy:
      resources:
        limits:
          memory: 12G
          cpus: '8.0'
    healthcheck:
      test: ["CMD", "pgrep", "StellarSystemServer"]
      interval: 30s
      timeout: 5s
      retries: 3
```

启动：
```bash
docker compose up -d
docker compose logs -f
```

---

## 多服务器集群

### 架构

```
                    ┌─────────────────┐
                    │   Load Balancer  │
                    │  (Steam DZ / 自研)│
                    └────────┬────────┘
                             │
            ┌────────────────┼────────────────┐
            ▼                ▼                ▼
    ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
    │  Server #1    │ │  Server #2   │ │  Server #N   │
    │  Port 7777    │ │  Port 7778   │ │  Port 7779   │
    │  32 players   │ │  32 players  │ │  32 players  │
    └──────┬───────┘ └──────┬───────┘ └──────┬───────┘
           │                 │                 │
           └────────────────┼────────────────┘
                            ▼
                  ┌─────────────────┐
                  │  Shared Database  │
                  │  (PostgreSQL +    │
                  │   Redis Cache)    │
                  └─────────────────┘
```

### 共享数据库（玩家数据跨服同步）

```sql
-- PostgreSQL schema
CREATE TABLE players (
    steam_id BIGINT PRIMARY KEY,
    player_name VARCHAR(64),
    credits BIGINT DEFAULT 0,
    reputation JSONB DEFAULT '{}',
    inventory JSONB DEFAULT '{}',
    loadout JSONB DEFAULT '{}',
    last_seen TIMESTAMP DEFAULT NOW(),
    banned BOOLEAN DEFAULT FALSE
);

CREATE TABLE server_heartbeats (
    server_id VARCHAR(64) PRIMARY KEY,
    server_name VARCHAR(128),
    current_players INT,
    max_players INT,
    last_heartbeat TIMESTAMP DEFAULT NOW()
);

CREATE INDEX idx_players_last_seen ON players(last_seen);
CREATE INDEX idx_heartbeats_time ON server_heartbeats(last_heartbeat);
```

---

## 监控与告警

### Prometheus + Grafana

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'stellarsystem'
    metrics_path: '/metrics'
    static_configs:
      - targets:
        - 'server1:9090'
        - 'server2:9090'
        - 'server3:9090'
```

关键监控指标：
- `stellar_players_current` — 当前玩家数
- `stellar_cpu_usage_percent` — CPU 使用率
- `stellar_memory_mb` — 内存占用
- `stellar_net_bandwidth_in_kbs` — 入站带宽
- `stellar_net_bandwidth_out_kbs` — 出站带宽
- `stellar_fps` — 服务器帧率
- `stellar_anticheat_violations` — 反作弊触发数

### 告警规则

```yaml
groups:
  - name: stellarsystem
    rules:
      - alert: ServerDown
        expr: stellar_up == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Server {{ $labels.instance }} is down"

      - alert: HighCPU
        expr: stellar_cpu_usage_percent > 90
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Server CPU > 90% for 5min"

      - alert: MemoryCritical
        expr: stellar_memory_mb > 12000
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "Server memory > 12GB"

      - alert: LowFPS
        expr: stellar_fps < 20
        for: 3m
        labels:
          severity: warning
        annotations:
          summary: "Server FPS < 20"
```

---

## 备份策略

### 自动备份脚本

```bash
#!/bin/bash
# backup.sh — 每天凌晨 3 点执行

BACKUP_DIR="/backup/stellarsystem"
DATE=$(date +%Y%m%d_%H%M%S)
SERVER_DIR="/opt/stellarsystem"

# 1. 通知服务器准备备份
# (通过 RCON 发送消息)

# 2. 强制存档
# (通过 RCON 发送 SaveNow)

# 3. 等待存档完成
sleep 5

# 4. 打包 Saved 目录
tar -czf "${BACKUP_DIR}/saved_${DATE}.tar.gz" \
    "${SERVER_DIR}/Saved/"

# 5. 打包 Config
tar -czf "${BACKUP_DIR}/config_${DATE}.tar.gz" \
    "${SERVER_DIR}/Config/"

# 6. 上传到 S3 / OSS
aws s3 cp "${BACKUP_DIR}/saved_${DATE}.tar.gz" \
    s3://my-backup-bucket/stellarsystem/

# 7. 清理旧备份（保留 30 天）
find "${BACKUP_DIR}" -name "*.tar.gz" -mtime +30 -delete

echo "Backup completed: ${DATE}"
```

Crontab：
```bash
0 3 * * * /opt/stellarsystem/backup.sh >> /var/log/stellarsystem_backup.log 2>&1
```

---

## 安全加固

### 1. SSH 加固

```bash
# /etc/ssh/sshd_config
Port 2222                    # 改端口
PermitRootLogin no            # 禁止 root 登录
PasswordAuthentication no     # 仅密钥登录
MaxAuthTries 3               # 限制尝试次数
ClientAliveInterval 300       # 超时断开
```

### 2. 防火墙最小化

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 2222/tcp       # SSH
sudo ufw allow 7777/udp       # Game
sudo ufw allow 7777/tcp       # Game (TCP fallback)
sudo ufw allow 27015/udp      # Steam
sudo ufw enable
```

### 3. Fail2Ban

```bash
sudo apt install -y fail2ban
sudo nano /etc/fail2ban/jail.local

[sshd]
enabled = true
port = 2222
maxretry = 3
bantime = 3600
```

### 4. 游戏服务器沙箱

```bash
# 使用 systemd 沙箱（见上方 service 文件）
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true

# 或 Docker（天然隔离）
# 或 Firejail
firejail --private=/opt/stellarsystem \
    --netfilter \
    --caps.drop=all \
    /opt/stellarsystem/RunServer.sh
```

---

## 性能调优清单

- [ ] `NetworkTickRate=30`（默认）→ 低带宽可降到 20
- [ ] `RelevancyDistance=50000` → 大地图可降到 30000
- [ ] `MaxPlayers=32` → 根据 CPU 核心数调整（每核 4-8 人）
- [ ] 启用 `Iris` 网络复制系统（UE5.3+）
- [ ] 配置 `ReplicationGraph`（大型场景必备）
- [ ] 服务器内核调优（见下方）
- [ ] 禁用不必要的日志级别（Shipping 模式自动处理）

### 内核参数调优

```bash
# /etc/sysctl.conf
# 增加文件描述符限制
fs.file-max = 655350

# 网络优化
net.core.somaxconn = 4096
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_fin_timeout = 15
net.ipv4.tcp_keepalive_time = 60
net.ipv4.tcp_keepalive_intvl = 10
net.ipv4.tcp_keepalive_probes = 6

# UDP 缓冲区（对游戏服务器至关重要）
net.core.rmem_max = 268435456
net.core.wmem_max = 268435456
net.core.rmem_default = 16777216
net.core.wmem_default = 16777216

# 应用
sudo sysctl -p
```

---

## 故障排查

| 症状 | 排查命令 |
|---|---|
| 服务器启动即退出 | `./StellarSystemServer -log` 看报错 |
| 玩家连不上 | `sudo netstat -tulpn \| grep 7777` |
| 高延迟 | `mtr player-ip` / `ping player-ip` |
| 内存泄漏 | `watch -n 5 'ps aux \| grep Stellar'` |
| CPU 100% | `top -H -p $(pgrep Stellar)` 看线程 |
| 磁盘满 | `df -h` + `du -sh /opt/stellarsystem/Saved/` |
| 端口被占 | `lsof -i :7777` |
| 权限错误 | `sudo -u stellarsystem ./RunServer.sh` |

---

## 回滚方案

```bash
# 保留最近 3 个版本
/opt/
├── stellarsystem/          → 当前版本（symlink）
├── stellarsystem_v6.7.0/   → 版本备份
├── stellarsystem_v6.6.0/   → 版本备份
└── stellarsystem_v6.5.0/   → 版本备份

# 回滚
sudo systemctl stop stellarsystem
sudo rm /opt/stellarsystem
sudo ln -s /opt/stellarsystem_v6.6.0 /opt/stellarsystem
sudo systemctl start stellarsystem
```
