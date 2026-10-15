#pragma once

#include <QWidget>
#include <QString>

class QLabel;

namespace MSMS {

class PlaceholderView : public QWidget {
    Q_OBJECT

public:
    explicit PlaceholderView(const QString &title, const QString &type, const QString &description, QWidget *parent = nullptr);
    void updateDetails(const QString &title, const QString &type, const QString &description);

private:
    QLabel *m_titleLabel = nullptr;
    QLabel *m_typeBadge = nullptr;
    QLabel *m_descLabel = nullptr;
};

} // namespace MSMS
