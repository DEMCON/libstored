float=1 frequency (Hz)

{
	float[10] x
} interconnect

{
	// Control frequency
	(float) frequency (Hz)

	// Inputs
	float y
	float setpoint
	bool=true enable

	// Coefficients
	float=0.1 Kp
	float=10 Ti (s)
	float=0 Td (s)
	float=0 Kff

	// Ti integrator
	float=0 int
	float=-inf int low
	float=inf int high

	// Bounds on the output
	float=0 low
	float=1 high
	float=inf epsilon

	// Misc control
	bool=true reset
	float=nan override

	// Output
	float u

	// Interconnections
	uint8=0 evaluation order
	int8=0 x setpoint
	int8=1 x y
	int8=2 x u
} pid

{
	float input
	bool=true enable
	float=2 gain
	float=0 offset
	float=-inf low
	float=inf high
	float=nan override
	float output

	// Interconnections
	uint8=1 evaluation order
	int8=2 x input
	int8=3 x output
} amp

{
	(float) sample frequency (Hz)
	float=1 amplitude
	float=0.159 frequency (Hz)
	float=0 phase (rad)
	bool=true enable
	float=nan override
	float y

	// Interconnections
	uint8=0 evaluation order
	int8=4 x y
} sine

{
	(float) sample frequency (Hz)
	float input
	float=1 cutoff frequency (Hz)
	bool=true enable
	bool reset
	float=nan override
	float output

	// Interconnections
	uint8=0 evaluation order
	int8=5 x input
	int8=6 x output
} lowpass

