#pragma once

#include <QWidget>

#include "measurement_model.h"

class QLabel;
class QPushButton;

class StatisticsWidget : public QWidget {
  Q_OBJECT
 public:
  explicit StatisticsWidget(MeasurementModel* model, QWidget* parent = nullptr);

 private slots:
  void on_statistics_updated(const MeasurementModel::Stats& stats);
  void on_start();
  void on_stop();
  void on_reset();

 private:
  MeasurementModel* model_;

  QLabel* current_;
  QLabel* average_;
  QLabel* stddev_;
  QLabel* min_;
  QLabel* max_;
  QLabel* count_;
  QLabel* rate_;

  QPushButton* start_button_;
  QPushButton* stop_button_;
  QPushButton* reset_button_;
};
