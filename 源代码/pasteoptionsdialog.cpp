
#include "pasteoptionsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>

PasteOptionsDialog::PasteOptionsDialog(QWidget *parent) :
    QDialog(parent),
    m_selectedMode(PasteMode::Replace)
{
    setupUi(); // <--- 调用的是本类的成员函数 setupUi()
    setWindowTitle(tr("粘贴选项"));
    setMinimumWidth(300);
}

PasteOptionsDialog::~PasteOptionsDialog()
{
    // 控件会自动被父对象(this)删除
}

// 这是 setupUi 函数的定义
void PasteOptionsDialog::setupUi()
{
    // 1. 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 2. 创建模式选择的 GroupBox
    QGroupBox *modeGroupBox = new QGroupBox(tr("选择粘贴模式"), this);
    QVBoxLayout *radioLayout = new QVBoxLayout();

    radioButtonReplace = new QRadioButton(tr("代替原内容"), modeGroupBox);
    radioButtonAdd = new QRadioButton(tr("与原内容相加"), modeGroupBox);
    radioButtonSubtract = new QRadioButton(tr("从原内容相减"), modeGroupBox);
    radioButtonReplaceIf = new QRadioButton(tr("按条件替换..."), modeGroupBox);

    radioButtonReplace->setChecked(true);

    radioLayout->addWidget(radioButtonReplace);
    radioLayout->addWidget(radioButtonAdd);
    radioLayout->addWidget(radioButtonSubtract);
    radioLayout->addWidget(radioButtonReplaceIf);
    modeGroupBox->setLayout(radioLayout);
    mainLayout->addWidget(modeGroupBox);

    // 3. 创建条件输入部分
    conditionPromptLabel = new QLabel(tr("条件 (例如: >10, ==\"text\", <0):"), this);
    conditionLineEdit = new QLineEdit(this); // 使用成员变量 conditionLineEdit

    conditionPromptLabel->setVisible(false);
    conditionLineEdit->setVisible(false);

    mainLayout->addWidget(conditionPromptLabel);
    mainLayout->addWidget(conditionLineEdit);

    // 4. 创建按钮盒
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // 5. 连接信号和槽
    connect(radioButtonReplace, &QRadioButton::toggled, this, [this](bool checked){
        if(checked) {
            conditionPromptLabel->hide();
            conditionLineEdit->hide();
            m_selectedMode = PasteMode::Replace;
        }
    });
    connect(radioButtonAdd, &QRadioButton::toggled, this, [this](bool checked){
        if(checked) {
            conditionPromptLabel->hide();
            conditionLineEdit->hide();
            m_selectedMode = PasteMode::Add;
        }
    });
    connect(radioButtonSubtract, &QRadioButton::toggled, this, [this](bool checked){
        if(checked) {
            conditionPromptLabel->hide();
            conditionLineEdit->hide();
            m_selectedMode = PasteMode::Subtract;
        }
    });
    connect(radioButtonReplaceIf, &QRadioButton::toggled, this, [this](bool checked){
        if(checked) {
            conditionPromptLabel->show();
            conditionLineEdit->show();
            conditionLineEdit->setFocus();
            m_selectedMode = PasteMode::ReplaceIf;
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &PasteOptionsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PasteOptionsDialog::reject);

    setLayout(mainLayout);
}

PasteOptionsDialog::PasteMode PasteOptionsDialog::selectedMode() const
{
    return m_selectedMode;
}

QString PasteOptionsDialog::condition() const
{
    if (m_selectedMode == PasteMode::ReplaceIf) {
        return conditionLineEdit->text(); // 使用成员变量 conditionLineEdit
    }
    return QString();
}
