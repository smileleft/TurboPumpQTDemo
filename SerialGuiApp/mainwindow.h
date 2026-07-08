#pragma once

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QElapsedTimer>
#include <QByteArray>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class QComboBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QListWidget;
class QFile;
class QTextStream;
class QCloseEvent;

/**
 * 터보펌프 계측/제어 데모용 Serial 통신 GUI (Qt/C++ 포팅판).
 *
 * 원본: WPF SerialGuiApp (MainWindow.xaml / MainWindow.xaml.cs)
 *
 * 통신 프로토콜 (줄바꿈으로 구분되는 텍스트 라인 기반):
 *   요청(Telecommand) : "GET:TEMP", "GET:PRESSURE", "SET:VALVE:OPEN", "SET:VALVE:CLOSE"
 *   응답              : "TEMP:23.45", "PRESSURE:101.32", "ACK:VALVE:OPEN"
 *   비동기 텔레메트리 : "TELEMETRY:TEMP:23.45,PRESSURE:101.32,VALVE:OPEN"
 *
 * WPF와의 핵심 차이:
 *   .NET의 SerialPort.DataReceived는 백그라운드 스레드에서 발생하므로
 *   Dispatcher.Invoke로 UI 스레드에 넘겨야 했지만, Qt의 QSerialPort는
 *   이벤트 루프 기반 비동기 I/O라서 readyRead 시그널이 이 창을 만든
 *   스레드(=GUI 스레드)에서 그대로 발생한다. 따라서 별도의 스레드 전환
 *   코드 없이 슬롯 안에서 바로 위젯을 갱신해도 안전하다.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onRefreshPorts();
    void onConnectClicked();

    void onGetTemp();
    void onGetPressure();
    void onValveOpen();
    void onValveClose();

    void onReadyRead();
    void onSerialErrorOccurred(QSerialPort::SerialPortError error);

    void onReconnectTimerTick();
    void onHeartbeatTimerTick();

    void onSaveLogClicked();
    void onClearLogClicked();
    void onAutoSaveToggled(bool checked);

private:
    enum class ConnectionState { Disconnected, Connected, Reconnecting };

    void setupUi();
    void setupPlot();

    void refreshPorts();
    bool tryOpenPort(bool fromUi);
    void disconnectPort(bool userInitiated);
    void setUiState(ConnectionState state);

    void sendCommand(const QString &command);
    void processIncomingLine(const QString &line);
    void updateTemp(const QString &rawValue);
    void updatePressure(const QString &rawValue);

    void addPlotPoint(QLineSeries *series, double value);
    void rescaleAxes();

    void appendLog(const QString &message);

    void startReconnectTimer();
    void stopReconnectTimer();
    void startHeartbeatMonitor();
    void stopHeartbeatMonitor();
    void stopAutoSave();

    // --- 포트 연결 영역 위젯 ---
    QComboBox *m_portComboBox = nullptr;
    QPushButton *m_refreshPortsButton = nullptr;
    QComboBox *m_baudRateComboBox = nullptr;
    QPushButton *m_connectButton = nullptr;
    QCheckBox *m_autoReconnectCheckBox = nullptr;
    QLabel *m_statusLight = nullptr;
    QLabel *m_statusText = nullptr;

    // --- 텔레메트리 표시 위젯 ---
    QLabel *m_tempValueText = nullptr;
    QLabel *m_pressureValueText = nullptr;
    QLabel *m_valveStateText = nullptr;
    QLabel *m_lastUpdateText = nullptr;

    // --- 명령(Telecommand) 버튼 ---
    QPushButton *m_getTempButton = nullptr;
    QPushButton *m_getPressureButton = nullptr;
    QPushButton *m_valveOpenButton = nullptr;
    QPushButton *m_valveCloseButton = nullptr;

    // --- 실시간 그래프 (Qt Charts) ---
    QChartView *m_chartView = nullptr;
    QChart *m_chart = nullptr;
    QLineSeries *m_tempSeries = nullptr;
    QLineSeries *m_pressureSeries = nullptr;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisTempY = nullptr;
    QValueAxis *m_axisPressureY = nullptr;
    QElapsedTimer m_plotElapsedTimer;
    static constexpr int kMaxPlotPoints = 300;

    // --- 통신 로그 위젯 ---
    QCheckBox *m_autoSaveCheckBox = nullptr;
    QLabel *m_autoSavePathText = nullptr;
    QListWidget *m_logListWidget = nullptr;

    // --- 시리얼 통신 상태 ---
    QSerialPort *m_serialPort = nullptr;
    QByteArray m_receiveBuffer;

    QFile *m_autoLogFile = nullptr;
    QTextStream *m_autoLogStream = nullptr;

    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    qint64 m_lastDataReceivedMs = 0;
    QString m_lastPortName;
    qint32 m_lastBaudRate = 9600;
    bool m_isReconnecting = false;
    int m_reconnectAttempt = 0;

    static constexpr int kHeartbeatTimeoutSeconds = 6;
    static constexpr int kReconnectIntervalSeconds = 3;
};
