#include "yarc_data_types.h"
#include "yarc_linked_list.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

namespace Yarc
{
	//----------------------------------------- DataType -----------------------------------------

	DataType::DataType()
	{
	}
	
	/*virtual*/ DataType::~DataType()
	{
	}

	/*static*/ DataType* DataType::ParseTree(const uint8_t* protocolData, uint32_t& protocolDataSize, bool* validStart /*= nullptr*/)
	{
		if (validStart)
			*validStart = true;

		DataType* dataType = nullptr;

		if (protocolDataSize > 0)
		{
			switch (protocolData[0])
			{
				case '+': dataType = new SimpleString();	break;
				case '-': dataType = new Error();			break;
				case ':': dataType = new Integer();			break;
				case '$': dataType = new BulkString();		break;
				case '*': dataType = new Array();			break;
			}

			if (!dataType)
			{
				if(validStart)
					*validStart = false;
			}
			else
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
		wordArray->SetSize(wordList.GetCount());
		j = 0;

		for (LinkedList<Word>::Node* node = wordList.GetHead(); node; node = node->GetNext())
		{
			const Word& word = node->value;

			BulkString* bulkString = new BulkString();
			bulkString->SetBuffer((uint8_t*)word.string, (uint32_t)strlen(word.string));

			wordArray->SetElement(j++, bulkString);
		}

		// Commands in the RESP are just arrays of bulk strings.
		return wordArray;
	}

	/*static*/ bool DataType::FindCRLF(const uint8_t* protocolData, uint32_t protocolDataSize, uint32_t& i)
	{
		if (protocolDataSize == 0)
			return false;

		while (i < protocolDataSize - 1)
		{
			if (protocolData[i] == '\r' && protocolData[i + 1] == '\n')
				return true;

			i++;
		}

		return false;
	}

	/*static*/ bool DataType::ParseInt(const uint8_t* protocolData, uint32_t& protocolDataSize, int32_t& result)
	{
		uint32_t i = 0;
		if (!FindCRLF(protocolData, protocolDataSize, i))
			return false;

		uint8_t buffer[128];
		uint32_t bufferSize = sizeof(buffer) / sizeof(uint8_t);
		assert(i + 1 <= bufferSize);

		memcpy_s(buffer, sizeof(buffer), &protocolData[1], i);
		buffer[i] = '\0';

		result = atoi((const char*)buffer);
		protocolDataSize = i + 2;
		return true;
	}

	/*static*/ bool DataType::ParseString(const uint8_t* protocolData, uint32_t& protocolDataSize, uint8_t*& result)
	{
		result = nullptr;

		uint32_t i = 0;
		if (!FindCRLF(protocolData, protocolDataSize, i))
			return false;

		result = new uint8_t[i];
		memcpy_s(result, i, &protocolData[1], i - 1);
		result[i - 1] = '\0';

		protocolDataSize = i + 2;
		return result;
	}

	/*static*/ DataType* DataType::Clone(const DataType* dataType)
	{
		uint8_t protocolData[10 * 1024];
		uint32_t protocolDataSize = sizeof(protocolData);
		if (!dataType->Print(protocolData, protocolDataSize))
			return nullptr;

		return DataType::ParseTree(protocolData, protocolDataSize);
	}

	//----------------------------------------- Error -----------------------------------------

	Error::Error()
	{
	}

	/*virtual*/ Error::~Error()
	{
	}

	/*virtual*/ bool Error::Print(uint8_t* protocolData, uint32_t& protocolDataSize) const
	{
		if (!this->string)
			return false;

		sprintf_s((char*)protocolData, protocolDataSize, "-%s\r\n", this->string);
		protocolDataSize = (uint32_t)strlen((char*)protocolData);
		return true;
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

	void SimpleString::SetString(const uint8_t* givenString)
	{
		delete[] this->string;
		uint32_t len = (uint32_t)strlen((const char*)givenString);
		this->string = new uint8_t[len + 1];
		strcpy_s((char*)this->string, len, (const char*)givenString);
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
		this->string = nullptr;
		return this->ParseString(protocolData, protocolDataSize, this->string);
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

		uint32_t i = 0;
		FindCRLF(protocolData, protocolDataSize, i);
		i += 2;

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
		if (!this->ParseInt(protocolData, i, (int32_t&)this->bufferSize))
			return false;

		// Note we skip a bunch looking for the CRLF, because a CRLF may exist in the bulk data.
		uint32_t j = this->bufferSize;
		if (!FindCRLF(&protocolData[i], protocolDataSize - i, j))
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
		this->number = 0;
		return ParseInt(protocolData, protocolDataSize, this->number);
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

	void Array::SetSize(uint32_t size)
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
		if (!this->ParseInt(protocolData, i, (int32_t&)this->dataTypeArraySize))
			return false;

		if (this->dataTypeArraySize > 0)
		{
			this->dataTypeArray = new DataType*[this->dataTypeArraySize];
			memset(this->dataTypeArray, 0, this->dataTypeArraySize * sizeof(DataType*));
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