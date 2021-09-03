{
	float=3.721 G (m/s^2)
	float=43.34e-3 air molar mass (kg/mol)
	float=210 temperature (K)
	float=600 surface air pressure (Pa)
} environment

{
	float=0.85 motor constant
	float=1.8 mass (kg)
	float=0.4 lift coefficient
	float=1e-4 drag coefficient

	float=0 height (m)
	float=0 speed (m/s)
} helicopter

// All parameters for stored::PID.
// Most fields are optional. If left out, the PID instance will be tuned
// accordingly. The names are predefined, but may add a suffix, like a unit.
{
	// Control frequency
	(float) frequency (Hz)

	// Inputs
	float y
	float setpoint
	bool=true enable

	// Coefficients
	float=0.0005 Kp
	float=60 Ti (s)
	float=30 Td (s)
	float=0 Kff

	// Ti integrator
	float=0.7 int
	float=-inf int low
	float=inf int high

	// Bounds on the output
	float=0 low
	float=1 high
	float=0.1 epsilon

	// Misc control
	bool=true reset
	float=nan override

	// Output
	float u
} pid

