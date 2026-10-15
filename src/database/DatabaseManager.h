#pragma once

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QObject>

namespace MSMS {

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    static DatabaseManager& instance();

    // Database lifecycle
    bool createDatabase(const QString &filePath, bool overwrite = false, QString *errorMessage = nullptr);
    bool openDatabase(const QString &filePath, QString *errorMessage = nullptr);
    void closeDatabase();

    // Status queries
    bool isOpen() const;
    QString databasePath() const;
    QSqlDatabase database() const;

    // Validation & Integrity
    static bool isSqliteFile(const QString &filePath, QString *errorMessage = nullptr);
    static bool validateSchema(QSqlDatabase db, QString *errorMessage = nullptr);
    bool verifyIntegrity(QString *resultMessage = nullptr);

    // Query helper for single statements with safety checks
    enum class StatementSafety {
        Safe,
        NeedsDeleteWarning,
        NeedsUpdateWithoutWhereWarning,
        ProhibitedSchemaModification,
        MultipleStatementsProhibited,
        Empty
    };

    static StatementSafety checkStatementSafety(const QString &sql);
    bool executeControlledQuery(const QString &sql, QSqlQuery &outQuery, QString *errorMessage = nullptr);

signals:
    void databaseOpened(const QString &path);
    void databaseClosed();

private:
    DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool enableForeignKeys(QSqlDatabase db, QString *errorMessage = nullptr);

    QString m_currentPath;
    static const QString CONNECTION_NAME;
};

} // namespace MSMS
