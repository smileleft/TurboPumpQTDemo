#include "devicesimulator.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QRandomGenerator>

namespace {
QTextStream &out()
{
    static QTextStream stream(stdout);
    // Windows 콘솔(cmd.exe)의 기본 로캘 코드페이지(CP949 등) 대신 항상 UTF-8로
    // 인코딩해서 씀. main()에서 SetConsoleOutputCP(CP_UTF8)로 콘솔 쪽 코드페이지도
    // UTF-8로 맞춰줘야 한글이 정상적으로 보임.
    stream.setEncoding(QStringConverter::Utf8);
    return stream;
}
}

DeviceSimulator::DeviceSimulator(const QString &portName, qint32 baudRate, QObject *parent)
    : QObject(parent)
{
    m_port.setPortName(portName);
    m_port.setBaudRate(baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        out() << QStringLiteral("[오류] 포트 열기 실패: %1\n").arg(m_port.errorString());
        out() << QStringLiteral("가상 COM 포트 페어가 올바르게 설치되어 있는지, 포트 이름이 맞는지 확인하세요.\n");
        out().flush();
        QMetaObject::invokeMethod(qApp, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
        return;
    }

    out() << QStringLiteral("[대기 중] %1 @ %2bps 에서 명령을 기다리는 중...\n").arg(portName).arg(baudRate);
    out() << QStringLiteral("2초마다 비동기 텔레메트리(TELEMETRY:...)를 자동으로 전송합니다.\n");
    out() << QStringLiteral("종료하려면 Ctrl+C 를 누르세요.\n\n");
    out().flush();

    connect(&m_port, &QSerialPort::readyRead, this, &DeviceSimulator::onReadyRead);

    connect(&m_telemetryTimer, &QTimer::timeout, this, &DeviceSimulator::onTelemetryTimer);
    m_telemetryTimer.start(2000); // 2초마다 비동기 텔레메트리 송신 (실제 실험설비의 주기적 상태 보고를 모사)
}

void DeviceSimulator::onReadyRead()
{
    const QByteArray chunk = m_port.readAll();
    m_receiveBuffer.append(chunk);

    QList<QByteArray> lines = m_receiveBuffer.split('\n');
    m_receiveBuffer = lines.takeLast();

    for (const QByteArray &lineBytes : std::as_const(lines)) {
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        handleCommand(line);
    }
}

void DeviceSimulator::handleCommand(const QString &command)
{
    out() << QStringLiteral("<< 수신: %1\n").arg(command);
    out().flush();

    QString response;
    if (command == QStringLiteral("GET:TEMP")) {
        response = QStringLiteral("TEMP:%1").arg(formatValue(m_temperature));
    } else if (command == QStringLiteral("GET:PRESSURE")) {
        response = QStringLiteral("PRESSURE:%1").arg(formatValue(m_pressure));
    } else if (command == QStringLiteral("SET:VALVE:OPEN")) {
        response = setValve(QStringLiteral("OPEN"));
    } else if (command == QStringLiteral("SET:VALVE:CLOSE")) {
        response = setValve(QStringLiteral("CLOSE"));
    } else {
        out() << QStringLiteral("   [경고] 알 수 없는 명령: %1\n").arg(command);
        out().flush();
        return;
    }

    writeLine(response);
}

QString DeviceSimulator::setValve(const QString &state)
{
    m_valveState = state;
    return QStringLiteral("ACK:VALVE:%1").arg(state);
}

void DeviceSimulator::onTelemetryTimer()
{
    if (!m_port.isOpen()) {
        return;
    }

    // 실측 데이터처럼 보이도록 약간의 랜덤 워크를 적용
    m_temperature += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.4;
    m_pressure += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.8;

    const QString payload = QStringLiteral("TELEMETRY:TEMP:%1,PRESSURE:%2,VALVE:%3")
        .arg(formatValue(m_temperature), formatValue(m_pressure), m_valveState);

    writeLine(payload);
}

void DeviceSimulator::writeLine(const QString &message)
{
    const QByteArray data = (message + QStringLiteral("\n")).toUtf8();
    const qint64 written = m_port.write(data);

    if (written == -1) {
        out() << QStringLiteral("[오류] 송신 실패: %1\n").arg(m_port.errorString());
    } else {
        out() << QStringLiteral(">> 송신: %1\n").arg(message);
    }
    out().flush();
}

QString DeviceSimulator::formatValue(double value)
{
    return QString::number(value, 'f', 2);
}
