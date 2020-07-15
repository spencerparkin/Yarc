#include "yarc_reducer.h"

namespace Yarc
{
	ReductionObject::ReductionObject()
	{
	}

	/*virtual*/ ReductionObject::~ReductionObject()
	{
	}

	/*static*/ void ReductionObject::ReduceList(ReductionObjectList* reductionObjectList)
	{
		ReductionObjectList::Node* node = reductionObjectList->GetHead();
		while (node)
		{
			ReductionObjectList::Node* nextNode = node->GetNext();
			ReductionObject* object = node->value;

			ReductionResult result = object->Reduce();
			if (result == RESULT_BAIL)
				break;

			if (result == RESULT_DELETE)
			{
				delete object;
				reductionObjectList->Remove(node);
			}

			node = nextNode;
		}
	}
}