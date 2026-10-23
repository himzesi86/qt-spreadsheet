# 简易电子表格

数据结构课程大作业。用 Qt Widgets 实现的桌面电子表格，支持单元格编辑、公式计算、条件粘贴、按列排序，以及 JSON 文件的保存与打开。

小组成员：黄锦中、吴炫宏、王起、王赫麟。

## 技术栈与关键词

- 技术栈：C++、Qt、Qt Widgets、QAbstractTableModel、QTableView、JSON、qmake
- 关键词：电子表格、Spreadsheet、公式求值、单元格引用、循环引用、SUM、AVERAGE、条件粘贴、排序、数据结构大作业、课程设计

## 功能

- 新建、打开、保存、另存为；关闭窗口时若有未保存修改会提示。
- 以 `=` 开头的内容按公式计算，否则按文本或数字保存。
- 公式支持 `+ - * /`、括号、单元格引用（如 `A1`）和区域（如 `A1:B3`）。
- 内置函数：`SUM`、`AVERAGE`、`MAX`、`MIN`、`IF`、`LEFT`、`RIGHT`、`CONCATENATE`、`TEXTJOIN`。
- 公式互相引用时检测循环，结果记为 `#CYCLE!`；除零记为 `#DIV/0!`。
- 复制选区到剪贴板；粘贴时可选择替换、加减乘除，或按条件写入。
- 右键表头可按某一列对整行升序或降序排序。
- 状态栏显示当前单元格位置和公式。

表格默认 50 行、26 列。文件以 JSON 数组保存，每个有内容的单元格记录行、列、原始输入和公式。

## 目录

```text
源代码/
├─ SpreadSheet2.pro          Qt 工程文件，目标名 QtSimpleSpreadsheet
├─ main.cpp                  创建 QApplication 和主窗口
├─ mainwindow.h/.cpp         菜单、工具栏、文件对话框和表格视图
├─ spreadsheetmodel.h/.cpp   表格数据、公式求值、复制粘贴和排序
├─ celldata.h                单元格：原始输入、显示值和公式
├─ pasteoptionsdialog.h/.cpp 粘贴方式与条件
└─ mainwindow.ui
可执行程序/                   已编译的 Windows 程序及运行库
```

`源代码` 里还带有 Qt Creator 生成的 Makefile 和 clangd 缓存，不参与阅读源码。

## 构建与运行

需要 Qt 5 或 Qt 6 的 Widgets 模块，以及对应的 MinGW 或 MSVC 工具链。

```powershell
cd 源代码
qmake SpreadSheet2.pro
mingw32-make
```

也可以用 Qt Creator 直接打开 `SpreadSheet2.pro`。已编译结果在 `可执行程序`，Windows 上可直接运行，需保留同目录下的 Qt 与 MinGW 运行库。

## 实现要点

数据放在 `QVector<QVector<CellData>>` 中，模型继承 `QAbstractTableModel`，界面使用 `QTableView`。

公式先剥掉最外层函数调用和括号，再按先乘除、后加减求值。单元格引用在求值时展开；区域只用于 `SUM`、`AVERAGE`、`MAX`、`MIN`。重算最多做 5 轮，直到显示值不再变化。文本拼接时，两个操作数只要有一个不是数字，`+` 就按字符串拼接处理。
