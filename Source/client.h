#pragma once

namespace Yarc
{
	class Client
	{
	public:

		Client();
		virtual ~Client();

		bool Connect(const char* address, int timeout = 30);
	};
}