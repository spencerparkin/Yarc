#include "yarc_data_types.h"
#include "yarc_linked_list.h"
#include "yarc_crc16.h"
#include "yarc_misc.h"
#include <stdio.h>
#include <string>
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
					dataType = new Nil();

					if (!dataType->Parse(protocolData, protocolDataSize))
					{
						delete dataType;
						dataType = nullptr;
					}
				}
			}
		}

		return dataType;
	}

	/*static*/ DataType* DataType::ParseCommand(const char* command)
	{
		struct Word
		{
			std::string str;
		};

		LinkedList<Word> wordList;
		Word newWord;
		uint32_t i = 0;
		bool inQuotedText = false;

		for(i = 0; command[i] != '\0'; i++)
		{
			char ch = command[i];
			if (ch == '"')
				inQuotedText = !inQuotedText;
			else if (ch == ' ' && !inQuotedText)
			{
				wordList.AddTail(newWord);
				newWord.str = "";
			}
			else
				newWord.str += ch;
		}

		if(newWord.str.length() > 0)
			wordList.AddTail(newWord);

		Array* wordArray = new Array();
		wordArray->SetSize(wordList.GetCount());
		i = 0;

		for (LinkedList<Word>::Node* node = wordList.GetHead(); node; node = node->GetNext())
		{
			const Word& word = node->value;

			BulkString* bulkString = new BulkString();
			bulkString->SetBuffer((uint8_t*)word.str.c_str(), (uint32_t)word.str.length());

			wordArray->SetElement(i++, bulkString);
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

	/*static*/ std::string DataType::FindCommandKey(const DataType* commandData)
	{
		// I have not yet run into a Redis command who's syntax was not of
		// the form: command [key] ..., so here we just return the second
		// bulk-string in the array.  However, if I find an exception to this,
		// we can update the logic in this subroutine.
		const Array* commandArray = Cast<Array>(commandData);
		if (commandArray && commandArray->GetSize() >= 2)
		{
			const BulkString* keyString = Cast<BulkString>(commandArray->GetElement(1));
			if (keyString)
			{
				char buffer[256];
				keyString->GetString((uint8_t*)buffer, sizeof(buffer));
				return buffer;
			}
		}

		return "";
	}

	/*static*/ uint16_t DataType::CalcCommandHashSlot(const DataType* commandData)
	{
		std::string keyStr = FindCommandKey(commandData);
		const char* key = keyStr.c_str();
		int keylen = strlen(key);

		// Note that we can't just hash the command key here, because
		// we want to provide support for hash tags.  The hash tag feature
		// provides a way for users to make keys that are different, yet
		// guarenteed to hash to the same hash slot.  This is necessary
		// for the use of the MULTI command where multiple commands, each
		// with their own key, are going to be executed by a single node
		// as a single (atomic?) transaction.
		//
		// The following code was taken directly from https://redis.io/topics/cluster-spec.

		int s, e; /* start-end indexes of { and } */

		/* Search the first occurrence of '{'. */
		for (s = 0; s < keylen; s++)
			if (key[s] == '{') break;

		/* No '{' ? Hash the whole key. This is the base case. */
		if (s == keylen) return crc16(key, keylen) & 16383;

		/* '{' found? Check if we have the corresponding '}'. */
		for (e = s + 1; e < keylen; e++)
			if (key[e] == '}') break;

		/* No '}' or nothing between {} ? Hash the whole key. */
		if (e == keylen || e == s + 1) return crc16(key, keylen) & 16383;

		/* If we are here there is both a { and a } on its right. Hash
		 * what is in the middle between { and }. */
		return crc16(key + s + 1, e - s - 1) & 16383;
	}

	/*static*/ DataType* DataType::Clone(const DataType* dataType)
	{
		DataType* cloneData = nullptr;
		uint8_t* protocolData = nullptr;
		uint32_t protocolDataSize = 1024 * 1024;

		try
		{
			protocolData = new uint8_t[protocolDataSize];
			if (!protocolData)
				throw new InternalException();

			if (!dataType->Print(protocolData, protocolDataSize))
				throw new InternalException();

			cloneData = DataType::ParseTree(protocolData, protocolDataSize);
		}
		catch (InternalException* exc)
		{
			delete exc;
		}

		delete[] protocolData;

		return cloneData;
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
		if (protocolDataSize < 5)
			return false;

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

	void BulkString::GetString(uint8_t* stringBuffer, uint32_t stringBufferSize) const
	{
		::strncpy_s((char*)stringBuffer, stringBufferSize, (const char*)this->buffer, this->bufferSize);
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
		int32_t parsedInteger = 0;
		if (!this->ParseInt(protocolData, i, parsedInteger))
			return false;

		if (parsedInteger < 0)
			return false;

		this->bufferSize = (unsigned)parsedInteger;

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
		int32_t parsedInteger = 0;
		if (!this->ParseInt(protocolData, i, parsedInteger))
			return false;

		if (parsedInteger < 0)
			return false;

		this->dataTypeArraySize = (unsigned)parsedInteger;

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