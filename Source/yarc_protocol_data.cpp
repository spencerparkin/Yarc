#include "yarc_protocol_data.h"
#include "yarc_misc.h"
#include "yarc_crc16.h"
#include <ctype.h>
#include <cfloat>
#include <cstdarg>

namespace Yarc
{
	//-------------------------- ProtocolData --------------------------

	ProtocolData::ProtocolData()
	{
		this->attributeData = nullptr;
	}

	/*virtual*/ ProtocolData::~ProtocolData()
	{
		delete this->attributeData;
	}

	/*static*/ std::string ProtocolData::FindCommandKey(const ProtocolData* commandData)
	{
		// TODO: Not all command's syntax requires a first argument key.
		const ArrayData* commandArrayData = Cast<ArrayData>(commandData);
		if (commandArrayData && commandArrayData->GetCount() >= 2)
		{
			const BlobStringData* keyStringData = Cast<BlobStringData>(commandArrayData->GetElement(1));
			if (keyStringData)
				return keyStringData->GetValue();
		}

		return "";
	}

	/*static*/ uint16_t ProtocolData::CalcCommandHashSlot(const ProtocolData* commandData)
	{
		std::string keyStr = FindCommandKey(commandData);
		return CalcKeyHashSlot(keyStr);
	}

	/*static*/ uint16_t ProtocolData::CalcKeyHashSlot(const std::string& keyStr)
	{
		const char* key = keyStr.c_str();
		int keylen = (int)strlen(key);

		// Note that we can't just hash the key here, because we want to
		// provide support for hash tags.  The hash tag feature provides
		// a way for users to make keys that are different, yet guarenteed
		// to hash to the same hash slot.  This is necessary for the use of
		// the MULTI command where multiple commands, each with their own
		// key, are going to be executed by a single node as a single atomic
		// transaction.
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

	/*static*/ ProtocolData* ProtocolData::ParseCommand(const char* commandFormat, ...)
	{
		va_list args;
		va_start(args, commandFormat);
		char command[1024];
		sprintf_s(command, sizeof(command), commandFormat, args);
		va_end(args);

		struct Word
		{
			std::string str;
		};

		LinkedList<Word> wordList;
		Word newWord;
		uint32_t i = 0;
		bool inQuotedText = false;

		for (i = 0; command[i] != '\0'; i++)
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

		if (newWord.str.length() > 0)
			wordList.AddTail(newWord);

		ArrayData* wordArray = new ArrayData();
		wordArray->SetCount(wordList.GetCount());
		i = 0;

		for (LinkedList<Word>::Node* node = wordList.GetHead(); node; node = node->GetNext())
		{
			const Word& word = node->value;

			BlobStringData* bulkStringData = new BlobStringData();
			bulkStringData->SetFromBuffer((uint8_t*)word.str.c_str(), (uint32_t)word.str.length());

			wordArray->SetElement(i++, bulkStringData);
		}

		// Commands in RESP are just arrays of bulk strings.
		return wordArray;
	}

	/*static*/ bool ProtocolData::ParseTree(ByteStream* byteStream, ProtocolData*& protocolData)
	{
		protocolData = nullptr;

		if (!ParseDataType(byteStream, protocolData))
			return false;

		AttributeData* attributeData = Cast<AttributeData>(protocolData);
		if (attributeData)
		{
			if (!ParseDataType(byteStream, protocolData))
			{
				delete attributeData;
				return false;
			}

			protocolData->attributeData = attributeData;
		}

		return true;
	}

	/*static*/ bool ProtocolData::PrintTree(ByteStream* byteStream, const ProtocolData* protocolData)
	{
		if (protocolData->attributeData)
			if (!ProtocolData::PrintDataType(byteStream, protocolData->attributeData))
				return false;

		return ProtocolData::PrintDataType(byteStream, protocolData);
	}

	/*static*/ bool ProtocolData::ParseDataType(ByteStream* byteStream, ProtocolData*& protocolData)
	{
		protocolData = nullptr;

		uint8_t byte = 0;
		if (!byteStream->ReadByte(byte))
			return false;

		switch (byte)
		{
			case '$': protocolData = new BlobStringData(); break;
			case '+': protocolData = new SimpleStringData(); break;
			case '-': protocolData = new SimpleErrorData(); break;
			case ':': protocolData = new NumberData(); break;
			case '_': protocolData = new NullData(); break;
			case ',': protocolData = new DoubleData(); break;
			case '#': protocolData = new BooleanData(); break;
			case '!': protocolData = new BlobErrorData(); break;
			case '=': protocolData = new VerbatimStreamData(); break;
			case '<': protocolData = new BigNumberData(); break;
			case '*': protocolData = new ArrayData(); break;
			case '%': protocolData = new MapData(); break;
			case '~': protocolData = new SetData(); break;
			case '|': protocolData = new AttributeData(); break;
			case '>': protocolData = new PushData(); break;
			case '.': protocolData = new EndData(); break;
			case ';': protocolData = new ChunkData(); break;
		}

		if (!protocolData)
			return false;

		if (byte != protocolData->DynamicDiscriminant())
			return false;

		if (!protocolData->Parse(byteStream))
		{
			delete protocolData;
			protocolData = nullptr;
			return false;
		}

		return true;
	}

	/*static*/ bool ProtocolData::PrintDataType(ByteStream* byteStream, const ProtocolData* protocolData)
	{
		if (!byteStream->WriteByte(protocolData->DynamicDiscriminant()))
			return false;

		return protocolData->Print(byteStream);
	}

	/*static*/ bool ProtocolData::ParseCount(ByteStream* byteStream, uint32_t& count, bool& streamed)
	{
		uint8_t byte = 0;
		streamed = false;
		count = 0;
		char buffer[128];
		int i = 0;

		while (true)
		{
			if (i >= sizeof(buffer) - 1)
				return false;

			if (!byteStream->ReadByte(byte))
				return false;

			if (byte == '?')
			{
				streamed = true;
				return ParseCRLF(byteStream);
			}

			if (!::isdigit(byte))
				break;

			buffer[i++] = byte;
		}

		// TODO: Did the number parse succeed?
		buffer[i] = '\0';
		count = (uint32_t)::strtol(buffer, nullptr, 10);

		if (byte != '\r')
			return false;

		if (!byteStream->ReadByte(byte))
			return false;

		if (byte != '\n')
			return false;

		return true;
	}

	/*static*/ bool ProtocolData::ParseCRLF(ByteStream* byteStream)
	{
		uint8_t cr = 0;
		if (!byteStream->ReadByte(cr) || cr != '\r')
			return false;

		uint8_t lf = 0;
		if (!byteStream->ReadByte(lf) || lf != '\n')
			return false;

		return true;
	}

	/*static*/ bool ProtocolData::ParseCRLFTerminatedString(ByteStream* byteStream, std::string& value)
	{
		value.clear();

		while (true)
		{
			uint8_t byte = 0;
			if (!byteStream->ReadByte(byte))
				return false;

			value += byte;

			int i = value.length();
			if (i >= 2)
			{
				const char* buffer = value.c_str();
				if (buffer[i - 2] == '\r' && buffer[i - 1] == '\n')
					break;
			}
		}

		value.resize(value.length() - 2);
		return true;
	}

	//-------------------------- SimpleData --------------------------

	SimpleData::SimpleData()
	{
	}

	/*virtual*/ SimpleData::~SimpleData()
	{
	}

	//-------------------------- AggregateData --------------------------

	AggregateData::AggregateData()
	{
	}

	/*virtual*/ AggregateData::~AggregateData()
	{
	}

	//-------------------------- ArrayData --------------------------

	ArrayData::ArrayData()
	{
		this->nestedDataArray = new NestedDataArray();
		this->isNull = false;
	}

	/*virtual*/ ArrayData::~ArrayData()
	{
		for (int i = 0; i < (signed)this->nestedDataArray->GetCount(); i++)
			delete (*this->nestedDataArray)[i];

		delete this->nestedDataArray;
	}

	/*virtual*/ bool ArrayData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		if (!streamed && count == -1)
		{
			// This is for backwards compatibility.
			this->isNull = true;
			return true;
		}

		while (streamed || count > 0)
		{
			ProtocolData* nestedData = nullptr;
			if (!ParseTree(byteStream, nestedData))
				return false;

			if (streamed)
			{
				EndData* endData = Cast<EndData>(nestedData);
				if (endData)
				{
					delete endData;
					break;
				}
			}

			this->nestedDataArray->SetCount(this->nestedDataArray->GetCount() + 1);
			(*this->nestedDataArray)[this->nestedDataArray->GetCount() - 1] = nestedData;

			if (!streamed)
				count--;
		}

		return true;
	}

	/*virtual*/ bool ArrayData::Print(ByteStream* byteStream) const
	{
		if (!byteStream->WriteFormat("%d", this->nestedDataArray->GetCount()))
			return false;

		if (!byteStream->WriteFormat("\r\n"))
			return false;

		for (int i = 0; i < (signed)this->nestedDataArray->GetCount(); i++)
			if (!PrintTree(byteStream, (*this->nestedDataArray)[i]))
				return false;

		return true;
	}

	uint32_t  ArrayData::GetCount(void) const
	{
		return this->nestedDataArray->GetCount();
	}

	bool  ArrayData::SetCount(uint32_t count)
	{
		uint32_t oldCount = this->nestedDataArray->GetCount();

		for (uint32_t i = count; i < oldCount; i++)
			delete (*this->nestedDataArray)[i];

		this->nestedDataArray->SetCount(count);

		for (uint32_t i = oldCount; i < count; i++)
			(*this->nestedDataArray)[i] = nullptr;

		return true;
	}

	ProtocolData* ArrayData::GetElement(uint32_t i)
	{
		if (i >= this->nestedDataArray->GetCount())
			return nullptr;

		return (*this->nestedDataArray)[i];
	}

	const ProtocolData* ArrayData::GetElement(uint32_t i) const
	{
		return const_cast<ArrayData*>(this)->GetElement(i);
	}

	bool  ArrayData::SetElement(uint32_t i, ProtocolData* protocolData)
	{
		if (i >= this->nestedDataArray->GetCount())
			return false;

		delete (*this->nestedDataArray)[i];
		(*this->nestedDataArray)[i] = protocolData;
		return true;
	}

	//-------------------------- EndData --------------------------

	EndData::EndData()
	{
	}

	/*virtual*/ EndData::~EndData()
	{
	}

	/*virtual*/ bool EndData::Parse(ByteStream* byteStream)
	{
		return ParseCRLF(byteStream);
	}

	/*virtual*/ bool EndData::Print(ByteStream* byteStream) const
	{
		if (!byteStream->WriteFormat("\r\n"))
			return false;

		return true;
	}

	//-------------------------- BlobStringData --------------------------

	BlobStringData::BlobStringData()
	{
		this->byteArray = new DynamicArray<uint8_t>();
		this->isNull = false;
	}

	BlobStringData::BlobStringData(const std::string& value)
	{
		this->byteArray = new DynamicArray<uint8_t>();
		this->isNull = false;
		this->SetValue(value);
	}

	BlobStringData::BlobStringData(const uint8_t* buffer, uint32_t bufferSize)
	{
		this->byteArray = new DynamicArray<uint8_t>();
		this->isNull = false;
		this->SetFromBuffer(buffer, bufferSize);
	}

	/*virtual*/ BlobStringData::~BlobStringData()
	{
		delete this->byteArray;
	}

	/*virtual*/ bool BlobStringData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		if (!streamed && count == -1)
		{
			// This is for backwards compatibility.
			this->isNull = true;
			return true;
		}

		if (streamed)
		{
			while (true)
			{
				ProtocolData* protocolData = nullptr;
				if (!ParseDataType(byteStream, protocolData))
					return false;

				ChunkData* chunkData = Cast<ChunkData>(protocolData);
				if (!chunkData)
				{
					delete protocolData;
					return false;
				}

				if (chunkData->byteArray->GetCount() == 0)
				{
					delete protocolData;
					return true;
				}

				for (int i = 0; i < (signed)chunkData->byteArray->GetCount(); i++)
				{
					this->byteArray->SetCount(this->byteArray->GetCount() + 1);
					(*this->byteArray)[this->byteArray->GetCount() - 1] = (*chunkData->byteArray)[i];
				}
			}
		}
		else
		{
			if (!this->ParseByteArrayData(byteStream, count))
				return false;
		}

		return true;
	}

	bool BlobStringData::ParseByteArrayData(ByteStream* byteStream, uint32_t count)
	{
		this->byteArray->SetCount(count);

		if (!byteStream->ReadBufferNow(&(*this->byteArray)[0], count))
			return false;

		return ParseCRLF(byteStream);
	}

	/*virtual*/ bool BlobStringData::Print(ByteStream* byteStream) const
	{
		if (!byteStream->WriteFormat("%d\r\n", this->byteArray->GetCount()))
			return false;

		if (!byteStream->WriteBufferNow(&(*this->byteArray)[0], this->byteArray->GetCount()))
			return false;

		if (!byteStream->WriteFormat("\r\n"))
			return false;

		return true;
	}

	DynamicArray<uint8_t>& BlobStringData::GetByteArray(void)
	{
		return *this->byteArray;
	}

	const DynamicArray<uint8_t>& BlobStringData::GetByteArray(void) const
	{
		return *this->byteArray;
	}

	bool BlobStringData::GetToBuffer(uint8_t* buffer, uint32_t bufferSize) const
	{
		if (bufferSize < this->byteArray->GetCount())
			return false;

		for (int i = 0; i < (signed)this->byteArray->GetCount(); i++)
			buffer[i] = (*this->byteArray)[i];

		return true;
	}

	bool BlobStringData::SetFromBuffer(const uint8_t* buffer, uint32_t bufferSize)
	{
		this->byteArray->SetCount(bufferSize);

		for (int i = 0; i < (signed)this->byteArray->GetCount(); i++)
			(*this->byteArray)[i] = buffer[i];

		return true;
	}

	std::string BlobStringData::GetValue() const
	{
		std::string byteArrayStr;
		for (int i = 0; i < (signed)this->byteArray->GetCount(); i++)
			byteArrayStr += (*this->byteArray)[i];
		
		return byteArrayStr;
	}

	bool BlobStringData::SetValue(const std::string& givenValue)
	{
		this->byteArray->SetCount(givenValue.length());
		for (int i = 0; i < (signed)givenValue.length(); i++)
			(*this->byteArray)[i] = givenValue[i];

		return true;
	}

	//-------------------------- ChunkData --------------------------

	ChunkData::ChunkData()
	{
	}

	/*virtual*/ ChunkData::~ChunkData()
	{
	}

	/*virtual*/ bool ChunkData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		if (streamed)
			return false;

		if (count == 0)
			return true;

		return this->ParseByteArrayData(byteStream, count);
	}

	//-------------------------- BlobErrorData --------------------------

	BlobErrorData::BlobErrorData()
	{
	}

	/*virtual*/ BlobErrorData::~BlobErrorData()
	{
	}

	std::string BlobErrorData::GetErrorCode(void) const
	{
		// TODO: Write this.
		return "";
	}

	//-------------------------- VerbatimStreamData --------------------------

	VerbatimStreamData::VerbatimStreamData()
	{
	}

	/*virtual*/ VerbatimStreamData::~VerbatimStreamData()
	{
	}

	std::string VerbatimStreamData::GetFormatCode(void) const
	{
		// TODO: Write this.
		return "";
	}

	std::string VerbatimStreamData::GetContent(void) const
	{
		// TODO: Write this.
		return "";
	}

	//-------------------------- SimpleStringData --------------------------

	SimpleStringData::SimpleStringData()
	{
		this->value = new std::string;
	}

	/*virtual*/ SimpleStringData::~SimpleStringData()
	{
		delete this->value;
	}

	/*virtual*/ bool SimpleStringData::Parse(ByteStream* byteStream)
	{
		return ParseCRLFTerminatedString(byteStream, *this->value);
	}

	/*virtual*/ bool SimpleStringData::Print(ByteStream* byteStream) const
	{
		return byteStream->WriteFormat("%s\r\n", this->value->c_str());
	}

	std::string SimpleStringData::GetValue() const
	{
		return *this->value;
	}

	bool SimpleStringData::SetValue(const std::string& givenValue)
	{
		*this->value = givenValue;
		return true;
	}

	//-------------------------- SimpleErrorData --------------------------

	SimpleErrorData::SimpleErrorData()
	{
	}

	/*virtual*/ SimpleErrorData::~SimpleErrorData()
	{
	}

	std::string SimpleErrorData::GetErrorCode(void) const
	{
		uint32_t i = this->value->find(' ');
		if (i == std::string::npos)
			return "";

		return this->value->substr(0, i);
	}

	//-------------------------- MapData --------------------------

	MapData::MapData()
	{
		this->fieldValuePairList = new FieldValuePairList();
	}

	/*virtual*/ MapData::~MapData()
	{
		for (FieldValuePairList::Node* node = this->fieldValuePairList->GetHead(); node; node = node->GetNext())
		{
			FieldValuePair& pair = node->value;
			delete pair.fieldData;
			delete pair.valueData;
		}

		delete this->fieldValuePairList;
	}

	/*virtual*/ bool MapData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		while (streamed || count > 0)
		{
			FieldValuePair pair;
			pair.fieldData = nullptr;
			pair.valueData = nullptr;

			if (!ParseTree(byteStream, pair.fieldData))
				return false;

			if (streamed)
			{
				EndData* endData = Cast<EndData>(pair.fieldData);
				if (endData)
				{
					delete pair.fieldData;
					break;
				}
			}

			if (!ParseTree(byteStream, pair.valueData))
			{
				delete pair.fieldData;
				return false;
			}

			this->fieldValuePairList->AddTail(pair);

			if (!streamed)
				count--;
		}

		return true;
	}

	/*virtual*/ bool MapData::Print(ByteStream* byteStream) const
	{
		if (!byteStream->WriteFormat("%d\r\n", this->fieldValuePairList->GetCount()))
			return false;

		for (const FieldValuePairList::Node* node = this->fieldValuePairList->GetHead(); node; node = node->GetNext())
		{
			const FieldValuePair& pair = node->value;

			if (!PrintTree(byteStream, pair.fieldData))
				return false;

			if (!PrintTree(byteStream, pair.valueData))
				return false;
		}

		return true;
	}

	ProtocolData* MapData::GetField(const std::string& key)
	{
		// TODO: Write this.
		return nullptr;
	}

	const ProtocolData* MapData::Getfield(const std::string& key) const
	{
		// TODO: Write this.
		return nullptr;
	}

	bool MapData::SetField(const std::string& key, ProtocolData* valueData)
	{
		// TODO: Write this.
		return false;
	}

	//-------------------------- SetData --------------------------

	SetData::SetData()
	{
	}

	/*virtual*/ SetData::~SetData()
	{
	}

	//-------------------------- AttributeData --------------------------

	AttributeData::AttributeData()
	{
	}

	/*virtual*/ AttributeData::~AttributeData()
	{
	}

	//-------------------------- PushData --------------------------

	PushData::PushData()
	{
	}

	/*virtual*/ PushData::~PushData()
	{
	}

	//-------------------------- DoubleData --------------------------

	DoubleData::DoubleData()
	{
		this->value = 0.0;
	}

	DoubleData::DoubleData(double givenValue)
	{
		this->value = givenValue;
	}

	/*virtual*/ DoubleData::~DoubleData()
	{
	}

	/*virtual*/ bool DoubleData::Parse(ByteStream* byteStream)
	{
		std::string valueString;
		if (!ParseCRLFTerminatedString(byteStream, valueString))
			return false;

		if (valueString == "inf")
			this->value = DBL_MAX;
		else if (valueString == "-inf")
			this->value = DBL_MIN;
		else
			this->value = ::strtod(valueString.c_str(), nullptr);	// TODO: Did parse fail here?

		return true;
	}

	/*virtual*/ bool DoubleData::Print(ByteStream* byteStream) const
	{
		if (this->value == DBL_MAX)
		{
			if (!byteStream->WriteFormat("inf\r\n"))
				return false;
		}
		else if (this->value == DBL_MIN)
		{
			if (!byteStream->WriteFormat("-inf\r\n"))
				return false;
		}
		else
		{
			if (!byteStream->WriteFormat("%f\r\n", this->value))
				return false;
		}

		return true;
	}

	double DoubleData::GetValue() const
	{
		return this->value;
	}

	bool DoubleData::SetValue(double givenValue)
	{
		this->value = givenValue;
		return true;
	}

	//-------------------------- NumberData --------------------------

	NumberData::NumberData()
	{
		this->value = 0;
	}

	NumberData::NumberData(int64_t givenValue)
	{
		this->value = givenValue;
	}

	/*virtual*/ NumberData::~NumberData()
	{
	}

	/*virtual*/ bool NumberData::Parse(ByteStream* byteStream)
	{
		std::string valueString;
		if (!ParseCRLFTerminatedString(byteStream, valueString))
			return false;

		this->value = ::strtol(valueString.c_str(), nullptr, 10);	// TODO: Did parse fail here?

		return true;
	}

	/*virtual*/ bool NumberData::Print(ByteStream* byteStream) const
	{
		return byteStream->WriteFormat("%lld\r\n", this->value);
	}

	int64_t NumberData::GetValue() const
	{
		return this->value;
	}

	bool NumberData::SetValue(int64_t givenValue)
	{
		this->value = givenValue;
		return true;
	}

	//-------------------------- BooleanData --------------------------

	BooleanData::BooleanData()
	{
		this->value = false;
	}

	BooleanData::BooleanData(bool givenValue)
	{
		this->value = givenValue;
	}

	/*virtual*/ BooleanData::~BooleanData()
	{
	}

	/*virtual*/ bool BooleanData::Parse(ByteStream* byteStream)
	{
		uint8_t byte = 0;
		if (!byteStream->ReadByte(byte))
			return false;

		this->value = bool(byte);

		return ParseCRLF(byteStream);
	}

	/*virtual*/ bool BooleanData::Print(ByteStream* byteStream) const
	{
		return byteStream->WriteFormat("%c\r\n", (this->value ? 't' : 'f'));
	}

	bool BooleanData::GetValue() const
	{
		return this->value;
	}

	bool BooleanData::SetValue(bool givenValue)
	{
		this->value = value;
		return true;
	}

	//-------------------------- BigNumberData --------------------------

	BigNumberData::BigNumberData()
	{
		this->value = new std::string;
	}

	/*virtual*/ BigNumberData::~BigNumberData()
	{
		delete this->value;
	}

	/*virtual*/ bool BigNumberData::Parse(ByteStream* byteStream)
	{
		if (!ParseCRLFTerminatedString(byteStream, *this->value))
			return false;

		for (int i = 0; i < (signed)this->value->length(); i++)
			if (!::isdigit(this->value->c_str()[i]))
				return false;

		return true;
	}

	/*virtual*/ bool BigNumberData::Print(ByteStream* byteStream) const
	{
		return byteStream->WriteFormat("%s\r\n", this->value->c_str());
	}

	//-------------------------- NullData --------------------------

	NullData::NullData()
	{
	}

	/*virtual*/ NullData::~NullData()
	{
	}

	/*virtual*/ bool NullData::Parse(ByteStream* byteStream)
	{
		return ParseCRLF(byteStream);
	}

	/*virtual*/ bool NullData::Print(ByteStream* byteStream) const
	{
		return byteStream->WriteFormat("\r\n");
	}
}