
#ifndef PASTEOPTIONSDIALOG_H
#define PASTEOPTIONSDIALOG_H

#include <QDialog>

// 前向声明我们将在 .cpp 中使用的 Qt 控件
class QRadioButton;
class QGroupBox;
class QLineEdit;
class QLabel;
class QDialogButtonBox;
class QVBoxLayout;

class PasteOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class PasteMode {
        Replace,
        Add,
        Subtract,
        ReplaceIf
    };

    explicit PasteOptionsDialog(QWidget *parent = nullptr);
    ~PasteOptionsDialog();

    PasteMode selectedMode() const;
    QString condition() const;

private:
    // 声明控件指针
    QRadioButton *radioButtonReplace;
    QRadioButton *radioButtonAdd;
    QRadioButton *radioButtonSubtract;
    QRadioButton *radioButtonReplaceIf;

    QLabel *conditionPromptLabel;
    QLineEdit *conditionLineEdit; // 我们在 .cpp 中创建和使用

    QDialogButtonBox *buttonBox;

    PasteMode m_selectedMode;

    void setupUi(); // 确保这个声明存在
};

#endif
