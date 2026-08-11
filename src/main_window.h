#pragma once

#include <QMainWindow>

class QCloseEvent;
class DataReceiver;
class HistoryManager;
class MeasurementModel;
class QSettings;
class NumericRatioWidget;
class ReceiverControlWidget;
class TimeSeriesWidget;
class TitleLineWidget;
class QDockWidget;
class QAction;
class QLabel;
class QPushButton;
class QString;
struct HistoryConfig;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  struct StartupOptions {
    bool has_protocol = false;
    QString protocol;
    bool has_port = false;
    int port = 0;
    bool has_measurement_title = false;
    QString measurementTitle;
  };

  explicit MainWindow(QWidget* parent = nullptr);
  explicit MainWindow(const StartupOptions& startup_options, QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void updateWindowIdentity();
  void onSaveConfigAs();
  void onLoadConfig();
  void onToggleAlwaysOnTop(bool enabled);
  void onHistorySettings();
  void onOpenHistoryFolder();
  void onClearHistoryWarning();
  void onHistoryStatusChanged(const QString& text, bool warning);
  void onReceiverStatusChanged(bool running, const QString& message);

 private:
  void loadSettings();
  void saveSettings() const;
  void applyStartupOverrides();
  QString buildWindowIdentity() const;
  QSettings createSettings() const;
  HistoryConfig loadHistoryConfig(const QString& measurement_id) const;
  void saveHistoryConfig(const QString& measurement_id, const HistoryConfig& config) const;

  MeasurementModel* model_;
  HistoryManager* history_;
  DataReceiver* receiver_;
  TitleLineWidget* title_line_widget_;
  NumericRatioWidget* numeric_ratio_widget_;
  ReceiverControlWidget* receiver_control_widget_;
  TimeSeriesWidget* time_series_widget_;
  QDockWidget* input_dock_;
  QDockWidget* title_line_dock_;
  QAction* always_on_top_action_;
  QLabel* history_status_label_;
  QPushButton* clear_history_warning_button_;
  StartupOptions startup_options_;
};
