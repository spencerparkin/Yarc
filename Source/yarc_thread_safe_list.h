#pragma once

#include "yarc_linked_list.h"
#include <Windows.h>

namespace Yarc
{
	template<typename T>
	class ThreadSafeList
	{
	public:

		ThreadSafeList()
		{
			::InitializeCriticalSection(&this->criticalSection);
		}

		virtual ~ThreadSafeList()
		{
			::DeleteCriticalSection(&this->criticalSection);
		}

		uint32_t GetCount() const
		{
			return this->linkedList.GetCount();
		}

		void AddTail(T value)
		{
			::EnterCriticalSection(&this->criticalSection);
			this->linkedList.AddTail(value);
			::EnterCriticalSection(&this->criticalSection);
		}

		void AddHead(T value)
		{
			::EnterCriticalSection(&this->criticalSection);
			this->linkedList.AddHead(value);
			::EnterCriticalSection(&this->criticalSection);
		}

		T RemoveTail()
		{
			::EnterCriticalSection(&this->criticalSection);
			T value = this->linkedList.GetTail()->value;
			this->linkedList.Remove(this->linkedList.GetTail());
			::EnterCriticalSection(&this->criticalSection);
			return value;
		}

		T RemoveHead()
		{
			::EnterCriticalSection(&this->criticalSection);
			T value = this->linkedList.GetHead()->value;
			this->linkedList.Remove(this->linkedList.GetHead());
			::EnterCriticalSection(&this->criticalSection);
			return value;
		}

		void Delete()
		{
			::EnterCriticalSection(&this->criticalSection);
			DeleteList<T>(this->linkedList);
			::EnterCriticalSection(&this->criticalSection);
		}

	private:

		CRITICAL_SECTION criticalSection;
		LinkedList<T> linkedList;
	};
}