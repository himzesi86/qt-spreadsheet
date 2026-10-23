#include "mainwindow.h"
#include "spreadsheetmodel.h"

#include <QTableView>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QCloseEvent>
#include <QMimeData>
#include "pasteoptionsdialog.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isModified(false)
{
    sheetModel = new SpreadsheetModel(this);
    tableView = new QTableView(this);
    tableView->setModel(sheetModel);

    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &MainWindow::showSortContextMenu);

    setCentralWidget(tableView);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    connect(sheetModel, &QAbstractItemModel::dataChanged, this, [this](){ isModified = true; });
    connect(tableView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::updateStatusBar);


    setCurrentFile(QString());
    setWindowTitle(tr("简易电子表格 [*]"));
    resize(800, 600);
    newFile();
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{

    newAction = new QAction(QIcon::fromTheme("document-new", QIcon(":/icons/new.png")), tr("&新建(N)"), this);
    newAction->setShortcut(QKeySequence::New);
    newAction->setStatusTip(tr("创建一个新的空白表格"));
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);

    openAction = new QAction(QIcon::fromTheme("document-open", QIcon(":/icons/open.png")), tr("&打开(O)..."), this);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip(tr("打开一个已存在的表格文件"));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    saveAction = new QAction(QIcon::fromTheme("document-save", QIcon(":/icons/save.png")), tr("&保存(S)"), this);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setStatusTip(tr("保存当前表格到磁盘"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    saveAsAction = new QAction(QIcon::fromTheme("document-save-as"), tr("另存为(A)..."), this);
    saveAsAction->setStatusTip(tr("以新文件名保存当前表格"));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveAsFile);

    exitAction = new QAction(tr("退出(X)"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip(tr("退出应用程序"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);

    copyAction = new QAction(QIcon::fromTheme("edit-copy", QIcon(":/icons/copy.png")), tr("&复制(C)"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setStatusTip(tr("复制选定内容到剪贴板"));
    connect(copyAction, &QAction::triggered, this, &MainWindow::copySelection);

    pasteAction = new QAction(QIcon::fromTheme("edit-paste", QIcon(":/icons/paste.png")), tr("&粘贴(P)"), this);
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setStatusTip(tr("从剪贴板粘贴内容"));
    connect(pasteAction, &QAction::triggered, this, &MainWindow::pasteSelection);

    sortAscAction = new QAction(tr("升序排列"), this);
    connect(sortAscAction, &QAction::triggered, this, &MainWindow::sortAscending);

    sortDescAction = new QAction(tr("降序排列"), this);
    connect(sortDescAction, &QAction::triggered, this, &MainWindow::sortDescending);

    recalculateAction = new QAction(tr("全部重算(R)"), this);
    recalculateAction->setShortcut(Qt::Key_F9);
    recalculateAction->setStatusTip(tr("重新计算表格中所有公式"));
    connect(recalculateAction, &QAction::triggered, this, &MainWindow::recalculateAll);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&文件(F)"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    editMenu = menuBar()->addMenu(tr("&编辑(E)"));
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);

    dataMenu = menuBar()->addMenu(tr("&数据(D)"));

    toolsMenu = menuBar()->addMenu(tr("&工具(T)"));
    toolsMenu->addAction(recalculateAction);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(newAction);
    fileToolBar->addAction(openAction);
    fileToolBar->addAction(saveAction);

    editToolBar = addToolBar(tr("Edit"));
    editToolBar->addAction(copyAction);
    editToolBar->addAction(pasteAction);
}

void MainWindow::createStatusBar()
{
    locationLabel = new QLabel(" ");
    locationLabel->setAlignment(Qt::AlignHCenter);
    locationLabel->setMinimumSize(locationLabel->sizeHint());

    formulaLabel = new QLabel(" ");
    formulaLabel->setIndent(3);

    statusBar()->addWidget(locationLabel);
    statusBar()->addWidget(formulaLabel, 1);
    updateStatusBar();
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (!isModified)
        return true;

    const QMessageBox::StandardButton ret =
        QMessageBox::warning(this, tr("简易电子表格"),
                             tr("文档已被修改.\n"
                                "是否要保存更改?"),
                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return saveFile();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}


void MainWindow::newFile()
{
    if (maybeSave()) {
        bool okRows, okCols;
        int rows = QInputDialog::getInt(this, tr("新建表格"), tr("行数:"), 100, 1, 10000, 1, &okRows);
        if (!okRows) return;
        int cols = QInputDialog::getInt(this, tr("新建表格"), tr("列数:"), 26, 1, 256, 1, &okCols);
        if (!okCols) return;

        sheetModel->createSheet(rows, cols);
        setCurrentFile(QString());
        isModified = false;
        setWindowModified(false);
    }
}

void MainWindow::openFile()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
                                                        tr("打开表格"), ".",
                                                        tr("电子表格文件 (*.qssjson *.json);;所有文件 (*.*)"));
        if (!fileName.isEmpty())
            loadFile(fileName);
    }
}

void MainWindow::loadFile(const QString &fileName)
{
    if (!sheetModel->loadFromFile(fileName)) {
        QMessageBox::warning(this, tr("加载错误"), tr("无法加载文件 %1").arg(QDir::toNativeSeparators(fileName)));
        setCurrentFile(QString());
        return;
    }
    setCurrentFile(fileName);
    isModified = false;
    setWindowModified(false);
    statusBar()->showMessage(tr("文件已加载"), 2000);
}


bool MainWindow::saveFile()
{
    if (currentFilePath.isEmpty()) {
        return saveAsFile();
    } else {
        if (sheetModel->saveToFile(currentFilePath)) {
            setCurrentFile(currentFilePath);
            isModified = false;
            setWindowModified(false);
            statusBar()->showMessage(tr("文件已保存"), 2000);
            return true;
        } else {
            QMessageBox::warning(this, tr("保存错误"), tr("无法保存文件 %1").arg(QDir::toNativeSeparators(currentFilePath)));
            return false;
        }
    }
}

bool MainWindow::saveAsFile()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("另存为"), currentFilePath.isEmpty() ? "." : currentFilePath,
                                                    tr("电子表格文件 (*.qssjson *.json);;所有文件 (*.*)"));
    if (fileName.isEmpty())
        return false;



    if (sheetModel->saveToFile(fileName)) {
        setCurrentFile(fileName);
        isModified = false;
        setWindowModified(false);
        statusBar()->showMessage(tr("文件已保存"), 2000);
        return true;
    } else {
        QMessageBox::warning(this, tr("保存错误"), tr("无法保存文件 %1").arg(QDir::toNativeSeparators(fileName)));
        return false;
    }
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    currentFilePath = fileName;
    isModified = false;
    setWindowModified(false);

    QString shownName = tr("未命名");
    if (!currentFilePath.isEmpty()) {
        shownName = QFileInfo(currentFilePath).fileName();
    }
    setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("简易电子表格")));
}


void MainWindow::copySelection() {
    QModelIndexList selection = tableView->selectionModel()->selectedIndexes();
    if (selection.isEmpty()) return;

    QString text = sheetModel->copyCells(selection);
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        statusBar()->showMessage(tr("已复制到剪贴板"), 2000);
    }
}

void MainWindow::pasteSelection() {
    QModelIndex current = tableView->currentIndex();
    if (!current.isValid()) {
        current = sheetModel->index(0,0);
        if (!current.isValid() && (sheetModel->rowCount() == 0 || sheetModel->columnCount() == 0)) {
            QMessageBox::information(this, tr("粘贴"), tr("请先创建一个表格或选择一个单元格。"));
            return;
        }
    }

    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (mimeData && mimeData->hasText()) {
        PasteOptionsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) { // 用户点击了 "确定"
            PasteOptionsDialog::PasteMode mode = dlg.selectedMode();
            QString condition;
            if (mode == PasteOptionsDialog::PasteMode::ReplaceIf) {
                condition = dlg.condition();
                if (condition.isEmpty()) {
                    QMessageBox::warning(this, tr("条件粘贴"), tr("条件不能为空！"));
                    return;
                }
            }

            // 将 mode 和 condition 传递给模型处理
            sheetModel->pasteCells(current, mimeData->text(), mode, condition);
            isModified = true;
            setWindowModified(true);
            statusBar()->showMessage(tr("已粘贴"), 2000);
        }
    }
}

void MainWindow::showSortContextMenu(const QPoint &pos) {
    qDebug() << "MainWindow::showSortContextMenu triggered at pos:" << pos;
    int column = tableView->horizontalHeader()->logicalIndexAt(pos);
    if (column < 0) return;

    QMenu contextMenu(tr("列操作"), this);

    QAction *ascAction = contextMenu.addAction(tr("按此列升序排列"));
    ascAction->setData(column);
    connect(ascAction, &QAction::triggered, this, &MainWindow::sortAscending);

    QAction *descAction = contextMenu.addAction(tr("按此列降序排列"));
    descAction->setData(column);
    connect(descAction, &QAction::triggered, this, &MainWindow::sortDescending);

    contextMenu.exec(tableView->horizontalHeader()->mapToGlobal(pos));
}

void MainWindow::sortAscending() {
    qDebug() << "MainWindow::sortAscending triggered.";
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
        int column = action->data().toInt();
        qDebug() << "  Action data (column):" << column;
        qDebug() << "  Calling sheetModel->sortRowsByColumn with Qt::AscendingOrder";
        sheetModel->sortRowsByColumn(column, Qt::AscendingOrder, true /* caseSensitive */);
        isModified = true;
        setWindowModified(true);
    } else {
        bool ok;
        int col = QInputDialog::getInt(this, tr("升序排序"), tr("请输入要排序的列号 (A=0, B=1...):"), 0, 0, sheetModel->columnCount()-1, 1, &ok);
        if(ok) {
            sheetModel->sortRowsByColumn(col, Qt::AscendingOrder, true);
            isModified = true;
            setWindowModified(true);
        }
    }
}

void MainWindow::sortDescending() {
    qDebug() << "MainWindow::sortDescending triggered.";
    QAction *action = qobject_cast<QAction*>(sender());

    if (action) {
        int column = action->data().toInt();
        qDebug() << "  Action data (column):" << column;
        qDebug() << "  Calling sheetModel->sortRowsByColumn with Qt::DescendingOrder";
        sheetModel->sortRowsByColumn(column, Qt::DescendingOrder, true /* caseSensitive */);
        isModified = true;
        setWindowModified(true);
    } else {
        bool ok;
        int col = QInputDialog::getInt(this, tr("降序排序"), tr("请输入要排序的列号 (A=0, B=1...):"), 0, 0, sheetModel->columnCount()-1, 1, &ok);
        if(ok) {
            sheetModel->sortRowsByColumn(col, Qt::DescendingOrder, true);
            isModified = true;
            setWindowModified(true);
        }
    }
}

void MainWindow::recalculateAll() {
    sheetModel->recalculateAllFormulas();
    statusBar()->showMessage(tr("表格已重算"), 2000);
}

#include <limits>


void MainWindow::updateStatusBar() {
    QModelIndex current = tableView->currentIndex();
    if (current.isValid()) {
        QString colStr = sheetModel->headerData(current.column(), Qt::Horizontal, Qt::DisplayRole).toString();
        QString rowStr = sheetModel->headerData(current.row(), Qt::Vertical, Qt::DisplayRole).toString();
        locationLabel->setText(tr("%1%2").arg(colStr).arg(rowStr));

        QString formulaOrValue = sheetModel->data(current, Qt::EditRole).toString();
        if (formulaOrValue.startsWith('=')) {
            formulaLabel->setText(tr("公式: %1").arg(formulaOrValue));
        } else {
            formulaLabel->setText(tr("值: %1").arg(sheetModel->data(current, Qt::DisplayRole).toString()));
        }
    } else {
        locationLabel->setText(" ");
        formulaLabel->setText(" ");
    }

    // 计算并显示选中区域的统计信息
    QModelIndexList selection = tableView->selectionModel()->selectedIndexes();
    if (!selection.isEmpty()) { // 即使只有一个单元格被选中，也尝试统计
        double sum = 0;
        int count = 0;
        double minVal = std::numeric_limits<double>::max();    // 初始化为最大可能值
        double maxVal = -std::numeric_limits<double>::max();   // 初始化为最小可能值 (或 std::numeric_limits<double>::lowest())
        bool hasNumeric = false;

        for (const QModelIndex& idx : selection) {
            if (idx.isValid()) { // 确保索引有效
                bool ok;
                QVariant cellDisplayValue = sheetModel->data(idx, Qt::DisplayRole);
                double val = cellDisplayValue.toDouble(&ok);
                if (ok) { // 只对有效的数字进行统计
                    sum += val;
                    count++;
                    if (val < minVal) {
                        minVal = val;
                    }
                    if (val > maxVal) {
                        maxVal = val;
                    }
                    hasNumeric = true;
                }
            }
        }

        QString statsMsg;
        if (hasNumeric) { // 只有当至少有一个数字被统计时才显示统计信息
            statsMsg = tr("选中 %1 单元格.").arg(selection.size());
            if (count > 0) { // 确保有数值参与统计
                statsMsg += tr(" 数值单元格: %1").arg(count);
                statsMsg += tr(", 合计: %1").arg(sum);
                statsMsg += tr(", 平均: %1").arg(QString::number(sum / count, 'f', 2)); // 保留两位小数
                statsMsg += tr(", 最小: %1").arg(minVal);
                statsMsg += tr(", 最大: %1").arg(maxVal);
            }
        } else if (!selection.isEmpty()) {
            statsMsg = tr("选中 %1 单元格.").arg(selection.size());
        }
        // 如果 statsMsg 非空，则显示它，否则可能清除或显示默认消息
        if (!statsMsg.isEmpty()) {
            statusBar()->showMessage(statsMsg, 0); // 0 表示消息持续显示，直到被新消息覆盖或清除
        } else if (current.isValid()){ // 如果没有统计信息，但有当前单元格，则不清除状态栏（locationLabel 和 formulaLabel 仍然可见）
            // statusBar()->clearMessage(); // 可能会清除 location 和 formula，所以避免
        } else {
            statusBar()->clearMessage(); // 如果没有当前单元格也没有统计，则清除
        }
        return; // 已经处理了状态栏消息
    }

    // 如果没有选择或者选择区域没有有效的数值，确保状态栏不显示旧的统计信息
    // 但同时保留单元格位置和公式信息（如果当前单元格有效）
    if (current.isValid()) {
    } else {
        statusBar()->clearMessage();
    }
}

