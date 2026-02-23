#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
QT_END_NAMESPACE

class MeasurementModel;

class TimeSeriesWidget : public QWidget {
  Q_OBJECT
 public:
  explicit TimeSeriesWidget(MeasurementModel* model, QWidget* parent = nullptr);

 private slots:
  void on_sample_updated(double value, double ratio, qint64 timestamp_ms);
  void on_history_reset();
  void on_auto_scale_toggled(bool checked);
  void on_range_edited();

 private:
  void apply_axis_range();

  MeasurementModel* model_;
  QChart* chart_;
  QChartView* chart_view_;
  QLineSeries* series_;
  QValueAxis* axis_x_;
  QValueAxis* axis_y_;

  QCheckBox* auto_scale_check_;
  QDoubleSpinBox* min_spin_;
  QDoubleSpinBox* max_spin_;
  QPushButton* reset_button_;

  qint64 window_ms_ = 10000;
};
