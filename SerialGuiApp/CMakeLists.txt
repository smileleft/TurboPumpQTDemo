find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Charts)

add_executable(SerialGuiApp
    main.cpp
    mainwindow.h
    mainwindow.cpp
)

target_link_libraries(SerialGuiApp PRIVATE
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Charts
)
