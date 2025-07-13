modded class MissionServer
{
	ref ExampleClass m_egInst;

	void MissionServer()
	{
		m_egInst = new ExampleClass();
	}

	override void OnEvent(EventType eventTypeId, Param params)
	{
		super.OnEvent(eventTypeId, params);

		if (eventTypeId == ClientReadyEventTypeID)
		{
			PlayerIdentity _identity;

			ClientReadyEventParams _params;
			Class.CastTo(_params, params);

			_identity = _params.param1;

			if (_identity)
			{
				m_egInst.DynamicProtoNativeMethod(_identity);
				m_egInst.DynamicProtoMethod(_identity);
			}
		}
	}
}