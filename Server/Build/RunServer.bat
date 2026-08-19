@echo off
REM ============================================================
REM  StellarSystem Dedicated Server - Windows 启动脚本
REM ============================================================
REM  使用方法：
REM    1. 将 StellarSystemServer.exe 和此脚本放在同一目录
REM    2. 双击运行 或 命令行带参数运行
REM
REM  常用参数：
REM    -log       输出日志到控制台
REM    -port=7777 指定端口
REM    -maxplayers=32 最大玩家数
REM    -servername="My Server" 服务器名称
REM    -nosplash  无启动画面
REM    -nullrhi   无渲染（Headless，推荐服务器使用）
REM
REM  示例：
REM    RunServer.bat -port=7777 -maxplayers=64 -log -nullrhi
REM ============================================================

setlocal

REM ---- 配置（可修改）----
set SERVER_EXE=StellarSystemServer.exe
set PORT=7777
set MAXPLAYERS=32
set SERVERNAME="StellarSystem Dedicated Server"
set GAME=StellarSystemMap?Game=/Script/StellarSystem.StellarDedicatedServer

REM ---- 启动参数 ----
set ARGS=%*
if "%ARGS%"=="" set ARGS=-port=%PORT% -maxplayers=%MAXPLAYERS% -servername=%SERVERNAME% -log -nosplash -nullrhi

echo.
echo  ========================================
echo   StellarSystem Dedicated Server v6.7
echo  ========================================
echo.
echo   Command: %SERVER_EXE% %ARGS%
echo.

REM ---- 启动服务器 ----
start "" "%SERVER_EXE%" %ARGS% %GAME%

REM ---- 等待退出 ----
timeout /t 2 /nobreak >nul
echo Server process started.
echo.
echo Tips:
echo   - 关闭服务器：在服务器控制台输入 'quit' 或按 Ctrl+C
echo   - 查看玩家：输入 'ListPlayers'
echo   - 发送公告：输入 'ServerSay 你的消息'
echo   - 保存存档：输入 'SaveNow'
echo   - 定时关闭：输入 'ShutdownServer 30'
echo.

endlocal
