#include "yarc_misc.h"
#include <stdlib.h>

namespace Yarc
{
	uint32_t RandomNumber(uint32_t min, uint32_t max)
	{
		float alpha = float(rand()) / float(RAND_MAX);
		uint32_t number = float(min) + alpha * float(max - min);
		if (number < min)
			number = min;
		if (number > max)
			number = max;
		return number;
	}
}