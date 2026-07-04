#include "title_line_widget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>

TitleLineWidget::TitleLineWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 4, 10, 4);

  label_ = new QLabel(this);
  QFont font = label_->font();
  font.setBold(true);
  font.setPointSizeF(font.pointSizeF() * 1.2);
  label_->setFont(font);
  label_->setAlignment(Qt::AlignCenter);
  label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  layout->addWidget(label_);

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void TitleLineWidget::setTitle(const QString& title) {
  label_->setText(title);
}
