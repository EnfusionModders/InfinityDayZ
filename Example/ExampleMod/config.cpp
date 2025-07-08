class CfgPatches
{
	class DZM_ExampleMod
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
		};
	};
};
class CfgMods
{
	class ExampleMod
	{
		dir="ExampleMod";
		hideName=0;
		hidePicture=0;
		name="Infinity DayZ Example";
		credits="DaOne";
		creditsJson="InfinityDayZ/Example/ExampleMod/data/Credits.json";
		author="DaOne";
		authorID="420420";
		inputs="InfinityDayZ/Example/ExampleMod/data/modded_Inputs.xml";
		version="Version 1.0";
		extra=0;
		type="mod";
		picture="InfinityDayZ/Example/ExampleMod/data/vpp_logo_m.paa";
		logoSmall="InfinityDayZ/Example/ExampleMod/data/vpp_logo_ss.paa";
		logo="InfinityDayZ/Example/ExampleMod/data/vpp_logo_s.paa";
		logoOver="InfinityDayZ/Example/ExampleMod/data/vpp_logo_s.paa";
		tooltip="";
		overview="";
		action="https://discord.gg/TTYd9mA";
		dependencies[]=
		{
			"Game",
			"World",
			"Mission"
		};
		class defs
		{
			class imageSets
			{
				files[]=
				{
				};
			};
			class gameScriptModule
			{
				value="";
				files[]=
				{
					"InfinityDayZ/Example/ExampleMod/Definitions",
					"InfinityDayZ/Example/ExampleMod/3_Game"
				};
			};
			class worldScriptModule
			{
				value="";
				files[]=
				{
					"InfinityDayZ/Example/ExampleMod/Definitions",
					"InfinityDayZ/Example/ExampleMod/4_World"
				};
			};
			class missionScriptModule
			{
				value="CreateMissionType";
				files[]=
				{
					"InfinityDayZ/Example/ExampleMod/Definitions",
					"InfinityDayZ/Example/ExampleMod/5_Mission"
				};
			};
		};
	};
};