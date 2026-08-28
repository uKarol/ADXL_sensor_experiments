/*
 * helper_functions.c
 *
 *  Created on: Aug 25, 2026
 *      Author: Karol
 */

#include "stdint.h"
#include "helper_functions.h"
#include "cmsis_gcc.h"

bool IsIsr()
{
	if(__get_IPSR() == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}
