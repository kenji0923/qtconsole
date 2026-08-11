#pragma once

#include <QVector>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QChart;
class QChartView;
class QCheckBox;
class QDoubleSpinBox;
class QLineSeries;
class QPushButton;
class QSettings;
class QTimer;
class QDateTimeAxis;
class QValueAxis;
QT_END_NAMESPACE

class MeasurementModel;
class HistoryManager;

class TimeSeriesWidget : public QWidget {
  Q_OBJECT
 public:
  explicit TimeSeriesWidget(MeasurementModel* model, HistoryManager* history,
                            QWidget* parent = nullptr);

  void loadSettings(QSettings* settings);
  void saveSettings(QSettings* settings) const;

 private slots:
  void onHistoryReset();
  void onAutoScaleToggled(bool checked);
  void onRangeEdited();
  void onDurationChanged(double duration_sec);
  void onPauseToggled();
  void onExportDataClicked();
  void onExportImageClicked();
  void renderFrame();

 private:
  void applyAxisRange();
  void updateXAxisRangeFor(qint64 right_ms);
  void updateXAxisAppearance(qint64 left_ms, qint64 right_ms);
  qint64 currentRightTimestampMs() const;

  MeasurementModel* model_;
  HistoryManager* history_;
  QChart* chart_;
  QChartView* chart_view_;
  QLineSeries* series_;
  QDateTimeAxis* axis_x_;
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
  qint64 paused_right_ms_ = 0;
};
