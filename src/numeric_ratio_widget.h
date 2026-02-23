#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QDoubleSpinBox;
class MeasurementModel;

class NumericRatioWidget : public QWidget {
  Q_OBJECT
 public:
  explicit NumericRatioWidget(MeasurementModel* model, QWidget* parent = nullptr);

 private slots:
  void on_sample_updated(double value, double ratio, qint64 timestamp_ms);
  void on_reference_max_changed(double value);

 private:
  MeasurementModel* model_;
  QLabel* value_label_;
  QProgressBar* ratio_bar_;
  QDoubleSpinBox* max_spin_;
};
