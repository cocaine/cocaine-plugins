/*
    Copyright (c) 2011-2013 Evgeny Safronov <esafronov@yandex-team.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

template<typename Socket>
class timeout_watcher {
    Socket &socket;
    const int receive_timeout;
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
