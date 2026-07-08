find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

add_executable(DeviceSimulator
    main.cpp
    devicesimulator.h
    devicesimulator.cpp
)

target_link_libraries(DeviceSimulator PRIVATE
    Qt6::Core
    Qt6::SerialPort
)
