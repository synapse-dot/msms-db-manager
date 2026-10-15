#include "MemberFormView.h"
#include "database/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QFrame>

namespace MSMS {

MemberFormView::MemberFormView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setCreateMode();
}

void MemberFormView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // Title and Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(this);
    m_titleLabel->setProperty("heading", true);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(m_titleLabel);

    headerLayout->addSpacing(16);

    m_idDisplayLabel = new QLabel(this);
    m_idDisplayLabel->setStyleSheet("color: palette(placeholder-text); font-size: 13px; font-style: italic;");
    headerLayout->addWidget(m_idDisplayLabel);
    headerLayout->addStretch();

    mainLayout->addLayout(headerLayout);

    // Form Panel
    QFrame *formPanel = new QFrame(this);
    formPanel->setProperty("panel", true);
    QVBoxLayout *panelLayout = new QVBoxLayout(formPanel);
    panelLayout->setContentsMargins(20, 20, 20, 20);
    panelLayout->setSpacing(16);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setSpacing(12);

    m_nameEdit = new QLineEdit(formPanel);
    m_nameEdit->setPlaceholderText(tr("Full member name (required)"));
    connect(m_nameEdit, &QLineEdit::textChanged, this, &MemberFormView::onFieldModified);
    formLayout->addRow(tr("Name *:"), m_nameEdit);

    m_classEdit = new QLineEdit(formPanel);
    m_classEdit->setPlaceholderText(tr("e.g. 10A (optional)"));
    connect(m_classEdit, &QLineEdit::textChanged, this, &MemberFormView::onFieldModified);
    formLayout->addRow(tr("Class:"), m_classEdit);

    m_phoneEdit = new QLineEdit(formPanel);
    m_phoneEdit->setPlaceholderText(tr("e.g. 0770001001 (optional)"));
    connect(m_phoneEdit, &QLineEdit::textChanged, this, &MemberFormView::onFieldModified);
    formLayout->addRow(tr("Phone:"), m_phoneEdit);

    m_emailEdit = new QLineEdit(formPanel);
    m_emailEdit->setPlaceholderText(tr("e.g. name@example.org (optional)"));
    connect(m_emailEdit, &QLineEdit::textChanged, this, &MemberFormView::onFieldModified);
    formLayout->addRow(tr("Email:"), m_emailEdit);

    m_statusCombo = new QComboBox(formPanel);
    m_statusCombo->addItem(tr("Active"), "Active");
    m_statusCombo->addItem(tr("Former"), "Former");
    connect(m_statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MemberFormView::onFieldModified);
    formLayout->addRow(tr("Status *:"), m_statusCombo);

    m_joinDateEdit = new QLineEdit(formPanel);
    m_joinDateEdit->setPlaceholderText(tr("YYYY-MM-DD (required)"));
    connect(m_joinDateEdit, &QLineEdit::textChanged, this, &MemberFormView::onFieldModified);
    formLayout->addRow(tr("Join Date *:"), m_joinDateEdit);

    panelLayout->addLayout(formLayout);

    // Status / validation banner
    m_statusMessageLabel = new QLabel(formPanel);
    m_statusMessageLabel->setWordWrap(true);
    m_statusMessageLabel->setVisible(false);
    panelLayout->addWidget(m_statusMessageLabel);

    // Actions Row: Save, Cancel, Delete
    QHBoxLayout *actionsLayout = new QHBoxLayout();

    m_btnSave = new QPushButton(tr("💾 Save"), formPanel);
    m_btnSave->setProperty("primary", true);
    connect(m_btnSave, &QPushButton::clicked, this, &MemberFormView::onSaveClicked);
    actionsLayout->addWidget(m_btnSave);

    m_btnCancel = new QPushButton(tr("Cancel"), formPanel);
    connect(m_btnCancel, &QPushButton::clicked, this, &MemberFormView::onCancelClicked);
    actionsLayout->addWidget(m_btnCancel);

    actionsLayout->addSpacing(24);

    m_btnDelete = new QPushButton(tr("🗑️ Delete Member"), formPanel);
    m_btnDelete->setStyleSheet("color: #d32f2f;");
    connect(m_btnDelete, &QPushButton::clicked, this, &MemberFormView::onDeleteClicked);
    actionsLayout->addWidget(m_btnDelete);

    actionsLayout->addStretch();
    panelLayout->addLayout(actionsLayout);

    mainLayout->addWidget(formPanel);
    mainLayout->addStretch();
}

void MemberFormView::setCreateMode()
{
    m_loading = true;
    m_memberId = -1;

    m_titleLabel->setText(tr("New Member"));
    m_idDisplayLabel->setText(tr("(New Record)"));

    m_nameEdit->clear();
    m_classEdit->clear();
    m_phoneEdit->clear();
    m_emailEdit->clear();
    m_statusCombo->setCurrentIndex(0); // Default to 'Active'

    // Preset current date in ISO format for convenience
    m_joinDateEdit->setText(QDate::currentDate().toString("yyyy-MM-dd"));

    m_btnDelete->setVisible(false);
    m_statusMessageLabel->setVisible(false);

    m_origName.clear();
    m_origClass.clear();
    m_origPhone.clear();
    m_origEmail.clear();
    m_origStatus = "Active";
    m_origJoinDate = m_joinDateEdit->text();

    m_loading = false;
    setDirty(false);
    m_nameEdit->setFocus();
}

bool MemberFormView::loadMember(int memberId)
{
    if (memberId <= 0) {
        setCreateMode();
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT member_id, name, class, phone, email, status, join_date FROM members WHERE member_id = :id;");
    q.bindValue(":id", memberId);

    if (!q.exec() || !q.next()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load member with ID %1.").arg(memberId));
        return false;
    }

    m_loading = true;
    m_memberId = memberId;

    m_titleLabel->setText(tr("Edit Member"));
    m_idDisplayLabel->setText(tr("Member ID: %1").arg(m_memberId));

    m_origName = q.value("name").toString();
    m_origClass = q.value("class").toString();
    m_origPhone = q.value("phone").toString();
    m_origEmail = q.value("email").toString();
    m_origStatus = q.value("status").toString();
    m_origJoinDate = q.value("join_date").toString();

    m_nameEdit->setText(m_origName);
    m_classEdit->setText(m_origClass);
    m_phoneEdit->setText(m_origPhone);
    m_emailEdit->setText(m_origEmail);

    int statusIdx = m_statusCombo->findData(m_origStatus);
    if (statusIdx >= 0) {
        m_statusCombo->setCurrentIndex(statusIdx);
    } else {
        m_statusCombo->setCurrentIndex(0);
    }

    m_joinDateEdit->setText(m_origJoinDate);

    m_btnDelete->setVisible(true);
    m_statusMessageLabel->setVisible(false);

    m_loading = false;
    setDirty(false);
    return true;
}

void MemberFormView::onFieldModified()
{
    if (m_loading) return;
    setDirty(true);
}

void MemberFormView::setDirty(bool dirty)
{
    m_isDirty = dirty;
    if (m_isDirty) {
        m_titleLabel->setText(m_memberId == -1 ? tr("New Member *") : tr("Edit Member *"));
    } else {
        m_titleLabel->setText(m_memberId == -1 ? tr("New Member") : tr("Edit Member"));
    }
}

bool MemberFormView::maybeConfirmDiscard()
{
    if (!m_isDirty) {
        return true;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("You have unsaved changes in the Member form.\n\nDo you want to discard your changes?"),
        QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel
    );

    if (reply == QMessageBox::Discard) {
        setDirty(false);
        return true;
    }

    return false;
}

bool MemberFormView::validateName(const QString &name, QString *errorMsg)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMsg) *errorMsg = tr("Name is required and cannot be blank.");
        return false;
    }
    return true;
}

bool MemberFormView::validateJoinDate(const QString &dateStr, QString *errorMsg)
{
    QString trimmed = dateStr.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMsg) *errorMsg = tr("Join Date is required.");
        return false;
    }

    QDate dt = QDate::fromString(trimmed, Qt::ISODate);
    if (!dt.isValid()) {
        if (errorMsg) *errorMsg = tr("Join Date must be a valid date in YYYY-MM-DD format.");
        return false;
    }

    if (dt.year() < 1900 || dt.year() > 2100) {
        if (errorMsg) *errorMsg = tr("Join Date year must be between 1900 and 2100.");
        return false;
    }

    return true;
}

bool MemberFormView::validateEmail(const QString &email, QString *errorMsg)
{
    QString trimmed = email.trimmed();
    if (trimmed.isEmpty()) {
        return true; // Optional field
    }

    static const QRegularExpression emailRegex(
        R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)"
    );

    if (!emailRegex.match(trimmed).hasMatch()) {
        if (errorMsg) *errorMsg = tr("Email address format is invalid (expected name@domain.tld).");
        return false;
    }

    return true;
}

bool MemberFormView::validatePhone(const QString &phone, QString *errorMsg)
{
    QString trimmed = phone.trimmed();
    if (trimmed.isEmpty()) {
        return true; // Optional field
    }

    // Must contain digits and only allowed phone symbols
    static const QRegularExpression phoneRegex(R"(^[0-9+() -]+$)");
    if (!phoneRegex.match(trimmed).hasMatch()) {
        if (errorMsg) *errorMsg = tr("Phone number contains invalid characters.");
        return false;
    }

    // Must have at least 5 digits
    int digitCount = 0;
    for (const QChar &c : trimmed) {
        if (c.isDigit()) digitCount++;
    }

    if (digitCount < 5) {
        if (errorMsg) *errorMsg = tr("Phone number is too short (minimum 5 digits).");
        return false;
    }

    return true;
}

bool MemberFormView::checkDuplicateName(const QString &name)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT member_id, name FROM members WHERE LOWER(TRIM(name)) = LOWER(TRIM(:name)) AND (:id IS NULL OR member_id != :id);");
    q.bindValue(":name", name.trimmed());
    q.bindValue(":id", m_memberId > 0 ? QVariant(m_memberId) : QVariant());

    if (q.exec() && q.next()) {
        return true; // Found matching member
    }
    return false;
}

void MemberFormView::onSaveClicked()
{
    QString name = m_nameEdit->text().trimmed();
    QString className = m_classEdit->text().trimmed();
    QString phone = m_phoneEdit->text().trimmed();
    QString email = m_emailEdit->text().trimmed();
    QString status = m_statusCombo->currentData().toString();
    QString joinDate = m_joinDateEdit->text().trimmed();

    QString valErr;

    // 1. Validation checks
    if (!validateName(name, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_nameEdit->setFocus();
        return;
    }

    if (!validateJoinDate(joinDate, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_joinDateEdit->setFocus();
        return;
    }

    if (!validateEmail(email, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_emailEdit->setFocus();
        return;
    }

    if (!validatePhone(phone, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_phoneEdit->setFocus();
        return;
    }

    // 2. Duplicate detection warning
    if (checkDuplicateName(name)) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            tr("Duplicate Member Name Warning"),
            tr("A member with the name \"%1\" already exists in the database.\n\n"
               "Member names are not required to be unique, but this may indicate an accidental duplicate.\n\n"
               "Do you wish to proceed with saving?").arg(name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply != QMessageBox::Yes) {
            // Keep form open with entered values
            return;
        }
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    bool isNew = (m_memberId == -1);

    if (isNew) {
        q.prepare(
            "INSERT INTO members (name, class, phone, email, status, join_date) "
            "VALUES (:name, :class, :phone, :email, :status, :join_date);"
        );
    } else {
        q.prepare(
            "UPDATE members "
            "SET name = :name, class = :class, phone = :phone, email = :email, "
            "    status = :status, join_date = :join_date "
            "WHERE member_id = :id;"
        );
        q.bindValue(":id", m_memberId);
    }

    q.bindValue(":name", name);
    q.bindValue(":class", className.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : className);
    q.bindValue(":phone", phone.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : phone);
    q.bindValue(":email", email.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : email);
    q.bindValue(":status", status);
    q.bindValue(":join_date", joinDate);

    if (!q.exec()) {
        // Failed save: preserve entered values, show database error
        QString dbErr = q.lastError().databaseText();
        if (dbErr.isEmpty()) dbErr = q.lastError().text();

        m_statusMessageLabel->setText(tr("Save failed: %1").arg(dbErr));
        m_statusMessageLabel->setStyleSheet("color: #d32f2f; font-weight: bold; padding: 4px;");
        m_statusMessageLabel->setVisible(true);

        QMessageBox::critical(this, tr("Database Error"), tr("Failed to save member record:\n\n%1").arg(dbErr));
        return;
    }

    if (isNew) {
        m_memberId = q.lastInsertId().toInt();
    }

    // Keep form open, refresh snapshots and dirty flag
    m_origName = name;
    m_origClass = className;
    m_origPhone = phone;
    m_origEmail = email;
    m_origStatus = status;
    m_origJoinDate = joinDate;

    setDirty(false);

    m_titleLabel->setText(tr("Edit Member"));
    m_idDisplayLabel->setText(tr("Member ID: %1").arg(m_memberId));
    m_btnDelete->setVisible(true);

    m_statusMessageLabel->setText(tr("✓ Member saved successfully."));
    m_statusMessageLabel->setStyleSheet("color: #2e7d32; font-weight: bold; padding: 4px;");
    m_statusMessageLabel->setVisible(true);

    emit memberSaved(m_memberId);
}

void MemberFormView::onCancelClicked()
{
    if (m_isDirty) {
        if (!maybeConfirmDiscard()) {
            return;
        }
    }

    if (m_memberId == -1) {
        // Clear new form
        setCreateMode();
    } else {
        // Reload original values
        loadMember(m_memberId);
    }

    emit cancelRequested();
}

void MemberFormView::onDeleteClicked()
{
    if (m_memberId <= 0) return;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Deletion"),
        tr("Are you sure you want to delete member \"%1\" (ID: %2)?\n\n"
           "This action permanently deletes the member record.").arg(m_origName).arg(m_memberId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("DELETE FROM members WHERE member_id = :id;");
    q.bindValue(":id", m_memberId);

    if (!q.exec()) {
        QString errText = q.lastError().text();
        QString dbText = q.lastError().databaseText();

        // Foreign key restriction protection
        if (errText.contains("FOREIGN KEY", Qt::CaseInsensitive) || dbText.contains("FOREIGN KEY", Qt::CaseInsensitive)) {
            QMessageBox::critical(
                this,
                tr("Cannot Delete Member"),
                tr("Cannot delete member \"%1\" because historical records (such as meeting attendance, "
                   "project membership, committee appointments, or competition participation) refer to this member.\n\n"
                   "To preserve society history and data integrity, members referenced by existing records cannot be deleted.\n\n"
                   "You may change their status to 'Former' instead.").arg(m_origName)
            );
        } else {
            QMessageBox::critical(
                this,
                tr("Database Error"),
                tr("Failed to delete member:\n\n%1").arg(dbText.isEmpty() ? errText : dbText)
            );
        }
        return;
    }

    int deletedId = m_memberId;
    setCreateMode();
    emit memberDeleted(deletedId);
}

} // namespace MSMS
