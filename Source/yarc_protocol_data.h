#pragma once

#include "yarc_dynamic_array.h"
#include "yarc_byte_stream.h"
#include "yarc_linked_list.h"
#include <stdint.h>
#include <string>
#include <map>

#define YARC_USE_DYNAMIC_CAST

namespace Yarc
{
	class ProtocolData;

	template<typename T>
	inline T* Cast(ProtocolData* protocolData)
	{
#if defined YARC_USE_DYNAMIC_CAST
		return dynamic_cast<T*>(protocolData);
#else
		return (protocolData->GetDynamicKind() == T::GetStaticKind()) ? (T*)protocolData : nullptr;
#endif
	}

	template<typename T>
	inline const T* Cast(const ProtocolData* protocolData)
	{
#if defined YARC_USE_DYNAMIC_CAST
		return dynamic_cast<const T*>(protocolData);
#else
		return (protocolData->GetDynamicKind() == T::GetStaticKind()) ? (const T*)protocolData : nullptr;
#endif
	}

	class ProtocolData
	{
	public:

		ProtocolData();
		virtual ~ProtocolData();

		static std::string FindCommandKey(const ProtocolData* commandData);
		static uint16_t CalcCommandHashSlot(const ProtocolData* commandData);
		static uint16_t CalcKeyHashSlot(const std::string& keyStr);

		static ProtocolData* ParseCommand(const char* commandFormat, ...);

		static bool ParseTree(ByteStream* byteStream, ProtocolData*& protocolData);
		static bool PrintTree(ByteStream* byteStream, const ProtocolData* protocolData);

		virtual bool Parse(ByteStream* byteStream) = 0;
		virtual bool Print(ByteStream* byteStream) const = 0;

		// This method is provided for backwards compatibility with RESP1,
		// and should be preferred over run-time type checking.
		virtual bool IsNull(void) const { return false; }

		// RESP3 removed redundant null types, but not redundant error types.
		virtual bool IsError(void) const { return false; }

	protected:

		static bool ParseDataType(ByteStream* byteStream, ProtocolData*& protocolData);
		static bool ParseCount(ByteStream* byteStream, uint32_t& count, bool& streamed);
		static bool ParseCRLF(ByteStream* byteStream);
		static bool ParseCRLFTerminatedString(ByteStream* byteStream, std::string& value);

		ProtocolData* attributeData;
	};

	class SimpleData : public ProtocolData
	{
	public:

		SimpleData();
		virtual ~SimpleData();
	};

	// This is the equivalent of the bulk-string in RESP1.
	// We handle the case of streamed strings here in the case that ? is given as the fixed size.
	class BlobStringData : public SimpleData
	{
	public:

		BlobStringData();
		virtual ~BlobStringData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		std::string GetValue() const;
		bool SetValue(const std::string& givenValue);

		bool GetToBuffer(uint8_t* buffer, uint32_t bufferSize) const;
		bool SetFromBuffer(const uint8_t* buffer, uint32_t bufferSize);

		DynamicArray<uint8_t>& GetByteArray(void);
		const DynamicArray<uint8_t>& GetByteArray(void) const;

		virtual bool IsNull(void) const override { return this->isNull; }

	protected:

		bool ParseByteArrayData(ByteStream* byteStream, uint32_t count);

		DynamicArray<uint8_t> byteArray;

		bool isNull;
	};

	class ChunkData : public BlobStringData
	{
	public:

		ChunkData();
		virtual ~ChunkData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;
	};

	class BlobErrorData : public BlobStringData
	{
	public:

		BlobErrorData();
		virtual ~BlobErrorData();

		virtual bool IsError(void) const override { return true; }
	};

	class VerbatimStreamData : public BlobStringData
	{
	public:

		VerbatimStreamData();
		virtual ~VerbatimStreamData();
	};

	class SimpleStringData : public SimpleData
	{
	public:

		SimpleStringData();
		virtual ~SimpleStringData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		std::string GetValue() const;
		bool SetValue(const std::string& givenValue);

	protected:

		std::string value;
	};

	class SimpleErrorData : public SimpleStringData
	{
	public:

		SimpleErrorData();
		virtual ~SimpleErrorData();

		virtual bool IsError(void) const override { return true; }

		std::string GetErrorCode(void) const;
	};

	class NumberData : public SimpleData
	{
	public:

		NumberData();
		NumberData(int64_t givenValue);
		virtual ~NumberData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		int64_t GetValue() const;
		bool SetValue(int64_t givenValue);

	protected:

		int64_t value;
	};

	class DoubleData : public SimpleData
	{
	public:

		DoubleData();
		DoubleData(double givenValue);
		virtual ~DoubleData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		double GetValue() const;
		bool SetValue(double givenValue);

	protected:

		double value;
	};

	class BooleanData : public SimpleData
	{
	public:

		BooleanData();
		BooleanData(bool givenValue);
		virtual ~BooleanData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		bool GetValue() const;
		bool SetValue(bool givenValue);

	private:

		bool value;
	};

	class BigNumberData : public SimpleData
	{
	public:

		BigNumberData();
		virtual ~BigNumberData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

	protected:

		std::string value;		// TODO: Replace with big number type?
	};

	class EndData : public SimpleData
	{
	public:

		EndData();
		virtual ~EndData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;
	};

	class NullData : public SimpleData
	{
	public:

		NullData();
		virtual ~NullData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual bool IsNull(void) { return true; }
	};

	class AggregateData : public ProtocolData
	{
	public:
		
		AggregateData();
		virtual ~AggregateData();
	};

	// We handle the case of a streamed aggregate type here in the case that ? is given as the fixed size.
	class ArrayData : public AggregateData
	{
	public:

		ArrayData();
		virtual ~ArrayData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		uint32_t GetCount(void) const;
		bool SetCount(uint32_t count);

		ProtocolData* GetElement(uint32_t i);
		const ProtocolData* GetElement(uint32_t i) const;
		bool SetElement(uint32_t i, ProtocolData* protocolData);

		virtual bool IsNull(void) const override { return this->isNull; }

	protected:

		typedef DynamicArray<ProtocolData*> NestedDataArray;
		NestedDataArray nestedDataArray;

		bool isNull;
	};

	class MapData : public AggregateData
	{
	public:

		MapData();
		virtual ~MapData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		ProtocolData* GetField(const std::string& key);
		const ProtocolData* Getfield(const std::string& key) const;
		bool SetField(const std::string& key, ProtocolData* valueData);

	protected:

		struct FieldValuePair
		{
			ProtocolData* fieldData;
			ProtocolData* valueData;
		};

		typedef LinkedList<FieldValuePair> FieldValuePairList;

		FieldValuePairList fieldValuePairList;

		typedef std::map<std::string, FieldValuePairList::Node*> NestedDataMap;
		NestedDataMap nestedDataMap;
	};

	class SetData : public ArrayData
	{
	public:

		SetData();
		virtual ~SetData();
	};

	class AttributeData : public MapData
	{
	public:

		AttributeData();
		virtual ~AttributeData();
	};

	class PushData : public ArrayData
	{
	public:

		PushData();
		virtual ~PushData();
	};
}