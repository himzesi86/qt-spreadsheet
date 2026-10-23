#ifndef CELLDATA_H
#define CELLDATA_H

#include <QString>
#include <QVariant>
#include <QDebug>

struct CellData {
    QString rawValue;
    QVariant displayValue;
    QString formula;

    CellData() {

    }

    CellData(const QString& raw) : rawValue(raw), displayValue(raw) {

    }


    CellData(const CellData& other) :
        rawValue(other.rawValue),
        displayValue(other.displayValue),
        formula(other.formula) {
    }


    CellData& operator=(const CellData& other) {
        if (this != &other) {
            rawValue = other.rawValue;
            displayValue = other.displayValue;
            formula = other.formula;
        }
        return *this;
    }

    CellData(CellData&& other) noexcept :
        rawValue(std::move(other.rawValue)),
        displayValue(std::move(other.displayValue)),
        formula(std::move(other.formula)) {
    }

    CellData& operator=(CellData&& other) noexcept {
        if (this != &other) {
            rawValue = std::move(other.rawValue);
            displayValue = std::move(other.displayValue);
            formula = std::move(other.formula);
        }
        return *this;
    }
};

#endif
