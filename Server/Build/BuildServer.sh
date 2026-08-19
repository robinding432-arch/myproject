#!/bin/bash
# ============================================================
#  StellarSystem - 服务器构建脚本 (Linux)
# ============================================================
#  功能：
#    1. 生成项目文件
#    2. 编译 Dedicated Server (Linux)
#    3. 打包 Server 目录
#    4. 输出可直接部署的包
#
#  使用方法：
#    chmod +x BuildServer.sh
#    ./BuildServer.sh
#
#  前置条件：
#    - UE 5.3+ 已安装到 /home/ue4/UnrealEngine/
#    - 已安装: clang, lld, cmake, make
#    - 至少 8GB RAM + 50GB 磁盘
# ============================================================

set -e  # 出错即停

# ---- 配置 ----
UE_ROOT="${UE_ROOT:-/home/ue4/UnrealEngine}"
PROJECT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
PROJECT_NAME="StellarSystem"
TARGET_NAME="StellarSystemServer"
CONFIG="Shipping"
PLATFORM="Linux"

echo ""
echo "  ╔══════════════════════════════════════╗"
echo "  ║   StellarSystem Server Build Script    ║"
echo "  ╚══════════════════════════════════════╝"
echo ""
echo "  UE Root:     ${UE_ROOT}"
echo "  Project:     ${PROJECT_NAME}"
echo "  Target:      ${TARGET_NAME}"
echo "  Platform:    ${PLATFORM}"
echo "  Config:      ${CONFIG}"
echo "  Project Dir: ${PROJECT_DIR}"
echo ""

# ---- 检查 UE 安装 ----
if [ ! -f "${UE_ROOT}/Engine/Build/BatchFiles/Linux/Build.sh" ]; then
    echo "  ❌ UE Engine not found at: ${UE_ROOT}"
    echo "  Set UE_ROOT env var to your UE installation path"
    exit 1
fi

# ---- Step 1: 生成项目文件 ----
echo "  [1/4] Generating project files..."
cd "${UE_ROOT}"
./Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh \
    -project="${PROJECT_DIR}/${PROJECT_NAME}.uproject" \
    -game
echo "  ✅ Project files generated"
echo ""

# ---- Step 2: 编译 Server Target ----
echo "  [2/4] Building Dedicated Server (this may take 10-30 min)..."
cd "${UE_ROOT}/Engine/Build/BatchFiles/Linux"
./Build.sh ${TARGET_NAME} ${PLATFORM} ${CONFIG} \
    -project="${PROJECT_DIR}/${PROJECT_NAME}.uproject" \
    -progress -threads=$(nproc)
echo "  ✅ Server built successfully"
echo ""

# ---- Step 3:  Cook 服务器内容 ----
echo "  [3/4] Cooking server content..."
cd "${UE_ROOT}/Engine/Binaries/Linux"
./UE4Editor "${PROJECT_DIR}/${PROJECT_NAME}.uproject" \
    -run=cook \
    -targetplatform=${PLATFORM} \
    -server \
    -unversioned \
    -buildmachine \
    -stdout
echo "  ✅ Content cooked"
echo ""

# ---- Step 4: 打包 ----
echo "  [4/4] Packaging server..."
OUTPUT_DIR="${PROJECT_DIR}/Server/Build/Packaged"
mkdir -p "${OUTPUT_DIR}"

# 复制可执行文件
cp "${PROJECT_DIR}/Binaries/Linux/${PROJECT_NAME}Server" \
   "${OUTPUT_DIR}/${PROJECT_NAME}Server"

# 复制脚本
cp "${PROJECT_DIR}/Server/Build/RunServer.sh" "${OUTPUT_DIR}/"
cp "${PROJECT_DIR}/Server/Build/RunServer.bat" "${OUTPUT_DIR}/"

# 复制配置
mkdir -p "${OUTPUT_DIR}/Config"
cp "${PROJECT_DIR}/Config/Server.ini" "${OUTPUT_DIR}/Config/"
cp "${PROJECT_DIR}/Config/DefaultEngine.ini" "${OUTPUT_DIR}/Config/"
cp "${PROJECT_DIR}/Config/DefaultGame.ini" "${OUTPUT_DIR}/Config/"

# 复制 Cooked 内容
mkdir -p "${OUTPUT_DIR}/Content"
cp -r "${PROJECT_DIR}/Content/"* "${OUTPUT_DIR}/Content/" 2>/dev/null || true

# 复制 Saved（存档目录结构）
mkdir -p "${OUTPUT_DIR}/Saved"

# 创建启动说明
cat > "${OUTPUT_DIR}/README.txt" << 'EOF'
============================================
  StellarSystem Dedicated Server v6.7
============================================

部署要求：
  - Linux: Ubuntu 20.04+ / CentOS 8+
  - 4+ CPU cores
  - 8GB+ RAM
  - 2GB+ disk space

快速启动：
  chmod +x RunServer.sh
  ./RunServer.sh

带参数启动：
  ./RunServer.sh -port=7777 -maxplayers=64 -log

后台运行：
  nohup ./RunServer.sh -log > server.log 2>&1 &

停止服务器：
  kill $(pgrep StellarSystemServer)

查看日志：
  tail -f server.log

配置文件：
  Config/Server.ini
============================================
EOF

echo "  ✅ Server packaged to: ${OUTPUT_DIR}"
echo ""
echo "  ╔══════════════════════════════════════╗"
echo "  ║  Build Complete!                      ║"
echo "  ║  Output: ${OUTPUT_DIR}"
echo "  ╚══════════════════════════════════════╝"
echo ""
echo "  File list:"
ls -lh "${OUTPUT_DIR}/"
echo ""
echo "  Next steps:"
echo "    1. 上传 ${OUTPUT_DIR}/ 到你的服务器"
echo "    2. chmod +x RunServer.sh"
echo "    3. ./RunServer.sh -log"
echo ""
