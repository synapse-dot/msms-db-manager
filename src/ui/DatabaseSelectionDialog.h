#pragma once

#include <QDialog>
#include <QString>

class QPushButton;
class QLabel;

namespace MSMS {

class DatabaseSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit DatabaseSelectionDialog(QWidget *parent = nullptr);
    QString selectedPath() const;

private slots:
    void onOpenExistingClicked();
    void onCreateNewClicked();

private:
    QString m_selectedPath;
    QPushButton *m_btnOpenExisting = nullptr;
    QPushButton *m_btnCreateNew = nullptr;
    QPushButton *m_btnCancel = nullptr;
};

} // namespace MSMS
