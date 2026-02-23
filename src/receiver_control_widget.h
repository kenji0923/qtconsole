#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class DataReceiver;

class ReceiverControlWidget : public QWidget {
  Q_OBJECT
 public:
  explicit ReceiverControlWidget(DataReceiver* receiver, QWidget* parent = nullptr);

 private slots:
  void on_start();
  void on_stop();
  void on_status_changed(bool running, const QString& message);

 private:
  DataReceiver* receiver_;
  QComboBox* mode_combo_;
  QSpinBox* port_spin_;
  QPushButton* start_button_;
  QPushButton* stop_button_;
  QLabel* status_label_;
};
