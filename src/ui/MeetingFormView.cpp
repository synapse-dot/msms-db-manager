#include "MeetingFormView.h"
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
#include <QDate>
#include <QTime>
#include <QRegularExpression>
#include <QFrame>

namespace MSMS {

MeetingFormView::MeetingFormView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setCreateMode();
}

void MeetingFormView::setupUi()
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

    m_titleEdit = new QLineEdit(formPanel);
    m_titleEdit->setPlaceholderText(tr("Meeting title (required)"));
    connect(m_titleEdit, &QLineEdit::textChanged, this, &MeetingFormView::onFieldModified);
    formLayout->addRow(tr("Title *:"), m_titleEdit);

    m_typeCombo = new QComboBox(formPanel);
    m_typeCombo->addItem(tr("AGM — Annual General Meeting"), "AGM");
    m_typeCombo->addItem(tr("EGM — Extraordinary General Meeting"), "EGM");
    m_typeCombo->addItem(tr("ECM — Executive Committee Meeting"), "ECM");
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeetingFormView::onFieldModified);
    formLayout->addRow(tr("Type *:"), m_typeCombo);

    m_dateEdit = new QLineEdit(formPanel);
    m_dateEdit->setPlaceholderText(tr("YYYY-MM-DD (optional for tentative meetings)"));
    connect(m_dateEdit, &QLineEdit::textChanged, this, &MeetingFormView::onFieldModified);
    formLayout->addRow(tr("Meeting Date:"), m_dateEdit);

    m_startTimeEdit = new QLineEdit(formPanel);
    m_startTimeEdit->setPlaceholderText(tr("HH:MM in 24-hour format, e.g. 14:00 (optional)"));
    connect(m_startTimeEdit, &QLineEdit::textChanged, this, &MeetingFormView::onFieldModified);
    formLayout->addRow(tr("Start Time:"), m_startTimeEdit);

    m_endTimeEdit = new QLineEdit(formPanel);
    m_endTimeEdit->setPlaceholderText(tr("HH:MM in 24-hour format, e.g. 15:30 (optional)"));
    connect(m_endTimeEdit, &QLineEdit::textChanged, this, &MeetingFormView::onFieldModified);
    formLayout->addRow(tr("End Time:"), m_endTimeEdit);

    m_locationEdit = new QLineEdit(formPanel);
    m_locationEdit->setPlaceholderText(tr("Meeting location, e.g. Science Laboratory (optional)"));
    connect(m_locationEdit, &QLineEdit::textChanged, this, &MeetingFormView::onFieldModified);
    formLayout->addRow(tr("Location:"), m_locationEdit);

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
    connect(m_btnSave, &QPushButton::clicked, this, &MeetingFormView::onSaveClicked);
    actionsLayout->addWidget(m_btnSave);

    m_btnCancel = new QPushButton(tr("Cancel"), formPanel);
    connect(m_btnCancel, &QPushButton::clicked, this, &MeetingFormView::onCancelClicked);
    actionsLayout->addWidget(m_btnCancel);

    actionsLayout->addSpacing(24);

    m_btnDelete = new QPushButton(tr("🗑️ Delete Meeting"), formPanel);
    m_btnDelete->setStyleSheet("color: #d32f2f;");
    connect(m_btnDelete, &QPushButton::clicked, this, &MeetingFormView::onDeleteClicked);
    actionsLayout->addWidget(m_btnDelete);

    actionsLayout->addStretch();
    panelLayout->addLayout(actionsLayout);

    mainLayout->addWidget(formPanel);
    mainLayout->addStretch();
}

void MeetingFormView::setCreateMode(const QString &defaultType)
{
    m_loading = true;
    m_meetingId = -1;

    m_titleLabel->setText(tr("New Meeting"));
    m_idDisplayLabel->setText(tr("(New Record)"));

    m_titleEdit->clear();

    int idx = m_typeCombo->findData(defaultType);
    if (idx >= 0) {
        m_typeCombo->setCurrentIndex(idx);
    } else {
        m_typeCombo->setCurrentIndex(0);
    }

    m_dateEdit->clear();
    m_startTimeEdit->clear();
    m_endTimeEdit->clear();
    m_locationEdit->clear();

    m_btnDelete->setVisible(false);
    m_statusMessageLabel->setVisible(false);

    m_origTitle.clear();
    m_origType = m_typeCombo->currentData().toString();
    m_origDate.clear();
    m_origStartTime.clear();
    m_origEndTime.clear();
    m_origLocation.clear();

    m_loading = false;
    setDirty(false);
    m_titleEdit->setFocus();
}

bool MeetingFormView::loadMeeting(int meetingId)
{
    if (meetingId <= 0) {
        setCreateMode();
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT meeting_id, title, type, meeting_date, start_time, end_time, location FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", meetingId);

    if (!q.exec() || !q.next()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load meeting with ID %1.").arg(meetingId));
        return false;
    }

    m_loading = true;
    m_meetingId = meetingId;

    m_titleLabel->setText(tr("Edit Meeting"));
    m_idDisplayLabel->setText(tr("Meeting ID: %1").arg(m_meetingId));

    m_origTitle = q.value("title").toString();
    m_origType = q.value("type").toString();
    m_origDate = q.value("meeting_date").toString();
    m_origStartTime = q.value("start_time").toString();
    m_origEndTime = q.value("end_time").toString();
    m_origLocation = q.value("location").toString();

    m_titleEdit->setText(m_origTitle);

    int idx = m_typeCombo->findData(m_origType);
    if (idx >= 0) {
        m_typeCombo->setCurrentIndex(idx);
    } else {
        m_typeCombo->setCurrentIndex(0);
    }

    m_dateEdit->setText(m_origDate);
    m_startTimeEdit->setText(m_origStartTime);
    m_endTimeEdit->setText(m_origEndTime);
    m_locationEdit->setText(m_origLocation);

    m_btnDelete->setVisible(true);
    m_statusMessageLabel->setVisible(false);

    m_loading = false;
    setDirty(false);
    return true;
}

void MeetingFormView::onFieldModified()
{
    if (m_loading) return;
    setDirty(true);
}

void MeetingFormView::setDirty(bool dirty)
{
    m_isDirty = dirty;
    if (m_isDirty) {
        m_titleLabel->setText(m_meetingId == -1 ? tr("New Meeting *") : tr("Edit Meeting *"));
    } else {
        m_titleLabel->setText(m_meetingId == -1 ? tr("New Meeting") : tr("Edit Meeting"));
    }
}

bool MeetingFormView::maybeConfirmDiscard()
{
    if (!m_isDirty) {
        return true;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("You have unsaved changes in the Meeting form.\n\nDo you want to discard your changes?"),
        QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel
    );

    if (reply == QMessageBox::Discard) {
        setDirty(false);
        return true;
    }

    return false;
}

bool MeetingFormView::validateTitle(const QString &title, QString *errorMsg)
{
    QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMsg) *errorMsg = tr("Title is required and cannot be blank.");
        return false;
    }
    return true;
}

bool MeetingFormView::validateType(const QString &type, QString *errorMsg)
{
    if (type != "AGM" && type != "EGM" && type != "ECM") {
        if (errorMsg) *errorMsg = tr("Meeting type must be AGM, EGM, or ECM.");
        return false;
    }
    return true;
}

bool MeetingFormView::validateDate(const QString &dateStr, QString *errorMsg)
{
    QString trimmed = dateStr.trimmed();
    if (trimmed.isEmpty()) {
        return true; // Optional for tentative meetings
    }

    QDate dt = QDate::fromString(trimmed, Qt::ISODate);
    if (!dt.isValid()) {
        if (errorMsg) *errorMsg = tr("Meeting Date must be a valid date in YYYY-MM-DD format.");
        return false;
    }

    if (dt.year() < 1900 || dt.year() > 2100) {
        if (errorMsg) *errorMsg = tr("Meeting Date year must be between 1900 and 2100.");
        return false;
    }

    return true;
}

bool MeetingFormView::validateTimes(const QString &startTime, const QString &endTime, QString *errorMsg)
{
    QString st = startTime.trimmed();
    QString et = endTime.trimmed();

    static const QRegularExpression timeRegex(R"(^(?:[01]\d|2[0-3]):[0-5]\d$)");

    if (!st.isEmpty() && !timeRegex.match(st).hasMatch()) {
        if (errorMsg) *errorMsg = tr("Start Time must be in valid 24-hour HH:MM format (e.g. 14:00).");
        return false;
    }

    if (!et.isEmpty() && !timeRegex.match(et).hasMatch()) {
        if (errorMsg) *errorMsg = tr("End Time must be in valid 24-hour HH:MM format (e.g. 15:30).");
        return false;
    }

    // Both present check
    if (!st.isEmpty() && !et.isEmpty()) {
        if (et < st) {
            if (errorMsg) *errorMsg = tr("End Time cannot be earlier than Start Time (%1 < %2).").arg(et, st);
            return false;
        }
    }

    return true;
}

void MeetingFormView::onSaveClicked()
{
    QString title = m_titleEdit->text().trimmed();
    QString type = m_typeCombo->currentData().toString();
    QString dateStr = m_dateEdit->text().trimmed();
    QString startTime = m_startTimeEdit->text().trimmed();
    QString endTime = m_endTimeEdit->text().trimmed();
    QString location = m_locationEdit->text().trimmed();

    QString valErr;

    // 1. Validation
    if (!validateTitle(title, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_titleEdit->setFocus();
        return;
    }

    if (!validateType(type, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_typeCombo->setFocus();
        return;
    }

    if (!validateDate(dateStr, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_dateEdit->setFocus();
        return;
    }

    if (!validateTimes(startTime, endTime, &valErr)) {
        QMessageBox::warning(this, tr("Validation Error"), valErr);
        m_endTimeEdit->setFocus();
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    bool isNew = (m_meetingId == -1);

    if (isNew) {
        q.prepare(
            "INSERT INTO meetings (title, type, meeting_date, start_time, end_time, location) "
            "VALUES (:title, :type, :date, :start_time, :end_time, :location);"
        );
    } else {
        q.prepare(
            "UPDATE meetings "
            "SET title = :title, type = :type, meeting_date = :date, "
            "    start_time = :start_time, end_time = :end_time, location = :location "
            "WHERE meeting_id = :id;"
        );
        q.bindValue(":id", m_meetingId);
    }

    q.bindValue(":title", title);
    q.bindValue(":type", type);
    q.bindValue(":date", dateStr.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : dateStr);
    q.bindValue(":start_time", startTime.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : startTime);
    q.bindValue(":end_time", endTime.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : endTime);
    q.bindValue(":location", location.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : location);

    if (!q.exec()) {
        QString dbErr = q.lastError().databaseText();
        if (dbErr.isEmpty()) dbErr = q.lastError().text();

        m_statusMessageLabel->setText(tr("Save failed: %1").arg(dbErr));
        m_statusMessageLabel->setStyleSheet("color: #d32f2f; font-weight: bold; padding: 4px;");
        m_statusMessageLabel->setVisible(true);

        QMessageBox::critical(this, tr("Database Error"), tr("Failed to save meeting record:\n\n%1").arg(dbErr));
        return;
    }

    if (isNew) {
        m_meetingId = q.lastInsertId().toInt();
    }

    // Keep form open, update original snapshot
    m_origTitle = title;
    m_origType = type;
    m_origDate = dateStr;
    m_origStartTime = startTime;
    m_origEndTime = endTime;
    m_origLocation = location;

    setDirty(false);

    m_titleLabel->setText(tr("Edit Meeting"));
    m_idDisplayLabel->setText(tr("Meeting ID: %1").arg(m_meetingId));
    m_btnDelete->setVisible(true);

    m_statusMessageLabel->setText(tr("✓ Meeting saved successfully."));
    m_statusMessageLabel->setStyleSheet("color: #2e7d32; font-weight: bold; padding: 4px;");
    m_statusMessageLabel->setVisible(true);

    emit meetingSaved(m_meetingId);
}

void MeetingFormView::onCancelClicked()
{
    if (m_isDirty) {
        if (!maybeConfirmDiscard()) {
            return;
        }
    }

    if (m_meetingId == -1) {
        setCreateMode();
    } else {
        loadMeeting(m_meetingId);
    }

    emit cancelRequested();
}

void MeetingFormView::onDeleteClicked()
{
    if (m_meetingId <= 0) return;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Deletion"),
        tr("Are you sure you want to delete meeting \"%1\" (ID: %2)?\n\n"
           "This action cannot be undone.").arg(m_origTitle).arg(m_meetingId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("DELETE FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", m_meetingId);

    if (!q.exec()) {
        QString errText = q.lastError().text();
        QString dbText = q.lastError().databaseText();

        if (errText.contains("FOREIGN KEY", Qt::CaseInsensitive) || dbText.contains("FOREIGN KEY", Qt::CaseInsensitive)) {
            QMessageBox::critical(
                this,
                tr("Cannot Delete Meeting"),
                tr("Cannot delete meeting \"%1\" because attendance records are associated with it.\n\n"
                   "To preserve data integrity and historical attendance records, meetings with recorded attendance cannot be deleted.").arg(m_origTitle)
            );
        } else {
            QMessageBox::critical(
                this,
                tr("Database Error"),
                tr("Failed to delete meeting:\n\n%1").arg(dbText.isEmpty() ? errText : dbText)
            );
        }
        return;
    }

    int deletedId = m_meetingId;
    setCreateMode();
    emit meetingDeleted(deletedId);
}

} // namespace MSMS
