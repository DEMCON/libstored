{
	float input
	bool=true enable
	float=2 gain
	float=0.5 offset
	float=-1 low
	float=10 high
	float=nan override
	float output
} amp

{
	float=3 gain
	float output
} simple amp

{
	(bool) pin
	int8=-1 override
	bool input
} gpio in

{
	bool output
	(int8) override
	(bool) pin
} gpio out

