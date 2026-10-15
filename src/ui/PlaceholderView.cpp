#include "PlaceholderView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

namespace MSMS {

PlaceholderView::PlaceholderView(const QString &title, const QString &type, const QString &description, QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 32, 32, 32);
    mainLayout->setSpacing(16);

    QFrame *card = new QFrame(this);
    card->setProperty("panel", true);
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(12);

    QHBoxLayout *topRow = new QHBoxLayout();
    m_titleLabel = new QLabel(title, card);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    topRow->addWidget(m_titleLabel);

    topRow->addSpacing(8);

    m_typeBadge = new QLabel(type, card);
    m_typeBadge->setStyleSheet("padding: 2px 8px; border-radius: 4px; font-weight: bold; font-size: 11px;");
    topRow->addWidget(m_typeBadge);
    topRow->addStretch();
    cardLayout->addLayout(topRow);

    m_descLabel = new QLabel(description, card);
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet("font-size: 13px; line-height: 1.5;");
    cardLayout->addWidget(m_descLabel);

    QLabel *footer = new QLabel(tr("This feature is scheduled for implementation in a subsequent milestone as defined in design.md."), card);
    footer->setStyleSheet("color: palette(placeholder-text); font-size: 12px; font-style: italic;");
    cardLayout->addWidget(footer);

    mainLayout->addWidget(card);
    mainLayout->addStretch();
}

void PlaceholderView::updateDetails(const QString &title, const QString &type, const QString &description)
{
    m_titleLabel->setText(title);
    m_typeBadge->setText(type);
    m_descLabel->setText(description);
}

} // namespace MSMS
