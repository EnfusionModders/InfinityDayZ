modded class DayZGame
{
	void DayZGame()
	{
		//Global
		GlobalFnTest("Hello C++");
		Print(GlobalNonNativeFn("HEY C++!"));

		//Static class defined engine functions
		Print(ExampleClass.TestFunction("hello from Enforce!")); //proto
		Print(ExampleClass.TestStaticNativeMethod("hey there from enforce!")); //proto native
	}
}