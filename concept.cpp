#include <iostream>
#include <concepts>

// ways of doing concepts in cpp 20+

template <typename T>
void print_number1(T n)
{
	static_assert(std::is_integral<T>::value, "Must pass in an integral argument");
	std::cout << "n: " << n << std::endl;
}

template <typename T>
requires std::integral<T>
T add_number1(T a, T b)
{
	return a + b;
}

template <typename T>
requires std::is_integral_v<T>
T add_number2(T a, T b)
{
	return a + b;
}

template<std::integral T>
T add_number3(T a, T b)
{
	return a + b;
}

auto add_number4(std::integral auto a, std::integral auto b)
{
	return a + b;
}

template <typename T>
T add_number5(T a, T b) requires std::integral<T>
{
	return a + b;
}

// custom concept

template <typename T>
concept MyIntegral = std::is_integral_v<T>;

template <typename T>
concept Multipliable = requires (T a, T b)
{
	a * b; // makes sure the syntax is valid
};

template <typename T>
concept Incrementable = requires (T a)
{
	a += 1;
	++a;
	a++;
};

template <typename T>
requires MyIntegral<T>
T add_number6(T a, T b)
{
	return a + b;
}

template <MyIntegral T>
T add_number7(T a, T b)
{
	return a + b;
}

auto add_number8(MyIntegral auto a, MyIntegral auto b)
{
	return a + b;
}

// check the size in concept
template <typename T>
concept TinyType = requires(T a, T b)
{
	sizeof(T) <= 4; // always True because cencept is checking sytanx
	requires sizeof(T) <= 4; // nested requirement
	{a + b} noexcept -> std::convertible_to<int>; // compond requirement
};


// template override - does not work :'(
// template <>
// requires std::floating_point<T>
// double add_number2(double x, double y)
// {
// 	return x + y;
// }

int main()
{
	// print_number1(30.2);
	// std::cout << add_number1(1.0, 4.2) << std::endl;
	// std::cout << add_number2(1.0, 4.2) << std::endl;
	// std::cout << add_number3(1.0, 4.2) << std::endl;
	// std::cout << add_number4(1.0, 4.2) << std::endl;
	// std::cout << add_number5(1.0, 4.2) << std::endl;


	return 0;
}
