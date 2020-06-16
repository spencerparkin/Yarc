#include <iostream>
#include <string>
#include <client.h>
#include <data_types.h>
#include <stdio.h>

using namespace Yarc;

int main()
{
	Client* client = new Client();

	if (client->Connect("127.0.0.1", 6379))
	{
#if false
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
				if (!client->MakeReqeustSync(commandData, resultData))
					std::cout << "Failed to issue command!" << std::endl;
				else
				{
					uint8_t protocolData[10 * 1024];
					uint32_t protocolDataSize = sizeof(protocolData);

					resultData->Print(protocolData, protocolDataSize);

					std::cout << protocolData << std::endl;
				}

				delete resultData;
			}
		}
#else
		Client::Callback callback = [](const DataType* publishedData) {
				std::cout << "=====================================" << std::endl;
				uint8_t protocolData[10 * 1024];
				uint32_t protocolDataSize = sizeof(protocolData);
				publishedData->Print(protocolData, protocolDataSize);
				std::cout << protocolData << std::endl;
				return true;
			};
		client->SetFallbackCallback(callback);
		DataType* commandData = DataType::ParseCommand("subscribe FrameUpdate");
		client->MakeRequestAsync(commandData, callback);
		while (client->IsConnected())
			client->Update(true);
#endif
	}

	delete client;

	return 0;
}
