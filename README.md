# RedAlert2‑YR Custom DLL Plugin
> Personal hobby project

## Feature Design
1. **Two‑side Synchronization**
All plugin features require both sides load this DLL. Single‑side usage is unsupported to prevent cheating.
2. **Multiplayer Network Sync**
All custom new operations send network packets for synchronization. Random values and game states are synced over network, avoid desync & crash caused by local random difference.


## References
- https://github.com/Phobos-developers/YRpp
- https://github.com/Ares-Developers/YRpp

## Credits
Special thanks to Phobos‑developers and Ares‑Developers for their great open‑source work on YR modding ecosystem.


# 红色警戒2 自定义DLL插件

本项目为**红色警戒2 尤里的复仇**自定义引擎扩展 DLL 插件，属于个人业余开发项目，仅用于技术学习与趣味拓展，非商用、非官方组件。

## 项目特点

### 1\. 全功能双方同步，无作弊漏洞

所有插件新增功能**强制联机双方同步加载**，不支持单方启用、单方生效。彻底杜绝单方面插件优势、作弊、权限不均等问题，保证联机公平性。

### 2\. 完整联机数据包同步机制

插件所有新增操作、自定义逻辑、随机运算均会**通过网络数据包同步至所有联机玩家**。统一全局随机状态、行为状态、游戏逻辑，彻底解决本地随机不同步导致的联机掉帧、错位、弹窗报错、对局崩溃等问题。

### 3\. 个人兴趣项目

本项目为个人的开源项目，不用于任何商业用途。

## 项目引用与参考

本项目基于 YRpp 引擎扩展框架进行二次开发，参考与引用以下开源仓库的代码结构、设计思路与底层实现：

- [Phobos\-developers/YRpp](https://github.com/Phobos-developers/YRpp)

- [Ares\-Developers/YRpp](https://github.com/Ares-Developers/YRpp)

## 特别感谢

由衷感谢 **Phobos\-developers**、**Ares\-Developers** 团队的开源贡献，为红警2尤里的复仇引擎扩展、MOD 开发、底层补丁开发生态提供了完整、成熟的技术基础与参考方案。

## 使用说明

- 联机对局**所有玩家必须使用同一版本 DLL**，否则会出现不同步、闪退、联机失败

- 仅适配 尤里的复仇\(YR\) 引擎，不兼容原版红警2 1\.006 无引擎扩展环境

- 禁止用于私服牟利

