#pragma once

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>
#include <QString>

/**
 * 실제 터보펌프 계측장비 대신 사용하는 콘솔 시뮬레이터 (Qt/C++ 포팅판).
 *
 * 원본: WPF 데모의 DeviceSimulator (Program.cs)
 *
 * 가상 COM 포트 페어 중 하나(예: COM11)를 열고, SerialGuiApp(예: COM10)이
 * 보내는 명령에 응답하며, 주기적으로 비동기 텔레메트리를 스스로 송신한다.
 *
 * WPF 버전과의 차이:
 *   .NET 버전은 System.Threading.Timer(별도 스레드)와 SerialPort.DataReceived
 *   이벤트(백그라운드 스레드)가 동시에 상태를 건드릴 수 있어 lock이 필요했다.
 *   Qt 버전은 QTimer와 QSerialPort::readyRead가 모두 같은 이벤트 루프
 *   스레드에서 순차적으로 실행되므로 별도의 동기화(mutex/lock)가 필요 없다.
 */
class DeviceSimulator : public QObject
{
    Q_OBJECT

public:
    explicit DeviceSimulator(const QString &portName, qint32 baudRate, QObject *parent = nullptr);

private slots:
    void onReadyRead();
    void onTelemetryTimer();

private:
    void handleCommand(const QString &command);
    QString setValve(const QString &state);
    void writeLine(const QString &message);
    static QString formatValue(double value);

    QSerialPort m_port;
    QByteArray m_receiveBuffer;

    double m_temperature = 23.0;
    double m_pressure = 101.3;
    QString m_valveState = QStringLiteral("CLOSE");

    QTimer m_telemetryTimer;
};
