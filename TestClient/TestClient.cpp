#include <iostream>
#include <string>
#include <yarc_simple_client.h>
#include <yarc_data_types.h>
#include <stdio.h>

using namespace Yarc;

int main()
{
	ClientInterface* client = new SimpleClient();

	if (client->Connect("127.0.0.1", 6379))
	{
		while (client->IsConnected())
		{
			std::cout << "Command: ";
			std::flush(std::cout);

			std::string command;
			std::getline(std::cin, command);

			if (command == "exit")
				break;

			DataType* commandData = DataType::ParseCommand(command.c_str());
			if (!commandData)
				std::cout << "Failed to parse!" << std::endl;
			else
			{
				DataType* resultData = nullptr;
				if (!client->MakeRequestSync(commandData, resultData))
					std::cout << "Failed to issue command!" << std::endl;
				else
				{
					uint8_t protocolData[1024 * 1024];
					uint32_t protocolDataSize = sizeof(protocolData);

					resultData->Print(protocolData, protocolDataSize);

					std::cout << protocolData << std::endl;
				}

				delete resultData;
			}
		}
	}

	delete client;

	return 0;
}
