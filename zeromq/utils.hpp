#pragma once

template<typename Socket>
class timeout_watcher {
    Socket &socket;
    int receive_timeout;
public:
   timeout_watcher(Socket &socket) :
       socket(socket),
       receive_timeout(socket.get_receive_timeout())
   {
       socket.set_receive_timeout(0);
   }

   ~timeout_watcher() {
       socket.set_receive_timeout(receive_timeout);
   }
};
