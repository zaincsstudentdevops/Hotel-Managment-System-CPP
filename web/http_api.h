#ifndef HOTEL_HTTP_API_H
#define HOTEL_HTTP_API_H

namespace httplib {
class Server;
}

class HotelBackend;

void registerHotelApi(httplib::Server& server, HotelBackend& backend);

#endif
