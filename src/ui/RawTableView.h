#pragma once

#include <QWidget>
#include <QString>

class QTableView;
class QSqlQueryModel;
class QLabel;
class QPushButton;

namespace MSMS {

class RawTableView : public QWidget {
    Q_OBJECT

public:
    explicit RawTableView(const QString &tableName, QWidget *parent = nullptr);
    void loadTable(const QString &tableName);
    void refresh();

private:
    QString m_tableName;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableView *m_tableView = nullptr;
    QSqlQueryModel *m_model = nullptr;
};

} // namespace MSMS
