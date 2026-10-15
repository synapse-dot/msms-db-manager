#pragma once

#include <QWidget>
#include <QString>
#include <QDate>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

namespace MSMS {

class MemberFormView : public QWidget {
    Q_OBJECT

public:
    explicit MemberFormView(QWidget *parent = nullptr);

    void setCreateMode();
    bool loadMember(int memberId);

    int currentMemberId() const { return m_memberId; }
    bool isCreateMode() const { return m_memberId == -1; }
    bool isDirty() const { return m_isDirty; }

    bool maybeConfirmDiscard();

    // Validation helpers (public for testing)
    static bool validateName(const QString &name, QString *errorMsg = nullptr);
    static bool validateJoinDate(const QString &dateStr, QString *errorMsg = nullptr);
    static bool validateEmail(const QString &email, QString *errorMsg = nullptr);
    static bool validatePhone(const QString &phone, QString *errorMsg = nullptr);

signals:
    void memberSaved(int memberId);
    void memberDeleted(int memberId);
    void cancelRequested();

private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onDeleteClicked();
    void onFieldModified();

private:
    void setupUi();
    void setDirty(bool dirty);
    bool checkDuplicateName(const QString &name);

    int m_memberId = -1;
    bool m_isDirty = false;
    bool m_loading = false;

    // UI elements
    QLabel *m_titleLabel = nullptr;
    QLabel *m_idDisplayLabel = nullptr;
    QLabel *m_statusMessageLabel = nullptr;

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_classEdit = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QComboBox *m_statusCombo = nullptr;
    QLineEdit *m_joinDateEdit = nullptr;

    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnDelete = nullptr;

    // Snapshot of original values for Cancel rollback
    QString m_origName;
    QString m_origClass;
    QString m_origPhone;
    QString m_origEmail;
    QString m_origStatus;
    QString m_origJoinDate;
};

} // namespace MSMS
