#pragma once

#include "yarc_linked_list.h"

namespace Yarc
{
	class ReductionObject;

	typedef LinkedList<ReductionObject*> ReductionObjectList;

	// TODO: I think this class is stupid and should be phased entirely out of use.
	class ReductionObject
	{
	public:

		ReductionObject();
		virtual ~ReductionObject();

		enum ReductionResult
		{
			RESULT_NONE,
			RESULT_DELETE,
			RESULT_BAIL
		};

		virtual ReductionResult Reduce(void* userData) = 0;

		static void ReduceList(ReductionObjectList* reductionObjectList, void* userData = nullptr);
	};
}