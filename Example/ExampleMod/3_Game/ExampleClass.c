proto native void GlobalFnTest(string someData);
proto string GlobalNonNativeFn(string someData);

class ExampleClass
{
	static proto string TestStaticFunction(string someStr, array<string> someArr);
	static proto native string TestStaticNativeFunction(string someData);
	
	proto void DynamicProtoMethod(PlayerIdentity pid);
	proto native void DynamicProtoNativeMethod(PlayerIdentity pid);

	string JtMethod(string myMsg)
	{
		Print("Ez pz DZ has been spanked - > " + myMsg);
		return "here's some return for you from Enforce!";
	}
}