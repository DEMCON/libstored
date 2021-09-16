int8		default int8
int16		default int16
int32		default int32
int64		default int64
uint8		default uint8
uint16		default uint16
uint32		default uint32
uint64		default uint64
float		default float
double		default double
bool		default bool
ptr32		default ptr32
ptr64		default ptr64
blob:5		default blob
string:10	default string

int32=42	init decimal
int32=-42	init negative
int32=0x54	init hex
int32=0b101	init bin
bool=true	init true
bool=False	init false
bool=0		init bool 0
bool=10		init bool 10
float=1		init float 1
float=3.14	init float 3.14
float=-4.0e3	init float -4000
float=nan	init float nan
float=inf	init float inf
float=-Infinity	init float neg inf

(double)	f read/write
(uint16)	f read-only
(string:4)	f write-only

bool[2]=True	array bool
bool=False	array bool[2]
string:4[3]	array string
(int32)[4]	array f int
(blob:2)[2]	array f blob
float[1]=3	array single

{
	bool inner bool
	int32 inner int
} scope

{
	bool some other inner bool
} some other scope

float value with unit (km/s)
float value with complex unit (J/s/m^2)
float value with abiguous unit (m/s)
float value with abiguous unit (m/h)

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
	float=3.5 gain
	float=nan override
	float output
} small amp

{
	float=-1 gain
	bool=false enable
	float output
} ambiguous amp

{
	double=-3 gain
} double amp

