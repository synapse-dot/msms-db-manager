#pragma once

#include <QWidget>
#include <QString>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

namespace MSMS {

class MeetingFormView : public QWidget {
    Q_OBJECT

public:
    explicit MeetingFormView(QWidget *parent = nullptr);

    void setCreateMode(const QString &defaultType = "AGM");
    bool loadMeeting(int meetingId);

    int currentMeetingId() const { return m_meetingId; }
    bool isCreateMode() const { return m_meetingId == -1; }
    bool isDirty() const { return m_isDirty; }

    bool maybeConfirmDiscard();

    // Validation helpers (public for testing)
    static bool validateTitle(const QString &title, QString *errorMsg = nullptr);
    static bool validateType(const QString &type, QString *errorMsg = nullptr);
    static bool validateDate(const QString &dateStr, QString *errorMsg = nullptr);
    static bool validateTimes(const QString &startTime, const QString &endTime, QString *errorMsg = nullptr);

signals:
    void meetingSaved(int meetingId);
    void meetingDeleted(int meetingId);
    void cancelRequested();

private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onDeleteClicked();
    void onFieldModified();

private:
    void setupUi();
    void setDirty(bool dirty);

    int m_meetingId = -1;
    bool m_isDirty = false;
    bool m_loading = false;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_idDisplayLabel = nullptr;
    QLabel *m_statusMessageLabel = nullptr;

    QLineEdit *m_titleEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_dateEdit = nullptr;
    QLineEdit *m_startTimeEdit = nullptr;
    QLineEdit *m_endTimeEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;

    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnDelete = nullptr;

    // Snapshot of original values
    QString m_origTitle;
    QString m_origType;
    QString m_origDate;
    QString m_origStartTime;
    QString m_origEndTime;
    QString m_origLocation;
};

} // namespace MSMS
