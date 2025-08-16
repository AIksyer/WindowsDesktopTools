# WindowsDesktopTools
**切换桌面图标的简单实现**

### HideDesktop.exe
- 直接运行
    - 切换桌面图标
- 通过传参运行
    - ShowWindow.exe true
    - ShowWindow.exe false
### 已知BUG
- 右键 **桌面** -> **查看** -> **显示桌面图标** 与程序逻辑 **不一致**
部分时候需要点击两次 **显示桌面图标** 才可以切换桌面图标

***

### AutoHideDesktop.exe
- 直接运行
    - 任务栏右下角出现系统托盘
- 系统托盘
    - 右键弹出选项 **恢复/暂停** **退出**
    - 双击系统托盘切换 **恢复/暂停** 状态
### 已知BUG
- 暂未发现