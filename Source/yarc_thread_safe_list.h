#pragma once

#include "yarc_linked_list.h"
#include "yarc_mutex.h"

namespace Yarc
{
	// Note that an important principle of any mutex locking is that it be
	// done as tightly as possible.  Here, more often than not, the mutex
	// is locked only long enough to perform some operation that should
	// be as fast as possible.
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
			T value = nullptr;
			if (this->linkedList.GetCount() > 0)	// Avoid mutex luck if unnecessary.
			{
				MutexLocker locker(this->mutex);
				if (this->linkedList.GetCount() > 0)	// Avoid race condition.
				{
					value = this->linkedList.GetTail()->value;
					this->linkedList.Remove(this->linkedList.GetTail());
				}
			}
			return value;
		}

		T RemoveHead()
		{
			T value = nullptr;
			if (this->linkedList.GetCount() > 0)	// Avoid mutex luck if unnecessary.
			{
				MutexLocker locker(this->mutex);
				if (this->linkedList.GetCount() > 0)	// Avoid race condition.
				{
					value = this->linkedList.GetHead()->value;
					this->linkedList.Remove(this->linkedList.GetHead());
				}
			}
			return value;
		}

		void Delete()
		{
			MutexLocker locker(this->mutex);
			DeleteList<T>(this->linkedList);
		}

		T Find(std::function<bool(T)> predicate, T notFoundValue, bool removeIfFound = false)
		{
			MutexLocker locker(this->mutex);
			return this->linkedList.Find(predicate, notFoundValue, removeIfFound);
		}

	private:

		Mutex mutex;
		LinkedList<T> linkedList;
	};
}