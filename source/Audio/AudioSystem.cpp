#include "AudioSystem.hpp"
#include <algorithm>
#include "../Core/Log.hpp"
#include "../Core/RootDirectory.hpp"
#include "../World/ComponentManager.hpp"
#include "AudioMacros.hpp"
#include "AudioSourceComponentLifetimePolicy.hpp"
#include <stdio.h>
namespace Mona {
	void AudioSystem::StartUp(unsigned int channels) noexcept {
		MONA_ASSERT(channels > 0, "AudioSystem Error: please request more than zero channels");
		m_audioDevice = alcOpenDevice(nullptr);
		if (!m_audioDevice) {
			MONA_LOG_ERROR("AudioSystem Error: Failed to open audio device.");
			return;
		}
		m_audioContext = alcCreateContext(m_audioDevice, NULL);
		if (!alcMakeContextCurrent(m_audioContext)) {
			MONA_LOG_ERROR("AudioSystem Error: Failed to make audio context current.");
			return;
		}

		ALCALL(alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED));
		m_masterVolume = 1.0f;
		ALCALL(alListenerf(AL_GAIN, m_masterVolume));
		m_channels = channels;
		m_openALSources.reserve(channels);
		for (unsigned int i = 0; i < channels; i++) {
			ALuint source = 0;
			ALCALL(alGenSources(1, &source));
			m_openALSources.emplace_back(source, i + 1);
		}
		m_firstFreeOpenALSourceIndex = 0;

	}

	void AudioSystem::ShutDown() noexcept {

		alcMakeContextCurrent(NULL);
		alcDestroyContext(m_audioContext);
		alcCloseDevice(m_audioDevice);
	}

	void AudioSystem::Update(const InnerComponentHandle& audioListenerTransformHandle,
		float timeStep,
		const TransformComponent::managerType& transformDataManager,
		AudioSourceComponent::managerType& audioDataManager) noexcept {

		glm::vec3 listenerPosition = glm::vec3(0.0f);
		if (transformDataManager.IsValid(audioListenerTransformHandle)) {
			const TransformComponent* listenerTransform = transformDataManager.GetComponentPointer(audioListenerTransformHandle);
			listenerPosition = listenerTransform->GetLocalTranslation();
			UpdateListener(listenerPosition, listenerTransform->GetFrontVector(), listenerTransform->GetUpVector());
		}
		else {
			UpdateListener(listenerPosition, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		}

		RemoveCompletedFreeAudioSources();
		UpdateFreeAudioSourcesTimers(timeStep);
		UpdateAudioSourceComponentsTimers(timeStep, audioDataManager);

		if (m_freeAudioSources.size() + audioDataManager.GetCount() <= m_openALSources.size()) {
			AssignOpenALSourceToFreeAudioSources(m_freeAudioSources.begin(), m_freeAudioSources.end());
			AssignOpenALSourceToAudioSourceComponents(audioDataManager, transformDataManager, 0, audioDataManager.GetCount());
		}
		else {
			auto firstOutFreeSource = PartitionAndRemoveOpenALSourceFromFreeAudioSources(listenerPosition);
			uint32_t firstOutAudioComponent = PartitionAndRemoveOpenALSourceFromAudioSourceComponents(audioDataManager, 
				transformDataManager, listenerPosition);
			if (std::distance(m_freeAudioSources.begin(), firstOutFreeSource) + firstOutAudioComponent <= m_openALSources.size()) {
				AssignOpenALSourceToFreeAudioSources(m_freeAudioSources.begin(), firstOutFreeSource);
				AssignOpenALSourceToAudioSourceComponents(audioDataManager, transformDataManager, 0, firstOutAudioComponent);
			}
			else {

				uint32_t countFreeSources[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)] = {};
				uint32_t countAudioComponents[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)] = {};
				SortFreeAudioSourcesByPriority(firstOutFreeSource, countFreeSources);
				SortAudioSourceComponentsByPriority(audioDataManager, firstOutAudioComponent, countAudioComponents);

				uint32_t firstToRemoveFreeSource = 0;
				uint32_t firstToRemoveSourceComponent = 0;
				uint32_t sourceCount = m_openALSources.size();
				for (int i = 0; i < static_cast<unsigned int>(AudioSourcePriority::PriorityCount); i++) {
					if (sourceCount <= countAudioComponents[i]) {
						firstToRemoveSourceComponent += sourceCount;
						break;
					}
					firstToRemoveSourceComponent += countAudioComponents[i];
					sourceCount -= countAudioComponents[i];

					if (sourceCount <= countFreeSources[i]) {
						firstToRemoveFreeSource += sourceCount;
						break;
					}
					firstToRemoveFreeSource += countFreeSources[i];
					sourceCount -= countFreeSources[i];

				}
				RemoveOpenALSourceFromFreeAudioSources(m_freeAudioSources.begin() + firstToRemoveFreeSource, m_freeAudioSources.end());
				RemoveOpenALSourceFromAudioSourceComponents(audioDataManager, firstToRemoveSourceComponent, audioDataManager.GetCount());
				AssignOpenALSourceToFreeAudioSources(m_freeAudioSources.begin(), m_freeAudioSources.begin() + firstToRemoveFreeSource);
				AssignOpenALSourceToAudioSourceComponents(audioDataManager, transformDataManager, 0, firstToRemoveSourceComponent);

				
				
			}
		}
	}

	float AudioSystem::GetMasterVolume() const noexcept {
		return m_masterVolume;
	}

	void AudioSystem::SetMasterVolume(float volume) noexcept {
		m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
		ALCALL(alListenerf(AL_GAIN, m_masterVolume));
	}

	void AudioSystem::PlayAudioClip3D(std::shared_ptr<AudioClip> audioClip,
		const glm::vec3& position,
		float volume,
		float pitch,
		float radius,
		AudioSourcePriority priority)
	{
		if (audioClip == nullptr) return;
		m_freeAudioSources.emplace_back(audioClip,
			std::clamp(volume, 0.0f, 1.0f),
			std::max(0.0f, pitch),
			std::max(0.0f, radius),
			priority,
			SourceType::Source3D,
			position);
	}

	void AudioSystem::PlayAudioClip2D(std::shared_ptr<AudioClip> audioClip,
		float volume,
		float pitch,
		AudioSourcePriority priority)
	{
		if (audioClip == nullptr) return;
		m_freeAudioSources.emplace_back(audioClip,
			std::clamp(volume, 0.0f, 1.0f),
			std::max(0.0f, pitch),
			1.0f,
			priority,
			SourceType::Source2D);
	}

	void AudioSystem::ClearSources() noexcept {
		for (auto& openALSource : m_openALSources) {
			ALCALL(alDeleteSources(1, &(openALSource.m_sourceID)));
		}
	}

	void AudioSystem::RemoveOpenALSource(uint32_t index) noexcept {
		auto& alSource = m_openALSources[index];
		ALCALL(alSourcef(alSource.m_sourceID, AL_GAIN, 0.0f));
		ALCALL(alSourceStop(alSource.m_sourceID));
		ALCALL(alSourcei(alSource.m_sourceID, AL_BUFFER, 0));
		FreeOpenALSource(index);
	}

	void AudioSystem::UpdateListener(const glm::vec3& position, const glm::vec3& frontVector, const glm::vec3& upVector) {
		ALCALL(alListener3f(AL_POSITION, position.x, position.y, position.z));
		ALfloat forwardAndUpVectors[] = {
			frontVector.x, frontVector.y, frontVector.z,
			upVector.x, upVector.y, upVector.z
		};
		ALCALL(alListenerfv(AL_ORIENTATION, forwardAndUpVectors));
	}

	void AudioSystem::RemoveCompletedFreeAudioSources() {
		auto beginRemove = std::partition(m_freeAudioSources.begin(), m_freeAudioSources.end(),
			[](const FreeAudioSource& audioSource) {
				return audioSource.m_timeLeft > 0.0f;
			});

		for (auto it = beginRemove; it != m_freeAudioSources.end(); it++) {
			if (it->m_openALsource) {
				AudioSource::OpenALSource openALSource = it->m_openALsource.value();
				ALCALL(alSourceStop(openALSource.m_sourceID));
				ALCALL(alSourcei(openALSource.m_sourceID, AL_BUFFER, 0));
				FreeOpenALSource(openALSource.m_sourceIndex);
			}
		}
		m_freeAudioSources.resize(std::distance(m_freeAudioSources.begin(), beginRemove));
	}

	void AudioSystem::UpdateFreeAudioSourcesTimers(float timeStep) {
		for (auto& freeAudioSource : m_freeAudioSources) {
			freeAudioSource.m_timeLeft -= timeStep * freeAudioSource.m_pitch;
		}
	}

	void AudioSystem::UpdateAudioSourceComponentsTimers(float timeStep, AudioSourceComponent::managerType& audioDataManager)
	{
		for (uint32_t i = 0; i < audioDataManager.GetCount(); i++) {
			auto& audioComponent = audioDataManager[i];
			if (audioComponent.m_sourceState == AudioSourceState::Paused || audioComponent.m_sourceState == AudioSourceState::Stopped) continue;
			if (audioComponent.m_timeLeft < 0) audioComponent.m_sourceState = AudioSourceState::Stopped;
			audioComponent.m_timeLeft -= timeStep * audioComponent.m_pitch;
			if (audioComponent.m_isLooping) {
				while (audioComponent.m_timeLeft < 0) {
					audioComponent.m_timeLeft += audioComponent.m_audioClip->GetTotalTime();
				}
			}
		}
	}

	std::vector<FreeAudioSource>::iterator AudioSystem::PartitionAndRemoveOpenALSourceFromFreeAudioSources(const glm::vec3& listenerPosition)
	{
		auto firstOutOfRange = std::partition(m_freeAudioSources.begin(), m_freeAudioSources.end(),
			[&listenerPosition](const FreeAudioSource& audioSource) {
				return audioSource.m_sourceType == SourceType::Source2D
					|| audioSource.m_radius * audioSource.m_radius > glm::length2(audioSource.m_position - listenerPosition);
			});

		for (auto it = firstOutOfRange; it != m_freeAudioSources.end(); it++) {
			if (it->m_openALsource) {
				AudioSource::OpenALSource openALSource = it->m_openALsource.value();
				ALCALL(alSourcef(openALSource.m_sourceID, AL_GAIN, 0.0f));
				ALCALL(alSourceStop(openALSource.m_sourceID));
				ALCALL(alSourcei(openALSource.m_sourceID, AL_BUFFER, 0));
				FreeOpenALSource(openALSource.m_sourceIndex);
				it->m_openALsource = std::nullopt;
			}
		}

		return firstOutOfRange;
	}

	uint32_t AudioSystem::PartitionAndRemoveOpenALSourceFromAudioSourceComponents(AudioSourceComponent::managerType& audioDataManager, const TransformComponent::managerType& transformDataManager, const glm::vec3& listenerPosition)
	{
		uint32_t currentIndex = 0;
		uint32_t backIndex = audioDataManager.GetCount() - 1;
		while (currentIndex <= backIndex) {
			AudioSourceComponent& audioSource = audioDataManager[currentIndex];
			const TransformComponent* transform = transformDataManager.GetComponentPointer(audioSource.m_transformHandle);
			const glm::vec3& position = transform->GetLocalTranslation();
			float squareRadius = audioSource.m_radius * audioSource.m_radius;
			float squareDistance = glm::distance2(position, listenerPosition);
			if (audioSource.m_sourceState == AudioSourceState::Playing &&
				squareDistance <= squareRadius) {
				currentIndex++;
			}
			else {
				audioDataManager.SwapComponents(currentIndex, backIndex);
				backIndex--;
			}
		}
		for (uint32_t i = currentIndex; i < audioDataManager.GetCount(); i++) {
			AudioSourceComponent& audioSource = audioDataManager[i];
			if (audioSource.m_openALsource) {
				AudioSource::OpenALSource openALSource = audioSource.m_openALsource.value();
				ALCALL(alSourcef(openALSource.m_sourceID, AL_GAIN, 0.0f));
				ALCALL(alSourceStop(openALSource.m_sourceID));
				ALCALL(alSourcei(openALSource.m_sourceID, AL_BUFFER, 0));
				FreeOpenALSource(openALSource.m_sourceIndex);
				audioSource.m_openALsource = std::nullopt;
			}
		}
		return currentIndex;
	}

	void AudioSystem::AssignOpenALSourceToFreeAudioSources(std::vector<FreeAudioSource>::iterator begin, std::vector<FreeAudioSource>::iterator end)
	{
		for (auto it = begin; it != end; it++) {
			if (!it->m_openALsource) {
				auto unusedOpenALSource = GetNextFreeSource();
				it->m_openALsource = unusedOpenALSource;
				if (it->m_sourceType == SourceType::Source2D) {
					ALCALL(alSourcei(unusedOpenALSource.m_sourceID, AL_SOURCE_RELATIVE, AL_TRUE));
					ALCALL(alSource3f(unusedOpenALSource.m_sourceID, AL_POSITION, 0.0f, 0.0f, 0.0f));
				}
				else {
					const glm::vec3& position = it->m_position;
					ALCALL(alSource3f(unusedOpenALSource.m_sourceID, AL_POSITION, position.x, position.y, position.z));
				}
				ALCALL(alSourcei(unusedOpenALSource.m_sourceID, AL_LOOPING, AL_FALSE));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_PITCH, it->m_pitch));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_GAIN, it->m_volume));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_MAX_DISTANCE, it->m_radius));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_REFERENCE_DISTANCE, it->m_radius * 0.2f));
				ALCALL(alSourcei(unusedOpenALSource.m_sourceID, AL_BUFFER, it->m_audioClip->GetBufferID()));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_SEC_OFFSET, it->m_audioClip->GetTotalTime() - it->m_timeLeft));
				ALCALL(alSourcePlay(unusedOpenALSource.m_sourceID));

			}
		}



	}

	void AudioSystem::AssignOpenALSourceToAudioSourceComponents(AudioSourceComponent::managerType& audioDataManager, const TransformComponent::managerType& transformDataManager, uint32_t firstIndex, uint32_t lastIndex)
	{
		for (uint32_t i = firstIndex; i < lastIndex; i++) {
			AudioSourceComponent& audioSource = audioDataManager[i];
			if (!audioSource.m_openALsource) {
				auto unusedOpenALSource = GetNextFreeSource();
				audioSource.m_openALsource = unusedOpenALSource;
				if (audioSource.m_sourceType == SourceType::Source2D) {
					ALCALL(alSourcei(unusedOpenALSource.m_sourceID, AL_SOURCE_RELATIVE, AL_TRUE));
					ALCALL(alSource3f(unusedOpenALSource.m_sourceID, AL_POSITION, 0.0f, 0.0f, 0.0f));
				}
				ALCALL(alSourcei(unusedOpenALSource.m_sourceID, AL_LOOPING, audioSource.m_isLooping));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_PITCH, audioSource.m_pitch));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_GAIN, audioSource.m_volume));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_MAX_DISTANCE, audioSource.m_radius));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_REFERENCE_DISTANCE, audioSource.m_radius * 0.2f));
				ALCALL(alSourcei(unusedOpenALSource.m_sourceID, AL_BUFFER, audioSource.m_audioClip->GetBufferID()));
				ALCALL(alSourcef(unusedOpenALSource.m_sourceID, AL_SEC_OFFSET, audioSource.m_audioClip->GetTotalTime() - audioSource.m_timeLeft));
				if (audioSource.m_sourceState == AudioSourceState::Playing) {
					ALCALL(alSourcePlay(unusedOpenALSource.m_sourceID));
				}

			}
			const TransformComponent* transform = transformDataManager.GetComponentPointer(audioSource.m_transformHandle);
			const glm::vec3& position = transform->GetLocalTranslation();
			ALCALL(alSource3f(audioSource.m_openALsource.value().m_sourceID, AL_POSITION, position.x, position.y, position.z));


		}
	}
	void AudioSystem::RemoveOpenALSourceFromFreeAudioSources(std::vector<FreeAudioSource>::iterator begin, std::vector<FreeAudioSource>::iterator end)
	{
		for (auto it = begin; it != end; it++) {
			if (it->m_openALsource) {
				AudioSource::OpenALSource openALSource = it->m_openALsource.value();
				ALCALL(alSourcef(openALSource.m_sourceID, AL_GAIN, 0.0f));
				ALCALL(alSourceStop(openALSource.m_sourceID));
				ALCALL(alSourcei(openALSource.m_sourceID, AL_BUFFER, 0));
				FreeOpenALSource(openALSource.m_sourceIndex);
				it->m_openALsource = std::nullopt;
			}
		}
	}

	void AudioSystem::RemoveOpenALSourceFromAudioSourceComponents(AudioSourceComponent::managerType& audioDataManager,
		uint32_t firstIndex, uint32_t lastIndex)
	{
		for (uint32_t i = firstIndex; i < lastIndex; i++) {
			AudioSourceComponent& audioSource = audioDataManager[i];
			if (audioSource.m_openALsource) {
				AudioSource::OpenALSource openALSource = audioSource.m_openALsource.value();
				ALCALL(alSourcef(openALSource.m_sourceID, AL_GAIN, 0.0f));
				ALCALL(alSourceStop(openALSource.m_sourceID));
				ALCALL(alSourcei(openALSource.m_sourceID, AL_BUFFER, 0));
				FreeOpenALSource(openALSource.m_sourceIndex);
				audioSource.m_openALsource = std::nullopt;
			}
		}
	}

	void AudioSystem::SortFreeAudioSourcesByPriority(std::vector<FreeAudioSource>::iterator end,
		uint32_t (&outCount)[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)])
	{
		unsigned int counts[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)] = {};
		unsigned int offsetCounts[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)] = {};

		for (auto it = m_freeAudioSources.begin(); it != end; it++) {
			counts[static_cast<unsigned int>(it->m_priority)]++;
			outCount[static_cast<unsigned int>(it->m_priority)]++;
		}
		for (int i = 1; i < static_cast<unsigned int>(AudioSourcePriority::PriorityCount); i++) {
			counts[i] += counts[i - 1];
			offsetCounts[i] = counts[i - 1];
		}
		auto begin = m_freeAudioSources.begin();
		for (auto it = begin; it != end; it++) {
			unsigned int priorityIndex = static_cast<unsigned int>(it->m_priority);

			while (std::distance(begin, it) != offsetCounts[priorityIndex] &&
				counts[priorityIndex] != offsetCounts[priorityIndex]) {
				if(it->m_priority != (begin + offsetCounts[priorityIndex])->m_priority)
					std::iter_swap(it, begin + offsetCounts[priorityIndex]);
				++offsetCounts[priorityIndex];
				unsigned int priorityIndex = static_cast<unsigned int>(it->m_priority);
			}
		}
	}
	void AudioSystem::SortAudioSourceComponentsByPriority(AudioSourceComponent::managerType& audioDataManager,
		uint32_t lastIndex,
		uint32_t(&outCount)[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)]) {

		unsigned int counts[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)] = {};
		unsigned int offsetCounts[static_cast<unsigned int>(AudioSourcePriority::PriorityCount)] = {};
		for (uint32_t i = 0; i < lastIndex; i++) {
			unsigned int priorityIndex = static_cast<unsigned int>(audioDataManager[i].m_priority);
			counts[priorityIndex]++;
			outCount[priorityIndex]++;
		}
		for (int i = 1; i < static_cast<unsigned int>(AudioSourcePriority::PriorityCount); i++) {
			counts[i] += counts[i - 1];
			offsetCounts[i] = counts[i - 1];
		}


		for (uint32_t i = 0; i < lastIndex; i++) {
			unsigned int priorityIndex = static_cast<unsigned int>(audioDataManager[i].m_priority);
			while ( i!= offsetCounts[priorityIndex] &&
				counts[priorityIndex] != offsetCounts[priorityIndex]) 
			{
				if(audioDataManager[i].m_priority != audioDataManager[offsetCounts[priorityIndex]].m_priority)
					audioDataManager.SwapComponents(i, offsetCounts[priorityIndex]);
				++offsetCounts[priorityIndex];
				unsigned int priorityIndex = static_cast<unsigned int>(audioDataManager[i].m_priority);
			}
		}
	}

	void AudioSystem::FreeOpenALSource(uint32_t index) {
		auto& freeEntry = m_openALSources[index];
		if (m_firstFreeOpenALSourceIndex == m_channels) {
			m_firstFreeOpenALSourceIndex = index;
			freeEntry.m_nextFreeIndex = m_channels;
		}
		else {
			auto& firstFreeEntry = m_openALSources[m_firstFreeOpenALSourceIndex];
			freeEntry.m_nextFreeIndex = m_firstFreeOpenALSourceIndex;
			m_firstFreeOpenALSourceIndex = index;
		}

	}

	AudioSource::OpenALSource AudioSystem::GetNextFreeSource()
	{
		MONA_ASSERT(m_firstFreeOpenALSourceIndex != m_channels, "AudioSystem Error: Not enough openal sources available");
		auto& entry = m_openALSources[m_firstFreeOpenALSourceIndex];
		uint32_t index = m_firstFreeOpenALSourceIndex;
		m_firstFreeOpenALSourceIndex = entry.m_nextFreeIndex;
		return AudioSource::OpenALSource(entry.m_sourceID, index);
	}


}