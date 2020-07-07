#pragma once

#include "yarc_api.h"
#include <stdint.h>

namespace Yarc
{
	class YARC_API DataType
	{
	public:
		DataType();
		virtual ~DataType();

		enum Kind
		{
			KIND_ERROR,
			KIND_NIL,
			KIND_SIMPLE_STRING,
			KIND_BULK_STRING,
			KIND_INTEGER,
			KIND_ARRAY
		};

		virtual Kind GetDynamicKind() const = 0;

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const = 0;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) = 0;

		// Parse the given buffer of the given size.  If successful, a data-type tree is
		// returned along with the amount of data parsed in the given buffer.  If unsuccessful,
		// null is returned and the given size parameter is left untouched.
		static DataType* ParseTree(const uint8_t* protocolData, uint32_t& protocolDataSize, bool* validStart = nullptr);
		
		static bool ParseInt(const uint8_t* protocolData, uint32_t& protocolDataSize, int32_t& result);
		static bool ParseString(const uint8_t* protocolData, uint32_t& protocolDataSize, uint8_t*& result);
		
		// Parse a command as you might enter it into any redis client.
		static DataType* ParseCommand(const char* command);

		static DataType* Clone(const DataType* dataType);

	protected:
		static bool FindCRLF(const uint8_t* protocolData, uint32_t protocolDataSize, uint32_t& i);
	};

	template<typename T>
	inline T* Cast(DataType* dataType)
	{
#if defined YARC_USE_DYNAMIC_CAST
		return dynamic_cast<T*>(dataType);
#else
		return (dataType->GetDynamicKind() == T::GetStaticKind()) ? (T*)dataType : nullptr;
#endif
	}

	template<typename T>
	inline const T* Cast(const DataType* dataType)
	{
#if defined YARC_USE_DYNAMIC_CAST
		return dynamic_cast<const T*>(dataType);
#else
		return (dataType->GetDynamicKind() == T::GetStaticKind()) ? (const T*)dataType : nullptr;
#endif
	}

	class YARC_API Error : public DataType
	{
	public:
		Error();
		virtual ~Error();

		virtual Kind GetDynamicKind() const override { return KIND_ERROR; }
		static Kind GetStaticKind() { return KIND_ERROR; }

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

		const uint8_t* GetErrorMessage() const { return this->errorMessage; }

	private:
		uint8_t* errorMessage;
	};

	class YARC_API Nil : public DataType
	{
	public:
		Nil();
		virtual ~Nil();

		virtual Kind GetDynamicKind() const override { return KIND_NIL; }
		static Kind GetStaticKind() { return KIND_NIL; }

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;
	};

	class YARC_API SimpleString : public DataType
	{
	public:
		SimpleString();
		virtual ~SimpleString();

		virtual Kind GetDynamicKind() const override { return KIND_SIMPLE_STRING; }
		static Kind GetStaticKind() { return KIND_SIMPLE_STRING; }

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

		void SetString(const uint8_t* givenString);
		const uint8_t* GetString() const { return this->string; }

	private:
		uint8_t* string;
	};

	class YARC_API BulkString : public DataType
	{
	public:
		BulkString();
		virtual ~BulkString();

		virtual Kind GetDynamicKind() const override { return KIND_BULK_STRING; }
		static Kind GetStaticKind() { return KIND_BULK_STRING; }

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

		void SetBuffer(const uint8_t* givenBuffer, uint32_t givenBufferSize);
		uint8_t* GetBuffer() { return this->buffer; }
		const uint8_t* GetBuffer() const { return this->buffer; }
		uint32_t GetBufferSize() const { return this->bufferSize; }

	private:
		uint8_t* buffer;
		uint32_t bufferSize;
	};

	class YARC_API Integer : public DataType
	{
	public:
		Integer();
		virtual ~Integer();

		virtual Kind GetDynamicKind() const override { return KIND_INTEGER; }
		static Kind GetStaticKind() { return KIND_INTEGER; }

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

		void SetNumber(int32_t givenNumber) { this->number = givenNumber; }
		int32_t GetNumber() const { return this->number; }

	private:
		int32_t number;
	};

	class YARC_API Array : public DataType
	{
	public:
		Array();
		virtual ~Array();

		virtual Kind GetDynamicKind() const override { return KIND_ARRAY; }
		static Kind GetStaticKind() { return KIND_ARRAY; }

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

		void SetSize(uint32_t size);
		uint32_t GetSize() const { return this->dataTypeArraySize; }
		DataType* GetElement(uint32_t i);
		const DataType* GetElement(uint32_t i) const;
		void SetElement(uint32_t i, DataType* dataType);

	private:
		DataType** dataTypeArray;
		uint32_t dataTypeArraySize;
	};
}