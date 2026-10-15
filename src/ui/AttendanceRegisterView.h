#pragma once

#include <QWidget>
#include <QList>
#include <QSet>
#include <QString>

class QComboBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

namespace MSMS {

class AttendanceRegisterView : public QWidget {
    Q_OBJECT

public:
    explicit AttendanceRegisterView(QWidget *parent = nullptr);

    bool isDirty() const { return m_isDirty; }
    bool maybeConfirmDiscard();

    // Reload meeting list and member checkboxes from DB (called after DB switch or nav)
    void loadMeetings();

    // Select a specific meeting on load
    void selectMeeting(int meetingId);

signals:
    void attendanceChanged(int meetingId);
    void cancelRequested();

private slots:
    void onMeetingChanged(int comboIndex);
    void onSaveClicked();
    void onCancelClicked();
    void onItemChanged(QListWidgetItem *item);

private:
    void setupUi();
    void loadRegister();
    void setDirty(bool dirty);

    int currentMeetingId() const;

    bool m_isDirty = false;
    bool m_loading = false;

    // The set of member IDs originally saved for the current meeting (from DB)
    QSet<int> m_savedAttendeeIds;
    // Former-member IDs that are in the saved set (must be preserved)
    QSet<int> m_formerMemberIds;

    QComboBox *m_meetingCombo = nullptr;
    QLabel *m_meetingDetailsLabel = nullptr;
    QListWidget *m_memberList = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusMessageLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;

    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnCancel = nullptr;
};

} // namespace MSMS
