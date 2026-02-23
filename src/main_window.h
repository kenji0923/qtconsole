#pragma once

#include <QMainWindow>

class MeasurementModel;
class DataReceiver;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private:
  MeasurementModel* model_;
  DataReceiver* receiver_;
};
