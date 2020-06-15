#pragma once

#include "framework.h"

namespace Yarc
{
	class YARC_API DataType
	{
	public:
		DataType();
		virtual ~DataType();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const = 0;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) = 0;

		static DataType* ParseTree(const char* protocolData, unsigned int protocolDataSize);
		static int ParseInt(const char* protocolData, unsigned int protocolDataSize);
		static char* ParseString(const char* protocolData, unsigned int protocolDataSize);
	};

	class YARC_API Error : public DataType
	{
	public:
		Error();
		virtual ~Error();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const override;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) override;

	private:
		char* errorMessage;
	};

	class YARC_API Nil : public DataType
	{
	public:
		Nil();
		virtual ~Nil();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const override;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) override;
	};

	class YARC_API SimpleString : public DataType
	{
	public:
		SimpleString();
		virtual ~SimpleString();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const override;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) override;

	private:
		char* string;
	};

	class YARC_API BulkString : public DataType
	{
	public:
		BulkString();
		virtual ~BulkString();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const override;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) override;

	private:
		char* buffer;
		unsigned int bufferSize;
	};

	class YARC_API Integer : public DataType
	{
	public:
		Integer();
		virtual ~Integer();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const override;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) override;

	private:
		int number;
	};

	class YARC_API Array : public DataType
	{
	public:
		Array();
		virtual ~Array();

		virtual bool Print(char* protocolData, unsigned int protocolDataSize) const override;
		virtual bool Parse(const char* protocolData, unsigned int protocolDataSize) override;

	private:
		DataType** dataTypeArray;
		unsigned int dataTypeArraySize;
	};
}