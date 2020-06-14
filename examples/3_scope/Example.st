// Scopes are surrounded by { } and have a name.  You can see the scope as a
// special type.  Within the scope, any variable can exist like at the
// top-level, including subscopes.
{
	bool some bool
	int8=123 an int
	{
		bool another bool
	} subscope
} scope

// Scopes can be an array too, like all other variables.
{
	int8 i
}[2] two scopes

// When accessed variables by string using find(), separate scopes by /, like a
// Unix filesystem. Now the quirky part: a / can also be part of a name, which
// implicitly creates a scope. In the following example, /top/level1/b and
// /top/level1/b2 are normal siblings in the generated code.
{
	{
		bool b
	} level1
	bool level1/b2
} top

// If you create the following variable, it will add a scope, but that's fine.
double=9.81 gravitational constant (m/s^2)

