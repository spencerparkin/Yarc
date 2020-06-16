#include "yarc_data_types.h"
#include "yarc_linked_list.h"
#include <stdio.h>
#include <string.h>
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

	/*static*/ DataType* DataType::ParseTree(const uint8_t* protocolData, uint32_t& protocolDataSize)
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

	/*static*/ DataType* DataType::ParseCommand(const char* command)
	{
		struct Word
		{
			char string[512];
		};

		LinkedList<Word> wordList;
		Word newWord;
		uint32_t i = 0, j = 0;
		bool inQuotedText = false;

		while(true)
		{
			if (j == sizeof(newWord.string))
				return nullptr;

			char ch = command[i];
			if (ch == '"')
				inQuotedText = !inQuotedText;
			else if (ch == '\0' || (ch == ' ' && !inQuotedText))
			{
				newWord.string[j] = '\0';
				wordList.AddTail(newWord);
				j = 0;
			}
			else
			{
				newWord.string[j++] = ch;
			}

			if (ch == '\0')
				break;

			i++;
		}

		Array* wordArray = new Array();
		wordArray->Resize(wordList.GetCount());
		j = 0;

		for (LinkedList<Word>::Node* node = wordList.GetHead(); node; node = node->GetNext())
		{
			const Word& word = node->value;

			BulkString* bulkString = new BulkString();
			bulkString->SetBuffer((uint8_t*)word.string, strlen(word.string));

			wordArray->SetElement(j++, bulkString);
		}

		// Commands in the RESP are just arrays of bulk strings.
		return wordArray;
	}

	/*static*/ uint32_t DataType::FindCRLF(const uint8_t* protocolData, uint32_t protocolDataSize)
	{
		if (protocolDataSize < 2)
			return -1;

		uint32_t i = 0;

		while (i < protocolDataSize - 2)
		{
			if (protocolData[i] == '\r' && protocolData[i + 1] == '\n')
				break;

			i++;
		}

		return i;
	}

	/*static*/ int32_t DataType::ParseInt(const uint8_t* protocolData, uint32_t& protocolDataSize)
	{
		uint32_t i = FindCRLF(protocolData, protocolDataSize);
		uint8_t buffer[128];
		memcpy(buffer, &protocolData[1], i);
		buffer[i] = '\0';
		int32_t result = atoi((const char*)buffer);
		protocolDataSize = i + 2;
		return result;
	}

	/*static*/ uint8_t* DataType::ParseString(const uint8_t* protocolData, uint32_t& protocolDataSize)
	{
		uint32_t i = FindCRLF(protocolData, protocolDataSize);
		uint8_t* result = new uint8_t[i];
		memcpy(result, &protocolData[1], i - 1);
		result[i - 1] = '\0';
		protocolDataSize = i + 2;
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

	/*virtual*/ bool Error::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		if (!this->errorMessage)
			return false;

		sprintf_s((char*)protocolData, protocolDataSize, "-%s\r\n", this->errorMessage);
		protocolDataSize = (uint32_t)strlen((char*)protocolData);
		return true;
	}

	/*virtual*/ bool Error::Parse(const uint8_t* protocolData, uint32_t& protocolDataSize)
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

	/*virtual*/ bool Nil::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		sprintf_s((char*)protocolData, protocolDataSize, "$-1\r\n");
		protocolDataSize = (uint32_t)strlen((char*)protocolData);
		return true;
	}

	/*virtual*/ bool Nil::Parse(const uint8_t* protocolData, uint32_t& protocolDataSize)
	{
		if (0 != strncmp((const char*)protocolData, "$-1\r\n", 5) && 0 != strncmp((const char*)protocolData, "*-1\r\n", 5))
			return false;

		protocolDataSize = 5;
		return true;
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

	/*virtual*/ bool SimpleString::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		if (!this->string)
			return false;

		sprintf_s((char*)protocolData, protocolDataSize, "+%s\r\n", this->string);
		protocolDataSize = (uint32_t)strlen((char*)protocolData);
		return true;
	}

	/*virtual*/ bool SimpleString::Parse(const uint8_t* protocolData, uint32_t& protocolDataSize)
	{
		delete[] this->string;
		this->string = this->ParseString(protocolData, protocolDataSize);
		return this->string != nullptr;
	}

	//----------------------------------------- BulkString -----------------------------------------

	BulkString::BulkString()
	{
		this->buffer = nullptr;
		this->bufferSize = 0;
	}

	/*virtual*/ BulkString::~BulkString()
	{
		delete[] this->buffer;
	}

	void BulkString::SetBuffer(const uint8_t* givenBuffer, uint32_t givenBufferSize)
	{
		delete[] this->buffer;
		this->buffer = nullptr;
		this->bufferSize = givenBufferSize;
		if (givenBufferSize > 0)
		{
			this->buffer = new uint8_t[givenBufferSize];
			memcpy(this->buffer, givenBuffer, givenBufferSize);
		}
	}

	/*virtual*/ bool BulkString::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		sprintf_s((char*)protocolData, protocolDataSize, "$%d\r\n", this->bufferSize);

		uint32_t i = FindCRLF(protocolData, protocolDataSize) + 2;
		for(uint32_t j = 0; j < this->bufferSize; j++)
		{
			if (i >= protocolDataSize - 3)
				return false;

			protocolData[i++] = this->buffer[j];
		}

		protocolData[i++] = '\r';
		protocolData[i++] = '\n';
		protocolData[i] = '\0';

		protocolDataSize = (uint32_t)strlen((char*)protocolData);
		return true;
	}

	/*virtual*/ bool BulkString::Parse(const uint8_t* protocolData, uint32_t& protocolDataSize)
	{
		delete[] this->buffer;
		this->buffer = nullptr;

		uint32_t i = protocolDataSize;
		this->bufferSize = (unsigned)this->ParseInt(protocolData, i);

		uint32_t j = FindCRLF(&protocolData[i], protocolDataSize - i);

		if (this->bufferSize == j - i)
			return false;

		this->buffer = new uint8_t[this->bufferSize];
		memcpy(this->buffer, &protocolData[i], this->bufferSize);

		protocolDataSize = i + j + 2;

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

	/*virtual*/ bool Integer::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		sprintf_s((char*)protocolData, protocolDataSize, ":%d\r\n", this->number);
		protocolDataSize = (uint32_t)strlen((char*)protocolData);
		return true;
	}

	/*virtual*/ bool Integer::Parse(const uint8_t* protocolData, uint32_t& protocolDataSize)
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
		if (this->dataTypeArray)
		{
			for (uint32_t i = 0; i < this->dataTypeArraySize; i++)
				delete this->dataTypeArray[i];
		}

		delete[] this->dataTypeArray;
	}

	void Array::Resize(uint32_t size)
	{
		delete[] this->dataTypeArray;
		this->dataTypeArraySize = size;
		this->dataTypeArray = new DataType*[size];
		memset(this->dataTypeArray, 0, sizeof(DataType*) * size);
	}

	DataType* Array::GetElement(uint32_t i)
	{
		if (i < this->dataTypeArraySize)
			return this->dataTypeArray[i];

		return nullptr;
	}

	const DataType* Array::GetElement(uint32_t i) const
	{
		return const_cast<Array*>(this)->GetElement(i);
	}

	void Array::SetElement(uint32_t i, DataType* dataType)
	{
		if (i < this->dataTypeArraySize)
			this->dataTypeArray[i] = dataType;
	}

	/*virtual*/ bool Array::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		bool success = true;

		sprintf_s((char*)protocolData, protocolDataSize, "*%d\r\n", this->dataTypeArraySize);

		if (this->dataTypeArraySize > 0)
		{
			for (uint32_t i = 0; i < this->dataTypeArraySize && success; i++)
			{
				const DataType* dataType = this->dataTypeArray[i];
				if (!dataType)
					success = false;
				else
				{
					uint8_t* subProtocolData = new uint8_t[protocolDataSize];

					uint32_t j = protocolDataSize;
					if (dataType->Print(subProtocolData, j))
						strcat_s((char*)protocolData, protocolDataSize, (char*)subProtocolData);
					else
						success = false;

					delete[] subProtocolData;
				}
			}
		}

		if(success)
			protocolDataSize = (uint32_t)strlen((char*)protocolData);

		return success;
	}

	/*virtual*/ bool Array::Parse(const uint8_t* protocolData, uint32_t& protocolDataSize)
	{
		delete this->dataTypeArray;
		this->dataTypeArray = nullptr;

		uint32_t i = protocolDataSize;
		this->dataTypeArraySize = this->ParseInt(protocolData, i);

		if (this->dataTypeArraySize > 0)
		{
			this->dataTypeArray = new DataType*[this->dataTypeArraySize];
			memset(this->dataTypeArray, 0, this->dataTypeArraySize);
			DataType** dataType = this->dataTypeArray;

			while(unsigned(dataType - this->dataTypeArray) < this->dataTypeArraySize)
			{
				uint32_t j = protocolDataSize - i;
				*dataType = this->ParseTree(&protocolData[i], j);
				if (*dataType == nullptr)
					return false;
				dataType++;
				i += j;
			}
		}

		protocolDataSize = i;
		return true;
	}
}