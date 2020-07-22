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
			bool deleteFlag = false;
			node->deleteFlag = &deleteFlag;
			ReductionObject* object = node->value;
			ReductionResult result = object->Reduce();	// This call might mutate the list we're processing here!
			if (result == RESULT_BAIL || deleteFlag)
				break;

			ReductionObjectList::Node* nextNode = node->GetNext();

			if (result == RESULT_DELETE)
			{
				delete object;
				reductionObjectList->Remove(node);
			}

			node = nextNode;
		}
	}
}