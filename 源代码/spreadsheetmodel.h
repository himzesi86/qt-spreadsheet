
#ifndef SPREADSHEETMODEL_H
#define SPREADSHEETMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QSet>
#include "celldata.h"
#include "pasteoptionsdialog.h"

class SpreadsheetModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SpreadsheetModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void createSheet(int rows, int cols);
    void clearSheet();

    QVariant getCellValue(int row, int col) const;
    void recalculateAllFormulas();

    bool saveToFile(const QString& filePath);
    bool loadFromFile(const QString& filePath);

    QString copyCells(const QModelIndexList& selection) const;
    void pasteCells(const QModelIndex& topLeftPasteIndex,
                    const QString& clipboardText,
                    PasteOptionsDialog::PasteMode pasteMode = PasteOptionsDialog::PasteMode::Replace,
                    const QString& condition = QString());

    void sortRowsByColumn(int column, Qt::SortOrder order, bool caseSensitive = true);


private:
    QVariant evaluateFormula(const QString& formulaStr, const QModelIndex& currentCellIndex);
    QVariant evaluateFormulaInternal(const QString& formulaStr, const QModelIndex& currentCellIndex, bool isSubExpression, QSet<QPair<int,int>> visitedInPathForCycle);
    QString solveAllParentheses(QString formula, const QModelIndex& currentCell, QSet<QPair<int,int>> visitedInPathForCycle);
    QVariant evaluateBetterSimpleArithmetic(QString expression, const QModelIndex& currentCell, QSet<QPair<int,int>> visitedInPathForCycle);

    QVariant applyPasteOperation(const QVariant& targetVal, const QVariant& sourceVal, int mode);

    QModelIndex cellReferenceToIndex(const QString& refStr, const QModelIndex& currentCellIndex = QModelIndex()) const;
    QList<QPair<int, int>> parseRange(const QString& rangeStr, const QModelIndex& currentCellIndex) const;

    void markForRecalculation(const QModelIndex& index);

    bool evaluatePasteCondition(const QVariant& cellValue, const QString& condition) const;
    QString resolveCellReferencesInExpression(QString expression, const QModelIndex& currentCell, QSet<QPair<int,int>>& visitedInPathForCycle);


private:
    QVector<QVector<CellData>> sheetData;
    int numRows;
    int numCols;
    QSet<QPair<int,int>> cellsToRecalculate;
};

#endif
