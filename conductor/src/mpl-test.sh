g++ -std=c++11 -I../include -I/dvl/co/porto/plugins-test/plugins/foreign/asio/asio/include \
  -I/dvl/co/porto/plugins-test/plugins/foreign/blackhole/src/ test.cpp -lpthread \
  && ./a.out | c++filt -t

