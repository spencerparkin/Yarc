#include "yarc_protocol_data.h"
#include "yarc_misc.h"
#include <ctype.h>
#include <cfloat>

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

	/*static*/ bool ProtocolData::ParseTree(ByteStream* byteStream, ProtocolData*& protocolData)
	{
		protocolData = nullptr;

		if (!ParseDataType(byteStream, protocolData))
			return false;

		AttributeData* attributeData = dynamic_cast<AttributeData*>(protocolData);
		if (attributeData)
		{
			if (!ParseTree(byteStream, protocolData))
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
		return false;
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

		if (!protocolData->Parse(byteStream))
		{
			delete protocolData;
			protocolData = nullptr;
			return false;
		}

		return true;
	}

	/*static*/ bool ProtocolData::ParseCount(ByteStream* byteStream, uint32_t& count, bool& streamed)
	{
		streamed = false;
		count = 0;
		char buffer[128];
		int i = 0;

		while (true)
		{
			if (i >= sizeof(buffer) - 1)
				return false;

			uint8_t byte = 0;
			if (!byteStream->ReadByte(byte))
				return false;

			if (byte == '?')
			{
				streamed = true;
				break;
			}

			if (!::isdigit(byte))
				return false;

			buffer[i++] = byte;
		}

		buffer[i] = '\0';
		count = (uint32_t)::strtol(buffer, nullptr, 10);
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
	}

	/*virtual*/ ArrayData::~ArrayData()
	{
	}

	/*virtual*/ bool ArrayData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		if (!streamed && count == -1)
			return false;

		if (!ParseCRLF(byteStream))
			return false;

		while (streamed || count > 0)
		{
			ProtocolData* nestedData = nullptr;
			if (!ParseTree(byteStream, nestedData))
				return false;

			if (streamed)
			{
				EndData* endData = dynamic_cast<EndData*>(nestedData);
				if (endData)
				{
					delete endData;
					break;
				}
			}

			this->nestedDataArray.SetCount(this->nestedDataArray.GetCount() + 1);
			this->nestedDataArray[this->nestedDataArray.GetCount() - 1] = nestedData;

			if (!streamed)
				count--;
		}

		return true;
	}

	/*virtual*/ bool ArrayData::Print(ByteStream* byteStream)
	{
		return false;
	}

	uint32_t  ArrayData::GetCount(void) const
	{
		return this->nestedDataArray.GetCount();
	}

	bool  ArrayData::SetCount(uint32_t count)
	{
		this->nestedDataArray.SetCount(count);
		return true;
	}

	ProtocolData* ArrayData::GetElement(uint32_t i)
	{
		if (i >= this->nestedDataArray.GetCount())
			return nullptr;

		return this->nestedDataArray[i];
	}

	const ProtocolData* ArrayData::GetElement(uint32_t i) const
	{
		return const_cast<ArrayData*>(this)->GetElement(i);
	}

	bool  ArrayData::SetElement(uint32_t i, ProtocolData* protocolData)
	{
		if (i >= this->nestedDataArray.GetCount())
			return false;

		delete this->nestedDataArray[i];
		this->nestedDataArray[i] = protocolData;
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

	/*virtual*/ bool EndData::Print(ByteStream* byteStream)
	{
		return false;
	}

	//-------------------------- BlobStringData --------------------------

	BlobStringData::BlobStringData()
	{
	}

	/*virtual*/ BlobStringData::~BlobStringData()
	{
	}

	/*virtual*/ bool BlobStringData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		if (!streamed && count == -1)
			return false;

		if (!ParseCRLF(byteStream))
			return false;

		if (streamed)
		{
			while (true)
			{
				ProtocolData* protocolData = nullptr;
				if (!ParseDataType(byteStream, protocolData))
					return false;

				ChunkData* chunkData = dynamic_cast<ChunkData*>(protocolData);
				if (!chunkData)
				{
					delete protocolData;
					return false;
				}

				if (chunkData->byteArray.GetCount() == 0)
				{
					delete protocolData;
					return true;
				}

				for (int i = 0; i < (signed)chunkData->byteArray.GetCount(); i++)
				{
					this->byteArray.SetCount(this->byteArray.GetCount() + 1);
					this->byteArray[this->byteArray.GetCount() - 1] = chunkData->byteArray[i];
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
		while (count > 0)
		{
			uint8_t byte = 0;
			if (!byteStream->ReadByte(byte))
				return false;

			this->byteArray.SetCount(this->byteArray.GetCount() + 1);
			this->byteArray[this->byteArray.GetCount() - 1] = byte;

			count--;
		}

		return ParseCRLF(byteStream);
	}

	/*virtual*/ bool BlobStringData::Print(ByteStream* byteStream)
	{
		return false;
	}

	DynamicArray<uint8_t>& BlobStringData::GetByteArray(void)
	{
		return this->byteArray;
	}

	const DynamicArray<uint8_t>& BlobStringData::GetByteArray(void) const
	{
		return this->byteArray;
	}

	bool BlobStringData::GetToBuffer(uint8_t* buffer, uint32_t bufferSize) const
	{
		return false;
	}

	bool BlobStringData::SetFromBuffer(const uint8_t* buffer, uint32_t bufferSize)
	{
		return false;
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

		if (!ParseCRLF(byteStream))
			return false;

		if (count == 0)
			return true;

		return this->ParseByteArrayData(byteStream, count);
	}

	/*virtual*/ bool ChunkData::Print(ByteStream* byteStream)
	{
		return false;
	}

	//-------------------------- BlobErrorData --------------------------

	BlobErrorData::BlobErrorData()
	{
	}

	/*virtual*/ BlobErrorData::~BlobErrorData()
	{
	}

	//-------------------------- VerbatimStreamData --------------------------

	VerbatimStreamData::VerbatimStreamData()
	{
	}

	/*virtual*/ VerbatimStreamData::~VerbatimStreamData()
	{
	}

	//-------------------------- SimpleStringData --------------------------

	SimpleStringData::SimpleStringData()
	{
	}

	/*virtual*/ SimpleStringData::~SimpleStringData()
	{
	}

	/*virtual*/ bool SimpleStringData::Parse(ByteStream* byteStream)
	{
		return ParseCRLFTerminatedString(byteStream, this->value);
	}

	/*virtual*/ bool SimpleStringData::Print(ByteStream* byteStream)
	{
		return false;
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
		return "";
	}

	//-------------------------- MapData --------------------------

	MapData::MapData()
	{
	}

	/*virtual*/ MapData::~MapData()
	{
	}

	/*virtual*/ bool MapData::Parse(ByteStream* byteStream)
	{
		uint32_t count = 0;
		bool streamed = false;
		if (!ParseCount(byteStream, count, streamed))
			return false;

		if (count % 2 != 0)
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
				EndData* endData = dynamic_cast<EndData*>(pair.fieldData);
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

			this->fieldValuePairList.AddTail(pair);

			if (!streamed)
				count -= 2;
		}

		return true;
	}

	/*virtual*/ bool MapData::Print(ByteStream* byteStream)
	{
		return false;
	}

	ProtocolData* MapData::GetField(const std::string& key)
	{
		return nullptr;
	}

	const ProtocolData* MapData::Getfield(const std::string& key) const
	{
		return nullptr;
	}

	bool MapData::SetField(const std::string& key, ProtocolData* valueData)
	{
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

	/*virtual*/ bool DoubleData::Print(ByteStream* byteStream)
	{
		return false;
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

	/*virtual*/ bool NumberData::Print(ByteStream* byteStream)
	{
		return false;
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

	/*virtual*/ bool BooleanData::Print(ByteStream* byteStream)
	{
		return false;
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
	}

	/*virtual*/ BigNumberData::~BigNumberData()
	{
	}

	/*virtual*/ bool BigNumberData::Parse(ByteStream* byteStream)
	{
		if (!ParseCRLFTerminatedString(byteStream, this->value))
			return false;

		for (int i = 0; i < (signed)this->value.length(); i++)
			if (!::isdigit(this->value.c_str()[i]))
				return false;

		return true;
	}

	/*virtual*/ bool BigNumberData::Print(ByteStream* byteStream)
	{
		return false;
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

	/*virtual*/ bool NullData::Print(ByteStream* byteStream)
	{
		return false;
	}
}