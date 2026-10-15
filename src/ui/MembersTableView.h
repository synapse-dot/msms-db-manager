#pragma once

#include <QWidget>
#include <QString>

class QTableView;
class QSqlQueryModel;
class QSortFilterProxyModel;
class QLineEdit;
class QLabel;
class QPushButton;

namespace MSMS {

enum class MembersFilter {
    All,
    ActiveOnly
};

class MembersTableView : public QWidget {
    Q_OBJECT

public:
    explicit MembersTableView(MembersFilter filter, QWidget *parent = nullptr);
    void refresh();
    void focusSearch();
    MembersFilter filter() const { return m_filter; }

signals:
    void newMemberRequested();
    void editMemberRequested(int memberId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onEditClicked();
    void onSelectionChanged();
    void onRowDoubleClicked(const QModelIndex &index);

private:
    void setupUi();
    void loadData();

    MembersFilter m_filter;
    QTableView *m_tableView = nullptr;
    QSqlQueryModel *m_sqlModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_emptyStateLabel = nullptr;
    QPushButton *m_btnNew = nullptr;
    QPushButton *m_btnEdit = nullptr;
    QPushButton *m_btnRefresh = nullptr;
};

// AllMembersView preserves backward compatibility with Milestone 1
class AllMembersView : public MembersTableView {
    Q_OBJECT
public:
    explicit AllMembersView(QWidget *parent = nullptr)
        : MembersTableView(MembersFilter::All, parent) {}
};

} // namespace MSMS
