#include "DatabaseManager.h"
#include "Schema.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSqlError>
#include <QSqlRecord>
#include <QSet>
#include <QRegularExpression>
#include <QDebug>

namespace MSMS {

const QString DatabaseManager::CONNECTION_NAME = QLatin1String(QSqlDatabase::defaultConnection);

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    closeDatabase();
}

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager s_instance;
    return s_instance;
}

bool DatabaseManager::isOpen() const
{
    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME, false);
    return db.isValid() && db.isOpen();
}

QString DatabaseManager::databasePath() const
{
    return m_currentPath;
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(CONNECTION_NAME, false);
}

bool DatabaseManager::enableForeignKeys(QSqlDatabase db, QString *errorMessage)
{
    QSqlQuery q(db);
    if (!q.exec("PRAGMA foreign_keys = ON;")) {
        if (errorMessage) {
            *errorMessage = QString("Failed to enable foreign keys: %1").arg(q.lastError().text());
        }
        return false;
    }

    if (q.exec("PRAGMA foreign_keys;") && q.next()) {
        int val = q.value(0).toInt();
        if (val != 1) {
            if (errorMessage) {
                *errorMessage = QString("Foreign keys could not be verified (returned %1, expected 1).").arg(val);
            }
            return false;
        }
    } else {
        if (errorMessage) {
            *errorMessage = QString("Failed to query foreign keys PRAGMA: %1").arg(q.lastError().text());
        }
        return false;
    }

    return true;
}

bool DatabaseManager::isSqliteFile(const QString &filePath, QString *errorMessage)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        if (errorMessage) *errorMessage = QString("File does not exist: %1").arg(filePath);
        return false;
    }
    if (!fi.isFile()) {
        if (errorMessage) *errorMessage = QString("Path is not a regular file: %1").arg(filePath);
        return false;
    }
    if (fi.size() < 100) {
        if (errorMessage) *errorMessage = QString("File is too small to be a valid SQLite database (%1 bytes).").arg(fi.size());
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QString("Cannot open file for reading: %1").arg(file.errorString());
        return false;
    }

    QByteArray header = file.read(16);
    file.close();

    const char sqliteHeader[] = "SQLite format 3\000";
    if (header.size() != 16 || memcmp(header.constData(), sqliteHeader, 16) != 0) {
        if (errorMessage) *errorMessage = QString("File does not contain a valid SQLite 3 header.");
        return false;
    }

    return true;
}

bool DatabaseManager::validateSchema(QSqlDatabase db, QString *errorMessage)
{
    if (!db.isValid() || !db.isOpen()) {
        if (errorMessage) *errorMessage = "Database connection is not open.";
        return false;
    }

    QSqlQuery q(db);

    // 1. Check quick_check or integrity_check
    if (!q.exec("PRAGMA quick_check;")) {
        if (errorMessage) *errorMessage = QString("Integrity check query failed: %1").arg(q.lastError().text());
        return false;
    }
    if (q.next()) {
        QString res = q.value(0).toString();
        if (res.toLower() != "ok") {
            if (errorMessage) *errorMessage = QString("Database integrity check failed: %1").arg(res);
            return false;
        }
    }

    // 2. Fetch all existing tables
    if (!q.exec("SELECT name FROM sqlite_master WHERE type='table';")) {
        if (errorMessage) *errorMessage = QString("Failed to list tables: %1").arg(q.lastError().text());
        return false;
    }
    QSet<QString> existingTables;
    while (q.next()) {
        existingTables.insert(q.value(0).toString().toLower());
    }

    // Check each required table and its columns
    for (const TableSchema &tbl : Schema::requiredTables()) {
        if (!existingTables.contains(tbl.name.toLower())) {
            if (errorMessage) {
                *errorMessage = QString("Incompatible database: missing required table '%1'.").arg(tbl.name);
            }
            return false;
        }

        // Check columns
        if (!q.exec(QString("PRAGMA table_info(\"%1\");").arg(tbl.name))) {
            if (errorMessage) {
                *errorMessage = QString("Failed to query table info for '%1': %2").arg(tbl.name, q.lastError().text());
            }
            return false;
        }
        QSet<QString> existingCols;
        while (q.next()) {
            existingCols.insert(q.value("name").toString().toLower());
        }

        for (const QString &col : tbl.requiredColumns) {
            if (!existingCols.contains(col.toLower())) {
                if (errorMessage) {
                    *errorMessage = QString("Incompatible database: table '%1' is missing required column '%2'.")
                                        .arg(tbl.name, col);
                }
                return false;
            }
        }
    }

    // 3. Fetch existing indexes
    if (!q.exec("SELECT name FROM sqlite_master WHERE type='index';")) {
        if (errorMessage) *errorMessage = QString("Failed to list indexes: %1").arg(q.lastError().text());
        return false;
    }
    QSet<QString> existingIndexes;
    while (q.next()) {
        existingIndexes.insert(q.value(0).toString().toLower());
    }

    for (const QString &idx : Schema::requiredIndexes()) {
        if (!existingIndexes.contains(idx.toLower())) {
            if (errorMessage) {
                *errorMessage = QString("Incompatible database: missing required index '%1'.").arg(idx);
            }
            return false;
        }
    }

    return true;
}

bool DatabaseManager::createDatabase(const QString &filePath, bool overwrite, QString *errorMessage)
{
    QFileInfo fi(filePath);
    if (fi.exists()) {
        if (!overwrite) {
            if (errorMessage) *errorMessage = QString("Destination file already exists: %1").arg(filePath);
            return false;
        }
        // Remove existing file explicitly before creating anew
        closeDatabase();
        if (!QFile::remove(filePath)) {
            if (errorMessage) *errorMessage = QString("Failed to remove existing file: %1").arg(filePath);
            return false;
        }
    } else {
        // Ensure parent directory exists
        QDir dir = fi.dir();
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                if (errorMessage) *errorMessage = QString("Failed to create directory: %1").arg(dir.absolutePath());
                return false;
            }
        }
    }

    closeDatabase();

    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", CONNECTION_NAME);
        db.setDatabaseName(filePath);

        if (!db.open()) {
            if (errorMessage) *errorMessage = QString("Failed to open database file: %1").arg(db.lastError().text());
        } else if (!enableForeignKeys(db, errorMessage)) {
            // failed
        } else if (!db.transaction()) {
            if (errorMessage) *errorMessage = QString("Failed to begin transaction: %1").arg(db.lastError().text());
        } else {
            bool ddlOk = true;
            QSqlQuery q(db);
            for (const QString &stmt : Schema::tableCreationStatements()) {
                if (!q.exec(stmt)) {
                    if (errorMessage) {
                        *errorMessage = QString("Failed to create table: %1\nQuery: %2").arg(q.lastError().databaseText(), stmt);
                    }
                    ddlOk = false;
                    break;
                }
            }

            if (ddlOk) {
                for (const QString &stmt : Schema::indexCreationStatements()) {
                    if (!q.exec(stmt)) {
                        if (errorMessage) {
                            *errorMessage = QString("Failed to create index: %1\nQuery: %2").arg(q.lastError().databaseText(), stmt);
                        }
                        ddlOk = false;
                        break;
                    }
                }
            }

            if (!ddlOk) {
                db.rollback();
            } else if (!db.commit()) {
                if (errorMessage) *errorMessage = QString("Failed to commit database initialisation: %1").arg(db.lastError().text());
                db.rollback();
            } else if (!validateSchema(db, errorMessage)) {
                // schema validation failed
            } else {
                ok = true;
            }
        }
    }

    if (!ok) {
        closeDatabase();
        QFile::remove(filePath);
        return false;
    }

    m_currentPath = filePath;
    emit databaseOpened(filePath);
    return true;
}

bool DatabaseManager::openDatabase(const QString &filePath, QString *errorMessage)
{
    // First: strictly check if it is a valid SQLite file without calling QSqlDatabase::open(),
    // so we never create or modify invalid files.
    if (!isSqliteFile(filePath, errorMessage)) {
        return false;
    }

    closeDatabase();

    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", CONNECTION_NAME);
        db.setDatabaseName(filePath);

        if (!db.open()) {
            if (errorMessage) *errorMessage = QString("Failed to open database: %1").arg(db.lastError().text());
        } else if (!enableForeignKeys(db, errorMessage)) {
            // failed
        } else if (!validateSchema(db, errorMessage)) {
            // failed
        } else {
            ok = true;
        }
    }

    if (!ok) {
        closeDatabase();
        return false;
    }

    m_currentPath = filePath;
    emit databaseOpened(filePath);
    return true;
}

void DatabaseManager::closeDatabase()
{
    if (QSqlDatabase::contains(CONNECTION_NAME)) {
        {
            QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME, false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }
    m_currentPath.clear();
    emit databaseClosed();
}

bool DatabaseManager::verifyIntegrity(QString *resultMessage)
{
    QSqlDatabase db = database();
    if (!db.isValid() || !db.isOpen()) {
        if (resultMessage) *resultMessage = "Database is not connected.";
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec("PRAGMA integrity_check;")) {
        if (resultMessage) *resultMessage = QString("PRAGMA integrity_check failed: %1").arg(q.lastError().text());
        return false;
    }

    QStringList results;
    while (q.next()) {
        results.append(q.value(0).toString());
    }

    if (results.isEmpty()) {
        if (resultMessage) *resultMessage = "PRAGMA integrity_check returned no rows.";
        return false;
    }

    if (resultMessage) {
        *resultMessage = results.join(QLatin1Char('\n'));
    }

    return (results.size() == 1 && results.first().trimmed().toLower() == "ok");
}

DatabaseManager::StatementSafety DatabaseManager::checkStatementSafety(const QString &sql)
{
    QString trimmed = sql.trimmed();
    if (trimmed.isEmpty()) {
        return StatementSafety::Empty;
    }

    // Strip trailing semicolon for single statement check
    while (trimmed.endsWith(';')) {
        trimmed.chop(1);
        trimmed = trimmed.trimmed();
    }

    // Check for multiple statements by looking for unquoted semicolons
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (int i = 0; i < trimmed.length(); ++i) {
        QChar c = trimmed[i];
        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else if (c == ';' && !inSingleQuote && !inDoubleQuote) {
            // Semicolon followed by additional content
            return StatementSafety::MultipleStatementsProhibited;
        }
    }

    // Prohibit schema modification statements
    static const QRegularExpression schemaRegex(
        R"(^\s*(CREATE|ALTER|DROP|VACUUM|ATTACH|DETACH|REINDEX)\b)",
        QRegularExpression::CaseInsensitiveOption
    );
    if (schemaRegex.match(trimmed).hasMatch()) {
        return StatementSafety::ProhibitedSchemaModification;
    }

    // Warn before DELETE
    static const QRegularExpression deleteRegex(
        R"(^\s*DELETE\b)",
        QRegularExpression::CaseInsensitiveOption
    );
    if (deleteRegex.match(trimmed).hasMatch()) {
        return StatementSafety::NeedsDeleteWarning;
    }

    // Warn before UPDATE without WHERE
    static const QRegularExpression updateRegex(
        R"(^\s*UPDATE\b)",
        QRegularExpression::CaseInsensitiveOption
    );
    if (updateRegex.match(trimmed).hasMatch()) {
        static const QRegularExpression whereRegex(
            R"(\bWHERE\b)",
            QRegularExpression::CaseInsensitiveOption
        );
        if (!whereRegex.match(trimmed).hasMatch()) {
            return StatementSafety::NeedsUpdateWithoutWhereWarning;
        }
    }

    return StatementSafety::Safe;
}

bool DatabaseManager::executeControlledQuery(const QString &sql, QSqlQuery &outQuery, QString *errorMessage)
{
    QSqlDatabase db = database();
    if (!db.isValid() || !db.isOpen()) {
        if (errorMessage) *errorMessage = "Database is not connected.";
        return false;
    }

    StatementSafety safety = checkStatementSafety(sql);
    if (safety == StatementSafety::ProhibitedSchemaModification) {
        if (errorMessage) *errorMessage = "Schema-modifying statements (CREATE, ALTER, DROP, etc.) are prohibited in the query console.";
        return false;
    }
    if (safety == StatementSafety::MultipleStatementsProhibited) {
        if (errorMessage) *errorMessage = "Only one SQL statement may be executed at a time.";
        return false;
    }
    if (safety == StatementSafety::Empty) {
        if (errorMessage) *errorMessage = "SQL statement is empty.";
        return false;
    }

    QString trimmed = sql.trimmed();
    bool isDataModifying = (trimmed.startsWith("INSERT", Qt::CaseInsensitive) ||
                            trimmed.startsWith("UPDATE", Qt::CaseInsensitive) ||
                            trimmed.startsWith("DELETE", Qt::CaseInsensitive) ||
                            trimmed.startsWith("REPLACE", Qt::CaseInsensitive));

    if (isDataModifying) {
        if (!db.transaction()) {
            if (errorMessage) *errorMessage = QString("Failed to begin transaction: %1").arg(db.lastError().databaseText());
            return false;
        }
    }

    outQuery = QSqlQuery(db);
    if (!outQuery.exec(sql)) {
        QString err = outQuery.lastError().databaseText();
        if (err.isEmpty()) {
            err = outQuery.lastError().text();
        }
        if (isDataModifying) {
            db.rollback();
        }
        if (errorMessage) *errorMessage = err;
        return false;
    }

    if (isDataModifying) {
        if (!db.commit()) {
            QString err = db.lastError().databaseText();
            if (errorMessage) *errorMessage = QString("Failed to commit transaction: %1").arg(err);
            db.rollback();
            return false;
        }
    }

    return true;
}

} // namespace MSMS
