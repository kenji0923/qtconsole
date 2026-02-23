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
  void onSampleUpdated(double raw_value, double processed_value, double averaged_value,
                       double ratio, qint64 timestamp_ms);
  void onReferenceMinChanged(double value);
  void onReferenceMaxChanged(double value);

 private:
  MeasurementModel* model_;
  QLabel* value_label_;
  QProgressBar* ratio_bar_;
  QDoubleSpinBox* min_spin_;
  QDoubleSpinBox* max_spin_;
};
