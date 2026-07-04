#pragma once

#include <QWidget>

class QLabel;

class TitleLineWidget : public QWidget {
  Q_OBJECT
 public:
  explicit TitleLineWidget(QWidget* parent = nullptr);

  // Sets the measurement identity shown on the title line.
  void setTitle(const QString& title);

 private:
  QLabel* label_;
};
