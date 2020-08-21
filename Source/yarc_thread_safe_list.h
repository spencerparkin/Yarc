#pragma once

#include "yarc_linked_list.h"
#include "yarc_mutex.h"

namespace Yarc
{
	template<typename T>
	class ThreadSafeList
	{
	public:

		ThreadSafeList()
		{
		}

		virtual ~ThreadSafeList()
		{
		}

		uint32_t GetCount() const
		{
			return this->linkedList.GetCount();
		}

		void AddTail(T value)
		{
			MutexLocker locker(this->mutex);
			this->linkedList.AddTail(value);
		}

		void AddHead(T value)
		{
			MutexLocker locker(this->mutex);
			this->linkedList.AddHead(value);
		}

		T RemoveTail()
		{
			MutexLocker locker(this->mutex);
			T value = this->linkedList.GetTail()->value;
			this->linkedList.Remove(this->linkedList.GetTail());
			return value;
		}

		T RemoveHead()
		{
			MutexLocker locker(this->mutex);
			T value = this->linkedList.GetHead()->value;
			this->linkedList.Remove(this->linkedList.GetHead());
			return value;
		}

		void Delete()
		{
			MutexLocker locker(this->mutex);
			DeleteList<T>(this->linkedList);
		}

	private:

		Mutex mutex;
		LinkedList<T> linkedList;
	};
}