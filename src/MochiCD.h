#pragma once
#include <YRPP.h>
class MochiCD
{

public:
	DEFINE_REFERENCE(MochiCD, Instance, 0xA8E8E8u)

		int Drives[26];
	int Count;
	int unknown_6C;
};

