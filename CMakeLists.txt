cmake_minimum_required(VERSION 3.10)
project(TicTacToeProject)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем строгие флаги компиляции
add_compile_options(-Wall -Wextra -pedantic -Werror)

# Подключаем boost-asio через vcpkg
find_package(Boost REQUIRED COMPONENTS asio system)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Boost libraries not found!")
endif()

# Настройка для клиента
add_executable(client client/client.cpp)
target_link_libraries(client Boost::asio Boost::system)

# Настройка для сервера
add_executable(server server/server.cpp)
target_link_libraries(server Boost::asio Boost::system)
