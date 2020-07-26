#pragma once

#include "yarc_dynamic_array.h"
#include "yarc_byte_stream.h"
#include "yarc_linked_list.h"
#include <stdint.h>
#include <string>
#include <map>

namespace Yarc
{
	class YARC_API ProtocolData
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

		virtual uint8_t DynamicDiscriminant() const = 0;

		// This method is provided for backwards compatibility with RESP1,
		// and should be preferred over run-time type checking.
		virtual bool IsNull(void) const { return false; }

		// RESP3 removed redundant null types, but not redundant error types.
		virtual bool IsError(void) const { return false; }

	protected:

		static bool ParseDataType(ByteStream* byteStream, ProtocolData*& protocolData);
		static bool PrintDataType(ByteStream* byteStream, const ProtocolData* protocolData);

		static bool ParseCount(ByteStream* byteStream, uint32_t& count, bool& streamed);
		static bool ParseCRLF(ByteStream* byteStream);
		static bool ParseCRLFTerminatedString(ByteStream* byteStream, std::string& value);

		ProtocolData* attributeData;
	};

	template<typename T>
	inline T* Cast(ProtocolData* protocolData)
	{
#if defined YARC_USE_DYNAMIC_CAST
		return dynamic_cast<T*>(protocolData);
#else
		return (protocolData->DynamicDiscriminant() == T::StaticDiscriminant()) ? (T*)protocolData : nullptr;
#endif
	}

	template<typename T>
	inline const T* Cast(const ProtocolData* protocolData)
	{
#if defined YARC_USE_DYNAMIC_CAST
		return dynamic_cast<const T*>(protocolData);
#else
		return (protocolData->DynamicDiscriminant() == T::StaticDiscriminant()) ? (const T*)protocolData : nullptr;
#endif
	}

	class YARC_API SimpleData : public ProtocolData
	{
	public:

		SimpleData();
		virtual ~SimpleData();
	};

	// This is the equivalent of the bulk-string in RESP1.
	// We handle the case of streamed strings here in the case that ? is given as the fixed size.
	class YARC_API BlobStringData : public SimpleData
	{
	public:

		BlobStringData();
		BlobStringData(const std::string& value);
		BlobStringData(const uint8_t* buffer, uint32_t bufferSize);
		virtual ~BlobStringData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '$'; }
		static uint8_t StaticDiscriminant() { return '$'; }

		std::string GetValue() const;
		bool SetValue(const std::string& givenValue);

		bool GetToBuffer(uint8_t* buffer, uint32_t bufferSize) const;
		bool SetFromBuffer(const uint8_t* buffer, uint32_t bufferSize);

		DynamicArray<uint8_t>& GetByteArray(void);
		const DynamicArray<uint8_t>& GetByteArray(void) const;

		virtual bool IsNull(void) const override { return this->isNull; }

	protected:

		bool ParseByteArrayData(ByteStream* byteStream, uint32_t count);

		DynamicArray<uint8_t>* byteArray;

		bool isNull;
	};

	class YARC_API ChunkData : public BlobStringData
	{
	public:

		ChunkData();
		virtual ~ChunkData();

		virtual uint8_t DynamicDiscriminant() const override { return ';'; }
		static uint8_t StaticDiscriminant() { return ';'; }

		virtual bool Parse(ByteStream* byteStream) override;
	};

	class YARC_API BlobErrorData : public BlobStringData
	{
	public:

		BlobErrorData();
		virtual ~BlobErrorData();

		virtual uint8_t DynamicDiscriminant() const override { return '!'; }
		static uint8_t StaticDiscriminant() { return '!'; }

		virtual bool IsError(void) const override { return true; }

		std::string GetErrorCode(void) const;
	};

	class YARC_API VerbatimStreamData : public BlobStringData
	{
	public:

		VerbatimStreamData();
		virtual ~VerbatimStreamData();

		virtual uint8_t DynamicDiscriminant() const override { return '='; }
		static uint8_t StaticDiscriminant() { return '='; }

		std::string GetFormatCode(void) const;
		std::string GetContent(void) const;
	};

	class YARC_API SimpleStringData : public SimpleData
	{
	public:

		SimpleStringData();
		virtual ~SimpleStringData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '+'; }
		static uint8_t StaticDiscriminant() { return '+'; }

		std::string GetValue() const;
		bool SetValue(const std::string& givenValue);

	protected:

		std::string* value;
	};

	class YARC_API SimpleErrorData : public SimpleStringData
	{
	public:

		SimpleErrorData();
		virtual ~SimpleErrorData();

		virtual uint8_t DynamicDiscriminant() const override { return '-'; }
		static uint8_t StaticDiscriminant() { return '-'; }

		virtual bool IsError(void) const override { return true; }

		std::string GetErrorCode(void) const;
	};

	class YARC_API NumberData : public SimpleData
	{
	public:

		NumberData();
		NumberData(int64_t givenValue);
		virtual ~NumberData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return ':'; }
		static uint8_t StaticDiscriminant() { return ':'; }

		int64_t GetValue() const;
		bool SetValue(int64_t givenValue);

	protected:

		int64_t value;
	};

	class YARC_API DoubleData : public SimpleData
	{
	public:

		DoubleData();
		DoubleData(double givenValue);
		virtual ~DoubleData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return ','; }
		static uint8_t StaticDiscriminant() { return ','; }

		double GetValue() const;
		bool SetValue(double givenValue);

	protected:

		double value;
	};

	class YARC_API BooleanData : public SimpleData
	{
	public:

		BooleanData();
		BooleanData(bool givenValue);
		virtual ~BooleanData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '#'; }
		static uint8_t StaticDiscriminant() { return '#'; }

		bool GetValue() const;
		bool SetValue(bool givenValue);

	private:

		bool value;
	};

	class YARC_API BigNumberData : public SimpleData
	{
	public:

		BigNumberData();
		virtual ~BigNumberData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '<'; }
		static uint8_t StaticDiscriminant() { return '<'; }

	protected:

		std::string* value;		// TODO: Replace with big number type?
	};

	class YARC_API EndData : public SimpleData
	{
	public:

		EndData();
		virtual ~EndData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '.'; }
		static uint8_t StaticDiscriminant() { return '.'; }
	};

	class YARC_API NullData : public SimpleData
	{
	public:

		NullData();
		virtual ~NullData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '_'; }
		static uint8_t StaticDiscriminant() { return '_'; }

		virtual bool IsNull(void) { return true; }
	};

	class YARC_API AggregateData : public ProtocolData
	{
	public:
		
		AggregateData();
		virtual ~AggregateData();
	};

	// We handle the case of a streamed aggregate type here in the case that ? is given as the fixed size.
	class YARC_API ArrayData : public AggregateData
	{
	public:

		ArrayData();
		virtual ~ArrayData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '*'; }
		static uint8_t StaticDiscriminant() { return '*'; }

		uint32_t GetCount(void) const;
		bool SetCount(uint32_t count);

		ProtocolData* GetElement(uint32_t i);
		const ProtocolData* GetElement(uint32_t i) const;
		bool SetElement(uint32_t i, ProtocolData* protocolData);

		virtual bool IsNull(void) const override { return this->isNull; }

	protected:

		typedef DynamicArray<ProtocolData*> NestedDataArray;
		NestedDataArray* nestedDataArray;

		bool isNull;
	};

	class YARC_API MapData : public AggregateData
	{
	public:

		MapData();
		virtual ~MapData();

		virtual bool Parse(ByteStream* byteStream) override;
		virtual bool Print(ByteStream* byteStream) const override;

		virtual uint8_t DynamicDiscriminant() const override { return '%'; }
		static uint8_t StaticDiscriminant() { return '%'; }

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
		FieldValuePairList* fieldValuePairList;

		// TODO: This won't export with the DLL, but should we make our own map container?
		//typedef std::map<std::string, FieldValuePairList::Node*> NestedDataMap;
		//NestedDataMap nestedDataMap;
	};

	class YARC_API SetData : public ArrayData
	{
	public:

		SetData();
		virtual ~SetData();

		virtual uint8_t DynamicDiscriminant() const override { return '~'; }
		static uint8_t StaticDiscriminant() { return '~'; }
	};

	class YARC_API AttributeData : public MapData
	{
	public:

		AttributeData();
		virtual ~AttributeData();

		virtual uint8_t DynamicDiscriminant() const override { return '|'; }
		static uint8_t StaticDiscriminant() { return '|'; }
	};

	class YARC_API PushData : public ArrayData
	{
	public:

		PushData();
		virtual ~PushData();

		virtual uint8_t DynamicDiscriminant() const override { return '>'; }
		static uint8_t StaticDiscriminant() { return '>'; }
	};
}