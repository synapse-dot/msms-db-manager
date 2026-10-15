#pragma once

#include <QWidget>

class QTableView;
class QSqlQueryModel;
class QSortFilterProxyModel;
class QLineEdit;
class QLabel;
class QPushButton;

namespace MSMS {

class AllMembersView : public QWidget {
    Q_OBJECT

public:
    explicit AllMembersView(QWidget *parent = nullptr);
    void refresh();
    void focusSearch();

private slots:
    void onSearchTextChanged(const QString &text);
    void onEditClicked();
    void onSelectionChanged();

private:
    void setupUi();
    void loadData();

    QTableView *m_tableView = nullptr;
    QSqlQueryModel *m_sqlModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_emptyStateLabel = nullptr;
    QPushButton *m_btnEdit = nullptr;
    QPushButton *m_btnRefresh = nullptr;
};

} // namespace MSMS
