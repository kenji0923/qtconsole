#include "title_line_widget.h"

#include <QSizePolicy>

TitleLineWidget::TitleLineWidget(QWidget* parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(0);
}
