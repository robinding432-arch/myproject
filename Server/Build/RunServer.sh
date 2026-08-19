#!/bin/bash
# ============================================================
#  StellarSystem Dedicated Server - Linux 启动脚本
# ============================================================
#  使用方法：
#    chmod +x RunServer.sh
#    ./RunServer.sh
#
#  带参数：
#    ./RunServer.sh -port=7777 -maxplayers=64 -log
#
#  后台运行：
#    nohup ./RunServer.sh -log > server.log 2>&1 &
#
#  停止：
#    kill $(pgrep StellarSystemServer)
#
#  常用参数：
#    -port=7777          端口
#    -maxplayers=32      最大玩家
#    -servername="xxx"    服务器名
#    -nullrhi            无渲染（必须！）
#    -nosplash           无启动画面
#    -log                输出日志
#    -unattended         无人值守模式
# ============================================================

# ---- 配置 ----
SERVER_EXE="./StellarSystemServer"
PORT=7777
MAXPLAYERS=32
SERVERNAME="StellarSystem Dedicated Server"
GAME_MAP="StellarSystemMap?Game=/Script/StellarSystem.StellarDedicatedServer"

# ---- 默认参数 ----
DEFAULT_ARGS="-port=${PORT} -maxplayers=${MAXPLAYERS} -servername=\"${SERVERNAME}\" -log -nosplash -nullrhi -unattended"

# ---- 合并用户参数 ----
if [ $# -eq 0 ]; then
    ARGS="${DEFAULT_ARGS}"
else
    ARGS="$@"
fi

echo ""
echo "  ========================================"
echo "    StellarSystem Dedicated Server v6.7"
echo "  ========================================"
echo ""
echo "  Executable: ${SERVER_EXE}"
echo "  Args: ${ARGS}"
echo "  Map: ${GAME_MAP}"
echo ""

# ---- 检查文件存在 ----
if [ ! -f "${SERVER_EXE}" ]; then
    echo "  ERROR: ${SERVER_EXE} not found!"
    echo "  Please build with: ./build.sh"
    exit 1
fi

# ---- 启动服务器 ----
echo "  Starting server..."
echo ""

${SERVER_EXE} ${ARGS} ${GAME_MAP}

# ---- 退出处理 ----
EXIT_CODE=$?
echo ""
echo "  Server exited with code: ${EXIT_CODE}"
echo ""

exit ${EXIT_CODE}
