# 成员 A 交付说明

## 范围与完成内容

按照项目计划，A 负责 `SeatManager`、登录窗口、主窗口、座位网格、演示账号与座位常量、工程配置。当前其他成员的公共实现及页面尚不完整，A 不代写预约、签到/违约页面、统计和 JSON 保存功能。

| 文件 | 内容 |
| --- | --- |
| `common/DemoData.h/.cpp` | 固定管理员和两个学生、20 个座位、4×5 网格尺寸、08:00—23:00、30 分钟粒度 |
| `managers/SeatManager.h/.cpp` | 读取同一个 `JsonStore`，返回固定座位、当前状态、所选时段占用情况 |
| `ui/LoginDialog.h/.cpp/.ui` | 密码隐藏、固定账号验证、错误提示、返回完整 `User` |
| `ui/MainWindow.h/.cpp/.ui` | 登录身份、座位页面、退出登录、集中刷新及页面接入接口 |
| `ui/SeatPage.h/.cpp/.ui` | 当天半小时时段选择、20 个座位、状态文字和座位选择信号 |
| `main.cpp` | 唯一 `JsonStore`、启动加载、登录与退出登录循环 |
| `StudyRoomReservation.sln/.vcxproj/.vcxproj.filters` | VS2022、v143、C++17、x64、Debug/Release、Qt Widgets、UTF-8 |
| `.gitignore` | 排除生成文件、个人配置和业务数据 |
| `common/Models.h` | 复制计划中已经约定的唯一模型，D 后续维护 |
| `common/Rules.h`、`storage/JsonStore.h` | 按既定接口补入声明，等待 B、D 实现 |
| `tests/MemberATests.cpp` | A 的固定时间业务与 Qt 界面测试 |
| `tests/MemberATestSupport.cpp` | 仅测试工程链接的公共依赖替身，无文件保存 |
| `tests/MemberATests.vcxproj` | 独立验收和界面预览构建 |

新增文件为 `docs/成员A交付说明.md` 与 `tests/` 下上述三个文件，其余为填充原空骨架。其他成员已有业务代码未修改，外部项目计划文件未修改。

## 座位与时间语义

`listSeats()` 始终返回 1—20 号座位，名称为 01号—20号。

`getSeatStates(start, end, now)` 使用公共 `toMinute()`、`isHalfHourAligned()`、`overlaps()`、`stateOf()`；不自行维护另一份时间或状态规则。无效时间、非半小时对齐、反向/相等区间、跨日、非当天或超出开放时间时返回空集合。

当前状态与所选时段分别计算：覆盖当前分钟的有效预约未签到为“已预约”，签到后为“使用中”；未来预约不占用当前座位。所选时段只检查该座位的 `Active` 预约重叠；已取消和已终止记录不阻塞。自然到时根据状态函数显示空闲，不写记录、不登记违约。

查询接口允许查看当天已过去时段的历史占用，它返回的 `availableForSelectedPeriod` 只表达计划第 6.3 节的座位冲突结果。页面对过去开始时间显示提示，不将其标为可预约。创建预约的时间与学生自身冲突必须由 B 的业务接口重新核验。

时段选择起点为08:00—22:30，终点仅提供晚于起点且不超过23:00的半小时项。跨日刷新会切换到新的一天，并重新选择起止时间。

## 登录与刷新

- 登录从 `DemoData::users()` 获取完整 `User`，账号密码匹配后返回，用户不能自行选择角色。
- 点击“退出登录”后，正式入口销毁本次主窗口，返回新的登录窗口；同一个已加载的 `JsonStore` 保留至程序退出。
- 普通关闭主窗口结束程序。
- 主窗口只连接一次 `dataChanged()`，统一发出 `refreshRequested(now)`；30 秒定时器和手动刷新也使用同一个入口。
- 查询、角色切换、定时器和手动刷新均不调用 `commit()`。
- 原生控件及布局，无样式表、装饰图片、图表或自定义颜色。

## B、C、D 的接入方式

1. B 实现 `common/Rules.cpp`，D 实现 `storage/JsonStore.cpp`，继续沿用当前公共声明与模型，不另定义同名类型。D 如需增加存储类私有方法可自行添加，不改变公共接口。
2. 各成员提供自己页面的构造函数、`refresh(const QDateTime &now)` 接口及文件清单。当前计划没有约定页面构造函数，因此 A 未替其他成员指定这些接口。
3. 由 A 在 `MainWindow` 中，用 `currentUser` 和同一个 `store` 创建对应角色页面，调用 `addPage()`。页面通过 Qt 父子关系由主窗口管理；所需业务管理器必须活得比其页面久。
4. 主窗口集中连接 `refreshRequested` 到每个页面的只读刷新函数。页面不要再次连接 `dataChanged()`，也不要在刷新函数内提交数据。
5. B 的预约页面可由主窗口连接 `seatSelectionChanged(int seatId, const QDateTime &start, const QDateTime &end)` 接收座位与时段。未选择座位时 `seatId == -1`。创建按钮必须重新调用正式预约接口，不依据网格文字判断业务结果。
6. 各成员页面实际完成后，由 A 将对应 `.cpp` 加入 `ClCompile`，有 `Q_OBJECT` 的头文件加入 `QtMoc`，非空 `.ui` 加入 `QtUic`。当前三个空页面的 `.ui` 仅作为 `None` 列出，避免 uic 解析空文件。
7. 正式工程绝不加入 `tests/MemberATestSupport.cpp`。B、D 实现接入后，测试工程也应改用真实公共实现，再验证完整流程。

示例仅说明连接位置；`page` 由负责成员的实际页面对象替换：

```cpp
addPage(QStringLiteral("我的预约"), page);
connect(this, &MainWindow::refreshRequested, page, &ReservationPage::refresh);
connect(this, &MainWindow::seatSelectionChanged, page, &ReservationPage::setSelection);
```

`ReservationPage::refresh/setSelection` 是建议的接入形式，尚未在 B 的空头文件中声明；不应将示例当成现成代码直接编译。业务权限仍须由各业务类校验，主窗口显示角色不替代权限校验。

## 验证与限制

本次使用真实 Qt 6.11.2 / MSVC2022 x64 对 A 验收工程分别进行 Debug 与 Release 构建。测试覆盖三个账号、错误输入、固定座位、当前/未来占用、签到状态、结束边界、相邻区间、取消/终止释放、无效时段、4×5 布局、选择信号、跨日刷新、一次提交一次刷新、30 秒定时器只读和主窗口角色切换。

实际结果：Debug 33 项通过、0 失败；Release 33 项通过、0 失败（QtTest 计数包含初始化和清理）。正式工程的 Debug 源码编译阶段也通过，尚未进行完整系统链接验收。

预览程序使用正式 `main.cpp`，另通过测试替身中的定时驱动验证：错误密码拒绝 → 管理员登录并退出 → 学生一登录并退出 → 学生二登录并退出 → 返回登录后退出程序；三个会话保留同一个 `JsonStore`。结果写入 `bin/x64/Release/login-cycle.txt`。

测试替身只用于隔离验收。它不验证 B 的实际规则实现、D 的 JSON 格式、保存失败、启动损坏文件和重启恢复，也不能替代 T02—T11、T13 的完整项目验收。T01 的窗口身份及退出信号、T12 的座位语义在此覆盖；完整登录循环另通过预览运行检查。

具体构建和预览命令见根目录 `README.md`。测试输出与窗口截图生成于对应的 `bin/x64/<Configuration>/`，不提交生成文件。
