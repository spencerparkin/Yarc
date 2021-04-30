#include "yarc_reducer.h"

namespace Yarc
{
	ReductionObject::ReductionObject()
	{
	}

	/*virtual*/ ReductionObject::~ReductionObject()
	{
	}

	/*static*/ void ReductionObject::ReduceList(ReductionObjectList* reductionObjectList, void* userData /*= nullptr*/)
	{
		ReductionObjectList::Node* node = reductionObjectList->GetHead();
		while (node)
		{
			bool deleteFlag = false;
			bool* otherDeleteFlag = node->deleteFlag;
			node->deleteFlag = &deleteFlag;

			ReductionObject* object = node->value;
			ReductionResult result = object->Reduce(userData);	// This call might mutate the list we're processing here!
			
			node->deleteFlag = otherDeleteFlag;
			if (otherDeleteFlag)
				*otherDeleteFlag = deleteFlag;

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