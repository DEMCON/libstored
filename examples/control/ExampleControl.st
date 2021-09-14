(float) frequency (Hz)

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
	float=0.5 Kp
	float=0.25 Ti (s)
	float=0.05 Td (s)
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
	int8=2 x setpoint
	int8=5 x y
	int8=3 x u
} pid

{
	float input
	bool=true enable
	float=2 gain
	float=-0.1 offset
	float=-inf low
	float=inf high
	float=nan override
	float output

	// Interconnections
	uint8=1 evaluation order
	int8=3 x input
	int8=4 x output
} amp

{
	(float) sample frequency (Hz)
	float=1 amplitude
	float=0.02 frequency (Hz)
	float=0 phase (rad)
	bool=true enable
	float=nan override
	float output

	// Interconnections
	uint8=0 evaluation order
	int8=0 x output
} sine

{
	(float) sample frequency (Hz)
	float=1 amplitude
	float=0.159 frequency (Hz)
	float=0 phase (rad)
	float=0.25 duty cycle
	bool=true enable
	float=nan override
	float output

	// Interconnections
	uint8=0 evaluation order
	int8=0 x duty cycle
	int8=1 x output
} pulse

{
	(float) sample frequency (Hz)
	float input
	float=0.2 cutoff frequency (Hz)
	bool=true enable
	bool reset
	float=nan override
	float output

	// Interconnections
	uint8=0 evaluation order
	int8=4 x input
	int8=5 x output
} lowpass

{
	(float) sample frequency (Hz)
	float input
	float=0.5 speed limit
	float=0.05 acceleration limit
	bool reset
	bool=true enable
	float=nan override
	float output

	// Interconnections
	uint8=0 evaluation order
	int8=1 x input
	int8=2 x output
} ramp

