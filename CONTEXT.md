# BeamMP-Launcher-Offline — Context / 项目记忆

> 项目完整背景请读主仓库的 [CONTEXT.md](https://github.com/dksslq/BeamMP-Offline/blob/main/CONTEXT.md)，
> 本文件是本仓库的简要记忆 + 合并手册。

## 本仓库是什么

BeamMP 官方启动器（BeamMP-Launcher/BeamMP-Launcher）的**纯离线版**分支：
- 不访问 auth.beammp.com / backend.beammp.com / forum.beammp.com。
- 登录 = 玩家在游戏内自选昵称，存在 Launcher 目录的 `player_name` 文件。
- 与离线服务器握手时，发送昵称（≤32 字节）替代原来的"公钥"。
- 客户端 mod（BeamMP.zip）不再从 backend 下载，而是从本地安装：
  1. 游戏 `mods/multiplayer/BeamMP.zip` 已存在 → 直接用；
  2. Launcher 同目录的 `BeamMP.zip` → 自动复制到游戏目录；
  3. 都没有 → 报错提示从主仓库 Release 下载。
- 服务器列表请求返回空数组，游戏内 UI 引导玩家使用 Direct Connect（手动输 IP）。

## 本仓库改动清单（合并上游时必须保护）

| 文件 | 离线改动 |
|---|---|
| `src/Security/Login.cpp` | 整文件重写：离线昵称登录 + 本地 `player_name` 持久化 |
| `src/Startup.cpp` `CheckForUpdates()` | 空操作 |
| `src/Startup.cpp` `PreGame()` | mod 本地安装逻辑（见下），删除 backend 下载；Launcher 同目录 zip 为权威副本，与游戏目录不一致时自动覆盖安装（一键修复被官方 Launcher 覆盖的 mod） |
| `.github/workflows/release.yml` | **新增**：tag（v*）触发，构建 Windows exe 并附到 Release |
| `src/Network/Core.cpp` case 'B' | 服务器列表：本地 `servers.json`（LAN/私有，排前）+ **只读**拉取 `https://backend.beammp.com/servers-info`（3s 连接/8s 总超时，离线/失败静默降级）合并返回；无任何认证。**v1.0.1 起** |
| `src/Network/Core.cpp` `IsAllowedLink()` | github 白名单放宽为任意仓库（UI 链接指向 dksslq/BeamMP-Offline） |
| `src/Network/Http.cpp` `GetWithTimeout()` | **新增**：带连接/总超时的只读 GET（每次新建 CURL 句柄，不污染 Get/Post 的 thread_local 句柄） |
| `src/Network/Resources.cpp` `Auth()` | 发送 `Username` 而非 `PublicKey`。**v1.0.2 起**：`Username` 为空（未命名/guest）时回退发送字面量 `"Guest"`，**永不发送 0 字节名字包** —— v1.0.1 及更早的离线服务端会把空载荷误判为"认证阶段连接断开"并踢出（"Connection closed during authentication"）。已知取舍：未命名玩家以普通名字 `Guest` 进入，不再触发服务端 `AllowGuests=false` 拦截（离线版该设置仅对仍发送空包的旧客户端有效） |
| `src/Network/Http.cpp` `StartProxy()` | 空操作（`ProxyPort=0`） |
| `src/main.cpp` | 报错提示指向离线仓库 |

## 上游合并

```bash
git remote add upstream https://github.com/BeamMP/BeamMP-Launcher.git  # 一次即可
git fetch upstream
git merge upstream/master     # 上游默认分支是 master
```

冲突原则：
1. 上游新功能/修复（尤其是协议、压缩、平台兼容性修复）全部接纳；
2. 上表离线语义必须保留；上游重写同区域时重新套用离线逻辑；
3. 合并后自检：`grep -rn "HTTP::Get\|HTTP::Post\|HTTP::Download\|beammp.com" src/`
   —— 除 `Http.cpp` 中未被调用的函数定义和注释外应为空；CI 绿灯。

## mod 安装优先级（语义要点）

1. Launcher 同目录 `BeamMP.zip` = 权威离线副本（与游戏目录 zip 大小不同 → 覆盖安装）；
2. 游戏目录已有 zip → 直接用；
3. 都没有 → 报错并提示从主仓库 Release 下载。
**警告**：不要对本游戏目录运行官方 Launcher —— 它会用官方 backend 版覆盖 BeamMP.zip。
我们的 Launcher 永不联网，离线 mod 不会被静默"更新"回官方版。

## 构建

`.github/workflows/{cmake-linux,cmake-windows}.yml`（上游自带）负责构建。
注意：`src/*.yml`（linux.yml 等）是上游误放目录的历史遗留，不在 `.github/workflows/`
下，不会被执行，保持原样即可。
