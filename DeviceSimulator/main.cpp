#include <QCoreApplication>
#include <QTextStream>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "devicesimulator.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Windows 콘솔(cmd.exe)의 기본 코드페이지는 로캘에 따라 CP949 등으로 설정되어
    // 있어, UTF-8로 인코딩된 한글 문자열을 그대로 출력하면 깨져 보인다.
    // 콘솔 입/출력 코드페이지를 UTF-8로 강제 설정해 이를 해결한다.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    QCoreApplication app(argc, argv);

    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Utf8);
    QTextStream in(stdin);
    in.setEncoding(QStringConverter::Utf8);

    out << "=== 터보펌프 장비 시뮬레이터 (DeviceSimulator) ===\n";
    out.flush();

    const QStringList args = QCoreApplication::arguments();

    QString portName;
    qint32 baudRate = 9600;

    if (args.size() > 1) {
        portName = args.at(1);
    } else {
        out << "연결할 가상 COM 포트 이름을 입력하세요 (예: COM11): ";
        out.flush();
        portName = in.readLine().trimmed();
    }

    if (args.size() > 2) {
        bool ok = false;
        const int parsed = args.at(2).toInt(&ok);
        if (ok) {
            baudRate = parsed;
        }
    }

    DeviceSimulator simulator(portName, baudRate);

    return app.exec();
}
