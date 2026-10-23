QT       += core gui widgets

TARGET = QtSimpleSpreadsheet
TEMPLATE = app

SOURCES += main.cpp \
           mainwindow.cpp \
           pasteoptionsdialog.cpp \
           spreadsheetmodel.cpp

HEADERS  += mainwindow.h \
            pasteoptionsdialog.h \
            spreadsheetmodel.h \
            celldata.h

FORMS +=
            mainwindow.ui \
