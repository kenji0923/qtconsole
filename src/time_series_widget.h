#pragma once

#include <QVector>
#include <QWidget>
#include <deque>

QT_BEGIN_NAMESPACE
class QChart;
class QChartView;
class QCheckBox;
class QDoubleSpinBox;
class QLineSeries;
class QPushButton;
class QSettings;
class QTimer;
class QValueAxis;
QT_END_NAMESPACE

class MeasurementModel;

class TimeSeriesWidget : public QWidget {
  Q_OBJECT
 public:
  explicit TimeSeriesWidget(MeasurementModel* model, QWidget* parent = nullptr);

  void loadSettings(QSettings* settings);
  void saveSettings(QSettings* settings) const;

 private slots:
  void onSampleUpdated(double raw_value, double processed_value, double averaged_value,
                       double ratio, qint64 timestamp_ms);
  void onHistoryReset();
  void onAutoScaleToggled(bool checked);
  void onRangeEdited();
  void onDurationChanged(double duration_sec);
  void onPauseToggled();
  void onExportDataClicked();
  void onExportImageClicked();
  void renderFrame();

 private:
  struct SamplePoint {
    qint64 timestamp_ms = 0;
    double raw_value = 0.0;
    double processed_value = 0.0;
    double averaged_value = 0.0;
  };

  void applyAxisRange();
  void updateXAxisRangeFor(qreal right_sec);
  QVector<QPointF> buildDisplayPoints() const;

  MeasurementModel* model_;
  QChart* chart_;
  QChartView* chart_view_;
  QLineSeries* series_;
  QValueAxis* axis_x_;
  QValueAxis* axis_y_;

  QCheckBox* auto_scale_check_;
  QDoubleSpinBox* min_spin_;
  QDoubleSpinBox* max_spin_;
  QDoubleSpinBox* duration_spin_;
  QPushButton* reset_button_;
  QPushButton* pause_button_;
  QPushButton* export_data_button_;
  QPushButton* export_image_button_;
  QTimer* render_timer_;

  qint64 window_ms_ = 10000;
  bool paused_ = false;
  QVector<SamplePoint> samples_;
  std::deque<SamplePoint> window_samples_;
};
