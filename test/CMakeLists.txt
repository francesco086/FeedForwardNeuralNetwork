include_directories(common/)
link_libraries(qnets)

add_executable(check main.cpp)
add_executable(ut1.exe ut1/main.cpp)
add_executable(ut2.exe ut2/main.cpp)
add_executable(ut3.exe ut3/main.cpp)
add_executable(ut4.exe ut4/main.cpp)
add_executable(ut5.exe ut5/main.cpp)
add_executable(ut6.exe ut6/main.cpp)
add_executable(ut7.exe ut7/main.cpp)
add_executable(ut8.exe ut8/main.cpp)
add_executable(ut9.exe ut9/main.cpp)
add_executable(ut10.exe ut10/main.cpp)
add_executable(ut11.exe ut11/main.cpp)
add_executable(ut12.exe ut12/main.cpp)
add_executable(ut13.exe ut13/main.cpp)

add_test(ut1 ut1.exe)
add_test(ut2 ut2.exe)
add_test(ut3 ut3.exe)
add_test(ut4 ut4.exe)
add_test(ut5 ut5.exe)
add_test(ut6 ut6.exe)
add_test(ut7 ut7.exe)
add_test(ut8 ut8.exe)
add_test(ut9 ut9.exe)
add_test(ut10 ut10.exe)
add_test(ut11 ut11.exe)
add_test(ut12 ut12.exe)
add_test(ut13 ut13.exe)
