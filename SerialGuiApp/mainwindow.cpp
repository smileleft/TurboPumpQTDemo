#include "mainwindow.h"

#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QDir>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("터보펌프 계측 제어 GUI (Serial Demo)"));
    resize(820, 600);
    setMinimumSize(760, 560);

    setupUi();
    setupPlot();

    refreshPorts();
    setUiState(ConnectionState::Disconnected);
}

MainWindow::~MainWindow()
{
    stopAutoSave();
}

// ---------------------------------------------------------------
// UI 구성
// ---------------------------------------------------------------

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // --- 포트 연결 ---
    auto *connectionGroup = new QGroupBox(tr("포트 연결"));
    auto *connectionLayout = new QHBoxLayout();

    connectionLayout->addWidget(new QLabel(tr("포트:")));
    m_portComboBox = new QComboBox();
    m_portComboBox->setFixedWidth(100);
    connectionLayout->addWidget(m_portComboBox);

    m_refreshPortsButton = new QPushButton(tr("새로고침"));
    m_refreshPortsButton->setFixedWidth(80);
    connectionLayout->addWidget(m_refreshPortsButton);
    connectionLayout->addSpacing(20);

    connectionLayout->addWidget(new QLabel(tr("Baud Rate:")));
    m_baudRateComboBox = new QComboBox();
    m_baudRateComboBox->setFixedWidth(90);
    m_baudRateComboBox->addItems({"9600", "19200", "38400", "57600", "115200"});
    connectionLayout->addWidget(m_baudRateComboBox);
    connectionLayout->addSpacing(20);

    m_connectButton = new QPushButton(tr("연결"));
    m_connectButton->setFixedWidth(90);
    connectionLayout->addWidget(m_connectButton);

    m_autoReconnectCheckBox = new QCheckBox(tr("자동 재연결"));
    m_autoReconnectCheckBox->setChecked(true);
    connectionLayout->addWidget(m_autoReconnectCheckBox);
    connectionLayout->addSpacing(20);

    m_statusLight = new QLabel();
    m_statusLight->setFixedSize(14, 14);
    m_statusLight->setStyleSheet("background-color: gray; border-radius: 7px;");
    connectionLayout->addWidget(m_statusLight);

    m_statusText = new QLabel(tr("연결 안 됨"));
    QFont statusFont = m_statusText->font();
    statusFont.setBold(true);
    m_statusText->setFont(statusFont);
    connectionLayout->addWidget(m_statusText);

    connectionLayout->addStretch();
    connectionGroup->setLayout(connectionLayout);
    mainLayout->addWidget(connectionGroup);

    // --- 텔레메트리 + 명령 (2단 구성) ---
    auto *midRowLayout = new QHBoxLayout();

    auto *telemetryGroup = new QGroupBox(tr("터보펌프 텔레메트리 (Telemetry)"));
    auto *telemetryLayout = new QGridLayout();

    auto addTelemetryRow = [&](int row, const QString &labelText, QLabel *&valueLabel, bool big) {
        auto *label = new QLabel(labelText);
        QFont labelFont = label->font();
        labelFont.setBold(true);
        label->setFont(labelFont);
        telemetryLayout->addWidget(label, row, 0);

        valueLabel = new QLabel("--");
        if (big) {
            QFont valueFont = valueLabel->font();
            valueFont.setPointSize(valueFont.pointSize() + 6);
            valueLabel->setFont(valueFont);
        }
        telemetryLayout->addWidget(valueLabel, row, 1);
    };

    addTelemetryRow(0, tr("온도 (°C):"), m_tempValueText, true);
    addTelemetryRow(1, tr("압력 (kPa):"), m_pressureValueText, true);
    addTelemetryRow(2, tr("밸브 상태:"), m_valveStateText, true);
    addTelemetryRow(3, tr("마지막 수신:"), m_lastUpdateText, false);
    m_lastUpdateText->setStyleSheet("color: gray;");

    telemetryGroup->setLayout(telemetryLayout);

    auto *commandGroup = new QGroupBox(tr("명령 (Telecommand)"));
    auto *commandLayout = new QVBoxLayout();

    m_getTempButton = new QPushButton(tr("온도 조회 (GET:TEMP)"));
    m_getTempButton->setFixedHeight(34);
    m_getTempButton->setEnabled(false);

    m_getPressureButton = new QPushButton(tr("압력 조회 (GET:PRESSURE)"));
    m_getPressureButton->setFixedHeight(34);
    m_getPressureButton->setEnabled(false);

    m_valveOpenButton = new QPushButton(tr("밸브 열기 (SET:VALVE:OPEN)"));
    m_valveOpenButton->setFixedHeight(34);
    m_valveOpenButton->setEnabled(false);

    m_valveCloseButton = new QPushButton(tr("밸브 닫기 (SET:VALVE:CLOSE)"));
    m_valveCloseButton->setFixedHeight(34);
    m_valveCloseButton->setEnabled(false);

    commandLayout->addWidget(m_getTempButton);
    commandLayout->addWidget(m_getPressureButton);
    commandLayout->addWidget(m_valveOpenButton);
    commandLayout->addWidget(m_valveCloseButton);
    commandLayout->addStretch();
    commandGroup->setLayout(commandLayout);

    midRowLayout->addWidget(telemetryGroup);
    midRowLayout->addWidget(commandGroup);
    mainLayout->addLayout(midRowLayout);

    // --- 실시간 그래프 ---
    auto *chartGroup = new QGroupBox(tr("실시간 그래프"));
    chartGroup->setFixedHeight(240);
    auto *chartGroupLayout = new QVBoxLayout();
    m_chartView = new QChartView();
    m_chartView->setMinimumHeight(190);
    chartGroupLayout->addWidget(m_chartView);
    chartGroup->setLayout(chartGroupLayout);
    mainLayout->addWidget(chartGroup);

    // --- 통신 로그 ---
    auto *logGroup = new QGroupBox(tr("통신 로그"));
    auto *logLayout = new QVBoxLayout();

    m_logListWidget = new QListWidget();
    QFont monoFont("Consolas");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);
    m_logListWidget->setFont(monoFont);
    logLayout->addWidget(m_logListWidget);

    auto *logButtonsLayout = new QHBoxLayout();
    logButtonsLayout->addStretch();

    m_autoSaveCheckBox = new QCheckBox(tr("자동 저장(실시간)"));
    logButtonsLayout->addWidget(m_autoSaveCheckBox);

    m_autoSavePathText = new QLabel();
    m_autoSavePathText->setStyleSheet("color: gray; font-size: 11px;");
    logButtonsLayout->addWidget(m_autoSavePathText);

    auto *saveLogButton = new QPushButton(tr("로그 저장"));
    saveLogButton->setFixedWidth(100);
    auto *clearLogButton = new QPushButton(tr("로그 지우기"));
    clearLogButton->setFixedWidth(100);

    logButtonsLayout->addWidget(saveLogButton);
    logButtonsLayout->addWidget(clearLogButton);
    logLayout->addLayout(logButtonsLayout);

    logGroup->setLayout(logLayout);
    mainLayout->addWidget(logGroup, /*stretch=*/1);

    // --- 시그널/슬롯 연결 ---
    connect(m_refreshPortsButton, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);

    connect(m_getTempButton, &QPushButton::clicked, this, &MainWindow::onGetTemp);
    connect(m_getPressureButton, &QPushButton::clicked, this, &MainWindow::onGetPressure);
    connect(m_valveOpenButton, &QPushButton::clicked, this, &MainWindow::onValveOpen);
    connect(m_valveCloseButton, &QPushButton::clicked, this, &MainWindow::onValveClose);

    connect(saveLogButton, &QPushButton::clicked, this, &MainWindow::onSaveLogClicked);
    connect(clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(m_autoSaveCheckBox, &QCheckBox::toggled, this, &MainWindow::onAutoSaveToggled);
}

void MainWindow::setupPlot()
{
    m_chart = new QChart();
    m_chart->setTitle(tr("터보펌프 텔레메트리"));

    m_tempSeries = new QLineSeries();
    m_tempSeries->setName(tr("온도(°C)"));
    m_tempSeries->setColor(QColor(255, 69, 0)); // OrangeRed

    m_pressureSeries = new QLineSeries();
    m_pressureSeries->setName(tr("압력(kPa)"));
    m_pressureSeries->setColor(QColor(70, 130, 180)); // SteelBlue

    m_chart->addSeries(m_tempSeries);
    m_chart->addSeries(m_pressureSeries);

    m_axisX = new QValueAxis();
    m_axisX->setTitleText(tr("경과 시간(초)"));
    m_axisX->setRange(0, 10);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_tempSeries->attachAxis(m_axisX);
    m_pressureSeries->attachAxis(m_axisX);

    m_axisTempY = new QValueAxis();
    m_axisTempY->setTitleText(tr("온도(°C)"));
    m_axisTempY->setLinePenColor(QColor(255, 69, 0));
    m_axisTempY->setLabelsColor(QColor(255, 69, 0));
    m_chart->addAxis(m_axisTempY, Qt::AlignLeft);
    m_tempSeries->attachAxis(m_axisTempY);

    m_axisPressureY = new QValueAxis();
    m_axisPressureY->setTitleText(tr("압력(kPa)"));
    m_axisPressureY->setLinePenColor(QColor(70, 130, 180));
    m_axisPressureY->setLabelsColor(QColor(70, 130, 180));
    m_chart->addAxis(m_axisPressureY, Qt::AlignRight);
    m_pressureSeries->attachAxis(m_axisPressureY);

    m_chartView->setChart(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    m_plotElapsedTimer.start();
}

// ---------------------------------------------------------------
// 포트 연결 / 해제
// ---------------------------------------------------------------

void MainWindow::onRefreshPorts()
{
    refreshPorts();
}

void MainWindow::refreshPorts()
{
    const QString currentSelection = m_portComboBox->currentText();
    m_portComboBox->clear();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        m_portComboBox->addItem(info.portName());
    }

    int idx = m_portComboBox->findText(currentSelection);
    if (idx >= 0) {
        m_portComboBox->setCurrentIndex(idx);
    } else if (m_portComboBox->count() > 0) {
        m_portComboBox->setCurrentIndex(0);
    }
}

void MainWindow::onConnectClicked()
{
    if (m_isReconnecting) {
        stopReconnectTimer();
        m_lastPortName.clear();
        setUiState(ConnectionState::Disconnected);
        appendLog(tr("[자동 재연결 취소됨]"));
        return;
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        disconnectPort(/*userInitiated=*/true);
        return;
    }

    tryOpenPort(/*fromUi=*/true);
}

bool MainWindow::tryOpenPort(bool fromUi)
{
    QString portName;
    qint32 baudRate;

    if (fromUi) {
        if (m_portComboBox->currentText().isEmpty()) {
            appendLog(tr("[오류] 포트를 선택하세요."));
            return false;
        }
        portName = m_portComboBox->currentText();
        baudRate = m_baudRateComboBox->currentText().toInt();
    } else {
        portName = m_lastPortName;
        baudRate = m_lastBaudRate;
        if (portName.isEmpty()) {
            return false;
        }
    }

    auto *port = new QSerialPort(this);
    port->setPortName(portName);
    port->setBaudRate(baudRate);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (!port->open(QIODevice::ReadWrite)) {
        const QString errorText = port->errorString();
        delete port;

        appendLog(fromUi
            ? tr("[오류] 포트 열기 실패: %1").arg(errorText)
            : tr("[재연결 실패 (%1회)] %2").arg(m_reconnectAttempt).arg(errorText));
        return false;
    }

    m_serialPort = port;
    connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &MainWindow::onSerialErrorOccurred);

    m_lastPortName = portName;
    m_lastBaudRate = baudRate;
    m_reconnectAttempt = 0;

    stopReconnectTimer();
    setUiState(ConnectionState::Connected);
    appendLog(fromUi
        ? tr("[연결됨] %1 @ %2bps").arg(portName).arg(baudRate)
        : tr("[재연결 성공] %1 @ %2bps").arg(portName).arg(baudRate));

    startHeartbeatMonitor();
    return true;
}

void MainWindow::disconnectPort(bool userInitiated)
{
    stopHeartbeatMonitor();

    if (m_serialPort) {
        m_serialPort->disconnect(this);
        if (m_serialPort->isOpen()) {
            m_serialPort->close();
        }
        m_serialPort->deleteLater();
        m_serialPort = nullptr;
    }

    if (userInitiated) {
        stopReconnectTimer();
        m_lastPortName.clear(); // 사용자가 직접 끊으면 자동 재연결 대상에서 제외
        setUiState(ConnectionState::Disconnected);
        appendLog(tr("[연결 해제됨]"));
        return;
    }

    appendLog(tr("[연결 끊김 감지]"));
    if (m_autoReconnectCheckBox->isChecked() && !m_lastPortName.isEmpty()) {
        startReconnectTimer();
    } else {
        setUiState(ConnectionState::Disconnected);
    }
}

void MainWindow::setUiState(ConnectionState state)
{
    const bool connected = state == ConnectionState::Connected;
    const bool reconnecting = state == ConnectionState::Reconnecting;

    switch (state) {
    case ConnectionState::Connected:
        m_connectButton->setText(tr("연결 해제"));
        break;
    case ConnectionState::Reconnecting:
        m_connectButton->setText(tr("재연결 취소"));
        break;
    default:
        m_connectButton->setText(tr("연결"));
        break;
    }

    switch (state) {
    case ConnectionState::Connected:
        m_statusText->setText(tr("연결됨"));
        break;
    case ConnectionState::Reconnecting:
        m_statusText->setText(tr("재연결 시도 중 (%1회)").arg(m_reconnectAttempt));
        break;
    default:
        m_statusText->setText(tr("연결 안 됨"));
        break;
    }

    QString color;
    switch (state) {
    case ConnectionState::Connected:
        color = QStringLiteral("limegreen");
        break;
    case ConnectionState::Reconnecting:
        color = QStringLiteral("orange");
        break;
    default:
        color = QStringLiteral("gray");
        break;
    }
    m_statusLight->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 7px;").arg(color));

    m_getTempButton->setEnabled(connected);
    m_getPressureButton->setEnabled(connected);
    m_valveOpenButton->setEnabled(connected);
    m_valveCloseButton->setEnabled(connected);

    m_portComboBox->setEnabled(!connected && !reconnecting);
    m_baudRateComboBox->setEnabled(!connected && !reconnecting);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    disconnectPort(/*userInitiated=*/true);
    stopAutoSave();
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------
// 명령 전송 (Telecommand)
// ---------------------------------------------------------------

void MainWindow::onGetTemp() { sendCommand(QStringLiteral("GET:TEMP")); }
void MainWindow::onGetPressure() { sendCommand(QStringLiteral("GET:PRESSURE")); }
void MainWindow::onValveOpen() { sendCommand(QStringLiteral("SET:VALVE:OPEN")); }
void MainWindow::onValveClose() { sendCommand(QStringLiteral("SET:VALVE:CLOSE")); }

void MainWindow::sendCommand(const QString &command)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        appendLog(tr("[오류] 포트가 연결되어 있지 않습니다."));
        return;
    }

    const QByteArray data = (command + QStringLiteral("\n")).toUtf8();
    const qint64 written = m_serialPort->write(data);

    if (written == -1) {
        appendLog(tr("[오류] 전송 실패, 연결 끊김으로 처리합니다: %1").arg(m_serialPort->errorString()));
        disconnectPort(/*userInitiated=*/false);
        return;
    }

    appendLog(QStringLiteral(">> %1").arg(command));
}

// ---------------------------------------------------------------
// 수신 처리 (Telemetry)
// ---------------------------------------------------------------

void MainWindow::onReadyRead()
{
    if (!m_serialPort) {
        return;
    }

    const QByteArray chunk = m_serialPort->readAll();
    m_lastDataReceivedMs = QDateTime::currentMSecsSinceEpoch();

    m_receiveBuffer.append(chunk);
    QList<QByteArray> lines = m_receiveBuffer.split('\n');

    // 마지막 조각은 아직 개행이 오지 않은 미완성 라인일 수 있으므로 버퍼에 남겨둔다.
    m_receiveBuffer = lines.takeLast();

    for (const QByteArray &lineBytes : std::as_const(lines)) {
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        processIncomingLine(line);
    }
}

void MainWindow::onSerialErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || !m_serialPort) {
        return;
    }

    appendLog(tr("[오류] 수신 중 예외 발생, 연결 끊김으로 처리합니다: %1").arg(m_serialPort->errorString()));
    disconnectPort(/*userInitiated=*/false);
}

void MainWindow::processIncomingLine(const QString &line)
{
    appendLog(QStringLiteral("<< %1").arg(line));

    if (line.startsWith(QStringLiteral("TEMP:"))) {
        updateTemp(line.mid(5));
    } else if (line.startsWith(QStringLiteral("PRESSURE:"))) {
        updatePressure(line.mid(9));
    } else if (line.startsWith(QStringLiteral("ACK:VALVE:"))) {
        m_valveStateText->setText(line.mid(10));
    } else if (line.startsWith(QStringLiteral("TELEMETRY:"))) {
        // 예: TELEMETRY:TEMP:23.45,PRESSURE:101.32,VALVE:OPEN
        const QString payload = line.mid(10);
        const QStringList fields = payload.split(',');
        for (const QString &field : fields) {
            const QStringList parts = field.split(':');
            if (parts.size() != 2) {
                continue;
            }
            if (parts[0] == QStringLiteral("TEMP")) {
                updateTemp(parts[1]);
            } else if (parts[0] == QStringLiteral("PRESSURE")) {
                updatePressure(parts[1]);
            } else if (parts[0] == QStringLiteral("VALVE")) {
                m_valveStateText->setText(parts[1]);
            }
        }
    }

    m_lastUpdateText->setText(QTime::currentTime().toString("HH:mm:ss"));
}

void MainWindow::updateTemp(const QString &rawValue)
{
    m_tempValueText->setText(rawValue);

    bool ok = false;
    const double value = rawValue.toDouble(&ok);
    if (ok) {
        addPlotPoint(m_tempSeries, value);
    }
}

void MainWindow::updatePressure(const QString &rawValue)
{
    m_pressureValueText->setText(rawValue);

    bool ok = false;
    const double value = rawValue.toDouble(&ok);
    if (ok) {
        addPlotPoint(m_pressureSeries, value);
    }
}

// ---------------------------------------------------------------
// 실시간 그래프
// ---------------------------------------------------------------

void MainWindow::addPlotPoint(QLineSeries *series, double value)
{
    const double x = m_plotElapsedTimer.elapsed() / 1000.0;
    series->append(x, value);

    while (series->count() > kMaxPlotPoints) {
        series->remove(0);
    }

    rescaleAxes();
}

void MainWindow::rescaleAxes()
{
    double minX = 0.0;
    double maxX = 10.0;
    bool haveX = false;

    auto scanSeries = [&](QLineSeries *series, QValueAxis *yAxis) {
        if (series->count() == 0) {
            return;
        }
        double minY = series->at(0).y();
        double maxY = minY;
        for (const auto &point : series->points()) {
            minY = std::min(minY, point.y());
            maxY = std::max(maxY, point.y());
            if (!haveX) {
                minX = maxX = point.x();
                haveX = true;
            } else {
                minX = std::min(minX, point.x());
                maxX = std::max(maxX, point.x());
            }
        }
        const double margin = (maxY - minY) * 0.1 + 0.5;
        yAxis->setRange(minY - margin, maxY + margin);
    };

    scanSeries(m_tempSeries, m_axisTempY);
    scanSeries(m_pressureSeries, m_axisPressureY);

    if (haveX) {
        if (maxX - minX < 10.0) {
            maxX = minX + 10.0;
        }
        m_axisX->setRange(minX, maxX);
    }
}

// ---------------------------------------------------------------
// 로그 UI
// ---------------------------------------------------------------

void MainWindow::appendLog(const QString &message)
{
    const QString formatted = QStringLiteral("[%1] %2")
        .arg(QTime::currentTime().toString("HH:mm:ss.zzz"), message);

    m_logListWidget->addItem(formatted);
    m_logListWidget->scrollToBottom();

    if (m_autoLogStream) {
        *m_autoLogStream << formatted << "\n";
        m_autoLogStream->flush();
    }
}

void MainWindow::onClearLogClicked()
{
    m_logListWidget->clear();
}

void MainWindow::onSaveLogClicked()
{
    const QString defaultName = QStringLiteral("SerialLog_%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("통신 로그 저장"), defaultName,
        tr("텍스트 파일 (*.txt);;모든 파일 (*.*)"));

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(tr("[오류] 로그 저장 실패: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    for (int i = 0; i < m_logListWidget->count(); ++i) {
        out << m_logListWidget->item(i)->text() << "\n";
    }
    file.close();

    appendLog(tr("[로그 저장 완료] %1").arg(fileName));
}

void MainWindow::onAutoSaveToggled(bool checked)
{
    if (checked) {
        const QString logsDirectory = QCoreApplication::applicationDirPath() + "/Logs";
        QDir().mkpath(logsDirectory);

        const QString filePath = logsDirectory + "/" + QStringLiteral("SerialLog_%1.txt")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

        m_autoLogFile = new QFile(filePath);
        if (!m_autoLogFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            appendLog(tr("[오류] 자동 저장 시작 실패: %1").arg(m_autoLogFile->errorString()));
            delete m_autoLogFile;
            m_autoLogFile = nullptr;
            m_autoSaveCheckBox->setChecked(false);
            return;
        }

        m_autoLogStream = new QTextStream(m_autoLogFile);
        m_autoSavePathText->setText(filePath);
        appendLog(tr("[자동 저장 시작] %1").arg(filePath));
    } else {
        appendLog(tr("[자동 저장 중지]"));
        stopAutoSave();
    }
}

void MainWindow::stopAutoSave()
{
    if (m_autoLogStream) {
        m_autoLogStream->flush();
        delete m_autoLogStream;
        m_autoLogStream = nullptr;
    }
    if (m_autoLogFile) {
        m_autoLogFile->close();
        delete m_autoLogFile;
        m_autoLogFile = nullptr;
    }
    m_autoSavePathText->clear();
}

// ---------------------------------------------------------------
// 자동 재연결
// ---------------------------------------------------------------

void MainWindow::startReconnectTimer()
{
    m_isReconnecting = true;
    m_reconnectAttempt = 0;
    setUiState(ConnectionState::Reconnecting);

    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        connect(m_reconnectTimer, &QTimer::timeout, this, &MainWindow::onReconnectTimerTick);
    }
    m_reconnectTimer->start(kReconnectIntervalSeconds * 1000);

    appendLog(tr("[자동 재연결] %1초 간격으로 %2 재연결을 시도합니다...")
        .arg(kReconnectIntervalSeconds).arg(m_lastPortName));
}

void MainWindow::onReconnectTimerTick()
{
    m_reconnectAttempt++;
    setUiState(ConnectionState::Reconnecting); // 시도 횟수 텍스트 갱신
    tryOpenPort(/*fromUi=*/false); // 성공하면 내부에서 StopReconnectTimer() 호출됨
}

void MainWindow::stopReconnectTimer()
{
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    m_isReconnecting = false;
}

// ---------------------------------------------------------------
// 하트비트(연결 끊김 감지)
// ---------------------------------------------------------------

void MainWindow::startHeartbeatMonitor()
{
    m_lastDataReceivedMs = QDateTime::currentMSecsSinceEpoch();

    if (!m_heartbeatTimer) {
        m_heartbeatTimer = new QTimer(this);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &MainWindow::onHeartbeatTimerTick);
    }
    m_heartbeatTimer->start(1000);
}

void MainWindow::stopHeartbeatMonitor()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
}

void MainWindow::onHeartbeatTimerTick()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        return;
    }

    const qint64 elapsedSec = (QDateTime::currentMSecsSinceEpoch() - m_lastDataReceivedMs) / 1000;
    if (elapsedSec > kHeartbeatTimeoutSeconds) {
        appendLog(tr("[경고] %1초 이상 수신 데이터 없음 — 연결 끊김으로 판단합니다.")
            .arg(kHeartbeatTimeoutSeconds));
        disconnectPort(/*userInitiated=*/false);
    }
}
