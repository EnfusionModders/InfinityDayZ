proto native void GlobalFnTest(string someData);
proto string GlobalNonNativeFn(string someData);

class ExampleClass
{
	static ref ExampleClass m_Instance; //Reference to our engine created instance so it never gets deleted!

	static ref map<int, ref QueuedPlayer> m_ServerQueue = new map<int, ref QueuedPlayer>;

	static proto string TestStaticFunction(string someStr, array<string> someArr);
	static proto native string TestStaticNativeFunction(string someData);

	proto void DynamicProtoMethod(PlayerIdentity pid);
	proto native void DynamicProtoNativeMethod(PlayerIdentity pid);

	//A dynamic class instance is created by our C++ plugin
	private void ExampleClass(){
		m_Instance = this;
	}

	//Class cannot be destroyed via Enforce
	private void ~ExampleClass();

	string JtMethod(string myMsg)
	{
		Print("Ez pz DZ has been spanked - > " + myMsg);
		return "here's some return for you from Enforce!";
	}

	//Called by C++ plugin
	private static void OnAddToQueue(int dpid, string name, string steam64Id, int posInQueue)
	{
		m_ServerQueue.Insert(dpid, new QueuedPlayer(dpid, name, steam64Id, posInQueue));
		string pLog = string.Format("Added (%1) %2 at queue pos: %3", dpid, name, posInQueue);
		Print("AddToQueue: " + pLog);
	}

	//Called by C++ plugin
	private static void OnRemoveFromQueue(int dpid)
	{
		QueuedPlayer player = m_ServerQueue.Get(dpid);
		
		if (!player)
		{
			Print("Attempted to remove dpid: " + dpid + " from queue couldn't be found!");
			return;
		}

		string pLog = string.Format("Removed (%1) %2 from queue pos: %3", dpid, player.m_name, player.m_PosInQueue);
		Print("RemoveFromQueue: " + pLog);

		m_ServerQueue.Remove(dpid);
	}
}

class QueuedPlayer
{
	int m_dpid; //unique session Id
	string m_name; //Player name
	string m_Steam64Id; //Unique Steam64 ID
	int m_PosInQueue;

	void QueuedPlayer(int dpid, string name, string steam64Id, int posInQueue)
	{
		m_dpid 			= dpid;
		m_name 			= name;
		m_Steam64Id 	= steam64Id;
		m_PosInQueue 	= posInQueue;
	}
}