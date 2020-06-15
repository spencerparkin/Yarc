#include "pch.h"
#include "data_types.h"
#include <stdio.h>
#include <stdlib.h>

namespace Yarc
{
	//----------------------------------------- DataType -----------------------------------------

	DataType::DataType()
	{
	}
	
	/*virtual*/ DataType::~DataType()
	{
	}

	/*static*/ DataType* DataType::ParseTree(const char* protocolData, unsigned int protocolDataSize)
	{
		DataType* dataType = nullptr;

		if (protocolDataSize > 0)
		{
			switch(protocolData[0])
			{
				case '+': dataType = new SimpleString();	break;
				case '-': dataType = new Error();			break;
				case ':': dataType = new Integer();			break;
				case '$': dataType = new BulkString();		break;
				case '*': dataType = new Array();			break;
			}

			if (dataType)
			{
				if (!dataType->Parse(protocolData, protocolDataSize))
				{
					delete dataType;
					dataType = nullptr;
				}
			}
		}

		return dataType;
	}

	/*static*/ int DataType::ParseInt(const char* protocolData, unsigned int protocolDataSize)
	{
		char buffer[128];
		strcpy_s(buffer, sizeof(buffer), protocolData);

		char* marker = strstr(buffer, "\r\n");
		*marker = '\0';

		int result = atoi(&marker[1]);
		return result;
	}

	/*static*/ char* DataType::ParseString(const char* protocolData, unsigned int protocolDataSize)
	{
		const char* marker = strstr(protocolData, "\r\n");
		if (!marker)
			return nullptr;

		int len = marker - protocolData - 1;
		char* result = new char[len + 1];
		int i;
		for (i = 0; i < len; i++)
			result[i] = protocolData[i + 1];
		result[i] = '\0';

		return result;
	}

	//----------------------------------------- Error -----------------------------------------

	Error::Error()
	{
		this->errorMessage = nullptr;
	}

	/*virtual*/ Error::~Error()
	{
		delete[] this->errorMessage;
	}

	/*virtual*/ bool Error::Print(char* protocolData, unsigned int protocolDataSize) const
	{
		if (!this->errorMessage)
			return false;

		sprintf_s(protocolData, protocolDataSize, "-%s\r\n", this->errorMessage);
		return true;
	}

	/*virtual*/ bool Error::Parse(const char* protocolData, unsigned int protocolDataSize)
	{
		delete[] this->errorMessage;
		this->errorMessage = this->ParseString(protocolData, protocolDataSize);
		return this->errorMessage != nullptr;
	}

	//----------------------------------------- Nil -----------------------------------------

	Nil::Nil()
	{
	}
	
	/*virtual*/ Nil::~Nil()
	{
	}

	/*virtual*/ bool Nil::Print(char* protocolData, unsigned int protocolDataSize) const
	{
		sprintf_s(protocolData, protocolDataSize, "$-1\r\n");
		return true;
	}

	/*virtual*/ bool Nil::Parse(const char* protocolData, unsigned int protocolDataSize)
	{
		return false;
	}

	//----------------------------------------- SimpleString -----------------------------------------

	SimpleString::SimpleString()
	{
		this->string = nullptr;
	}

	/*virtual*/ SimpleString::~SimpleString()
	{
		delete[] this->string;
	}

	/*virtual*/ bool SimpleString::Print(char* protocolData, unsigned int protocolDataSize) const
	{
		if (!this->string)
			return false;

		sprintf_s(protocolData, protocolDataSize, "+%s\r\n", this->string);
		return true;
	}

	/*virtual*/ bool SimpleString::Parse(const char* protocolData, unsigned int protocolDataSize)
	{
		delete[] this->string;
		this->string = this->ParseString(protocolData, protocolDataSize);
		return this->string != nullptr;
	}

	//----------------------------------------- BulkString -----------------------------------------

	BulkString::BulkString()
	{
	}

	/*virtual*/ BulkString::~BulkString()
	{
	}

	/*virtual*/ bool BulkString::Print(char* protocolData, unsigned int protocolDataSize) const
	{
		sprintf_s(protocolData, protocolDataSize, "$%d\r\n", this->bufferSize);

		char* byte = strstr(protocolData, "\r\n") + 2;
		for(unsigned int i = 0; i < this->bufferSize; i++)
		{
			if (unsigned(byte - protocolData) >= protocolDataSize - 2)
				return false;

			*byte++ = this->buffer[i];
		}

		*byte++ = '\r';
		*byte = '\n';

		return true;
	}

	/*virtual*/ bool BulkString::Parse(const char* protocolData, unsigned int protocolDataSize)
	{
		delete[] this->buffer;
		this->buffer = nullptr;

		this->bufferSize = (unsigned)this->ParseInt(protocolData, protocolDataSize);

		if (this->bufferSize > 0)
		{
			this->buffer = new char[this->bufferSize];

			//...
		}

		return true;
	}

	//----------------------------------------- Integer -----------------------------------------

	Integer::Integer()
	{
		this->number = 0;
	}

	/*virtual*/ Integer::~Integer()
	{
	}

	/*virtual*/ bool Integer::Print(char* protocolData, unsigned int protocolDataSize) const
	{
		sprintf_s(protocolData, protocolDataSize, ":%d\r\n", this->number);
		return true;
	}

	/*virtual*/ bool Integer::Parse(const char* protocolData, unsigned int protocolDataSize)
	{
		this->number = ParseInt(protocolData, protocolDataSize);
		return true;
	}

	//----------------------------------------- Array -----------------------------------------

	Array::Array()
	{
		this->dataTypeArray = nullptr;
		this->dataTypeArraySize = 0;
	}

	/*virtual*/ Array::~Array()
	{
		for (unsigned int i = 0; i < this->dataTypeArraySize; i++)
			delete this->dataTypeArray[i];

		delete[] this->dataTypeArray;
	}

	/*virtual*/ bool Array::Print(char* protocolData, unsigned int protocolDataSize) const
	{
		bool success = true;

		sprintf_s(protocolData, protocolDataSize, "*%d", this->dataTypeArraySize);

		if (this->dataTypeArraySize == 0)
			strcat_s(protocolData, protocolDataSize, "\r\n");
		else
		{
			for (unsigned int i = 0; i < this->dataTypeArraySize && success; i++)
			{
				const DataType* dataType = this->dataTypeArray[i];

				char* subProtocolData = new char[protocolDataSize];

				if (dataType->Print(subProtocolData, protocolDataSize))
					strcat_s(protocolData, protocolDataSize, subProtocolData);
				else
					success = false;

				delete[] subProtocolData;
			}
		}

		return success;
	}

	/*virtual*/ bool Array::Parse(const char* protocolData, unsigned int protocolDataSize)
	{
		return false;
	}
}