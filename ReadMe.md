# 概述
`WeComPC` 是一款基于WeChatPc修改的 企业微信多开工具，WeChatPC原项目地址：https://github.com/cheungxiongwei/WeChatPC.git。

本文主要使用代码实现多开，简单修改可破除类似原理的网络游戏限制多开。
### 使用说明
运行 `WeCom.exe` 即可。

### 主要特征
* 支持无限多开。
* 一键启动企业微信。

### 下载地址
https://github.com/zhongliangliu/WeComPC/releases/download/v0.1/weComPC.exe

### 源代码编译
`git clone https://github.com/cheungxiongwei/WeChatPC.git`

打开 `WeChatPC` 目录。

使用 VS2010 或更高版本 打开 `WeChatPC.sln` 项目文件，正常编译即可。

> 注意：一定要选择 x86 进行编译。

### 主要原理
企业微信使用互斥体作为限制，正常情况下使其仅能开启一个客户端。
通过使用 `火绒剑工具` 查看目标进程句柄列表，可发现`红色标记框`中的2个句柄关闭后，即可继续开启微信。

![](./WeComPC-master/images/2023-12-28_000706.png)

类似的网络游戏使用此原理限制多开，比如 `天龙八部` 、`笑傲江湖OL` 等。

### 主要流程
1. 使用 `ZwQuerySystemInformation` 函数枚举系统进程信息，找到 `WXWork.exe` 进程，获取其进程 id。
2. 使用 `ZwQuerySystemInformation` 函数枚举全局句柄列表（系统上所有的句柄列表），通过比较句柄拥有者进程 id，获取 `WXWork.exe` 目标句柄。
3. 使用 `DuplicateHandle` 跨进程复制句柄，第一次复制仅查询比较 `类型` 、`名称`，第二次复制关闭找到的目标句柄。

![](./images/Snipaste_2020-04-26_10-45-44.jpg)

### 声明
本项目仅供技术研究，请勿用于任何商业用途，请勿用于非法用途，如有任何人凭此做何非法事情，均于作者无关，特此声明
