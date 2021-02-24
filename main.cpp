#include <iostream>
#include "HttpLinster.h"
int main()
{
	HttpServices::HttpLinster server;
	server.Get("/", [=](const HttpServices::Request&rq, HttpServices::Response&rp) {
		rp.SetContent({ "hello" });
	});
	server.Listen("0.0.0.0", 80);
	std::cout << "Hello World!\n";
}
