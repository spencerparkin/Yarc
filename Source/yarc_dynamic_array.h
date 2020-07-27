#pragma once

#include <assert.h>
#include <memory.h>
#include <stdlib.h>

namespace Yarc
{
	// Note that we don't call constructors or destructors here.
	// Also note that it is not safe to hang on to pointers into the array.
	template<typename T>
	class DynamicArray
	{
	public:

		DynamicArray()
		{
			this->data = nullptr;
			this->count = 0;
			this->size = 0;
		}

		virtual ~DynamicArray()
		{
			::free(this->data);
		}

		const T& operator[](unsigned int i) const
		{
			return const_cast<DynamicArray*>(this)->operator[](i);
		}

		T& operator[](unsigned int i)
		{
			assert(i < this->count);
			return this->data[i];
		}

		void operator=(const DynamicArray<T>& dynamicArray)
		{
			this->SetCount(dynamicArray.GetCount());
			for (unsigned int i = 0; i < dynamicArray.GetCount(); i++)
				this->data[i] = dynamicArray.data[i];
		}

		unsigned int GetCount() const { return this->count; }

		void SetCount(unsigned int newCount)
		{
			if (newCount > this->size)
			{
				unsigned int newSize = (this->size == 0) ? 1 : this->size;
				while (newCount > newSize)
					newSize *= 2;

				// This could move and copy the allocation, which means
				// that pointers into the buffer could become stale.
				this->data = (T*)::realloc(this->data, newSize * sizeof(T));
				this->size = newSize;
			}

			this->count = newCount;
		}

		T* GetBuffer()
		{
			return this->data;
		}

		const T* GetBuffer() const
		{
			return this->data;
		}

	private:

		T* data;
		unsigned int count;
		unsigned int size;
	};
}