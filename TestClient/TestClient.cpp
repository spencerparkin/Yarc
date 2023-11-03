#include <iostream>
#include <string>
#include <yarc_simple_client.h>
#include <yarc_byte_stream.h>
#include <yarc_protocol_data.h>
#include <yarc_connection_pool.h>
#include <stdio.h>

using namespace Yarc;

int main()
{
	ClientInterface* client = new SimpleClient();

	while(true)
	{
		std::cout << "Command: ";
		std::flush(std::cout);

		std::string command;
		std::getline(std::cin, command);

		if (command == "exit")
			break;

		ProtocolData* commandData = ProtocolData::ParseCommand(command.c_str());
		if (!commandData)
			std::cout << "Failed to parse!" << std::endl;
		else
		{
			ProtocolData* resultData = nullptr;
			if (!client->MakeRequestSync(commandData, resultData))
				std::cout << "Failed to issue command!" << std::endl;
			else
			{
				std::string protocolDataStr;
				Yarc::StringStream stringStream(&protocolDataStr);
				ProtocolData::PrintTree(&stringStream, resultData);
				std::cout << protocolDataStr << std::endl;
			}

			delete resultData;
		}
	}

	delete client;

	return 0;
}
