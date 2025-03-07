#pragma once
#include <Player.h>

namespace ALYSLC
{
	namespace Serialization
	{
		// Get the form that corresponds to the retrieved serialized FID.
		RE::TESForm* GetFormFromRetrievedFID
		(
			SKSE::SerializationInterface* a_intfc, 
			RE::FormID& a_fid, 
			RE::TESDataHandler* a_dataHandler
		);

		// Called when a save is loaded. Load all of our serialized data.
		void Load(SKSE::SerializationInterface* a_intfc);

		// Attempt to retrieve a float value with the given record type 
		// and store in the data outparam.
		void RetrieveFloatData
		(
			SKSE::SerializationInterface* a_intfc, float& a_data, const uint32_t& a_recordType
		);

		// Attempt to retrieve a fixed string of the given size and record type 
		// and store in the data outparam.
		void RetrieveStringData
		(
			SKSE::SerializationInterface* a_intfc,
			RE::BSFixedString& a_data, 
			const uint32_t& a_recordType,
			const uint32_t& a_size
		);

		// Attempt to retrieve an unsigned 8-bit integer with the given record type 
		// and store in the data outparam.
		void RetrieveUInt8Data
		(
			SKSE::SerializationInterface* a_intfc, uint8_t& a_data, const uint32_t& a_recordType
		);

		// Attempt to retrieve an unsigned 16-bit integer with the given record type 
		// and store in the data outparam.
		void RetrieveUInt16Data
		(
			SKSE::SerializationInterface* a_intfc, uint16_t& a_data, const uint32_t& a_recordType
		);

		// Attempt to retrieve an unsigned 32-bit integer with the given record type 
		// and store in the data outparam.
		void RetrieveUInt32Data
		(
			SKSE::SerializationInterface* a_intfc, uint32_t& a_data, const uint32_t& a_recordType
		);

		// Called on "PreLoadGame' and when going back to the main menu. 
		// Tear down any active co-op session.
		void Revert(SKSE::SerializationInterface* a_intfc);

		// Called when the game is saved. Save all our serializable data.
		void Save(SKSE::SerializationInterface* a_intfc);

		// Attempt to serialize a float value of the given record type.
		void SerializePlayerFloatData
		(
			SKSE::SerializationInterface* a_intfc,
			const float& a_data, 
			const uint32_t& a_recordType
		);

		// Attempt to serialize a string of the given record type.
		void SerializePlayerStringData
		(
			SKSE::SerializationInterface* a_intfc,
			const RE::BSFixedString& a_data,
			const uint32_t& a_recordType
		);

		// Attempt to serialize an unsigned 8-bit integer of the given record type.
		void SerializePlayerUInt8Data
		(
			SKSE::SerializationInterface* a_intfc, 
			const uint8_t& a_data, 
			const uint32_t& a_recordType
		);

		// Attempt to serialize an unsigned 16-bit integer of the given record type.
		void SerializePlayerUInt16Data
		(
			SKSE::SerializationInterface* a_intfc, 
			const uint16_t& a_data, 
			const uint32_t& a_recordType
		);

		// Attempt to serialize an unsigned 32-bit integer of the given record type.
		void SerializePlayerUInt32Data
		(
			SKSE::SerializationInterface* a_intfc, 
			const uint32_t& a_data,
			const uint32_t& a_recordType
		);

		// Serialize default player data if no data exists for all players,
		// or if there was an error getting the serialization interface.
		void SetDefaultRetrievedData(SKSE::SerializationInterface* a_intfc);

		// Returns the name of a serializable data type.
		std::string TypeToString(uint32_t a_type);

		// Mutex for reading/writing serializable data.
		static inline std::mutex serializationMutex;
	};
};

