proto native void GlobalFnTest(string someData);
proto string GlobalNonNativeFn(string someData);

class ExampleClass
{
	static proto string TestFunction(string someStr);

	static proto native string TestStaticNativeMethod(string someData);
	
	proto void TestMethod();
	proto native void BigMethod(PlayerIdentity pid);

	string JtMethod(string myMsg)
	{
		Print("Ez pz DZ has been spanked - > " + myMsg);
		return "here's some return for you from Enforce!";
	}
}