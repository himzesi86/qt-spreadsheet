#include "spreadsheetmodel.h"
#include <QColor>
#include <QBrush>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QRegularExpression>
#include <QtMath>
#include <algorithm>
#include <QMetaType>
#include <functional>

SpreadsheetModel::SpreadsheetModel(QObject *parent)
    : QAbstractTableModel(parent), numRows(0), numCols(0)
{
}

void SpreadsheetModel::createSheet(int rows, int cols)
{
    beginResetModel();
    numRows = rows;
    numCols = cols;
    sheetData.clear();
    sheetData.resize(numRows);
    for (int i = 0; i < numRows; ++i) {
        sheetData[i].resize(numCols);
    }
    endResetModel();
}

void SpreadsheetModel::clearSheet() {
    if (numRows == 0 || numCols == 0) return;
    beginResetModel();
    for (int r = 0; r < numRows; ++r) {
        for (int c = 0; c < numCols; ++c) {
            sheetData[r][c] = CellData();
        }
    }
    endResetModel();
}

int SpreadsheetModel::rowCount(const QModelIndex & /*parent*/) const { return numRows; }
int SpreadsheetModel::columnCount(const QModelIndex & /*parent*/) const { return numCols; }

QVariant SpreadsheetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= numRows || index.column() >= numCols)
        return QVariant();
    const CellData& cell = sheetData[index.row()][index.column()];
    if (role == Qt::DisplayRole) return cell.displayValue;
    if (role == Qt::EditRole) return cell.rawValue;
    if (role == Qt::ToolTipRole && !cell.formula.isEmpty())
        return QString("公式: =%1\n计算值: %2").arg(cell.formula).arg(cell.displayValue.toString());
    return QVariant();
}

bool SpreadsheetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.row() >= numRows || index.column() >= numCols)
        return false;
    CellData& cell = sheetData[index.row()][index.column()];
    QString rawString = value.toString();
    cell.rawValue = rawString;
    if (rawString.startsWith('=')) {
        cell.formula = rawString.mid(1);
        cell.displayValue = evaluateFormula(cell.formula, index);
    } else {
        cell.formula.clear();
        bool ok;
        double numValue = rawString.toDouble(&ok);
        cell.displayValue = ok ? QVariant(numValue) : QVariant(rawString);
    }
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::BackgroundRole});
    return true;
}

void SpreadsheetModel::markForRecalculation(const QModelIndex& /*changedIndex*/) {
}

QVariant SpreadsheetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Horizontal) {
        QString colName; int temp = section; if (temp < 0) return "ERR";
        do { colName.prepend(QChar('A' + (temp % 26))); temp = temp / 26 - 1; } while (temp >= 0);
        return colName;
    } else { return QString::number(section + 1); }
}

Qt::ItemFlags SpreadsheetModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

QVariant SpreadsheetModel::getCellValue(int row, int col) const {
    if (row >= 0 && row < numRows && col >= 0 && col < numCols)
        return sheetData[row][col].displayValue;
    return QVariant("#REF!");
}

QModelIndex SpreadsheetModel::cellReferenceToIndex(const QString& refStr, const QModelIndex& /*currentCellIndex*/) const {
    QString cleanRef = refStr.trimmed(); cleanRef.remove('$'); cleanRef = cleanRef.toUpper();
    QRegularExpression re("^([A-Z]+)([1-9][0-9]*)$");
    QRegularExpressionMatch match = re.match(cleanRef);
    if (!match.hasMatch()) return QModelIndex();
    QString colPart = match.captured(1); QString rowPart = match.captured(2);
    int colVal = 0; for (int i = 0; i < colPart.length(); ++i) colVal = colVal * 26 + (colPart[i].unicode() - 'A');
    bool ok; int rowVal = rowPart.toInt(&ok) - 1;
    if (!ok || rowVal < 0 || rowVal >= numRows || colVal < 0 || colVal >= numCols) return QModelIndex();
    return index(rowVal, colVal);
}

QList<QPair<int, int>> SpreadsheetModel::parseRange(const QString& rangeStr, const QModelIndex& currentCellIndex) const {
    QList<QPair<int, int>> cellsInRange;
    QStringList parts = rangeStr.split(':');
    if (parts.size() == 2) {
        QModelIndex startIdx = cellReferenceToIndex(parts[0].trimmed(), currentCellIndex);
        QModelIndex endIdx = cellReferenceToIndex(parts[1].trimmed(), currentCellIndex);
        if (startIdx.isValid() && endIdx.isValid()) {
            int r1 = qMin(startIdx.row(), endIdx.row()), r2 = qMax(startIdx.row(), endIdx.row());
            int c1 = qMin(startIdx.column(), endIdx.column()), c2 = qMax(startIdx.column(), endIdx.column());
            for (int r = r1; r <= r2; ++r) for (int c = c1; c <= c2; ++c) cellsInRange.append({r, c});
        }
    } else if (parts.size() == 1 && !parts[0].trimmed().isEmpty()) {
        QModelIndex idx = cellReferenceToIndex(parts[0].trimmed(), currentCellIndex);
        if (idx.isValid()) cellsInRange.append({idx.row(), idx.column()});
    }
    return cellsInRange;
}


QVariant SpreadsheetModel::evaluateFormula(const QString& formulaStr, const QModelIndex& currentCellIndex) {
    QSet<QPair<int,int>> initialVisitedPath;
    return evaluateFormulaInternal(formulaStr, currentCellIndex, false, initialVisitedPath);
}

QString SpreadsheetModel::solveAllParentheses(QString formula, const QModelIndex& currentCell, QSet<QPair<int,int>> visitedInPathForCycle) {
    QRegularExpression parensRegex("\\(([^\\(\\)]*)\\)");
    QRegularExpressionMatch match;
    int safetyCounter = 0; const int MAX_PAREN_ITERATIONS = 50;
    while ((match = parensRegex.match(formula)).hasMatch() && safetyCounter < MAX_PAREN_ITERATIONS) {
        safetyCounter++;
        QString innerExpr = match.captured(1);
        QVariant innerResult = evaluateFormulaInternal(innerExpr, currentCell, true, visitedInPathForCycle);
        if (innerResult.userType() == QMetaType::QString && innerResult.toString().startsWith('#')) {
            return innerResult.toString();
        }
        formula.replace(match.capturedStart(0), match.capturedLength(0), innerResult.toString());
    }
    if (safetyCounter >= MAX_PAREN_ITERATIONS) return "#ERROR_PAREN!";
    return formula;
}

QVariant SpreadsheetModel::evaluateBetterSimpleArithmetic(QString expression, const QModelIndex& currentCell, QSet<QPair<int,int>> visitedInPathForCycle) {
    expression = expression.trimmed();
    if (expression.isEmpty()) return QVariant("#VALUE!");
    bool okDirectNum;
    double directNum = expression.toDouble(&okDirectNum);
    if (okDirectNum) return directNum;

    if (expression.startsWith('"') && expression.endsWith('"') && expression.length() >=2) {
        return expression.mid(1, expression.length() - 2);
    }
    QModelIndex directRef = cellReferenceToIndex(expression, currentCell);
    if (directRef.isValid()) {
        if (directRef == currentCell) return QString("#CYCLE!");
        if (visitedInPathForCycle.contains(qMakePair(directRef.row(), directRef.column()))) return QString("#CYCLE!");
        return getCellValue(directRef.row(), directRef.column());
    }


    QString tempExpr = expression;
    int safety = 0; const int MAX_OP_ITERATIONS_PER_PASS = 30;
    QRegularExpressionMatch matchResult;
    const QString operandPattern = "(-?(?:[0-9]+(?:\\.[0-9]+)?(?:E[+-]?[0-9]+)?|[A-Z]+\\$?[0-9]+)|\"[^\"]*\")";


    QRegularExpression mulDivRegex(operandPattern + "\\s*([\\*\\/])\\s*" + operandPattern);
    safety = 0;
    while(safety < MAX_OP_ITERATIONS_PER_PASS) {
        matchResult = mulDivRegex.match(tempExpr);
        if (!matchResult.hasMatch()) break;
        safety++;
        QString lhsStr = matchResult.captured(1);
        QString op = matchResult.captured(2);
        QString rhsStr = matchResult.captured(3);

        QVariant lhsVal = evaluateFormulaInternal(lhsStr, currentCell, true, visitedInPathForCycle);
        QVariant rhsVal = evaluateFormulaInternal(rhsStr, currentCell, true, visitedInPathForCycle);

        if (lhsVal.userType() == QMetaType::QString && lhsVal.toString().startsWith('#')) return lhsVal;
        if (rhsVal.userType() == QMetaType::QString && rhsVal.toString().startsWith('#')) return rhsVal;

        bool okL, okR;
        double numL = lhsVal.toDouble(&okL);
        double numR = rhsVal.toDouble(&okR);

        if (!okL || !okR) return QString("#VALUE!_MD_OP");
        if (op == "/" && qFuzzyCompare(numR, 0.0)) return QString("#DIV/0!");

        double resultVal = (op == "*") ? (numL * numR) : (numL / numR);
        tempExpr.replace(matchResult.capturedStart(0), matchResult.capturedLength(0), QString::number(resultVal));
    }

    QRegularExpression addSubRegex(operandPattern + "\\s*([\\+\\-])\\s*" + operandPattern);
    safety = 0;
    while(safety < MAX_OP_ITERATIONS_PER_PASS) {
        matchResult = addSubRegex.match(tempExpr);
        if (!matchResult.hasMatch()) break;
        safety++;
        QString lhsStr = matchResult.captured(1);
        QString op = matchResult.captured(2);
        QString rhsStr = matchResult.captured(3);

        QVariant lhsVal = evaluateFormulaInternal(lhsStr, currentCell, true, visitedInPathForCycle);
        QVariant rhsVal = evaluateFormulaInternal(rhsStr, currentCell, true, visitedInPathForCycle);

        if (lhsVal.userType() == QMetaType::QString && lhsVal.toString().startsWith('#')) return lhsVal;
        if (rhsVal.userType() == QMetaType::QString && rhsVal.toString().startsWith('#')) return rhsVal;

        bool okL, okR;
        double numL = lhsVal.toDouble(&okL);
        double numR = rhsVal.toDouble(&okR);

        if (op == "+") {
            if (okL && okR) {
                tempExpr.replace(matchResult.capturedStart(0), matchResult.capturedLength(0), QString::number(numL + numR));
            } else {
                tempExpr.replace(matchResult.capturedStart(0), matchResult.capturedLength(0), QString("\"%1%2\"").arg(lhsVal.toString()).arg(rhsVal.toString()));
            }
        } else {
            if (!okL || !okR) return QString("#VALUE!_AS_OP");
            tempExpr.replace(matchResult.capturedStart(0), matchResult.capturedLength(0), QString::number(numL - numR));
        }
    }

    if (safety >= MAX_OP_ITERATIONS_PER_PASS && (mulDivRegex.match(tempExpr).hasMatch() || addSubRegex.match(tempExpr).hasMatch())) {
        return "#ERROR_ARITH_LOOP!";
    }

    bool okFinal; double finalNum = tempExpr.toDouble(&okFinal); if (okFinal) return finalNum;
    if (tempExpr.startsWith('"') && tempExpr.endsWith('"') && tempExpr.length() >= 2) return tempExpr.mid(1, tempExpr.length() - 2);
    QModelIndex finalRefIdx = cellReferenceToIndex(tempExpr, currentCell);
    if (finalRefIdx.isValid()) return getCellValue(finalRefIdx.row(), finalRefIdx.column());
    if (tempExpr.startsWith('#')) return QVariant(tempExpr);

    return QString("#NAME?");
}

QVariant SpreadsheetModel::evaluateFormulaInternal(const QString& formulaStr,
                                                    const QModelIndex& currentCellIndex,
                                                    bool isSubExpression,
                                                    QSet<QPair<int,int>> visitedInPathForCycle) {

    QString currentFormula = formulaStr.trimmed();
    if (currentFormula.isEmpty()) {

        return QString("#ERROR_EMPTY!");
    }

    if (isSubExpression &&
        !currentFormula.contains(QRegularExpression("[\\+\\-\\*\\/\\(\\)]")) &&
        !currentFormula.contains(QRegularExpression("^(SUM|AVERAGE|MAX|MIN|IF|LEFT|RIGHT|CONCATENATE|TEXTJOIN)\\(", QRegularExpression::CaseInsensitiveOption))) // Not a function call
    {
        bool okNum;
        double numVal = currentFormula.toDouble(&okNum);
        if (okNum) return numVal;

        QModelIndex refIdx = cellReferenceToIndex(currentFormula, currentCellIndex);
        if (refIdx.isValid()) {
            if (refIdx == currentCellIndex) return QString("#CYCLE!");
            if (visitedInPathForCycle.contains(qMakePair(refIdx.row(), refIdx.column()))) return QString("#CYCLE!");
            QSet<QPair<int,int>> pathForRef = visitedInPathForCycle;
            pathForRef.insert(qMakePair(currentCellIndex.row(), currentCellIndex.column()));
            return getCellValue(refIdx.row(), refIdx.column());
        }
        if (currentFormula.startsWith('"') && currentFormula.endsWith('"') && currentFormula.length() >= 2) {
            return currentFormula.mid(1, currentFormula.length() - 2);
        }
        if (QVariant(currentFormula).typeId() == QMetaType::QString) {
            if (currentFormula.startsWith('#')) return currentFormula;
            return currentFormula;
        }
    }


    if (visitedInPathForCycle.contains(qMakePair(currentCellIndex.row(), currentCellIndex.column()))) {
        qWarning() << "Cycle detected! Re-entering cell R" << currentCellIndex.row()+1 << "C" << currentCellIndex.column()+1 << " Formula:" << formulaStr << " Path:" << visitedInPathForCycle;
        return QString("#CYCLE!");
    }
    QSet<QPair<int,int>> pathForThisBranch = visitedInPathForCycle;
    pathForThisBranch.insert(qMakePair(currentCellIndex.row(), currentCellIndex.column()));


    QRegularExpression funcRegex("^(SUM|AVERAGE|MAX|MIN|IF|LEFT|RIGHT|CONCATENATE|TEXTJOIN)\\((.*)\\)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch funcMatch = funcRegex.match(currentFormula);
    if (funcMatch.hasMatch()) {
        QString funcName = funcMatch.captured(1).toUpper();
        QString argsStr = funcMatch.captured(2);

        QList<QVariant> evaluatedArgs;
        if (!argsStr.isEmpty()) {
            int parenLevel = 0; QString currentArgStr;
            for (int k = 0; k < argsStr.length(); ++k) {
                QChar ch = argsStr[k];
                if (ch == '(') parenLevel++;
                else if (ch == ')') parenLevel--;
                else if (ch == ',' && parenLevel == 0) {
                    evaluatedArgs.append(evaluateFormulaInternal(currentArgStr.trimmed(), currentCellIndex, true, pathForThisBranch));
                    currentArgStr.clear(); continue;
                }
                currentArgStr.append(ch);
            }
            if (!currentArgStr.isEmpty()) evaluatedArgs.append(evaluateFormulaInternal(currentArgStr.trimmed(), currentCellIndex, true, pathForThisBranch));
        }

        for(const QVariant& argVal : evaluatedArgs) if (argVal.userType() == QMetaType::QString && argVal.toString().startsWith('#')) return argVal;

        if (funcName == "SUM" || funcName == "AVERAGE" || funcName == "MAX" || funcName == "MIN") {
            QList<double> numericValues;
            for (const QVariant& arg : evaluatedArgs) {
                if (arg.userType() == QMetaType::QString) {
                    QList<QPair<int, int>> cellsInArgRange = parseRange(arg.toString(), currentCellIndex);
                    if (!cellsInArgRange.isEmpty()) {
                        for(const auto& cellPos : cellsInArgRange) {
                            QVariant cellVal = getCellValue(cellPos.first, cellPos.second);
                            bool ok; double num = cellVal.toDouble(&ok);
                            if (ok) numericValues.append(num);
                            else if (cellVal.userType() == QMetaType::QString && cellVal.toString().startsWith('#')) return cellVal;
                        } continue;
                    }
                }
                bool ok; double num = arg.toDouble(&ok);
                if (ok) numericValues.append(num);
                else if (arg.userType() == QMetaType::QString && arg.toString().startsWith('#')) return arg;
            }
            if (numericValues.isEmpty()) {
                if (funcName == "SUM") return 0.0; if (funcName == "AVERAGE") return QString("#DIV/0!"); return QString("#NUM!");
            }
            if (funcName == "SUM") { double sum = 0; for(double v : numericValues) sum+=v; return sum; }
            if (funcName == "AVERAGE") { double sum = 0; for(double v : numericValues) sum+=v; return sum / numericValues.size(); }
            if (funcName == "MAX") return *std::max_element(numericValues.constBegin(), numericValues.constEnd());
            if (funcName == "MIN") return *std::min_element(numericValues.constBegin(), numericValues.constEnd());
        } else if (funcName == "IF") {
            if (evaluatedArgs.size() != 3) return QString("#N/A_IF");
            bool conditionResult;
            if (evaluatedArgs[0].userType() == QMetaType::Bool) conditionResult = evaluatedArgs[0].toBool();
            else { bool okNum; double numVal = evaluatedArgs[0].toDouble(&okNum);
                if (okNum) conditionResult = !qFuzzyCompare(numVal, 0.0);
                else conditionResult = !evaluatedArgs[0].toString().isEmpty();
            }
            return conditionResult ? evaluatedArgs[1] : evaluatedArgs[2];
        } else if (funcName == "LEFT" || funcName == "RIGHT") {
            if (evaluatedArgs.size() < 1 || evaluatedArgs.size() > 2) return QString("#N/A_TEXT_FUNC_ARGS");
            QString text = evaluatedArgs[0].toString();
            int numChars = (evaluatedArgs.size() == 2) ? evaluatedArgs[1].toInt() : 1;
            if (numChars < 0) return QString("#VALUE!");
            return (funcName == "LEFT") ? text.left(numChars) : text.right(numChars);
        } else if (funcName == "CONCATENATE" || funcName == "TEXTJOIN") {
            QString result_str;
            if (funcName == "TEXTJOIN") {
                if (evaluatedArgs.size() < 2) return QString("#N/A_TEXTJOIN_ARGS");
                QString delimiter = evaluatedArgs[0].toString();
                bool ignoreEmpty = true; int textStartIndex = 1;
                if (evaluatedArgs.size() >= 3 && evaluatedArgs[1].canConvert<bool>()) {
                    ignoreEmpty = evaluatedArgs[1].toBool(); textStartIndex = 2;
                }
                bool first = true;
                for (int k = textStartIndex; k < evaluatedArgs.size(); ++k) {
                    QString currentText = evaluatedArgs[k].toString();
                    if (ignoreEmpty && currentText.isEmpty()) continue;
                    if (!first) result_str += delimiter;
                    result_str += currentText; first = false;
                }
            } else { for (const QVariant& arg : evaluatedArgs) result_str += arg.toString(); }
            return result_str;
        } else return QString("#NAME?");
    }

    QString formulaAfterParens = solveAllParentheses(currentFormula, currentCellIndex, pathForThisBranch);
    if (formulaAfterParens.startsWith('#')) return formulaAfterParens;

    QVariant finalResult = evaluateBetterSimpleArithmetic(formulaAfterParens, currentCellIndex, pathForThisBranch);

    return finalResult;
}

void SpreadsheetModel::recalculateAllFormulas() {
    int maxIterations = 5; bool changedInLastPass = true;
    for (int iter = 0; iter < maxIterations && changedInLastPass; ++iter) {
        changedInLastPass = false;
        for (int r = 0; r < numRows; ++r) for (int c = 0; c < numCols; ++c) {
                CellData& cell = sheetData[r][c];
                if (!cell.formula.isEmpty()) {
                    QVariant oldDisplayValue = cell.displayValue;
                    QSet<QPair<int,int>> visitedForThisCellCalculation;
                    cell.displayValue = evaluateFormulaInternal(cell.formula, index(r,c), false, visitedForThisCellCalculation);
                    if (cell.displayValue != oldDisplayValue) {
                        emit dataChanged(index(r,c), index(r,c), {Qt::DisplayRole, Qt::ToolTipRole});
                        changedInLastPass = true;
                    }
                }
            }
    }
}

bool SpreadsheetModel::saveToFile(const QString& filePath) {
    QJsonArray sheetArray;
    for (int r = 0; r < numRows; ++r) {
        for (int c = 0; c < numCols; ++c) {
            const CellData& cell = sheetData[r][c];
            if (!cell.rawValue.isEmpty()) {
                QJsonObject cellObject;
                cellObject["row"] = r; cellObject["col"] = c; cellObject["raw"] = cell.rawValue;
                sheetArray.append(cellObject);
            }
        }
    }
    QJsonObject mainObject;
    mainObject["rows"] = numRows; mainObject["cols"] = numCols; mainObject["cells"] = sheetArray;
    QJsonDocument doc(mainObject); QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Couldn't open save file:" << filePath << file.errorString(); return false;
    }
    file.write(doc.toJson()); file.close(); return true;
}

bool SpreadsheetModel::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Couldn't open load file:" << filePath << file.errorString(); return false;
    }
    QByteArray jsonData = file.readAll(); file.close();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) { qWarning() << "Invalid JSON file content."; return false; }
    QJsonObject mainObject = doc.object();
    int loadedRows = mainObject["rows"].toInt(100); int loadedCols = mainObject["cols"].toInt(26);
    beginResetModel(); createSheet(loadedRows, loadedCols);
    QJsonArray cellsArray = mainObject["cells"].toArray();
    for (const QJsonValue& cellValue : cellsArray) {
        QJsonObject cellObject = cellValue.toObject();
        int r = cellObject["row"].toInt(-1); int c = cellObject["col"].toInt(-1);
        QString raw = cellObject["raw"].toString();
        if (r >= 0 && r < numRows && c >= 0 && c < numCols) setData(index(r,c), raw, Qt::EditRole);
    }
    endResetModel(); recalculateAllFormulas(); return true;
}

QString SpreadsheetModel::copyCells(const QModelIndexList& selection) const {
    if (selection.isEmpty()) return QString();
    int minRow = numRows, maxRow = -1, minCol = numCols, maxCol = -1;
    QSet<QPair<int,int>> selectedCellPositions;
    for (const QModelIndex& idx : selection) {
        if (idx.isValid()) {
            minRow = qMin(minRow, idx.row()); maxRow = qMax(maxRow, idx.row());
            minCol = qMin(minCol, idx.column()); maxCol = qMax(maxCol, idx.column());
            selectedCellPositions.insert({idx.row(), idx.column()});
        }
    }
    if (maxRow == -1) return QString(); QString clipboardString;
    for (int r = minRow; r <= maxRow; ++r) {
        for (int c = minCol; c <= maxCol; ++c) {
            if (selectedCellPositions.contains({r,c})) clipboardString += sheetData[r][c].rawValue;
            if (c < maxCol) clipboardString += "\t";
        }
        if (r < maxRow) clipboardString += "\n";
    }
    return clipboardString;
}

bool SpreadsheetModel::evaluatePasteCondition(const QVariant& cellValue, const QString& condition) const {
    if (condition.isEmpty()) return true;
    QRegularExpression numRegex("^([><]=?|==|!=)\\s*(-?[0-9]+(?:\\.[0-9]+)?)$");
    QRegularExpressionMatch numMatch = numRegex.match(condition.trimmed());
    if (numMatch.hasMatch()) {
        bool okCellNum; double cellNum = cellValue.toDouble(&okCellNum);
        if (!okCellNum && cellValue.isValid() && !cellValue.isNull()) return false;
        if(!okCellNum && !(cellValue.isNull() || !cellValue.isValid())) return false;
        QString op = numMatch.captured(1);
        bool okCondNum; double condNum = numMatch.captured(2).toDouble(&okCondNum);
        if (!okCondNum) { qWarning() << "Invalid number in paste condition:" << numMatch.captured(2); return false; }
        if (op == ">") return cellNum > condNum; if (op == "<") return cellNum < condNum;
        if (op == ">=") return cellNum >= condNum; if (op == "<=") return cellNum <= condNum;
        if (op == "==") return qFuzzyCompare(cellNum, condNum); if (op == "!=") return !qFuzzyCompare(cellNum, condNum);
    }
    QRegularExpression strRegex("^([=!]=)\\s*\"((?:[^\"\\\\]|\\\\.)*)\"$");
    QRegularExpressionMatch strMatch = strRegex.match(condition.trimmed());
    if (strMatch.hasMatch()) {
        QString op = strMatch.captured(1); QString condStr = strMatch.captured(2);
        condStr.replace("\\\"", "\"").replace("\\\\", "\\");
        QString cellStr = cellValue.toString();
        if (op == "==") return cellStr == condStr; if (op == "!=") return cellStr != condStr;
    }
    qWarning() << "Unsupported condition format for paste:" << condition; return false;
}

void SpreadsheetModel::pasteCells(const QModelIndex& topLeftPasteIndex,
                                  const QString& clipboardText,
                                  PasteOptionsDialog::PasteMode pasteMode,
                                  const QString& condition) {
    if (!topLeftPasteIndex.isValid()) { qWarning("PasteCells: topLeftPasteIndex is invalid."); return; }
    QStringList rows = clipboardText.split('\n');
    int startRow = topLeftPasteIndex.row(); int startCol = topLeftPasteIndex.column();
    for (int r_offset = 0; r_offset < rows.size(); ++r_offset) {
        if (r_offset == rows.size() - 1 && rows[r_offset].isEmpty() && clipboardText.endsWith('\n')) continue;
        QStringList cols = rows[r_offset].split('\t');
        for (int c_offset = 0; c_offset < cols.size(); ++c_offset) {
            int targetRow = startRow + r_offset; int targetCol = startCol + c_offset;
            if (targetRow < numRows && targetCol < numCols) {
                QModelIndex targetIndex = index(targetRow, targetCol);
                QVariant currentDisplayValue = data(targetIndex, Qt::DisplayRole);
                QString pastedRawValue = cols[c_offset];
                QString valueToSetInRaw; bool shouldUpdateCell = false;
                switch (pasteMode) {
                case PasteOptionsDialog::PasteMode::Replace:
                    valueToSetInRaw = pastedRawValue; shouldUpdateCell = true; break;
                case PasteOptionsDialog::PasteMode::Add:
                case PasteOptionsDialog::PasteMode::Subtract: {
                    bool conversionSuccessForCurrent; bool conversionSuccessForPasted;
                    double currentNum = currentDisplayValue.toDouble(&conversionSuccessForCurrent);
                    double pastedNum = pastedRawValue.toDouble(&conversionSuccessForPasted);
                    if (conversionSuccessForCurrent && conversionSuccessForPasted) {
                        double result = (pasteMode == PasteOptionsDialog::PasteMode::Add) ? (currentNum + pastedNum) : (currentNum - pastedNum);
                        valueToSetInRaw = QString::number(result); shouldUpdateCell = true;
                    }
                    break;
                }
                case PasteOptionsDialog::PasteMode::ReplaceIf:
                    if (evaluatePasteCondition(currentDisplayValue, condition)) {
                        valueToSetInRaw = pastedRawValue; shouldUpdateCell = true;
                    } break;
                }
                if (shouldUpdateCell) setData(targetIndex, valueToSetInRaw, Qt::EditRole);
            }
        }
    }
    recalculateAllFormulas();
}

void SpreadsheetModel::sortRowsByColumn(int column, Qt::SortOrder order, bool caseSensitive) {
    if (column < 0 || column >= numCols || numRows <= 1) return;
    beginResetModel();
    std::sort(sheetData.begin(), sheetData.end(),
              [&](const QVector<CellData>& rowA, const QVector<CellData>& rowB) {
                  if (column < 0 || column >= rowA.size() || column >= rowB.size()) {
                      qCritical() << "Sort Comparator: Column index out of bounds."; return false;
                  }
                  const CellData& cellA = rowA[column]; const CellData& cellB = rowB[column];
                  QVariant valA = cellA.displayValue; QVariant valB = cellB.displayValue;
                  bool valAIsEmpty = !valA.isValid() || valA.isNull() || (valA.userType() == QMetaType::QString && valA.toString().isEmpty());
                  bool valBIsEmpty = !valB.isValid() || valB.isNull() || (valB.userType() == QMetaType::QString && valB.toString().isEmpty());
                  if (valAIsEmpty && valBIsEmpty) return false;
                  if (valAIsEmpty) return false;
                  if (valBIsEmpty) return true;
                  bool isNumA = false; double numA = valA.toDouble(&isNumA);
                  bool isNumB = false; double numB = valB.toDouble(&isNumB);
                  if (order == Qt::AscendingOrder) {
                      if (isNumA && isNumB) return numA < numB;
                      if (isNumA) return true; if (isNumB) return false;
                      return valA.toString().compare(valB.toString(), caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive) < 0;
                  } else {
                      if (isNumA && isNumB) return numA > numB;
                      if (isNumA) return false; if (isNumB) return true;
                      return valA.toString().compare(valB.toString(), caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive) > 0;
                  }
              });
    endResetModel();
    recalculateAllFormulas();
}
