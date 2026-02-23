#pragma once

#include <QMainWindow>

class QCloseEvent;
class DataReceiver;
class MeasurementModel;
class QSettings;
class ReceiverControlWidget;
class TimeSeriesWidget;
class QDockWidget;
class QAction;
class QString;

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

 private:
  void loadSettings();
  void saveSettings() const;
  void applyStartupOverrides();
  QString buildWindowIdentity() const;
  QSettings createSettings() const;

  MeasurementModel* model_;
  DataReceiver* receiver_;
  ReceiverControlWidget* receiver_control_widget_;
  TimeSeriesWidget* time_series_widget_;
  QDockWidget* input_dock_;
  QAction* always_on_top_action_;
  StartupOptions startup_options_;
};
