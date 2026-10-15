#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace MSMS {

struct TableSchema {
    QString name;
    QStringList requiredColumns;
};

class Schema {
public:
    static const QStringList& tableCreationStatements();
    static const QStringList& indexCreationStatements();
    static const QList<TableSchema>& requiredTables();
    static const QStringList& requiredIndexes();
    static const QString& fullSchemaSql();
};

} // namespace MSMS
