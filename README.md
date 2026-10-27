# 校园自习室预约管理系统

当前交付为**成员 A 模块**。已实现登录、主窗口、20 个座位的查询网格、角色切换、公共演示常量和 VS 工程配置。界面使用 Qt 原生控件及布局。

## 环境

- C++17、Visual Studio 2022、MSVC v143、x64。
- 本机核实的 Qt 版本：**Qt 6.11.2 / MSVC2022 64bit**。
- 本机 Qt 目录：`E:\Qt\QT6.11\6.11.2\msvc2022_64`。
- Qt VS Tools / Qt MSBuild：3.5.0。
- Debug、Release 分开输出到 `bin/x64/Debug`、`bin/x64/Release`。
- 工程启用 `/utf-8`，包含目录使用 `$(ProjectDir)` 等相对宏，无额外数据库或图表依赖。

其他电脑需安装相同 Qt 版本，并在 VS 的 Qt Project Settings 中选择对应的 Qt Installation；工程使用名称 `6.11.2_msvc2022_64`。本机命令行构建通过 `QtInstallDir` 显式传入路径，不修改系统或其他成员的配置。

## 当前可运行内容

`tests/MemberATests.vcxproj` 是 A 的独立 Qt 验收工程。它链接 `tests/MemberATestSupport.cpp` 中的内存数据与规则替身，**不读写业务 JSON**。测试使用真实 Qt 类型、控件、信号和 A 的正式源码。

在 **VS2022 Developer PowerShell** 中进入项目目录，执行：

```powershell
msbuild .\tests\MemberATests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:QtInstallDir=E:\Qt\QT6.11\6.11.2\msvc2022_64
.\bin\x64\Debug\MemberATests.exe
```

仅预览 A 的登录与座位界面，使用同一个测试工程的预览配置：

```powershell
msbuild .\tests\MemberATests.vcxproj /p:MemberAPreview=true /p:Configuration=Release /p:Platform=x64 /p:QtInstallDir=E:\Qt\QT6.11\6.11.2\msvc2022_64
.\bin\x64\Release\MemberAPreview.exe
```

预览使用正式 `main.cpp` 和测试替身，初始没有预约；退出进程后测试数据丢弃。它用于查看 A 的界面，不是已集成的完整系统。

三个演示账号分别为 `admin`、`student01`、`student02`，密码均为 `123456`；角色直接取自固定用户对象，不提供角色选择框。

## 完整系统接入状态

`StudyRoomReservation.sln` / `StudyRoomReservation.vcxproj` 是正式系统工程。当前 B 的 `common/Rules.cpp` 和 D 的 `storage/JsonStore.cpp` 仍为空，完整系统尚不能链接运行。可先在解决方案中**单独生成 MemberATests 项目**；直接生成整个解决方案会遇到这些缺失实现。

- `common/Models.h` 按计划原文补入唯一模型定义，后续由 D 维护。
- `common/Rules.h`、`storage/JsonStore.h` 按计划补入接口声明，没有正式实现，没有另建业务保存通道。
- B、C、D 的功能页面仍为空，由各成员提供后接入；现有 C 的业务源码保持原样。
- 主窗口的 `addPage()`、`refreshRequested()`、`seatSelectionChanged()` 提供页面接入位置，详细说明见 [A 模块交接](docs/成员A交付说明.md)。

生成目录、个人 VS 配置、业务 JSON 不作为源码交接内容。当前未创建 Git 提交。
