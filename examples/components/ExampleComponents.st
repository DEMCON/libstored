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
	float=1e-3 drag coefficient

	float=0 height (m)
	float=0 speed (m/s)
} helicopter

{
	(float) frequency (Hz)
	float y
	float setpoint
	bool=true enable
	float=1 Kp
	float=inf Ti (s)
	float=0 Td (s)
	float=0 Kff
	float int
	float=-inf int low
	float=inf int high
	float=-inf low
	float=inf high
	bool reset
	float=nan override
	float u
} pid

