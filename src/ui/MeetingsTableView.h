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

class MeetingsTableView : public QWidget {
    Q_OBJECT

public:
    explicit MeetingsTableView(const QString &meetingType, QWidget *parent = nullptr);
    void refresh();
    void focusSearch();
    QString meetingType() const { return m_meetingType; }

signals:
    void newMeetingRequested(const QString &defaultType);
    void editMeetingRequested(int meetingId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onEditClicked();
    void onSelectionChanged();
    void onRowDoubleClicked(const QModelIndex &index);

private:
    void setupUi();
    void loadData();

    QString m_meetingType; // "AGM", "EGM", or "ECM"
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

} // namespace MSMS
