modded class DayZGame
{
	ref array<string> m_someArray;

	void DayZGame()
	{
		//Global
		GlobalFnTest("Hello C++");
		Print(GlobalNonNativeFn("HEY C++!"));

		//Static class defined engine functions
		m_someArray = new array<string>;
		m_someArray.Insert("hi c++!");

		Print(ExampleClass.TestStaticFunction("hello from Enforce!", m_someArray)); //proto

		m_someArray.Debug(); //C++ would have populated our array with some more data

		Print(ExampleClass.TestStaticNativeFunction("hey there from enforce!")); //proto native
	}
}

string MyGlobalMethod(string someData)
{
	Print("C++ said: " + someData);
	return "Hi there plugin!";
}