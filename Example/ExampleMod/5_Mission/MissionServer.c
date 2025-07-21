modded class MissionServer
{
	void MissionServer()
	{
	}

	override void OnEvent(EventType eventTypeId, Param params)
	{
		super.OnEvent(eventTypeId, params);

		if (eventTypeId == ClientReadyEventTypeID)
		{
			PlayerIdentity _identity;
			PlayerBase _player;

			ClientReadyEventParams _params;
			Class.CastTo(_params, params);

			_identity = _params.param1;
			Class.CastTo(_player, _params.param2);

			if (_identity && _player)
			{
				ExampleClass.m_Instance.DynamicProtoNativeMethod(_identity);
				ExampleClass.m_Instance.DynamicProtoMethod(_identity);
			}
		}
	}
}