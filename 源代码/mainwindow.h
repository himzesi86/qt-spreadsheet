#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "pasteoptionsdialog.h" // 包含对话框头文件
QT_BEGIN_NAMESPACE
class QTableView;
class QAction;
class QMenu;
class QToolBar;
class QLabel;
QT_END_NAMESPACE

class SpreadsheetModel;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveAsFile();
    void copySelection();
    void pasteSelection();
    void sortAscending();
    void sortDescending();
    void recalculateAll();
    void updateStatusBar();
private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();

    bool maybeSave();
    void loadFile(const QString& fileName);
    void setCurrentFile(const QString &fileName);
    void showSortContextMenu(const QPoint &pos);


    QTableView *tableView;
    SpreadsheetModel *sheetModel;

    QString currentFilePath;
    bool isModified;
    QAction *newAction;
    QAction *openAction;
    QAction *saveAction;
    QAction *saveAsAction;
    QAction *exitAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *sortAscAction;
    QAction *sortDescAction;
    QAction *recalculateAction;

    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *dataMenu;
    QMenu *toolsMenu;

    QToolBar *fileToolBar;
    QToolBar *editToolBar;

    QLabel *locationLabel;
    QLabel *formulaLabel;
};

#endif
