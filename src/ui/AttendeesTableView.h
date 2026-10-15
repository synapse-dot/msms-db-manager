#pragma once

#include <QWidget>
#include <QList>

class QTableView;
class QSqlQueryModel;
class QSortFilterProxyModel;
class QLineEdit;
class QLabel;
class QPushButton;
class QSpinBox;

namespace MSMS {

class AttendeesTableView : public QWidget {
    Q_OBJECT

public:
    explicit AttendeesTableView(QWidget *parent = nullptr);
    void refresh();
    void focusSearch();
    int currentMeetingId() const { return m_currentMeetingId; }
    void selectMeeting(int meetingId);

signals:
    void openRegisterRequested(int meetingId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onPrevMeeting();
    void onNextMeeting();
    void onMeetingIdEntered();

private:
    void setupUi();
    void loadMeetingList();
    void loadCurrentMeeting();

    QList<int> m_meetingIds;
    int m_currentIndex = -1;
    int m_currentMeetingId = -1;

    QLabel *m_meetingTitleLabel = nullptr;
    QLabel *m_meetingDetailsLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_emptyAttendeesLabel = nullptr;
    QLabel *m_noMeetingsLabel = nullptr;

    QTableView *m_tableView = nullptr;
    QSqlQueryModel *m_sqlModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
    QLineEdit *m_searchEdit = nullptr;

    QPushButton *m_btnPrev = nullptr;
    QPushButton *m_btnNext = nullptr;
    QPushButton *m_btnManageRegister = nullptr;
    QPushButton *m_btnRefresh = nullptr;

    QLineEdit *m_meetingIdInput = nullptr;
};

} // namespace MSMS
