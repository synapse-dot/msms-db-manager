#pragma once

#include <QWidget>
#include <QString>

class QLineEdit;
class QPushButton;
class QTableView;
class QSqlQueryModel;
class QLabel;

namespace MSMS {

class QueryResultView : public QWidget {
    Q_OBJECT

public:
    explicit QueryResultView(QWidget *parent = nullptr);
    void displaySelectResult(QSqlQueryModel *model, const QString &query, int rowCount);
    void displayModificationResult(const QString &query, int rowsAffected);
    void displayError(const QString &query, const QString &errorMessage);

private:
    QLabel *m_queryHeaderLabel = nullptr;
    QLabel *m_statusBadge = nullptr;
    QLabel *m_statusMessageLabel = nullptr;
    QTableView *m_tableView = nullptr;
};

class QueryEntryBar : public QWidget {
    Q_OBJECT

public:
    explicit QueryEntryBar(QWidget *parent = nullptr);
    void focusInput();
    QString currentQuery() const;
    void setQuery(const QString &query);

signals:
    void executeQueryRequested(const QString &sql);
    void openDialogRequested();

private slots:
    void onExecuteClicked();

private:
    QLineEdit *m_lineEdit = nullptr;
    QPushButton *m_btnExecute = nullptr;
    QPushButton *m_btnDialog = nullptr;
};

} // namespace MSMS
