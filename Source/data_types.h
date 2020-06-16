#pragma once

#include "api.h"
#include <stdint.h>

namespace Yarc
{
	class YARC_API DataType
	{
	public:
		DataType();
		virtual ~DataType();

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const = 0;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) = 0;

		// Parse the given buffer of the given size.  If successful, a data-type tree is
		// returned along with the amount of data parsed in the given buffer.  If unsuccessful,
		// null is returned and the given size parameter is left untouched.
		static DataType* ParseTree(const uint8_t* protocolData, uint32_t& protocolDataSize);
		
		static int32_t ParseInt(const uint8_t* protocolData, uint32_t& protocolDataSize);
		static uint8_t* ParseString(const uint8_t* protocolData, uint32_t& protocolDataSize);
		
		// Parse a command as you might enter it into any redis client.
		static DataType* ParseCommand(const char* command);

	protected:
		static uint32_t FindCRLF(const uint8_t* protocolData, uint32_t protocolDataSize);
	};

	class YARC_API Error : public DataType
	{
	public:
		Error();
		virtual ~Error();

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

	private:
		uint8_t* errorMessage;
	};

	class YARC_API Nil : public DataType
	{
	public:
		Nil();
		virtual ~Nil();

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;
	};

	class YARC_API SimpleString : public DataType
	{
	public:
		SimpleString();
		virtual ~SimpleString();

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

	private:
		uint8_t* string;
	};

	class YARC_API BulkString : public DataType
	{
	public:
		BulkString();
		virtual ~BulkString();

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

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

	private:
		int32_t number;
	};

	class YARC_API Array : public DataType
	{
	public:
		Array();
		virtual ~Array();

		virtual bool Print(uint8_t* protocolData, uint32_t& protocolDataSize) const override;
		virtual bool Parse(const uint8_t* protocolData, uint32_t& protocolDataSize) override;

		void Resize(uint32_t size);
		DataType* GetElement(uint32_t i);
		const DataType* GetElement(uint32_t i) const;
		void SetElement(uint32_t i, DataType* dataType);

	private:
		DataType** dataTypeArray;
		uint32_t dataTypeArraySize;
	};
}