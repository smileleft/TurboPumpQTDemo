cmake_minimum_required(VERSION 3.16)
project(TurboPumpQtDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 위젯/시그널-슬롯 코드 생성을 위한 자동화 옵션
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(SerialGuiApp)
add_subdirectory(DeviceSimulator)
